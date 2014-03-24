/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140324 20:20:30 */

#ifndef __DZIP_H__
#define __DZIP_H__

#include <pthread.h>
#include <csio.h>

static int       compress_level = 7;
static struct    timespec default_tm = {1, 0};
static int       stop = 0;
static const int MAX_QUEUE_LEN = 100;

#endif // __DZIP_H__

