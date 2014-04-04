/**@author Merder Kim <Merder Kim>
 * @date 20140404 19:40:10 */

#include <Messages.hpp>
#include <string>

using namespace csio;

class TestMessages : public ::testing::Test
{
protected:
	void SetUp()
	{
		zmq_ctx = zmq_init(0);
		ASSERT_TRUE(zmq_ctx != NULL) << zmq_strerror(errno);
		int zero = 0;
		sock_in = zmq_socket(zmq_ctx, ZMQ_PAIR);
		ASSERT_TRUE(sock_in != NULL) << zmq_strerror(errno);
		ASSERT_NE(zmq_setsockopt(sock_in, ZMQ_LINGER, &zero, sizeof(zero)), -1) << zmq_strerror(errno);
		sock_out = zmq_socket(zmq_ctx, ZMQ_PAIR);
		ASSERT_TRUE(sock_out != NULL) << zmq_strerror(errno);
		ASSERT_NE(zmq_setsockopt(sock_out, ZMQ_LINGER, &zero, sizeof(zero)), -1) << zmq_strerror(errno);
		ASSERT_NE(zmq_bind(sock_in, "inproc://test"), -1) << zmq_strerror(errno);
		ASSERT_NE(zmq_connect(sock_out, "inproc://test"), -1) << zmq_strerror(errno);
	}
	void TearDown()
	{
		zmq_close(sock_in);
		zmq_close(sock_out);
		zmq_ctx_destroy(zmq_ctx);
	}
	static const std::string data_s;
	static const uint8_t* data;
	static const size_t datasz;
	void* zmq_ctx, *sock_in, *sock_out;
};

const std::string TestMessages::data_s("Hello, world!");
const uint8_t* TestMessages::data = (const uint8_t*)data_s.c_str();
const size_t TestMessages::datasz = data_s.length();

TEST_F(TestMessages, MsgFChunk)
{
	Message msg(data, datasz, 100);
	ASSERT_EQ(msg.Type(), Message::TYPE_FCHUNK);
	uint8_t* dt = msg.Data();
	ASSERT_EQ(msg.DataSize(), datasz);
	ASSERT_TRUE(std::equal(dt, dt + msg.DataSize(), data));
	ASSERT_EQ(msg.Num(), 100);
	ASSERT_EQ(msg.Send(sock_out), data_s.length() + 1 + 2) << zmq_strerror(errno);
	Message msg_(sock_in);
	ASSERT_EQ(msg_.Type(), Message::TYPE_FCHUNK);
	dt = msg_.Data();
	ASSERT_EQ(msg_.DataSize(), datasz);
	ASSERT_TRUE(std::equal(dt, dt + msg_.DataSize(), data));
	ASSERT_EQ(msg_.Num(), 100);
}

TEST_F(TestMessages, MsgInfo)
{
	Message msg(data_s);
	ASSERT_EQ(msg.Type(), Message::TYPE_INFO);
	ASSERT_EQ(msg.What(), std::string((const char*)data, datasz));
	ASSERT_EQ(msg.Send(sock_out), data_s.length() + 1) << zmq_strerror(errno);
	Message msg_(sock_in);
	ASSERT_EQ(msg_.Type(), Message::TYPE_INFO);
	ASSERT_EQ(msg_.What(), std::string((const char*)data, datasz));
	ASSERT_EQ(msg_, msg_);
}

