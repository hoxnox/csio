/**@author Merder Kim <$usermail>
 * @date 20140404 19:40:10*/

#include "Messages.hpp"
#include <logging.hpp>
#include <gettext.h>

namespace csio {

const bool Message::BLOCKING_MODE = true;

int
Message::Send(void* sock, bool blocking /*=false*/) const
{
	zmq_msg_t msg;
	if (!data_ || datasz_ == 0)
		return 0;
	if (zmq_msg_init_size(&msg, datasz_) == -1)
	{
		VLOG(2) << _("Error initializing zmq_msg_t in Send.")
		        << _(" Message: ") << zmq_strerror(errno);
	}
	memcpy(zmq_msg_data(&msg), data_.get(), datasz_);
	int rs = zmq_msg_send(&msg, sock, blocking ? 0 : ZMQ_DONTWAIT);
	if (rs == -1)
	{
		VLOG_IF(2, errno != EAGAIN && errno != EWOULDBLOCK)
			<< _("Error message sending.")
			<< _(" Message: ") << zmq_strerror(errno);
	}
	// according to man page we mus not close message on send
	// zmq_msg_close(&msg);
	return rs;
}

void
Message::Fetch(void* zmq_sock, bool blocking /*=false*/)
{
	Clear();
	zmq_msg_t msg;
	if (zmq_msg_init(&msg) == -1)
	{
		VLOG(2) << _("Error initializing zmq_msg in Message::Fetch.")
		        << _(" Message: ") << zmq_strerror(errno);
		return;
	}
	if (zmq_msg_recv(&msg, zmq_sock, blocking ? 0 : ZMQ_DONTWAIT) == -1)
	{
		VLOG_IF(2, errno != EAGAIN)
			<< _("Error receiving data "
			     "in message initialization.")
			<< _(" Message: ") << zmq_strerror(errno);
	}
	else
	{
		datasz_ = zmq_msg_size(&msg);
		void *msgdata = zmq_msg_data(&msg);
		if (datasz_ > 0 && msgdata != NULL)
		{
			data_.reset(new uint8_t[datasz_]);
			memset(data_.get(), 0, datasz_);
			memcpy(data_.get(), msgdata, datasz_);
		}
		else
		{
			VLOG(2) << _("Error retrieving data from zmq_msg.");
		}
	}
	zmq_msg_close(&msg);
}

Message::Message(void* sock, bool blocking)
	: datasz_(0)
{
	Fetch(sock, blocking);
}

Message::Message(const uint8_t* data, size_t sz, uint16_t num)
{
	datasz_ = sz + 1 + 2;
	data_.reset(new uint8_t[datasz_]);
	memset(data_.get(), 0, datasz_);
	data_[0] = TYPE_FCHUNK;
	u16be num_(num);
	memcpy(data_.get() + 1, num_.bytes, 2);
	if (data && sz > 0)
		memcpy(data_.get() + 1 + 2, data, sz);
}

Message::Message(const std::string& msg)
{
	datasz_ = msg.length() + 1;
	data_.reset(new uint8_t[datasz_]);
	memset(data_.get(), 0, datasz_);
	data_[0] = TYPE_INFO;
	if (!msg.empty())
		memcpy(data_.get() + 1, msg.c_str(), msg.length());
}

} // namespace

