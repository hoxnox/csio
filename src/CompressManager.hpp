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
#include <zlib.h>

#include "ProcessManagerBase.hpp"
#include "Config.hpp"
#include "Writer.hpp"
#include "Compressor.hpp"
#include "Messages.hpp"

namespace csio {

class CompressManager : public ProcessManagerBase
{
public:
	CompressManager(const Config& cfg);
	~CompressManager();

protected:
	virtual bool doStart();
	virtual bool doStop();
	static void  loop(CompressManager* self);

public:
	typedef struct _IFStat
	{
		_IFStat() { Clear(); };
		void Clear();
		int         handler;      //!< input file handler
		size_t      bytes;        //!< size of input file
		std::string basename;     //!< input file basename
		size_t      members;      //!< total members count
		size_t      chunks;       //!< total chunks count
		u32le       mtime;        //!< input file modification time
		uint8_t     level;        //!< compression level
		size_t      chunksz;      //!< CHUNK_SIZE
		// compression total info
		size_t      members_tx;   //!< members transfered to the writer
		size_t      bytes_rx;     //!< bytes read from the input file
		size_t      bytes_tx;     //!< bytes transfered to the writer
		size_t      chunks_rx;    //!< chunks read from the input file
		size_t      chunks_tx;    //!< chunks transfered to the writer
		// current member info
		u32le      cur_bytes_rx; //!< bytes_rx for current member only
		u32le      cur_bytes_tx; //!< bytes_tx for current member only
		u16le       cur_chunks_rx;//!< chunks_rx for current member only
		u16le       cur_chunks_tx;//!< chunks_tx for current member only
		u16le       cur_chunks_mx;//!< maximum chunks_rx for this member
		uLong       cur_crc32;    //!< uncompressed data crc32 for
		                          //!< current member only
	} IFStat;

private:
	bool createSocks();
	bool waitChildrenReady(const size_t timeout_ms);
	bool makeInitialPush();
	bool makeRegularPush();
	bool flushOrderingSet();
	bool openFiles();

	enum PollStatus :bool
	{
		POLL_BREAK    = false,
		POLL_CONTINUE = true
	};
	PollStatus processCompressorIncoming();
	PollStatus processWriterIncoming();

private:
	void* zmq_ctx_;
	void* sock_inbox_;
	void* sock_outbox_;
	void* sock_writer_;

	std::unique_ptr<Writer>                    writer_instance_;
	std::vector<std::unique_ptr<Compressor> >  compressors_instances_;
	std::unique_ptr<std::thread>               writer_thread_;
	std::vector<std::unique_ptr<std::thread> > workers_threads_;
	std::unique_ptr<std::thread>               loop_thread_;
	std::set<Message>                          ordering_set_;

	Config       cfg_;
	IFStat       ifs_;
	int          ofd_;
	bool         stop_;
	const size_t ORDERING_SET_HWM;
	const size_t MSG_QUEUE_HWM;
	int          msg_pushed_;
	size_t       compressors_count_;

};

////////////////////////////////////////////////////////////////////////

inline void
CompressManager::_IFStat::Clear()
{
	handler = -1;
	bytes = 0;
	basename = "";
	members = 0;
	chunks = 0;
	mtime = 0;
	level = 0;
	chunksz = CHUNK_SIZE;
	members_tx = 0;
	bytes_rx = 0;
	bytes_tx = 0;
	chunks_rx = 0;
	chunks_tx = 0;
	cur_bytes_rx = 0;
	cur_bytes_tx = 0;
	cur_chunks_rx = 0;
	cur_chunks_tx = 0;
	cur_crc32 = crc32(0L, Z_NULL, 0);
}

} // namespace


#endif // __READER_HPP__
