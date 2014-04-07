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

const size_t CompressManager::TICK  = 10;

CompressManager::CompressManager(const Config& cfg)
	: zmq_ctx_(zmq_init(0))
	, sock_jobs_(NULL)
	, sock_feedback_(NULL)
	, sock_writer_(NULL)
	, writer_instance_(new Writer(zmq_ctx_))
	, cfg_(cfg)
	, ifd_(-1)
	, ofd_(-1)
	, stop_(false)
	, CHUNKS_HIGH_WATERMARK(cfg.CompressorsCount()*4)
	, pushing_semaphore_(cfg.CompressorsCount()*(-1))
	, ifsize_(0)
	, bytes_compressed_(0)
{
	if (!zmq_ctx_)
	{
		LOG(ERROR) << _("Error creating communication context.")
		           << _(" Message: ") << zmq_strerror(errno);
		return;
	}
	for(size_t i = 0; i < cfg_.CompressorsCount(); ++i)
	{
		compressors_instances_.push_back(
			std::unique_ptr<Compressor>(new Compressor(zmq_ctx_)));
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

int
CompressManager::makeInitialPush()
{
	char buf[CHUNK_SIZE*cfg_.CompressorsCount()];
	memset(buf, 0, sizeof(buf));
	size_t rdsize =ifsize_ < CHUNK_SIZE*cfg_.CompressorsCount()
		? ifsize_ : CHUNK_SIZE*cfg_.CompressorsCount();
	int rs = read(ifd_, buf, rdsize);
	if (rs == -1)
	{
		LOG(ERROR) << _("CompressorManager: error reading file.")
		           << _(" Message: ") << strerror(errno);
		return -1;
	}
	rdseq_ = 0;
	for (; rdseq_ < rs/CHUNK_SIZE; ++rdseq_)
	{
		Message msg((uint8_t*)buf + rdseq_*CHUNK_SIZE, CHUNK_SIZE, rdseq_ + 1);
		if (msg.Send(sock_jobs_, Message::BLOCKING_MODE) < CHUNK_SIZE)
		{
			VLOG(2) << _("CompressorManager: error initially"
			             " pushing chunk #") << rdseq_;
			return -1;
		}
	}
	if (rs % CHUNK_SIZE > 0)
	{
		Message msg((uint8_t*)buf + rdseq_*CHUNK_SIZE,
				rs % CHUNK_SIZE, rdseq_ + 1);
		if (msg.Send(sock_jobs_, Message::BLOCKING_MODE)
				< rs - rdseq_*CHUNK_SIZE)
		{
			VLOG(2) << _("CompressorManager: error initially"
			             " pushing chunk #") << rdseq_;
			return -1;
		}
		++rdseq_;
	}
	bytes_compressed_ = rs;
	VLOG(2) << _("Initial push with ") << rdseq_ << (" elements.");
	return 0;
}

int
CompressManager::flushChunks()
{
	while (!chunks_.empty() && chunks_.begin()->Num() == wrseq_ + 1)
	{
		if (chunks_.begin()->Send(sock_writer_, Message::BLOCKING_MODE)
				< chunks_.begin()->DataSize())
		{
			VLOG(2) << _("Error sending message to the writer.")
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
		VLOG(2) << _("CompressorManager: received MSG_ERROR"
		             " from one of the Compressors.");
		return -1;
	}
	if ( msg.Type() != Message::TYPE_FCHUNK)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;
		LOG(ERROR) << _("CompressorManager: error"
		                " fetching regular message")
		           << _(" Message: ") << zmq_strerror(errno);
		return -1;
	}
	--pushing_semaphore_;
	chunks_.insert(msg);
	if (flushChunks() == -1)
	{
		LOG(ERROR) << _("CompressorManager: error transmitting"
		                " chunks to the writer");
		return -1;
	}
	if (bytes_compressed_ >= ifsize_)
	{
		MSG_STOP.Send(sock_jobs_);
		return 0;
	}
	if (chunks_.empty() || (chunks_.end()->Num() - wrseq_ + 1 
	                                               < CHUNKS_HIGH_WATERMARK))
	{
		while (pushing_semaphore_ < 0 && bytes_compressed_ < ifsize_)
		{
			char buf[CHUNK_SIZE];
			memset(buf, 0, sizeof(buf));
			int rs = read(ifd_, buf, sizeof(buf));
			if (rs < 0)
			{
				LOG(ERROR) << _("CompressorManager: error file read.")
				           << _(" Message: ") << strerror(errno);
				return -1;
			}
			Message msg((uint8_t*)buf, rs, ++rdseq_);
			if (msg.Send(sock_jobs_) < msg.DataSize())
			{
				LOG(ERROR) << _("CompressorManager: error transmitting "
				                " chunk to compress.")
				           << " Message: " << zmq_strerror(errno);
				return -1;
			}
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
		VLOG(2) << _("CompressorManager: received MSG_ERROR"
		             " from the writer.");
		return -1;
	}
	if (msg.Type() == Message::TYPE_UNKNOWN)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;
		LOG(ERROR) << _("CompressorManager: error"
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
	zmq_pollitem_t items[2] =  {
		{self->sock_feedback_, 0, ZMQ_POLLIN, 0},
		{self->sock_writer_,   0, ZMQ_POLLIN, 0}
	};
	self->wrseq_ = 0;
	self->pushing_semaphore_ = 0;
	while(!self->stop_)
	{
		int rs = zmq_poll(items, 2, TICK);
		if (rs < 0)
		{
			LOG(ERROR) << _("CompressorManager: error polling.")
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
	self->Stop();
}

int
CompressManager::createSocks()
{
	sock_jobs_     = createBindSock(zmq_ctx_, "inproc://jobs",    ZMQ_PUSH);
	sock_feedback_ = createBindSock(zmq_ctx_, "inproc://feedback",ZMQ_PULL);
	sock_writer_   = createBindSock(zmq_ctx_, "inproc://writer",  ZMQ_PAIR);
	VLOG_IF(2, sock_jobs_     == NULL) << _("Error with jobs socket.");
	VLOG_IF(2, sock_feedback_ == NULL) << _("Error with feedback socket.");
	VLOG_IF(2, sock_writer_   == NULL) << _("Error with writer socket.");
	if (!sock_jobs_ || !sock_feedback_ || !sock_writer_)
		return -1;
	return 0;
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
	ifsize_ = fstat.st_size;
	ifd_ = open(cfg_.IFName().c_str(), O_RDONLY);
	if (ifd_ == -1)
	{
		LOG(ERROR) << _("Error opening input file.")
		           << _(" Filename: '") << cfg_.IFName() <<"'."
		           << _(" Message: ") << strerror(errno);
		close(ofd_);
		return -1;
	}

	mode_t omode = O_WRONLY | O_CREAT;
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
	VLOG(2) << "CompressManger: starting."
	        << " Options: " << cfg_.GetOptions();
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

	writer_.reset(new std::thread(Writer::Start, writer_instance_.get()));
	for (size_t i = 0; i < cfg_.CompressorsCount(); ++i)
	{
		workers_pool_.push_back(std::unique_ptr<std::thread>(
			new std::thread(Compressor::Start,
			                compressors_instances_[i].get())
		));
	}
	if (!waitThreadsReady(10*TICK))
	{
		LOG(ERROR) << _("Some threads wasn't ready in given timeout.")
		           << _(" Use verbose for more info.");
		return false;
	}
	loop_.reset(new std::thread(loop, this));
	VLOG(2) << _("CompressManager: all threads started.");
	return true;
}

bool
CompressManager::doStop()
{
	VLOG(2) << "CompressManger: stopping.";
	stop_ = true;
	MSG_STOP.Send(sock_writer_);
	for(size_t i = 0; i < cfg_.CompressorsCount(); ++i)
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
