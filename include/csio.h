/**@author Merder Kim <hoxnox@gmail.com>
 * @date $date$
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

#endif // __CSIO_H__
