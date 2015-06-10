/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140221 15:40:20
 *
 * Stdlib based compressed stream I/O library. It gives you ability
 * transparently work with compressed streams as if they are not.
 *
 * # DZIP file structure:
 *
 * 	+=============+=============+ ... +=============+
 * 	|DZIP_MEMBER_1|DZIP_MEMBER_2|     |DZIP_MEMBER_N|
 * 	+=============+=============+ ... +=============+
 *
 * DZIP_MEMBER:
 *
 * 	+---+---+---+---+---+---+---+---+---+---+---+---+
 * 	|x1F|x8B|x08|FLG|     MTIME     |XFL|OS | XLEN  |->
 * 	+---+---+---+---+---+---+---+---+---+---+---+---+
 * 	+===========+===========+======+
 * 	| RA_EXTRA  | FNAME     | BODY |
 * 	+===========+===========+======+
 *
 * 	FLG      - flags. FEXTRA|FNAME is used
 * 	MTIME    - modification time of the original file (filled only
 * 	           for first member, other members has 0)
 * 	XFL      - extra flags about the compression.
 * 	OS       - operating system
 * 	XLEN     - total extra fields length (RA_EXTRA)
 * 	RA_EXTRA - RFC1952 formated Random Access header's extra field (later)
 * 	FNAME    - zero terminated string - base (without directory)
 * 	           file name (filled only for the first member, others
 * 	           has zero-length FNAME)
 * 	BODY     - see below
 * 	CRC32    - CRC-32
 * 	SIZE     - data size in this member (unpacked)
 *
 * RA_EXTRA:
 *
 * 	+---+---+---+---+---+---+---+---+---+---+============+
 * 	|x52|x41| EXLEN | VER=1 | CHLEN | CHCNT | CHUNK_DATA |
 * 	+---+---+---+---+---+---+---+---+---+---+============+
 *
 * 	EXLEN      - length of VER, CHLEN, CHCNT and CHUNK_DATA summary
 * 	CHUNK_DATA - CHCNT 2-bytes lengths of compressed chunks
 * 	CHLEN      - the length of one uncompressed chunk
 * 	CHCNT      - count of 2-bytes lengths in CHUNK_DATA
 *
 * Only first member has valid MTIME and FNAME.
 *
 * BODY:
 *
 * 	+==========+=...=+==========+==========+---+---+---+---+---+---+---+---+
 * 	| CCHUNK_1 |     | CCHUNK_N | Z_FINISH | CRC32         | SIZE          |
 * 	+==========+=...=+==========+==========+---+---+---+---+---+---+---+---+
 *
 * CCHUNK - Z_NO_FLUSH compressed chunk of file (size - CHLEN), then
 *          Z_FULL_FLUSH zlib data. So we can work with the chunk
 *          (inflate/deflate) independently.
 * CRC32  - CRC32 check sum of uncompressed member data.
 * SIZE   - size of the uncompressed member data.
 *
 * */

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
static const char FRESERVED = (char)0xfe;
static const char OS_CODE_UNIX = 3;

#define CHUNK_SIZE 58315
static const size_t CHUNKS_PER_MEMBER = (0xffff - (2 + 2) - (2 + 2 + 2)) / 2;
static const size_t EMPTY_FINISH_BLOCK_LEN = 2;
static const size_t GZIP_CRC32_LEN = 4;


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
