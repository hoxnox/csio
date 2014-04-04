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
	, ifd_(0)
	, ofd_(0)
	, stop_(false)
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

void
CompressManager::loop(CompressManager* self)
{
	char buf[CHUNK_SIZE];
	memset(buf, 0, sizeof(buf));
	int passed;
	size_t num = 0;

	// read objects: sock_writer_, sock_feedback_, ifd
	// we need to zmq_poll though all of them

	/*
	while(!stop_)
	{
		const int NEED_TO_READ = CHUNK_SIZE - passed;
		int rs = read(self->ifd_, &buf + passed, NEED_TO_READ);
		if (rs < NEED_TO_READ)
		{
			if (errno == EAGAIN || errno == EINTR
			 || errno == EWOULDBLOCK)
			{
				passed += rs;
				continue;
			}
			else // else if eof
			{
				MSG_ERROR.Send(self->sock_writer_);
				LOG(ERROR) << _("Error file reading.")
				           << _(" Message: ")<< strerror(errno);
				break;
			}
		}
		++num;
		Message msg((uint8_t*)buf, rs, num);
		msg.Send(self->sock_jobs_);
		Message compressed(self->sock_feedback_);
	}*/
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
	int ifd_ = open(cfg_.IFName().c_str(), O_RDONLY);
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
	int ofd_ = open(cfg_.OFName().c_str(), omode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
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
