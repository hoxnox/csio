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

TEST_F(TestCQueue, main_functionality)
{
	EXPECT_TRUE(is_chunks_queue_empty(&queue));
	EXPECT_FALSE(is_chunks_queue_closed(&queue));
	char data[CHUNK_SIZE];
	memset(data, 1, sizeof(data));
	ASSERT_EQ(push_back_chunks_queue(&queue, data, sizeof(data), 1), 0);
	ASSERT_FALSE(is_chunks_queue_empty(&queue));
	EXPECT_FALSE(is_chunks_queue_closed(&queue));
	ASSERT_EQ(queue.front->chunk_no, 1);
	ASSERT_EQ(queue.front, queue.back);
	ASSERT_NE(queue.front, &queue.end);
	ASSERT_NE(queue.back, &queue.end);
	size_t i = 3;
	for (; i < 10; ++i)
	{
		ASSERT_EQ(push_back_chunks_queue(&queue, data, sizeof(data), i), 0);
		ASSERT_EQ(queue.front->chunk_no, 1);
		ASSERT_EQ(queue.back->chunk_no, i);
		ASSERT_NE(queue.front, &queue.end);
		ASSERT_NE(queue.back, &queue.end);
		ASSERT_EQ(queue.end.next, queue.front);
		ASSERT_EQ(queue.end.prev, queue.back);
		ASSERT_EQ(queue.front->prev, &queue.end);
		ASSERT_EQ(queue.back->next, &queue.end);
		ASSERT_FALSE(is_chunks_queue_empty(&queue));
		EXPECT_FALSE(is_chunks_queue_closed(&queue));
	}
	ASSERT_EQ(push_back_chunks_queue(&queue, data, sizeof(data) - 2, i+1), 0);
	ASSERT_EQ(queue.front->chunk_no, 1);
	ASSERT_EQ(queue.back->chunk_no, i+1);
	ASSERT_NE(queue.front, &queue.end);
	ASSERT_NE(queue.back, &queue.end);
	ASSERT_EQ(queue.end.next, queue.front);
	ASSERT_EQ(queue.end.prev, queue.back);
	ASSERT_EQ(queue.front->prev, &queue.end);
	ASSERT_EQ(queue.back->next, &queue.end);
	ASSERT_FALSE(is_chunks_queue_empty(&queue));
	EXPECT_TRUE(is_chunks_queue_closed(&queue));
}

#endif // __TCQUEUE_HPP__

