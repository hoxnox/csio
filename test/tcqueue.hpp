/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140324 20:20:30 */

#ifndef __TCQUEUE_HPP__
#define __TCQUEUE_HPP__

#include <cqueue.h>

class TestCQueue : public ::testing::Test
{
protected:
	void SetUp()
	{
		ASSERT_EQ(init_chunks_queue(&queue), 0);
	}
	void TearDown()
	{
		free_chunks_queue(&queue);
	}
	FileChunksQueue queue;
};

TEST_F(TestCQueue, simple)
{
	char data[] = "DATA";
	push_back_chunks_queue(&queue, data, sizeof(data), 1);
}

#endif // __TCQUEUE_HPP__

