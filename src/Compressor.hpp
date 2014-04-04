/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#ifndef __COMPRESSOR_HPP__
#define __COMPRESSOR_HPP__

namespace csio {

class Compressor
{
public:
	Compressor(void* zmq_ctx)
		: zmq_ctx_(zmq_ctx)
	{
	}

	static void* Start(Compressor* self);
private:
	Compressor() = delete;
	Compressor& operator=(const Compressor&) = delete;
	Compressor(const Compressor&) = delete;
	void* zmq_ctx_;
};

} // namespace

#endif // __COMPRESSOR_HPP__
