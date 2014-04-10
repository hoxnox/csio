/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include <Compressor.hpp>
#include <logging.hpp>
#include <gettext.h>
#include <zmq.h>
#include "Utils.hpp"
#include "Messages.hpp"
#include "CompressManager.hpp"
#include <zlib.h>

namespace csio {

#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif

void*
Compressor::Start(Compressor* self, int level)
{
	self->break_ = false;
	int hwm = self->cfg_.MsgHWM();
	void* sock_in_  =
		createConnectSock(self->zmq_ctx_, "inproc://jobs", ZMQ_PULL,
				hwm);
	void* sock_out_ =
		createConnectSock(self->zmq_ctx_, "inproc://feedback", ZMQ_PUSH,
				hwm);
	VLOG_IF(2, !sock_in_) << "Compressor (" << self << "):"
	                      <<_(" error with input socket.");
	VLOG_IF(2, !sock_out_) << "Compressor (" << self << "):"
	                       <<_(" error with output socket.");
	if (!sock_in_ || !sock_out_)
	{
		LOG(ERROR) << "Compressor (" << self << "):"
		           << _(" error initializing communications.");
		return NULL;
	}
	z_stream zst;
	zst.zalloc    = Z_NULL;
	zst.zfree     = Z_NULL;
	zst.opaque    = Z_NULL;
	zst.avail_in  = 0;
	zst.avail_out = 0;
	zst.total_in  = 0;
	zst.total_out = 0;
	zst.next_in   = NULL;
	zst.next_out  = NULL;
	int rs = deflateInit2(&zst, level, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, 0);
	if (rs != Z_OK)
	{
		LOG(ERROR) << "Compressor (" << self << "):"
		           << _(" error initializing zstream.");
		zmq_close(sock_in_);
		zmq_close(sock_out_);
		return NULL;
	}
	MSG_READY.Send(sock_out_);
	zmq_pollitem_t event = {sock_in_, 0, ZMQ_POLLIN, 0};
	while(!self->break_)
	{
		int rs = zmq_poll(&event, 1, TICK);
		if (rs == 0)
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
		if (msg.DataSize() == 0 || msg.Data() == NULL)
		{
			VLOG(2) << "Compressor (" << self << "):"
			        << _(" failed receiving file chunk.");
			MSG_ERROR.Send(sock_out_);
			break;
		}
		zst.avail_in = msg.DataSize();
		zst.avail_out = sizeof(self->buf_);
		zst.next_in = (Bytef*)msg.Data();
		zst.next_out = (Bytef*)self->buf_;
		zst.total_in = 0;
		zst.total_out = 0;
		rs = deflate(&zst, Z_NO_FLUSH);
		if (rs != Z_OK || zst.avail_in != 0)
		{
			std::stringstream info;
			if (zst.avail_in != 0)
				info << strerror(errno);
			else
				info << _(" not all data was compressed");
			LOG(ERROR) << "Compressor (" << self << "):"
			           << _(" compression error.")
			           << _(" Message: ") << info.str();
			MSG_ERROR.Send(sock_out_);
			break;
		}
		zst.avail_in = 0;
		zst.avail_out = sizeof(self->buf_) - zst.total_out;
		zst.next_in = NULL;
		zst.next_out = (Bytef*)self->buf_ + zst.total_out;
		rs = deflate(&zst, Z_FULL_FLUSH);
		if (rs != Z_OK || zst.avail_in != 0)
		{
			std::stringstream info;
			if (zst.avail_in != 0)
				info << strerror(errno);
			else
				info << _(" not all data was compressed");
			LOG(ERROR) << "Compressor (" << self << "):"
			           << _(" compression error.")
			           << _(" Message: ") << info.str();
			MSG_ERROR.Send(sock_out_);
			break;
		}
		if (zst.total_out == 0)
		{
			LOG(ERROR) << "Compressor (" << self << "):"
			           << _(" compression error.")
			           << _(" Wrong available out value ");
			MSG_ERROR.Send(sock_out_);
			break;
		}
		Message result((uint8_t*)self->buf_, zst.total_out, msg.Num());
		if (result.DataSize() == 0)
		{
			VLOG(2) << "Compressor (" << self << "):"
			        << _(" error creating compressed message.");
			MSG_ERROR.Send(sock_out_);
			break;
		}
		if (result.Send(sock_out_) < result.DataSize())
		{
			VLOG(2) << "Compressor (" << self << "):"
			        << _(" failed send file chunk.");
			MSG_ERROR.Send(sock_out_);
			break;
		}
	}
	if (self->break_)
		VLOG(2) << "Compressor (" << self << "): breaked.";
	rs = deflateEnd(&zst);
	VLOG_IF(2, rs != Z_DATA_ERROR && rs != Z_OK)
		<< _("compressor cleaning error.") << _(" Code:") << rs;
	zmq_close(sock_in_);
	zmq_close(sock_out_);
}

} // namespace

