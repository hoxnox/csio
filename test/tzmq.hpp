/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 
 *
 * ZMQ communication concepts tests.*/

#include <thread>
#include <vector>
#include <sstream>
#include <zmq.h>

class TestZMQ : public ::testing::Test
{
public:
	void SetUp()
	{
		context = zmq_init(0);
		ASSERT_TRUE(context != NULL);
		stop = false;
	}
	void TearDown()
	{
		if (context)
			zmq_ctx_destroy(context);
	}
	void *context;
	bool stop;
};

TEST_F(TestZMQ, zmq_pair)
{
	char buffer [10];
	void *socket = zmq_socket(context, ZMQ_PAIR);
	ASSERT_TRUE(socket != NULL);
	ASSERT_EQ(zmq_bind(socket, "inproc://test"), 0) << zmq_strerror(errno);

	std::thread client([this, &buffer]()
	{
		memset(buffer, 0, sizeof(buffer));
		void* socket = zmq_socket(context, ZMQ_PAIR);
		ASSERT_EQ(zmq_connect(socket, "inproc://test"), 0) << zmq_strerror(errno);
		zmq_send(socket, "Hello", 5, 0);
		zmq_recv(socket, buffer, 10, 0);
		ASSERT_EQ(std::string(buffer), "World");
		stop = true;
		zmq_close(socket);
	});

	memset(buffer, 0, sizeof(buffer));
	zmq_recv (socket, buffer, 10, 0);
	ASSERT_EQ(std::string(buffer), "Hello");
	zmq_send (socket, "World", 5, 0);
	zmq_close(socket);

	client.join();
}

TEST_F(TestZMQ, zmq_workers_pool)
{
	size_t replyer = 0;
	const int count = 100;
	std::vector<std::thread*> workers_pool;
	void *socket = zmq_socket(context, ZMQ_PUSH);
	void *ready = zmq_socket(context, ZMQ_PULL);
	ASSERT_TRUE(socket != NULL);
	ASSERT_EQ(zmq_bind(socket, "inproc://test"), 0) << zmq_strerror(errno);
	ASSERT_EQ(zmq_bind(ready, "inproc://ready"), 0) << zmq_strerror(errno);

	bool stop = false;
	for(size_t i = 0; i < count; ++i)
	{
		std::thread* worker = new std::thread([this,&stop,i]()
		{
			char buffer [10];
			memset(buffer, 0, sizeof(buffer));
			void* socket = zmq_socket(context, ZMQ_PULL);
			void* ready  = zmq_socket(context, ZMQ_PUSH);
			ASSERT_EQ(zmq_connect(socket, "inproc://test"), 0) << zmq_strerror(errno);
			ASSERT_EQ(zmq_connect(ready, "inproc://ready"), 0) << zmq_strerror(errno);
			zmq_send(ready, "STARTED", 7, 0);
			while(!stop)
			{
				zmq_pollitem_t ev;
				ev.socket = socket;
				ev.events = ZMQ_POLLIN;
				int rs = zmq_poll(&ev, 1, 10 + i);
				ASSERT_NE(rs, -1) << zmq_strerror(errno);
				if (rs == 0)
					continue;
				if (ev.revents & ZMQ_POLLIN)
				ASSERT_GT(zmq_recv(socket, buffer, 10, ZMQ_DONTWAIT), 0) << zmq_strerror(errno);
				ASSERT_EQ(std::string(buffer), "Hello");
				std::stringstream result;
				result << "RESULT" << i;
				zmq_send(ready, result.str().c_str(), result.str().length(), 0);
			}
			zmq_close(socket);
			zmq_close(ready);
		});
		workers_pool.push_back(worker);
	}
	char buffer [10];
	for (size_t i = 0; i < count; ++i)
	{
		memset(buffer, 0, sizeof(buffer));
		ASSERT_NE(zmq_recv(ready, buffer, 10, 0), -1) << zmq_strerror(errno);
		ASSERT_EQ(std::string(buffer), "STARTED");
	}
	for (size_t i = 0; i < count; ++i)
		ASSERT_NE(zmq_send(socket, "Hello", 5, 0), -1) << zmq_strerror(errno);
	std::string prev_fingerprint;
	bool different_workers = false;
	for (size_t i = 0; i < count; ++i)
	{
		memset(buffer, 0, sizeof(buffer));
		ASSERT_NE(zmq_recv(ready, buffer, 10, 0), -1) << zmq_strerror(errno);
		ASSERT_EQ(std::string(buffer).substr(0,6), "RESULT");
		if(!prev_fingerprint.empty() && prev_fingerprint != buffer)
			different_workers = true;
		prev_fingerprint.assign(buffer);
	}
	ASSERT_TRUE(different_workers);
	stop = true;
	for(size_t i = 0; i < count; ++i)
	{
		workers_pool[i]->join();
		delete workers_pool[i];
	}
	zmq_close(socket);
	zmq_close(ready);
}

