/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#ifndef __READER_HPP__
#define __READER_HPP__

#include <thread>
#include <vector>
#include <set>
#include <string>
#include <csio.h>
#include <zmq.h>

#include "ProcessManagerBase.hpp"
#include "Config.hpp"
#include "Writer.hpp"
#include "Compressor.hpp"

namespace csio {

/**@brief Compress manager
 *
 * The main subject of the compression process. CompressManager starts
 * several compressors and connect them through dealer-replier paradigm
 * (see zeromq manual). CompressManager gets file chunks, send them to
 * the Compressors, receives answers (compressed chunk, or error),
 * reorders them and provide to Writer. On file change, it waits until
 * all Compressors finished, and after the last compressed file chunk of
 * the previous file, it sends `renew` message to the Writer.
 *
 * Communication diagram:
 *
 * +-------------------+              +--------+
 * | CompressManager   |PAIR------PAIR| Writer |
 * +-------------------+              +--------+
 *  PULL(feedback) PUSH(jobs)
 *   |              |
 *   |     +--------+--- ... ---+
 *   |     |        |           |
 *   |    PULL     PULL       PULL
 *   |  +------+ +--*---+   +------+
 *   |  |Cmprs | |Cmprs |   |Cmprs |
 *   |  +------+ +------+   +------+
 *   |    PUSH     PUSH       PUSH
 *   |     |        |          |
 *   +-----+--------+----------+
 **/

class CompressManager : public ProcessManagerBase
{
public:

	static const size_t TICK;

	CompressManager(const Config& cfg);
	~CompressManager();

protected:
	virtual bool doStart();
	virtual bool doStop();

private:
	static void loop(CompressManager* self);
	int         createSocks();
	int         openFiles();
	int         waitThreadsReady(const size_t timeout_ms);

private:
	void* zmq_ctx_;
	void* sock_jobs_;
	void* sock_feedback_;
	void* sock_writer_;

	std::unique_ptr<Writer>                    writer_instance_;
	std::vector<std::unique_ptr<Compressor> >  compressors_instances_;
	std::unique_ptr<std::thread>               writer_;
	std::vector<std::unique_ptr<std::thread> > workers_pool_;
	std::unique_ptr<std::thread>               loop_;

	Config cfg_;
	int    ofd_, ifd_;
	bool   stop_;
};

} // namespace


#endif // __READER_HPP__
