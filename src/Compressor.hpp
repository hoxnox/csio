/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#ifndef __COMPRESSOR_HPP__
#define __COMPRESSOR_HPP__

#include "Config.hpp"
#include <csio.h>

namespace csio {

class Compressor
{
public:
	Compressor(void* zmq_ctx, Config& cfg)
		: zmq_ctx_(zmq_ctx)
		, break_(false)
		, cfg_(cfg)
	{
	}

	static void* Start(Compressor* self, int level);
	void Break() { break_ = true; }
private:
	int compress(char* data, size_t datasz);
	Compressor() = delete;
	Compressor& operator=(const Compressor&) = delete;
	Compressor(const Compressor&) = delete;
	void* zmq_ctx_;
	bool  break_;
	Config cfg_;
	char buf_[CHUNK_SIZE*2];
};

} // namespace

#endif // __COMPRESSOR_HPP__
