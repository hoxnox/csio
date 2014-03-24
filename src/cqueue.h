/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140324 20:20:30*/

#ifndef __CQUEUE_H__
#define __CQUEUE_H__

#include "dzip.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FileChunk
{
	char               data[CHUNK_SIZE];
	char               zdata[CHUNK_SIZE*2];
	size_t             zdatasz;
	struct _FileChunk* next;
	struct _FileChunk* prev;
	unsigned short     chunk_no;
} FileChunk;
int is_file_chunk_empty(FileChunk* chunk);

typedef struct
{
	FileChunk*      front;
	FileChunk*      back;
	FileChunk       end;
	pthread_mutex_t mtx;
	pthread_cond_t  can_pop;
	pthread_cond_t  can_push;
	size_t          chunks_passed;
	unsigned char   last_element_datasz;
	int             max_queue_len;
} FileChunksQueue;

int        init_chunks_queue      (FileChunksQueue* queue);
void       free_chunks_queue      (FileChunksQueue* queue);
int        is_chunks_queue_empty  (FileChunksQueue* queue);
int        is_chunks_queue_closed (FileChunksQueue* queue);
int        push_back_chunks_queue (FileChunksQueue* queue,
                                   char* data, size_t datasz,
                                   unsigned short chunk_no);
FileChunk  pop_front_chunks_queue (FileChunksQueue* queue);

#ifdef __cplusplus
}
#endif


#endif // __CQUEUE_H__

