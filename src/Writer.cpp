/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include <Writer.hpp>
#include <cstdlib>
#include <zmq.h>
#include <zlib.h>
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
	FILE* fstream = fdopen(ofd, "wb");
	if (!fstream)
	{
		LOG(ERROR) << _("Writer: error streaming input file.")
		           << _(" Message: ") << strerror(errno);
		return NULL;
	}
	void* sock =
		createConnectSock(self->zmq_ctx_, "inproc://writer", ZMQ_PAIR,
				self->hwm_);
	if (!sock)
	{
		LOG(ERROR) << "Writer: error initializing communications.";
		return NULL;
	}
	MSG_READY.Send(sock);
	zmq_pollitem_t event = {sock, 0, ZMQ_POLLIN, 0};
	memset(self->lbuf_, 0, sizeof(self->lbuf_));
	self->lbufsz_ = 0;
	bool first_member = true;
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
			off_t off = MEMBER_HEADER_MINSZ;
			if (!first_member)
				off += sizeof(Z_FINISH_TEMPLATE) + (4 + 4);
			else
				first_member = false;
			off_t old_header_pos = self->header_pos_;
			self->header_pos_ = ftello(fstream);
			fseeko(fstream, old_header_pos + off, SEEK_SET);
			if (fwrite_unlocked(self->lbuf_, self->lbufsz_, 1, fstream) != 1)
			{
				LOG(ERROR) << _("Writer: error header filling.")
				           << _(" Message: ") << strerror(errno);
				MSG_ERROR.Send(sock);
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
		if (fwrite_unlocked(msg.Data(), msg.DataSize(), 1, fstream) != 1)
		{
			LOG(ERROR) << _("Writer: error data writing.")
			           << _(" Message: ") << strerror(errno);
			MSG_ERROR.Send(sock);
			break;
		}
		if (msg.Num() != 0)
		{
			if (msg.DataSize() == 0)
			{
				VLOG(2) << "Writer: written " << msg.DataSize()
				        << " bytes (seq " << msg.Num() << ")";
				continue;
			}
			if (self->lbufsz_ > CHUNKS_PER_MEMBER*2 - 2)
			{
				VLOG(2) << _("Writer: lbuf corruption.");
				MSG_ERROR.Send(sock);
				break;
			}
			uint16_t tmp(msg.DataSize());
			memcpy(self->lbuf_ + self->lbufsz_, &tmp, 2);
			self->lbufsz_ += 2;
			VLOG(2) << "Writer: written " << msg.DataSize()
			        << " bytes (seq " << msg.Num() << ")";
		}
		else
		{
			VLOG(2) << "Writer: written " << msg.DataSize()
			        << " bytes (service info)";
		}
	}
	fclose(fstream);
	if (self->break_)
		VLOG(2) << "Writer breaked.";
	zmq_close(sock);
	return NULL;
}

} // namespace
