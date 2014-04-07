/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include <Compressor.hpp>
#include <logging.hpp>
#include <gettext.h>
#include <zmq.h>
#include "Utils.hpp"
#include "Messages.hpp"
#include "CompressManager.hpp"

namespace csio {

void*
Compressor::Start(Compressor* self)
{
	void* sock_in_  = 
		createConnectSock(self->zmq_ctx_, "inproc://jobs", ZMQ_PULL);
	void* sock_out_ =
		createConnectSock(self->zmq_ctx_, "inproc://feedback", ZMQ_PUSH);
	VLOG_IF(2, !sock_in_) << "Compressor (" << self << "):"
	                      <<_(" error with input socket.");
	VLOG_IF(2, !sock_in_) << "Compressor (" << self << "):"
	                      <<_(" error with output socket.");
	if (!sock_in_ || ! sock_out_)
	{
		LOG(ERROR) << "Compressor (" << self << "):"
		           << _(" error initializing communications.");
		return NULL;
	}
	MSG_READY.Send(sock_out_);
	zmq_pollitem_t event = {sock_in_, 0, ZMQ_POLLIN, 0};
	while(true)
	{
		int rs = zmq_poll(&event, 1, CompressManager::TICK);
		if(rs == 0)
		{
			continue;
		}
		else if (rs == -1)
		{
			VLOG(2) << "Compressor (" << self << "):"
			        << _(" error polling.") << zmq_strerror(errno);
			MSG_ERROR.Send(sock_out_);
			break;
		}
		Message msg(sock_in_);
		if (msg == MSG_STOP)
		{
			VLOG(2) << "Compressor (" << self << "):"
			        << _(" received MSG_STOP. Stopping.");
			break;
		}
		if (msg.Type() != Message::TYPE_FCHUNK)
		{
			VLOG(2) << "Compressor (" << self << "):"
			        << _(" received unexpected message type.")
			        << _(" Type: ") << msg.Type();
			MSG_ERROR.Send(sock_out_);
			break;
		}
		if (msg.DataSize() == 0)
		{
			VLOG(2) << "Compressor (" << self << "):"
			        << _(" failed receiving file chunk.");
			MSG_ERROR.Send(sock_out_);
			break;
		}
		VLOG(2) << self << " TODO: COMPRESSION " << msg.Num() << std::endl;
		if (msg.Send(sock_out_) < msg.DataSize())
		{
			VLOG(2) << "Compressor (" << self << "):"
			        << _(" failed send file chunk.");
			MSG_ERROR.Send(sock_out_);
			break;
		}
	}
	zmq_close(sock_in_);
	zmq_close(sock_out_);
}

} // namespace

