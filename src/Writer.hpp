/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10*/

#ifndef __WRITER_HPP__
#define __WRITER_HPP__

#include <cstdio>
#include "Utils.hpp"
#include "Messages.hpp"

namespace csio {

class Writer
{
public:
	Writer(void* zmq_ctx, int hwm)
		: zmq_ctx_(zmq_ctx)
		, fstream_(NULL)
		, chunks_lengths_off_(0)
		, break_(false)
	{
		sock_ = createConnectSock(
			zmq_ctx_, "inproc://writer", ZMQ_PAIR, hwm);
	}

	static void* Start(Writer* self, int out_file_descriptor);
private:
	bool processMessage(const Message& msg);
	Writer() = delete;
	Writer(const Writer&) = delete;
	Writer& operator=(const Writer&) = delete;
	void*   zmq_ctx_;
	FILE*   fstream_;
	void*   sock_;
	off_t   chunks_lengths_off_;
	bool    break_;
	uint8_t lbuf_[CHUNKS_PER_MEMBER*2];
	size_t  lbufsz_;
};

} // namespace

#endif // __WRITER_HPP__
