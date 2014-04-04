/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10*/

#ifndef __WRITER_HPP__
#define __WRITER_HPP__

namespace csio {

class Writer
{
public:
	Writer(void* zmq_ctx)
		: zmq_ctx_(zmq_ctx)
	{
	}

	static void* Start(Writer* self);
private:
	Writer() = delete;
	Writer(const Writer&) = delete;
	Writer& operator=(const Writer&) = delete;
	void* zmq_ctx_;
};

} // namespace

#endif // __WRITER_HPP__
