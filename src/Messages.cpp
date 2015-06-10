/**@author Merder Kim <$usermail>
 * @date 20140404 19:40:10*/

#include "Messages.hpp"
#include <logging.hpp>
#include <gettext.h>
#include "Utils.hpp"
#include <cassert>

namespace csio {

/**@fun Message::Message()
 * @brief makes empty message*/

/**@fun Message::Message(const Message&)
 * @brief copy ctor*/

/**@fun Message::Message(void*, SendMode)
 * @brief fetching ctor*/
Message::Message(void* sock, SendMode mode)
	: datasz_(0)
{
	Fetch(sock, mode);
}

/**@fun Message::Message(const uint8_t*, size_t, uint16_t)
 * @brief TYPE_FCHUNK ctor*/
Message::Message(const uint8_t* data, size_t sz, u16le num)
{
	datasz_ = sizeof(MessageType) + sizeof(u16le) + sz;
	data_.reset(new uint8_t[datasz_]);
	memset(data_.get(), 0, datasz_);
	uint8_t* pos = data_.get();

	add_to_buf(pos, TYPE_FCHUNK);
	add_to_buf(pos, num);
	add_to_buf(pos, data, sz);
	assert(pos == data_.get() + datasz_);
}

/**@fun Message::Message(const std::string&)
 * @brief TYPE_INFO ctor*/
Message::Message(const std::string& msg)
{
	datasz_ = sizeof(MessageType) + msg.length();
	data_.reset(new uint8_t[datasz_]);
	memset(data_.get(), 0, datasz_);
	uint8_t* pos = data_.get();

	add_to_buf(pos, TYPE_INFO);
	add_to_buf(pos, msg.c_str(), msg.length());
	assert(pos == data_.get() + datasz_);
}

/**@fun Message::Message(uint32_t, uint32_t)
 * @brief TYPE_MEMBER_CLOSE ctor
 *
 * See DZIP message structure
 *
 * 	+---+---+---+---+---+---+---+---+---+---+
 * 	| Z_FIN | MEMBER_CRC32  | MEMBER_FSIZE  |
 * 	+---+---+---+---+---+---+---+---+---+---+
 *
 * */
Message::Message(u32le fsize, u32le crc)
{
	const char Z_FINISH_TPL[] = {0x03, 0x00};
	datasz_ = sizeof(MessageType)
	        + sizeof(u16le)
	        + sizeof(Z_FINISH_TPL)
	        + sizeof(crc)
	        + sizeof(fsize);
	data_.reset(new uint8_t[datasz_]);
	memset(data_.get(), 0, datasz_);

	uint8_t* pos = data_.get();
	add_to_buf(pos, (uint8_t)TYPE_MCLOSE);
	add_to_buf(pos, u16le(0));
	add_to_buf(pos, Z_FINISH_TPL, sizeof(Z_FINISH_TPL));
	add_to_buf(pos, u32le(crc));
	add_to_buf(pos, fsize);
	assert(pos == data_.get() + datasz_);
}

/**@fun Message::Message(uint16_t, char, std::string, uint32_t)
 * @brief TYPE_HEADER ctor
 *
 * See DZIP message structure.
 *
 * 	+---+---+---+---+---+---+---+---+---+---+---+---+==========+=======+
 * 	|x1F|x8B|x08|FLG|     MTIME     |XFL|OS | XLEN  | RA_EXTRA | FNAME |
 * 	+---+---+---+---+---+---+---+---+---+---+---+---+==========+=======+
 *
 * RA_EXTRA:
 *
 * 	+---+---+---+---+---+---+---+---+---+---+================+
 * 	|x52|x41| EXLEN | VER=1 | CHLEN | CHCNT | CHUNKS_LENGTHS |
 * 	+---+---+---+---+---+---+---+---+---+---+================+
 * */
uint8_t       Message::FLG = FEXTRA;
uint8_t       Message::XFL = 0;
const uint8_t Message::OS  = OS_CODE_UNIX;
const u16le   Message::RA_EXT_HEADER_SIZE = 2 + 2 + (2 + 2 + 2);
const size_t  Message::GZIP_HEADER_SIZE = sizeof(GZIP_DEFLATE_ID)
                                        + sizeof(FLG) + sizeof(u32le)
                                        + sizeof(XFL)
                                        + sizeof(OS)
                                        + 2;
const off_t   Message::CHUNKS_LENGTHS_HEADER_OFFSET = GZIP_HEADER_SIZE
                                                    + RA_EXT_HEADER_SIZE;
Message::Message(u16le       chunks_count,
                 char        cmpr_level,
                 std::string fname,
                 u32le       mtime)
{
	if (cmpr_level == Z_BEST_COMPRESSION)
		XFL = 0x02;
	datasz_ = sizeof(MessageType)
	        + sizeof(u16le)
	        + CHUNKS_LENGTHS_HEADER_OFFSET
		+ chunks_count*2;
	Message::FLG = FEXTRA;
	if(!fname.empty())
	{
		datasz_ += fname.length() + 1;
		 FLG |= FNAME;
	}
	data_.reset(new uint8_t[datasz_]);
	memset(data_.get(), 0, datasz_);

	uint8_t* pos = data_.get();
	add_to_buf(pos, TYPE_MHEADER);
	add_to_buf(pos, u16le(0));
	add_to_buf(pos, GZIP_DEFLATE_ID, sizeof(GZIP_DEFLATE_ID));
	add_to_buf(pos, FLG);
	add_to_buf(pos, mtime);
	add_to_buf(pos, XFL);
	add_to_buf(pos, OS);
	add_to_buf(pos, u16le(RA_EXT_HEADER_SIZE + chunks_count*2));

	add_to_buf(pos, "RA", 2);
	add_to_buf(pos, u16le(RA_EXT_HEADER_SIZE - (2 + 2) + chunks_count*2));
	add_to_buf(pos, u16le(1));
	add_to_buf(pos, u16le(CHUNK_SIZE));
	add_to_buf(pos, u16le(chunks_count));
	pos += chunks_count*2;

	if(!fname.empty())
	{
		add_to_buf(pos, fname.c_str(), fname.length());
		add_to_buf(pos, uint8_t(0));
	}
	assert(pos == data_.get() + datasz_);
}

/**@brief Send message into socket
 * @param blocking sending mode (blocking/nonblocking)
 * @return on success returns true, otherwise - false*/
bool
Message::Send(void* sock, SendMode blocking) const
{
	zmq_msg_t msg;
	if (!data_ || datasz_ == 0)
		return false;
	if (zmq_msg_init_size(&msg, datasz_) == -1)
	{
		VLOG(2) << _("Message: error initializing zmq_msg_t in send.")
		        << _(" Message: ") << zmq_strerror(errno);
		return false;
	}
	memcpy(zmq_msg_data(&msg), data_.get(), datasz_);
	int rs = zmq_msg_send(&msg, sock, blocking ? 0 : ZMQ_DONTWAIT);
	if (rs == -1)
	{
		VLOG_IF(2, errno != EAGAIN && errno != EWOULDBLOCK)
			<< _("Message: error message sending.")
			<< _(" Message: ") << zmq_strerror(errno);
		return false;
	}
	if (rs != datasz_)
	{
		VLOG(2) << _("Message: not all data was sent.")
		        << _(" Message: ") << zmq_strerror(errno);
		return false;
	}
	return true;
}

/**@brief Get message from socket
 * @param blocking receiving mode (blocking/nonblocking)*/
void
Message::Fetch(void* zmq_sock, SendMode blocking)
{
	Clear();
	zmq_msg_t msg;
	if (zmq_msg_init(&msg) == -1)
	{
		VLOG(2) << _("Message: error initializing zmq_msg in fetch.")
		        << _(" Message: ") << zmq_strerror(errno);
		return;
	}
	if (zmq_msg_recv(&msg, zmq_sock, blocking ? 0 : ZMQ_DONTWAIT) == -1)
	{
		VLOG_IF(2, errno != EAGAIN)
			<< _("Message: error receiving data "
			     "in message initialization.")
			<< _(" Message: ") << zmq_strerror(errno);
	}
	else
	{
		datasz_ = zmq_msg_size(&msg);
		void *msgdata = zmq_msg_data(&msg);
		if (datasz_ > 0 && msgdata != NULL)
		{
			data_.reset(new uint8_t[datasz_]);
			memset(data_.get(), 0, datasz_);
			memcpy(data_.get(), msgdata, datasz_);
		}
		else
		{
			VLOG(2) << _("Message: error retrieving data"
			             " from zmq_msg.");
		}
	}
	zmq_msg_close(&msg);
}

/**@fun Message::Clear()
 * @brief Clear resources.*/

/**@fun Message::Type()
 * @brief Get Message type.*/

/**@fun Message::Data() const
 * @brief Get data to be appended to file
 *
 * Useless for TYPE_INFO Messages. These messages mustn't be written to
 * output file. So for TYPE_INFO this function returns NULL.*/

/**@fun Message::DataSize() const
 * @brief Get size of data to be appended to file
 *
 * Useless for TYPE_INFO Messages. These messages mustn't be written to
 * output file. So for TYPE_INFO this function returns 0.*/

/**@fun Message::Num() const
 * @brief Get fchunks sequence number
 *
 * For type TYPE_FCHUNK returning value is greater zero, for other types
 * - equal.*/

/**@fun Message::What() const
 * @brief Get info message
 *
 * For types, other then TYPE_INFO returns empty string.*/

/**@fun Message::operator<(const Message&) const
 * @brief sequence less operator
 *
 * Used for sorting TYPE_FCHUNK by sequence number*/

/**@fun Message::operator=(const Message&)
 * @brief Velid copy internal resources.*/

} // namespace

