/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140221 15:40:20
 *
 * Stdlib based compressed stream I/O library. It gives you ability
 * transparently work with compressed streams as if they are not.*/

#ifndef __CSIO_H__
#define __CSIO_H__

#include <stdio.h>
#include <stdint.h>
#include <zlib.h>
#include "csio_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum 
{
	GZIP    = 1,
	DICTZIP = 2,
	NONE = 0
} CompressionMethod;

static const int INITIALIZED = 0x484F584E;
static const char GZIP_DEFLATE_ID[3] = {(char)0x1f, (char)0x8b, (char)0x08};
static const char FTEXT     = 1;
static const char FHCRC     = 1 << 1;
static const char FEXTRA    = 1 << 2;
static const char FNAME     = 1 << 3;
static const char FCOMMENT  = 1 << 4;
static const char FRESERVED = 0xfe;
static const char OS_CODE_UNIX = 3;

#define CHUNK_SIZE 58315
static const size_t   EMPTY_FINISH_BLOCK_LEN = 2;
static const size_t   GZIP_CRC32_LEN = 4;


typedef struct {
	FILE*             stream;
	off_t             currpos;
	uint16_t          chlen;
	uint64_t          size;
	off_t             bufoff;
	uint16_t          bufsz;
	char              need_close;
	CompressionMethod compression;
	char              buf[0x10000];
	size_t            idxsz;
	char*             idx;
	z_stream          zst;
	int               init_magic;
	int               eof;
} CFILE;


CSIO_API CFILE* cfopen(const char* name, const char* mode);
CSIO_API int    cferror(CFILE* stream);
CSIO_API CFILE* cfinit(FILE* stream);
CSIO_API void   cfclose(CFILE** stream);
CSIO_API int    cfeof(CFILE* stream);
CSIO_API int    cfseek(CFILE* stream, long pos, int mode);
CSIO_API int    cfseeko(CFILE* stream, off_t offset, int mode);
CSIO_API long   cftell(CFILE* stream);
CSIO_API off_t  cftello(CFILE* stream);
CSIO_API size_t cfread(void* dest, size_t size, size_t count, CFILE* stream);
CSIO_API int    cfgetc(CFILE* stream);

#ifdef __cplusplus
}
#endif

#endif
