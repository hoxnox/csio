/**@author Merder Kim <$usermail>
 * @date 20140404 19:40:10*/

#ifndef __MESSAGES_HPP__
#define __MESSAGES_HPP__

#include <zmq.h>
#include <stdint.h>
#include <string>
#include <endians.hpp>
#include <algorithm>
#include <memory>
#include <cstring>

namespace csio {

class Message
{
public:
	enum MessageType :uint8_t
	{
		TYPE_INFO              = 1,
		TYPE_FCHUNK            = 2,
		TYPE_UNKNOWN           = 0
	};
	static const bool BLOCKING_MODE;

	Message() : datasz_(0) { }
	Message(void* sock, bool blocking = false);
	Message(const uint8_t* data, size_t datasz, uint16_t num);
	Message(const std::string& msg);
	Message(const Message& msg) { operator=(msg); }

	virtual ~Message() { Clear(); }

	void        Clear();
	MessageType Type();
	int         Send(void* sock, bool blocking = false) const;
	void        Fetch(void* zmq_sock, bool blocking = false);

	uint8_t*    Data() const;
	size_t      DataSize() const;
	uint16_t    Num() const;
	std::string What() const;

	bool operator==(const Message& rhv) const;
	bool operator<(const Message& rhv) const
	{
		if(Num() < rhv.Num())
			return true;
		return false;
	}
	Message& operator=(const Message& copy)
	{
		datasz_ = copy.datasz_;
		data_.reset(new uint8_t[datasz_]);
		memcpy(data_.get(), copy.data_.get(), datasz_);
	}

protected:
	size_t   datasz_;
	std::unique_ptr<uint8_t[]> data_;
};

const Message MSG_READY("READY");
const Message MSG_ERROR("ERROR");
const Message MSG_STOP("STOP");
const Message MSG_FILLN("FILLN");


////////////////////////////////////////////////////////////////////////
// inline

inline void
Message::Clear()
{
	if(data_)
		data_.reset(NULL);
	datasz_ = 0;
}

inline Message::MessageType
Message::Type()
{
	return data_ && datasz_ > 0 ? (Message::MessageType)data_[0] : TYPE_UNKNOWN;
};

inline uint8_t*
Message::Data() const
{
	return data_ && datasz_ > 3 ? data_.get() + 1 + 2: NULL;
}

inline size_t
Message::DataSize() const
{
	if (datasz_ < 1 + 2)
		return 0;
	return datasz_ - 1 - 2;
}

inline uint16_t
Message::Num() const
{
	u16be num_ = 0;
	if(data_ && datasz_ > 2)
	{
		num_.bytes[0] = data_[1];
		num_.bytes[1] = data_[2];
	}
	return num_;
}

inline std::string
Message::What() const
{
	std::string rs;
	if (data_ && datasz_ > 0)
		rs.assign((const char*)data_.get() + 1, datasz_ - 1);
	return rs;
};

inline bool 
Message::operator==(const Message& rhv) const
{
	if (datasz_ != rhv.datasz_ || datasz_ == 0)
		return false;
	uint8_t* lhv_data = data_.get();
	uint8_t* rhv_data = rhv.data_.get();
	if (!lhv_data || !rhv_data)
		return false;
	if (std::equal(lhv_data, lhv_data + datasz_, rhv_data))
		return true;
	return false;
}

} // namespace

#endif // __MESSAGES_HPP__

