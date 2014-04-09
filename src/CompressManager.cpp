/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include <glog/logging.h>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>

#include "CompressManager.hpp"
#include "Utils.hpp"
#include "Messages.hpp"

namespace csio {


/**
 * DZIP file structure:
 *
 * 	+=============+=============+ ... +=============+
 * 	|DZIP_MEMBER_1|DZIP_MEMBER_2|     |DZIP_MEMBER_N|
 * 	+=============+=============+ ... +=============+
 *
 * DZIP_MEMBER:
 *
 * 	+---+---+---+---+---+---+---+---+---+---+---+---+
 * 	|x1F|x8B|x08|FLG|     MTIME     |XFL|OS | XLEN  |->
 * 	+---+---+---+---+---+---+---+---+---+---+---+---+
 * 	+===========+===========+===========+---+---+---+---+---+---+---+---+
 * 	| RA_EXTRA  | FNAME     | CHUNKS    | CRC32         | SIZE          |
 * 	+===========+===========+===========+---+---+---+---+---+---+---+---+
 *
 * 	FLG      - flags. FEXTRA|FNAME is used
 * 	MTIME    - modification time of the original file
 * 	XFL      - extra flags about the compression.
 * 	OS       - operating system
 * 	XLEN     - total extra fields length (RA_EXTRA)
 * 	RA_EXTRA - RFC1952 formated Random Access header's extra field (later)
 * 	FNAME    - zero terminated string - base (without directory) file name
 * 	CHUNKS   - compressed file chunks (lengths are stored in RA_EXTRA)
 * 	CRC32    - CRC-32
 * 	SIZE     - data size in this member (unpacked)
 *
 * RA_EXTRA:
 *
 * 	+---+---+---+---+---+---+---+---+---+---+============+
 * 	|x52|x41| EXLEN | VER=1 | CHLEN | CHCNT | CHUNK_DATA |
 * 	+---+---+---+---+---+---+---+---+---+---+============+
 *
 * 	EXLEN      - length of VER, CHLEN, CHCNT and CHUNK_DATA summary
 * 	CHUNK_DATA - CHCNT 2-bytes lengths of compressed chunks
 * 	CHLEN      - the length of one uncompressed chunk
 * 	CHCNT      - count of 2-bytes lengths in CHUNK_DATA
 *
 * Only first member has valid MTIME and FNAME.
 * */


CompressManager::CompressManager(const Config& cfg)
	: zmq_ctx_(zmq_init(0))
	, sock_jobs_(NULL)
	, sock_feedback_(NULL)
	, sock_writer_(NULL)
	, cfg_(cfg)
	, hwm_(cfg_.CompressorsCount()*2 + 5)
	, ifd_(-1)
	, ofd_(-1)
	, stop_(false)
	, CHUNKS_HIGH_WATERMARK(cfg.CompressorsCount()*2)
	, pushing_semaphore_(cfg.CompressorsCount()*(-1))
	, pushing_semaphore_min_(0)
	, mtime_(0)
	, ifsize_(0)
	, bytes_compressed_(0)
	, member_bytes_compressed_(0)
	, crc32_(crc32(0L, Z_NULL, 0))
	, writer_instance_(new Writer(zmq_ctx_, hwm_))
{
	if (!zmq_ctx_)
	{
		LOG(ERROR) << _("CompressManager: error creating communication"
		                " context.")
		           << _(" Message: ") << zmq_strerror(errno);
		return;
	}
	for(size_t i = 0; i < cfg_.CompressorsCount(); ++i)
	{
		compressors_instances_.push_back(
		    std::unique_ptr<Compressor>(new Compressor(zmq_ctx_, cfg_)));
	}
};

CompressManager::~CompressManager()
{
	zmq_close(sock_jobs_);
	zmq_close(sock_feedback_);
	zmq_close(sock_writer_);
	zmq_ctx_destroy(zmq_ctx_);
	compressors_instances_.clear();
};

inline std::string
get_base_name(std::string path)
{
	return path;
}

int
CompressManager::makeInitialPush()
{
	rdseq_ = 0;
	std::string bname = get_base_name(cfg_.IFName());
	char msgbuf[MEMBER_HEADER_MINSZ + 0xffff + bname.length() + 1];
	int rs = makeMemberHeaderTemplate(
			*chunks_cnt_, msgbuf, sizeof(msgbuf), bname, mtime_);
	if (rs == -1)
	{
		LOG(ERROR) << _("CompressManager: error creating member"
		                " header.");
		return -1;
	}
	Message first_member_header((uint8_t*)msgbuf, rs, 0);
	first_member_header.Send(sock_writer_);

	char* buf = new char[CHUNK_SIZE*cfg_.CompressorsCount()];
	if (buf == NULL)
	{
		LOG(ERROR) << _("CompressManager:  error initial reading.")
		           << strerror(errno);
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	rs = read(ifd_, buf, CHUNK_SIZE*cfg_.CompressorsCount());
	if (rs == -1)
	{
		LOG(ERROR) << _("CompressManager: error reading file.")
		           << _(" Message: ") << strerror(errno);
		return -1;
	}
	pushing_semaphore_min_ = 0;
	for (; rdseq_ < rs/CHUNK_SIZE; ++rdseq_)
	{
		Message msg((uint8_t*)buf + rdseq_*CHUNK_SIZE, CHUNK_SIZE, rdseq_ + 1);
		if (msg.Send(sock_jobs_, Message::BLOCKING_MODE) < CHUNK_SIZE)
		{
			VLOG(2) << _("CompressManager: error initially"
			             " pushing chunk #") << rdseq_;
			return -1;
		}
		crc32_ = crc32(crc32_, (Bytef*)buf + rdseq_*CHUNK_SIZE, CHUNK_SIZE);
		member_bytes_compressed_ += CHUNK_SIZE;
		--pushing_semaphore_min_;
	}
	if (rs % CHUNK_SIZE > 0)
	{
		Message msg((uint8_t*)buf + rdseq_*CHUNK_SIZE,
				rs % CHUNK_SIZE, rdseq_ + 1);
		if (msg.Send(sock_jobs_, Message::BLOCKING_MODE)
				< rs - rdseq_*CHUNK_SIZE)
		{
			VLOG(2) << _("CompressManager: error initially"
			             " pushing chunk #") << rdseq_;
			return -1;
		}
		crc32_ = crc32(crc32_, (Bytef*)buf + rdseq_*CHUNK_SIZE, rs % CHUNK_SIZE);
		member_bytes_compressed_ += rs % CHUNK_SIZE;
		++rdseq_;
		--pushing_semaphore_min_;
	}
	delete [] buf;
	bytes_compressed_ = rs;
	VLOG(2) << _("CompressManager: initial push with ")
	        << rdseq_ << (" elements.");
	return 0;
}

int
CompressManager::flushChunks()
{
	while (!chunks_.empty() && chunks_.begin()->Num() == wrseq_ + 1  && !stop_)
	{
		if (chunks_.begin()->Send(sock_writer_, Message::BLOCKING_MODE)
				< chunks_.begin()->DataSize())
		{
			VLOG(2) << _("CompressManager: srror sending message to"
			             " the writer.")
			        << _(" Message: ") << zmq_strerror(errno);
			return -1;
		}
		chunks_.erase(chunks_.begin());
		++wrseq_;
	}
	return 0;
}

int
CompressManager::processCompressorIncoming()
{
	Message msg;
	msg.Fetch(sock_feedback_);
	if (msg == MSG_ERROR)
	{
		VLOG(2) << _("CompressManager: received MSG_ERROR"
		             " from one of the Compressors.");
		return -1;
	}
	if ( msg.Type() != Message::TYPE_FCHUNK)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;
		LOG(ERROR) << _("CompressManager: error"
		                " fetching regular message")
		           << _(" Message: ") << zmq_strerror(errno);
		return -1;
	}
	--pushing_semaphore_;
	chunks_.insert(msg);
	if (flushChunks() == -1)
	{
		LOG(ERROR) << _("CompressManager: error transmitting"
		                " chunks to the writer");
		return -1;
	}
#		ifndef NDEBUG
		VLOG(2) << "Semaphore state :" << pushing_semaphore_;
#		endif // NDEBUG
	if (bytes_compressed_ >= ifsize_)
	{
		if (pushing_semaphore_ == pushing_semaphore_min_)
		{
			char buf[sizeof(Z_FINISH_TEMPLATE) + 4 + 4];
			memcpy(buf, Z_FINISH_TEMPLATE, sizeof(Z_FINISH));
			memcpy(buf + sizeof(Z_FINISH_TEMPLATE), &crc32_, 4);
			memcpy(buf + 4 + sizeof(Z_FINISH_TEMPLATE), &member_bytes_compressed_, 4);
			Message finish((uint8_t*)buf, sizeof(buf), 0);
			finish.Send(sock_writer_);
			MSG_FILLN.Send(sock_writer_);
			MSG_STOP.Send(sock_writer_);
			VLOG(2) << _("CompressManager: compression finished.");
			return -1;
		}
		MSG_STOP.Send(sock_jobs_);
		return 0;
	}
	if (chunks_.empty() || (chunks_.end()->Num() - wrseq_ + 1
	                                               < CHUNKS_HIGH_WATERMARK))
	{
		while (pushing_semaphore_ < 0 && bytes_compressed_ < ifsize_)
		{
			if (rdseq_ == *chunks_cnt_)
			{
				if (!chunks_.empty() || pushing_semaphore_
						!= pushing_semaphore_min_)
				{
					return 0;
				}
				int rs = MSG_FILLN.Send(sock_writer_);
				if (rs < MSG_FILLN.DataSize())
				{
					VLOG(2) << _("CompressManager: error sending"
					             " MSG_FILLN to the writer.");
					return -1;
				}
				++chunks_cnt_;
				if (chunks_cnt_ == member_chunks_cnt_.end()
						&& bytes_compressed_ < ifsize_)
				{
					VLOG(2) << _("CompressManager: chunks_cnt reached the"
					             " end of member_chunks_cnt before EOF.");
					return -1;
				}

				char buf[sizeof(Z_FINISH_TEMPLATE) + 4 + 4 + MEMBER_HEADER_MINSZ + 0xffff + 1];
				rs = makeMemberHeaderTemplate(
					*chunks_cnt_, buf + sizeof(Z_FINISH_TEMPLATE) + 4 + 4, sizeof(buf) - (4 + 4));
				if (rs == -1)
				{
					LOG(ERROR) << _("CompressManager: error creating member"
					                " header.");
					return -1;
				}
				memcpy(buf, Z_FINISH_TEMPLATE, sizeof(Z_FINISH));
				memcpy(buf + sizeof(Z_FINISH_TEMPLATE), &crc32_, 4);
				memcpy(buf + 4 + sizeof(Z_FINISH_TEMPLATE), &member_bytes_compressed_, 4);
				Message member_header((uint8_t*)buf, rs + sizeof(Z_FINISH_TEMPLATE) + 4 + 4, 0);
				member_header.Send(sock_writer_);
				rs = member_header.Send(sock_writer_);
				if (rs < member_header.DataSize())
				{
					VLOG(2) << _("CompressManager: error sending"
					             " new member header template to"
					             " the writer.");
					return -1;
				}
				rdseq_ = 0;
				wrseq_ = 0;
				crc32_ = crc32(0L, Z_NULL, 0);
				member_bytes_compressed_ = 0;
			}
			char buf[CHUNK_SIZE];
			memset(buf, 0, sizeof(buf));
			int rs = read(ifd_, buf, sizeof(buf));
			if (rs < 0)
			{
				LOG(ERROR) << _("CompressManager: error file read.")
				           << _(" Message: ") << strerror(errno);
				return -1;
			}
			crc32_ = crc32(crc32_, (Bytef*)buf, rs);
			Message msg((uint8_t*)buf, rs, ++rdseq_);
			if (msg.Send(sock_jobs_) < msg.DataSize())
			{
				LOG(ERROR) << _("CompressManager: error transmitting "
				                " chunk to compress.")
				           << " Message: " << zmq_strerror(errno);
				return -1;
			}
			member_bytes_compressed_ += rs;
			bytes_compressed_ += rs;
			++pushing_semaphore_;
		}
	}
	return 0;
}

int
CompressManager::processWriterIncoming()
{
	Message msg;
	msg.Fetch(sock_writer_);
	if (msg == MSG_ERROR)
	{
		VLOG(2) << _("CompressManager: received MSG_ERROR"
		             " from the writer.");
		return -1;
	}
	if (msg.Type() == Message::TYPE_UNKNOWN)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;
		LOG(ERROR) << _("CompressManager: error"
		                " fetching regular message")
		           << _(" Message: ") << zmq_strerror(errno);
		return -1;
	}
	return 0;
}

void
CompressManager::loop(CompressManager* self)
{
	if (self->makeInitialPush() == -1)
	{
		self->Stop();
		return;
	}
	self->pushing_semaphore_ = 0; // all Compressors have job
	zmq_pollitem_t items[2] =  {
		{self->sock_feedback_, 0, ZMQ_POLLIN, 0},
		{self->sock_writer_,   0, ZMQ_POLLIN, 0}
	};
	self->wrseq_ = 0;
	while(!self->stop_)
	{
		int rs = zmq_poll(items, 2, TICK);
		if (rs < 0)
		{
			LOG(ERROR) << _("CompressManager: error polling.")
			           << _(" Message: ") << zmq_strerror(errno);
			break;
		}
		if (rs == 0)
			continue;
		if (items[0].revents & ZMQ_POLLIN)
		{
			if (self->processCompressorIncoming() == -1)
				break;
		}
		if (items[1].revents & ZMQ_POLLIN)
		{
			if (self->processWriterIncoming() == -1)
				break;
		}
	}
	if (!self->stop_)
		self->Stop();
}

int
CompressManager::createSocks()
{
	sock_jobs_     = createBindSock(zmq_ctx_, "inproc://jobs",     ZMQ_PUSH, hwm_);
	sock_feedback_ = createBindSock(zmq_ctx_, "inproc://feedback", ZMQ_PULL, hwm_);
	sock_writer_   = createBindSock(zmq_ctx_, "inproc://writer",   ZMQ_PAIR, hwm_);
	VLOG_IF(2, sock_jobs_     == NULL) << _("Error with jobs socket.");
	VLOG_IF(2, sock_feedback_ == NULL) << _("Error with feedback socket.");
	VLOG_IF(2, sock_writer_   == NULL) << _("Error with writer socket.");
	if (!sock_jobs_ || !sock_feedback_ || !sock_writer_)
		return -1;
	return 0;
}

inline char* copy16(char*& pos, uint16_t num)
{
	memcpy(pos, &num, 2);
	pos += 2;
	return pos;
}

int
CompressManager::makeMemberHeaderTemplate(uint16_t chunks_count,
                                          char* buf, size_t bufsz,
                                          std::string extra /*= ""*/,
                                          uint32_t mtime /* = 0 */)
{
	size_t datasz = MEMBER_HEADER_MINSZ;
	datasz += 2*chunks_count;
	datasz += extra.length();
	datasz += 1;
	if (bufsz < datasz)
	{
		VLOG(2) << _("CompressManager: attempt to write header into"
		             " small buffer.");
		return -1;
	}
	memset(buf, 0, datasz);
	char* pos = buf;

	memcpy(pos, GZIP_DEFLATE_ID, 3);
	pos += 3;
	*pos++ = FEXTRA | FNAME;
	memcpy(pos, &mtime, 4);
	pos += 4;
	*pos++ = cfg_.CompressionLevel() == Z_BEST_COMPRESSION ? 0x02 : 0;
	*pos++ = OS_CODE_UNIX;

	copy16(pos, 2 + 2 + 2 + 2 + 2 + 2*chunks_count);
	*pos++ = 'R';
	*pos++ = 'A';
	copy16(pos, 2 + 2 + 2 + 2*chunks_count);
	copy16(pos, 1);
	copy16(pos, CHUNK_SIZE);
	copy16(pos, chunks_count);
	pos += 2*chunks_count;
	if(!extra.empty())
		memcpy(pos, extra.c_str(), extra.length());
	return datasz;
}

int
CompressManager::openFiles()
{
	struct stat fstat;
	if (stat(cfg_.IFName().c_str(), &fstat) == -1)
	{
		LOG(ERROR) << _("Error input file reading.")
		           << _(" Filename: '") << cfg_.IFName() <<"'"
		           << _(" Message: ") << strerror(errno);
		return -1;
	}
	if (!S_ISREG(fstat.st_mode))
	{
		LOG(ERROR) << _("Error - input is not a regular file.")
		           << _(" Filename: '") << cfg_.IFName() <<"'.";
		return -1;
	}
	if(fstat.st_mtim.tv_sec < 0xFFFFFFFFL)
		mtime_ = fstat.st_mtim.tv_sec;
	ifsize_ = fstat.st_size;
	size_t chcnt = ifsize_ / CHUNK_SIZE;
	if (ifsize_ % CHUNK_SIZE != 0)
		++chcnt;
	while (chcnt > CHUNKS_PER_MEMBER)
	{
		member_chunks_cnt_.push_back(CHUNKS_PER_MEMBER);
		chcnt -= CHUNKS_PER_MEMBER;
	}
	if (chcnt > 0)
		member_chunks_cnt_.push_back(chcnt);
	chunks_cnt_ = member_chunks_cnt_.begin();

	ifd_ = open(cfg_.IFName().c_str(), O_RDONLY);
	if (ifd_ == -1)
	{
		LOG(ERROR) << _("Error opening input file.")
		           << _(" Filename: '") << cfg_.IFName() <<"'."
		           << _(" Message: ") << strerror(errno);
		close(ofd_);
		return -1;
	}

	mode_t omode = O_WRONLY | O_CREAT | O_TRUNC;
	if(!cfg_.Force())
		omode |= O_EXCL;
	ofd_ = open(cfg_.OFName().c_str(), omode, S_IRUSR | S_IWUSR | S_IRGRP
	                                        | S_IROTH);
	if (ofd_ == -1)
	{
		if (errno == EEXIST)
		{
			LOG(ERROR) << _("Error - output already exists.")
			           << _(" Filename: '") << cfg_.OFName() <<"'.";
			return -1;
		}
		LOG(ERROR) << _("Error opening output.")
		           << _(" Filename: '") << cfg_.OFName() << "'.";
		return -1;
	}
	return 0;
}

int
CompressManager::waitThreadsReady(const size_t timeout_ms)
{
	std::chrono::time_point<std::chrono::steady_clock> deadline
		= std::chrono::steady_clock::now()
			+ std::chrono::milliseconds(timeout_ms);
	zmq_pollitem_t poll_items[2] = {
		{sock_feedback_, 0, ZMQ_POLLIN, 0},
		{sock_writer_, 0, ZMQ_POLLIN, 0}
	};
	size_t compressors_ready = 0;
	bool writer_ready = false;
	while(deadline > std::chrono::steady_clock::now())
	{
		int rs = zmq_poll(poll_items, 2, TICK);
		VLOG_IF(2, rs == -1)
			<< _("CompressManager: error threads initialization.")
			<< _(" Message: ") << zmq_strerror(errno);

		if (rs == -1)
			return 0;
		if (rs == 0)
			continue;
		if (poll_items[0].revents & ZMQ_POLLIN)
		{
			Message msg(sock_feedback_);
			if (msg == MSG_READY)
				++compressors_ready;
		}
		if (poll_items[1].revents & ZMQ_POLLIN)
		{
			Message msg(sock_writer_);
			if (msg == MSG_READY)
				writer_ready = true;
		}
		if(writer_ready && compressors_ready == cfg_.CompressorsCount())
			break;
	}
	if (compressors_ready != cfg_.CompressorsCount())
	{
		VLOG(2) << _("CompressManager: not all compressors ready.");
		return 0;
	}
	if (!writer_ready)
	{
		VLOG(2) << _("CompressManager: writer is not ready.");
		return 0;
	}
	return 1;
}

bool
CompressManager::doStart()
{
	VLOG(2) << "CompressManager: starting."
	        << cfg_.GetOptions();
	if (createSocks() == -1)
	{
		LOG(ERROR) << _("Error creating inter-thread communications.")
		           << _(" Use verbose for more info.");
		return false;
	}
	VLOG(2) << _("CompressManager: sockets created.");
	if (openFiles() == -1)
		return false;
	VLOG(2) << _("CompressManager: files opened.");

	writer_.reset(new std::thread(Writer::Start, writer_instance_.get(), ofd_));
	for (size_t i = 0; i < cfg_.CompressorsCount(); ++i)
	{
		workers_pool_.push_back(std::unique_ptr<std::thread>(
			new std::thread(Compressor::Start,
			                compressors_instances_[i].get())
		));
	}
	if (!waitThreadsReady(10*TICK))
	{
		LOG(ERROR) << _("CompressManager: Some threads wasn't ready in"
		                " the given timeout.");
		doStop();
		return false;
	}
	loop_.reset(new std::thread(loop, this));
	VLOG(2) << _("CompressManager: all threads started.");
	return true;
}

bool
CompressManager::doStop()
{
	VLOG(2) << _("CompressManger: stopping.");
	stop_ = true;
	MSG_STOP.Send(sock_writer_);
	for(size_t i = 0; i < compressors_instances_.size(); ++i)
		MSG_STOP.Send(sock_jobs_);
	if (writer_)
	{
		writer_->join();
		writer_.reset();
	}
	writer_ = NULL;
	if (loop_)
	{
		loop_->join();
		loop_.reset();
	}
	loop_ = NULL;
	for (size_t i = 0; i < workers_pool_.size(); ++i)
	{
		if (workers_pool_[i])
		{
			workers_pool_[i]->join();
			workers_pool_[i].reset();
		}
	}
	workers_pool_.clear();
	close(ofd_);
	close(ifd_);
	zmq_close(sock_jobs_);
	zmq_close(sock_feedback_);
	zmq_close(sock_writer_);
	return true;
}

} // namespace
