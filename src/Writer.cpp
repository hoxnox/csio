/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include <Writer.hpp>
#include <cstdlib>
#include <zmq.h>
#include <zlib.h>
#include <logging.hpp>
#include <gettext.h>
#include "CompressManager.hpp"
#include "endians.hpp"

namespace csio {

bool
Writer::processMessage(const Message& msg)
{
	switch(msg.Type())
	{
		case Message::TYPE_MCLOSE: {
			VLOG(2) << _("Writer: member close. Filling header.");
			off_t curpos = ftello(fstream_);
			fseeko(fstream_, chunks_lengths_off_, SEEK_SET);
			if (fwrite_unlocked(lbuf_, lbufsz_, 1, fstream_) != 1)
			{
				LOG(ERROR) << _("Writer: error header filling.")
				           << _(" Message: ") <<strerror(errno);
				MSG_ERROR.Send(sock_);
				return false;
			}
			fseeko(fstream_, curpos, SEEK_SET);
			lbufsz_ = 0;
			} break;
		case Message::TYPE_MHEADER: {
			VLOG(2) << _("Writer: member header received.");
			chunks_lengths_off_ = ftello(fstream_)
				+ Message::CHUNKS_LENGTHS_HEADER_OFFSET;
			} break;
		case Message::TYPE_FCHUNK: {
			if (msg.DataSize() == 0)
			{
				VLOG(2) << _("Writer: zero-length file chunk.");
				MSG_ERROR.Send(sock_);
				return false;
			}
			if (lbufsz_ > CHUNKS_PER_MEMBER*2 - 2)
			{
				VLOG(2) << _("Writer: lbuf corruption.");
				MSG_ERROR.Send(sock_);
				return false;
			}
			u16le tmp(msg.DataSize());
			memcpy(lbuf_ + lbufsz_, tmp.bytes, 2);
			lbufsz_ += 2;
#			ifndef NDEBUG
			VLOG(2) << "Writer: written " << msg.DataSize()
			        << " bytes (seq " << msg.Num() << ")";
#			endif // DEBUG
			} break;
		default:
			VLOG(2) << _("Writer: received unexpected msg type.")
			        << _(" Type: ") << msg.Type();
			MSG_ERROR.Send(sock_);
			return false;
	}
	if (fwrite_unlocked(msg.Data(), msg.DataSize(), 1, fstream_) != 1)
	{
		LOG(ERROR) << _("Writer: error data writing.")
		           << _(" Message: ") << strerror(errno);
		MSG_ERROR.Send(sock_);
		return false;
	}
	return true;
}

void*
Writer::Start(Writer* self, int ofd)
{
	self->break_ = false;
	if (!self->zmq_ctx_)
		return NULL;
	self->fstream_ = fdopen(ofd, "wb");
	if (!self->fstream_)
	{
		LOG(ERROR) << _("Writer: error streaming input file.")
		           << _(" Message: ") << strerror(errno);
		return NULL;
	}
	if (!self->sock_)
	{
		LOG(ERROR) << "Writer: error initializing communications.";
		return NULL;
	}
	MSG_READY.Send(self->sock_);
	zmq_pollitem_t event = {self->sock_, 0, ZMQ_POLLIN, 0};
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
			VLOG(2) << _("Witer: error polling. ")
			        << _(" Message: ") << zmq_strerror(errno);
			MSG_ERROR.Send(self->sock_);
			break;
		}
		Message msg(self->sock_);
		if (msg == MSG_STOP)
		{
			VLOG(2) << "Writer:"
			        << _(" received MSG_STOP. Stopping.");
			break;
		}
		if (!self->processMessage(msg))
			break;
	}
	fclose(self->fstream_);
	if (self->break_)
		VLOG(2) << "Writer breaked.";
	zmq_close(self->sock_);
	return NULL;
}

} // namespace
