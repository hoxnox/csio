/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include <Writer.hpp>
#include <cstdlib>
#include <zmq.h>
#include <logging.hpp>
#include <gettext.h>
#include "Utils.hpp"
#include "Messages.hpp"
#include "CompressManager.hpp"

namespace csio {

void*
Writer::Start(Writer* self)
{
	if (!self->zmq_ctx_)
		return NULL;
	void* sock = 
		createConnectSock(self->zmq_ctx_, "inproc://writer", ZMQ_PAIR);
	if (!sock)
	{
		LOG(ERROR) << "Writer: error initializing communications.";
		return NULL;
	}
	MSG_READY.Send(sock);
	zmq_pollitem_t event = {sock, 0, ZMQ_POLLIN, 0};
	while(true)
	{
		int rs = zmq_poll(&event, 1, CompressManager::TICK);
		if(rs == 0)
		{
			continue;
		}
		else if (rs == -1)
		{
			VLOG(2) << "Witer:"
			        << _(" error polling.") << zmq_strerror(errno);
			MSG_ERROR.Send(sock);
			break;
		}
		Message msg(sock);
		if (msg == MSG_STOP)
		{
			VLOG(2) << "Writer:"
			        << _(" received MSG_STOP. Stopping.");
			break;
		}
		if (msg.Type() != Message::TYPE_FCHUNK)
		{
			VLOG(2) << "Writer:"
			        << _(" received unexpected message type.")
			        << _(" Type: ") << msg.Type();
			break;
		}
		sleep(1); // writing
	}
	zmq_close(sock);
	return NULL;
}

} // namespace
