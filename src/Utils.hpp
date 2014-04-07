/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include <zmq.h>
#include <logging.hpp>
#include <csio.h>

namespace csio {

inline void*
createSock(void* ctx, int type)
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
	return sock;
}

inline void*
createBindSock(void* ctx, std::string path, int type)
{
	void* sock = createSock(ctx, type);
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
createConnectSock(void* ctx, std::string path, int type)
{
	void* sock = createSock(ctx, type);
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

} // namespace

#endif // __UILS_HPP__

