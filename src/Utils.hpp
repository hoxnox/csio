/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include <zmq.h>
#include <logging.hpp>
#include <csio.h>
#include <gettext.h>

namespace csio {

inline void*
createSock(void* ctx, int type, int hwm = 50)
{
	void* sock = zmq_socket(ctx, type);
	if (!sock)
	{
		VLOG(2) << _("Error creating socket.")
		        << _(" Message: ") << zmq_strerror(errno);
		return NULL;
	}
	const int zero = 0;
	const int64_t msgsz = CHUNK_SIZE + 10;
	if (zmq_setsockopt(sock, ZMQ_MAXMSGSIZE, &msgsz, sizeof(msgsz)) == -1)
	{
		VLOG(2) << _("Error setting MAXMSGSIZE on socket.")
		        << _(" Message: ") << zmq_strerror(errno);
		return NULL;
	}
	if (zmq_setsockopt(sock, ZMQ_LINGER, &zero, sizeof(zero)) == -1)
	{
		VLOG(2) << _("Error setting LINGER on socket.")
		        << _(" Message: ") << zmq_strerror(errno);
		return NULL;
	}
	if (zmq_setsockopt(sock, ZMQ_RCVHWM, &hwm, sizeof(hwm)) == -1)
	{
		VLOG(2) << _("Error setting high water mark on recv to socket.")
		        << _(" Message: ") << zmq_strerror(errno);
		return NULL;
	}
	if (zmq_setsockopt(sock, ZMQ_SNDHWM, &hwm, sizeof(hwm)) == -1)
	{
		VLOG(2) << _("Error setting high water mark on send to socket.")
		        << _(" Message: ") << zmq_strerror(errno);
		return NULL;
	}
	return sock;
}

inline void*
createBindSock(void* ctx, std::string path, int type, int hwm = 50)
{
	void* sock = createSock(ctx, type, hwm);
	if (!sock)
		return NULL;
	if (zmq_bind(sock, path.c_str()) == -1)
	{
		VLOG(2) << _("Error binding socket.")
		        << _(" Message: ") << zmq_strerror(errno);
		return NULL;
	}
	return sock;
}

inline void*
createConnectSock(void* ctx, std::string path, int type, int hwm = 50)
{
	void* sock = createSock(ctx, type, hwm);
	if (!sock)
		return NULL;
	if (zmq_connect(sock, path.c_str()) == -1)
	{
		VLOG(2) << _("Error connecting socket.")
		        << _(" Message: ") << zmq_strerror(errno);
		return NULL;
	}
	return sock;
}

const size_t TICK  = 10;
/**@brief maximum chunks count in the member
 *
 * This value limited by XLEN (see file structure). This value is two
 * byte long. So maximum 0xffff. We need to subfield header 'RA'+length
 * bytes, 2 bytes for version, 2 bytes for chunk size and 2 bytes for
 * chunks count. left bytes can be used to write chunk's lengths.*/
const size_t CHUNKS_PER_MEMBER = (0xffff - (2 + 2) - (2 + 2 + 2)) / 2;
const size_t MEMBER_HEADER_MINSZ = 10 + (2 + (2 + 2 + (2 + 2 + 2)));
const char   Z_FINISH_TEMPLATE[] = {0x03, 0x00};


} // namespace

#endif // __UILS_HPP__

