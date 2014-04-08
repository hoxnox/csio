/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10*/

#ifndef __WRITER_HPP__
#define __WRITER_HPP__

#include <cstdio>
#include "Utils.hpp"

namespace csio {

class Writer
{
public:
	Writer(void* zmq_ctx, int hwm)
		: zmq_ctx_(zmq_ctx)
		, break_(false)
		, hwm_(hwm)
		, header_pos_(0)
	{
	}

	static void* Start(Writer* self, int out_file_descriptor);
	void Break() { break_ = true; };
private:
	Writer() = delete;
	Writer(const Writer&) = delete;
	Writer& operator=(const Writer&) = delete;
	void*  zmq_ctx_;
	bool   break_;
	int    hwm_;
	off_t  header_pos_;
	size_t bytes_written_;
	char   lbuf_[CHUNKS_PER_MEMBER*2];
	size_t lbufsz_;
};

} // namespace

#endif // __WRITER_HPP__
