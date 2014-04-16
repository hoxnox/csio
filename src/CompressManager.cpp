/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include <glog/logging.h>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>

#include "CompressManager.hpp"
#include "Utils.hpp"
#include "Messages.hpp"

namespace csio {


CompressManager::CompressManager(const Config& cfg)
	: zmq_ctx_(zmq_init(0))
	, sock_inbox_(NULL)
	, sock_outbox_(NULL)
	, sock_writer_(NULL)
	, cfg_(cfg)
	, ifs_()
	, ofd_(-1)
	, stop_(false)
	, MSG_QUEUE_HWM(cfg_.CompressorsCount()*2 + 5)
	, ORDERING_SET_HWM(cfg.CompressorsCount()*3)
	, msg_pushed_(0)
	, compressors_count_(0)
{
	LOG_IF(ERROR, !zmq_ctx_)
		<< _("CompressManager: error creating communication context.")
		<< _(" Message: ") << zmq_strerror(errno);
};

CompressManager::~CompressManager()
{
	zmq_close(sock_outbox_);
	zmq_close(sock_inbox_);
	zmq_close(sock_writer_);
	zmq_ctx_destroy(zmq_ctx_);
};

bool
CompressManager::makeInitialPush()
{
	size_t rdbufsz = ifs_.chunksz*cfg_.CompressorsCount();
	std::unique_ptr<uint8_t[]> rdbuf(new uint8_t[rdbufsz]);
	int rdbytes = read(ifs_.handler, rdbuf.get(), rdbufsz);
	if (rdbytes == -1)
	{
		LOG(ERROR) << _("CompressManager: error reading file.")
		           << _(" Message: ") << strerror(errno);
		return false;
	}
	if (rdbytes == 0)
	{
		VLOG(2) << _("CompressManager: the file is empty.");
		return false;
	}
	Message header(ifs_.cur_chunks_mx,
	               ifs_.level,
	               ifs_.basename,
	               ifs_.mtime);
	if (!header.Send(sock_writer_))
	{
		VLOG(2) << _("CompressManager: error sending header to the"
		             " writer.")
		        << _(" Message: ") << zmq_strerror(errno);
		return false;
	}
	while(ifs_.bytes_rx < rdbytes)
	{
		u16le chunksz = std::min(rdbytes - ifs_.bytes_rx, ifs_.chunksz);
		Message fchunk(rdbuf.get() + ifs_.bytes_rx,
		               chunksz,
		               ifs_.cur_chunks_rx + 1);
		if (!fchunk.Send(sock_outbox_, Message::BLOCKING_MODE))
		{
			VLOG(2) << _("CompressManager: error initially"
			             " pushing chunk #") << ifs_.chunks_rx;
			return false;
		}
		ifs_.cur_crc32 = crc32(ifs_.cur_crc32, 
		                       (Bytef*)rdbuf.get() + ifs_.bytes_rx,
		                       chunksz);
		ifs_.bytes_rx += chunksz;
		ifs_.cur_bytes_rx += chunksz;
		++ifs_.chunks_rx;
		++ifs_.cur_chunks_rx;
		++compressors_count_;
		++msg_pushed_;
	}
	VLOG(2) << _("CompressManager: initial push with ")
	        << ifs_.chunks_rx << (" elements.");
	return true;
}

bool
CompressManager::flushOrderingSet()
{
	if (ordering_set_.empty())
		return true;
	if (stop_)
		return false;
	std::set<Message>::iterator os_head = ordering_set_.begin();
	while (os_head != ordering_set_.end()
	    && os_head->Num() == ifs_.cur_chunks_tx + 1)
	{
		if (!os_head->Send(sock_writer_, Message::BLOCKING_MODE))
		{
			VLOG(2) << _("CompressManager: srror sending file chunk"
			             " to the writer.")
			        << _(" Message: ") << zmq_strerror(errno);
			return false;
		}
		ifs_.bytes_tx += os_head->DataSize();
		ifs_.cur_bytes_tx += os_head->DataSize();
		++ifs_.cur_chunks_tx;
		++ifs_.chunks_tx;
		ordering_set_.erase(os_head);
		os_head = ordering_set_.begin();
	}
	return true;
}

CompressManager::PollStatus
CompressManager::processCompressorIncoming()
{
#		ifndef NDEBUG
		VLOG(2) << "Pushed messages count / compressors count: "
			<< msg_pushed_ << "/" << compressors_count_ << ")";
#		endif // NDEBUG
	Message msg;
	msg.Fetch(sock_inbox_);
	if (msg.Type() == Message::TYPE_UNKNOWN)
	{
		VLOG(2) << _("CompressManager: error receiving data"
		             " from one of the Compressors.");
		return POLL_BREAK;
	}
	if (msg == MSG_ERROR)
	{
		VLOG(2) << _("CompressManager: received MSG_ERROR"
		             " from one of the Compressors.");
		return POLL_BREAK;
	}
	if (msg.Type() != Message::TYPE_FCHUNK || msg.DataSize() == 0)
	{
		LOG(ERROR) << _("CompressManager: error"
		                " fetching regular message")
		           << _(" Message: ") << zmq_strerror(errno);
		return POLL_BREAK;
	}
	--msg_pushed_;
	ordering_set_.insert(msg);
	if (!flushOrderingSet())
	{
		LOG(ERROR) << _("CompressManager: error transmitting"
		                " chunks to the writer");
		return POLL_BREAK;
	}
	if (ifs_.bytes_rx == ifs_.bytes)
	{
		if (msg_pushed_ == 0)
		{
			Message mclose(u32le(ifs_.cur_bytes_rx),ifs_.cur_crc32);
			bool rs = mclose.Send(sock_writer_);
			VLOG(2) << (rs ?
				_("CompressManager: compression finished.") :
				_("ComrpessManager: error sending mclose."));
			return POLL_BREAK;
		}
		MSG_STOP.Send(sock_outbox_);
		return POLL_CONTINUE;
	}
	std::set<Message>::reverse_iterator os_last = ordering_set_.rbegin();
	if (os_last != ordering_set_.rend()
	 && os_last->Num() - ifs_.cur_chunks_tx >= ORDERING_SET_HWM)
	{
		VLOG(2) << _("CompressManager: skipping file chunk reading,"
		             " because ordering set is full");
		if (msg_pushed_ == 0)
		{
			VLOG(2) << ("CompressManager: oredering set if full and"
			            " there is no messages in outbox. Stopping"
			            " to prevent hanging.");
			return POLL_BREAK;
		}
		return POLL_CONTINUE;
	}
	if (!makeRegularPush())
		return POLL_BREAK;
	return POLL_CONTINUE;
}

bool
CompressManager::makeRegularPush()
{
	while (msg_pushed_ < compressors_count_ && ifs_.bytes_rx < ifs_.bytes)
	{
		if (ifs_.cur_chunks_rx == ifs_.cur_chunks_mx)
		{
			if (!ordering_set_.empty() || msg_pushed_ > 0)
				return true;

			Message mclose(u32le(ifs_.cur_bytes_rx),ifs_.cur_crc32);
			if (!mclose.Send(sock_writer_, Message::BLOCKING_MODE))
			{
				VLOG(2) << _("CompressManager: error sending"
				             " member close message.");
				return false;
			}
			if (ifs_.bytes_rx >= ifs_.bytes)
			{
				VLOG(2) << _("CompressManager: unexpected eof.");
				return false;
			}
			ifs_.cur_bytes_rx = 0;
			ifs_.cur_bytes_tx = 0;
			ifs_.cur_chunks_rx = 0;
			ifs_.cur_chunks_tx = 0;
			ifs_.cur_chunks_mx = CHUNKS_PER_MEMBER;
			ifs_.cur_crc32 = crc32(0L, Z_NULL, 0);
			++ifs_.members_tx;
			if (ifs_.members == ifs_.members_tx + 1)
			{
				ifs_.cur_chunks_mx = ifs_.chunks 
					- (ifs_.members-1)*CHUNKS_PER_MEMBER;
			}
			Message header(ifs_.cur_chunks_mx, ifs_.level);
			if (!header.Send(sock_writer_, Message::BLOCKING_MODE))
			{
				VLOG(2) << _("CompressManager: error sending"
				             " new member header template to"
				             " the writer.");
				return false;
			}
		}
		uint8_t buf[CHUNK_SIZE];
		memset(buf, 0, sizeof(buf));
		int rdsize = read(ifs_.handler, buf, sizeof(buf));
		if (rdsize < 0)
		{
			LOG(ERROR) << _("CompressManager: error file read.")
			           << _(" Message: ") << strerror(errno);
			return false;
		}
		if (rdsize == 0)
			return true;
		Message msg(buf, rdsize, ifs_.cur_chunks_rx + 1);
		if (!msg.Send(sock_outbox_))
		{
			LOG(ERROR) << _("CompressManager: error transmitting "
			                " chunk to compress.")
			           << " Message: " << zmq_strerror(errno);
			return false;
		}
		ifs_.cur_bytes_rx += rdsize;
		ifs_.bytes_rx += rdsize;
		++ifs_.chunks_rx;
		++ifs_.cur_chunks_rx;
		ifs_.cur_crc32 = crc32(ifs_.cur_crc32, (Bytef*)buf, rdsize);
		++msg_pushed_;
	}
	return true;
}

CompressManager::PollStatus
CompressManager::processWriterIncoming()
{
	Message msg;
	msg.Fetch(sock_writer_);
	LOG_IF(ERROR, msg == MSG_ERROR)
		<< _("CompressManager: received MSG_ERROR from the writer.");
	LOG_IF(ERROR, msg.Type() == Message::TYPE_UNKNOWN)
		<< _("CompressManager: error fetching message from the writer")
		<< _(" Message: ") << zmq_strerror(errno);
	return POLL_BREAK;
}

void
CompressManager::loop(CompressManager* self)
{
	VLOG(2) << _("CompressManager: sending initial header.");
	if (self->ifs_.members > 1)
		self->ifs_.cur_chunks_mx = CHUNKS_PER_MEMBER;
	else
		self->ifs_.cur_chunks_mx = self->ifs_.chunks;
	if (!self->makeInitialPush()
	 || self->msg_pushed_ != self->compressors_count_)
	{
		self->Stop();
		VLOG(2) << _("CompressManager: error making initial push.");
		return;
	}
	zmq_pollitem_t items[2] =  {
		{self->sock_inbox_, 0, ZMQ_POLLIN, 0},
		{self->sock_writer_,   0, ZMQ_POLLIN, 0}
	};
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
			if (self->processCompressorIncoming()
					== CompressManager::POLL_BREAK)
			{
				break;
			}
		}
		if (items[1].revents & ZMQ_POLLIN)
		{
			if (self->processWriterIncoming()
					== CompressManager::POLL_BREAK)
			{
				break;
			}
		}
	}
	if (!self->stop_)
		self->Stop();
}

bool
CompressManager::createSocks()
{
	sock_outbox_  = createBindSock(zmq_ctx_,
	                                "inproc://outbox",
	                                ZMQ_PUSH,
	                                MSG_QUEUE_HWM);
	sock_inbox_   = createBindSock(zmq_ctx_,
	                                "inproc://inbox",
	                                ZMQ_PULL,
	                                MSG_QUEUE_HWM);
	sock_writer_   = createBindSock(zmq_ctx_,
	                                "inproc://writer",
	                                ZMQ_PAIR,
	                                MSG_QUEUE_HWM);
	VLOG_IF(2, sock_outbox_ == NULL) << _("Error with jobs socket.");
	VLOG_IF(2, sock_inbox_  == NULL) << _("Error with feedback socket.");
	VLOG_IF(2, sock_writer_ == NULL) << _("Error with writer socket.");
	if (!sock_outbox_ || !sock_inbox_ || !sock_writer_)
		return false;
	return true;
}

inline std::string
get_base_name(std::string path)
{
	std::string result;
	for(size_t i = path.length() - 1 ; i > 0 && path[i] != '/'; --i)
		result = path[i] + result;
	return result;
}

bool
CompressManager::openFiles()
{
	ifs_.basename = get_base_name(cfg_.IFName());
	struct stat fstat;
	if (stat(cfg_.IFName().c_str(), &fstat) == -1)
	{
		LOG(ERROR) << _("Error input file reading.")
		           << _(" Filename: '") << cfg_.IFName() <<"'"
		           << _(" Message: ") << strerror(errno);
		return false;
	}
	if (!S_ISREG(fstat.st_mode))
	{
		LOG(ERROR) << _("Error - input is not a regular file.")
		           << _(" Filename: '") << cfg_.IFName() <<"'.";
		return false;
	}
	if(fstat.st_mtim.tv_sec < 0xFFFFFFFFL)
		ifs_.mtime = fstat.st_mtim.tv_sec;
	ifs_.bytes = fstat.st_size;
	ifs_.chunks = ifs_.bytes/CHUNK_SIZE + (ifs_.bytes%CHUNK_SIZE>0 ? 1 : 0);
	ifs_.members = ifs_.chunks/CHUNKS_PER_MEMBER + 
		(ifs_.chunks%CHUNKS_PER_MEMBER>0 ? 1 : 0);
	ifs_.level = cfg_.CompressionLevel();
	ifs_.chunksz = CHUNK_SIZE;

	ifs_.handler = open(cfg_.IFName().c_str(), O_RDONLY);
	if (ifs_.handler == -1)
	{
		LOG(ERROR) << _("Error opening input file.")
		           << _(" Filename: '") << cfg_.IFName() <<"'."
		           << _(" Message: ") << strerror(errno);
		return false;
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
			return false;
		}
		LOG(ERROR) << _("Error opening output.")
		           << _(" Filename: '") << cfg_.OFName() << "'.";
		return false;
	}
	return true;
}

bool
CompressManager::waitChildrenReady(const size_t timeout_ms)
{
	std::chrono::time_point<std::chrono::system_clock> deadline
		= std::chrono::system_clock::now()
			+ std::chrono::milliseconds(timeout_ms);
	zmq_pollitem_t poll_items[2] = {
		{sock_inbox_,  0, ZMQ_POLLIN, 0},
		{sock_writer_, 0, ZMQ_POLLIN, 0}
	};
	size_t compressors_ready = 0;
	bool writer_ready = false;
	while(deadline > std::chrono::system_clock::now())
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
			Message msg(sock_inbox_);
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
		return false;
	}
	if (!writer_ready)
	{
		VLOG(2) << _("CompressManager: writer is not ready.");
		return false;
	}
	return true;
}

bool
CompressManager::doStart()
{
	VLOG(2) << "CompressManager: starting."
	        << cfg_.GetOptions();
	if (!openFiles())
		return false;
	VLOG(2) << _("CompressManager: files opened.");
	if (!createSocks())
	{
		LOG(ERROR) << _("Error creating inter-thread communications.")
		           << _(" Use verbose for more info.");
		return false;
	}
	for(size_t i = 0; i < cfg_.CompressorsCount(); ++i)
		compressors_instances_.push_back(std::unique_ptr<Compressor>(
			new Compressor(zmq_ctx_, cfg_)));
	writer_instance_.reset(new Writer(zmq_ctx_, MSG_QUEUE_HWM));
	VLOG(2) << _("CompressManager: sockets created.");
	writer_thread_.reset(new std::thread(
				Writer::Start, writer_instance_.get(), ofd_));
	for (size_t i = 0; i < cfg_.CompressorsCount(); ++i)
	{
		workers_threads_.push_back(std::unique_ptr<std::thread>(
			new std::thread(Compressor::Start,
			                compressors_instances_[i].get(),
			                cfg_.CompressionLevel())
		));
	}
	if (!waitChildrenReady(10*TICK))
	{
		LOG(ERROR) << _("CompressManager: Some threads wasn't ready in"
		                " the given timeout.");
		doStop();
		return false;
	}
	loop_thread_.reset(new std::thread(loop, this));
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
		MSG_STOP.Send(sock_outbox_);
	if (writer_thread_)
	{
		writer_thread_->join();
		writer_thread_.reset();
	}
	writer_thread_ = NULL;
	if (loop_thread_)
	{
		loop_thread_->join();
		loop_thread_.reset();
	}
	loop_thread_ = NULL;
	for (size_t i = 0; i < workers_threads_.size(); ++i)
	{
		if (workers_threads_[i])
		{
			workers_threads_[i]->join();
			workers_threads_[i].reset();
		}
	}
	workers_threads_.clear();
	close(ofd_);
	close(ifs_.handler);
	zmq_close(sock_outbox_);
	zmq_close(sock_inbox_);
	zmq_close(sock_writer_);
	return true;
}

} // namespace
