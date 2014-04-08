/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include <Writer.hpp>
#include <cstdlib>
#include <zmq.h>
#include <logging.hpp>
#include <gettext.h>
#include "Messages.hpp"
#include "CompressManager.hpp"
#include "endians.hpp"

namespace csio {

void*
Writer::Start(Writer* self, int ofd)
{
	self->break_ = false;
	if (!self->zmq_ctx_)
		return NULL;
	void* sock = 
		createConnectSock(self->zmq_ctx_, "inproc://writer", ZMQ_PAIR,
				self->hwm_);
	if (!sock)
	{
		LOG(ERROR) << "Writer: error initializing communications.";
		return NULL;
	}
	FILE* fstream = fdopen(ofd, "w");
	if (!fstream)
	{
		LOG(ERROR) << _("Writer: error opening file stream from fd.")
		           << _(" Message: ") << strerror(errno);
		zmq_close(sock);
		return NULL;
	}
	MSG_READY.Send(sock);
	zmq_pollitem_t event = {sock, 0, ZMQ_POLLIN, 0};
	while(!self->break_)
	{
		int rs = zmq_poll(&event, 1, TICK);
		if(rs == 0)
		{
			continue;
		}
		else if (rs == -1)
		{
			VLOG(2) << "Witer:"
			        << _(" error polling. ")
			        << _(" Message: ") << zmq_strerror(errno);
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
		if (msg == MSG_FILLN)
		{
			VLOG(2) << _("Writer: member finished. Filling header.");
			const off_t off = 10 + (2 + 2 + (2 + 2 + 2));
			off_t old_header_pos = self->header_pos_;
			self->header_pos_ = ftello(fstream);
			fseeko(fstream, old_header_pos + off, SEEK_SET);
			int rs = write(ofd, self->lbuf_, self->lbufsz_);
			if (rs != self->lbufsz_)
			{
				LOG(ERROR) << _("Writer: error header filling.")
				           << _(" Message: ") << strerror(errno);
				break;
			}
			fseeko(fstream, self->header_pos_, SEEK_SET);
			self->lbufsz_ = 0;
			continue;
		}
		if (msg.Type() != Message::TYPE_FCHUNK)
		{
			VLOG(2) << _("Writer: received unexpected msg type.")
			        << _(" Type: ") << msg.Type();
			MSG_ERROR.Send(sock);
			break;
		}
		if (write(ofd, msg.Data(), msg.DataSize()) != msg.DataSize())
		{
			LOG(ERROR) << _("Writer: error data writing.")
			           << _(" Message: ") << strerror(errno);
			MSG_ERROR.Send(sock);
			break;
		}
		if (self->lbufsz_ >= CHUNKS_PER_MEMBER*2 - 2)
		{
			VLOG(2) << _("Writer: lbuf corruption.");
			MSG_ERROR.Send(sock);
			break;
		}
		memcpy(self->lbuf_ + self->lbufsz_, u16be(msg.DataSize()).bytes, 2);
		if (msg.Num() == 0)
			VLOG(2) << "Writer: written " << msg.DataSize() 
			        << " bytes (header)";
		else
			VLOG(2) << "Writer: written " << msg.DataSize()
			        << " bytes (seq " << msg.Num() << ")";
	}
	if (self->break_)
		VLOG(2) << "Writer breaked.";
	zmq_close(sock);
	return NULL;
}

} // namespace
