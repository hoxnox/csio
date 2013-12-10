#ifndef __CSIO_INTERNAL_HPP__
#define __CSIO_INTERNAL_HPP__

#include <stdio.h>
#include <stdint.h>
#include <csio.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct
{
	uint8_t  id1;
	uint8_t  id2;
	uint8_t  cm;
	uint8_t  flg;
	uint32_t mtime;
	uint8_t  xfl;
	uint8_t  os;
	uint16_t chcnt;          // from RA extension
	uint16_t chunks[0xffff]; // from RA extension
	uint16_t chlen;          // from RA extension
	off_t    dataoff;        // Data offset
	uint16_t fnamelen;
	uint16_t commentlen;
	uint32_t isize;
} GZIPHeader;
CompressionMethod get_compression(FILE* );
int               get_gzip_header(FILE*, GZIPHeader* );
int               get_gzip_stat(FILE*, size_t*, size_t*, size_t*);
int               init_dictzip(FILE*, CFILE*);
int               fill_buf(CFILE* cstream, off_t pos);
#ifdef __cplusplus
}
#endif

#endif // __CSIO_INTERNAL_HPP__

