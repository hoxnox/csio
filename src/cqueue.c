/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140324 20:20:30*/

#include <memory.h>
#include <malloc.h>

#include "cqueue.h"

/**@brief Initialize chunks queue.
 * @note Use free_chunks_queue to release resources.
 * @return 0 on success*/
int
init_chunks_queue(FileChunksQueue* queue)
{
	queue->back = &(queue->end);
	queue->front = queue->back;
	queue->end.next = queue->back;
	queue->end.prev = queue->back;
	queue->chunks_passed = 0;
	queue->last_element_datasz = 0;
	queue->max_queue_len = MAX_QUEUE_LEN;
	if (pthread_mutex_init(&queue->mtx, NULL) != 0
	 || pthread_cond_init(&queue->can_push, NULL) != 0
	 || pthread_cond_init(&queue->can_pop, NULL) != 0 )
	{
		return -1;
	}
	return 0;
}

int
is_chunks_queue_empty(FileChunksQueue* queue)
{
	pthread_mutex_lock(&queue->mtx);
	if (queue == NULL)
	{
		pthread_mutex_unlock(&queue->mtx);
		return 1;
	}
	if (queue->back == &queue->end || queue->front == &queue->end)
	{
		pthread_mutex_unlock(&queue->mtx);
		return 1;
	}
	pthread_mutex_unlock(&queue->mtx);
	return 0;
}

int
is_chunks_queue_closed(FileChunksQueue* queue)
{
	return queue->last_element_datasz != 0;
}

/**@return 0 on success, -1 on error */
int
push_back_chunks_queue(FileChunksQueue* queue, char* data,
		size_t datasz, unsigned short chunk_no)
{
	pthread_mutex_lock(&queue->mtx);
	while (chunk_no - queue->chunks_passed >= queue->max_queue_len)
	{
		struct timespec tm = default_tm;
		pthread_cond_timedwait(&queue->can_push, &queue->mtx, &tm);
	}
	if (queue == NULL)
	{
		pthread_mutex_unlock(&queue->mtx);
		return -1;
	}
	if (datasz > CHUNK_SIZE)
	{
		pthread_mutex_unlock(&queue->mtx);
		return -1;
	}
	if (is_chunks_queue_closed(queue))
	{
		pthread_mutex_unlock(&queue->mtx);
		return -1;
	}
	FileChunk* aux = (FileChunk*)malloc(sizeof(FileChunk));
	memset(aux, 0, sizeof(FileChunk));
	if (!aux)
	{
		pthread_mutex_unlock(&queue->mtx);
		return -1;
	}
	queue->back->next = aux;
	aux->prev = queue->back;
	queue->end.prev = aux;
	aux->next = &(queue->end);
	if (queue->front == &queue->end)
		queue->front = aux;
	queue->back = aux;
	if (datasz != CHUNK_SIZE)
		queue->last_element_datasz = datasz;
	memcpy(aux->data, data, datasz);
	aux->chunk_no = chunk_no;
	pthread_cond_signal(&queue->can_pop);
	pthread_mutex_unlock(&queue->mtx);
	return 0;
}

int
is_file_chunk_empty(FileChunk* chunk)
{
	if (chunk->next == NULL)
		return 1;
	if (chunk->prev == NULL)
		return 1;
	return 0;
}

FileChunk
pop_front_chunks_queue(FileChunksQueue* queue)
{
	FileChunk result;
	result.next = NULL;
	result.prev = NULL;
	pthread_mutex_lock(&queue->mtx);
	if (queue == NULL)
	{
		pthread_mutex_unlock(&queue->mtx);
		return result;
	}
	if (queue->back == &queue->end || queue->front == &queue->end)
	{
		pthread_mutex_unlock(&queue->mtx);
		return result;
	}
	memcpy(&result, queue->front, sizeof(FileChunk));
	queue->front->next->prev = &(queue->end);
	queue->end.next = queue->front->next;
	free(queue->front);
	queue->front = queue->end.next;
	++queue->chunks_passed;
	pthread_cond_signal(&queue->can_push);
	pthread_mutex_unlock(&queue->mtx);
	return result;
}

void
free_chunks_queue(FileChunksQueue* queue)
{
	while (!is_chunks_queue_empty(queue))
		pop_front_chunks_queue(queue);
	if (queue)
	{
		queue->last_element_datasz = 0;
		pthread_mutex_destroy(&queue->mtx);
		pthread_cond_destroy(&queue->can_pop);
		pthread_cond_destroy(&queue->can_push);
	}
}


