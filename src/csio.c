#include <csio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <zlib.h>

static const char GZIP_DEFLATE_ID[3] = {0x1f, 0x8b, 0x08};
static const char FTEXT     = 1;
static const char FHCRC     = 1 << 1;
static const char FEXTRA    = 1 << 2;
static const char FNAME     = 1 << 3;
static const char FCOMMENT  = 1 << 4;
static const char FRESERVED = 0xfe;
static const char OS_CODE_UNIX = 3;

static const uint16_t CHUNK_SIZE = 58315;
static const size_t   EMPTY_FINISH_BLOCK_LEN = 2;
static const size_t   GZIP_CRC32_LEN = 4;

/**@brief stdio fopen analogue
 *
 * If the file is compressed with supported format, the library will
 * identify it automatically.*/
CFILE*
cfopen(const char* name, const char* mode)
{
	CFILE* rs = cfinit(fopen(name, mode));
	if(rs)
		rs->need_close = 1;
	return rs;
}

size_t skip_cstr(FILE* strm)
{
	size_t result = 0;
	if (strm)
		while (fgetc(strm) != '\0')
			++result;
	return result;
}

/**@brief Determine compression method*/
CompressionMethod
get_compression(FILE* stream)
{
	off_t currpos = ftello(stream);
	CompressionMethod result = NONE;
	if (currpos == -1)
		return result;
	char buf[3];
	if (fread((void*)buf, 1, 3, stream) != 3)
		result = NONE;
	else if (memcmp(buf, GZIP_DEFLATE_ID, 3) == 0)
		result = GZIP;
	fseeko(stream, currpos, SEEK_SET);
	return result;
}

/**@brief dictzip extended gzip header*/
typedef struct
{
	uint8_t  id1;
	uint8_t  id2;
	uint8_t  cm;
	uint8_t  flg;
	uint32_t mtime;
	uint8_t  xfl;
	uint8_t  os;
	uint16_t chcnt;
	uint16_t chunks[0xffff];
	uint16_t chlen;
	off_t    dataoff;
	uint16_t fnamelen;
	uint16_t commentlen;
	uint32_t isize;
} GZIPHeader;

/**@brief Reads gzip header from stream
 * @return 1 on success, 0 if the stream is not in gzip format, -1 on
 *         error.
 *
 * ra_data format:
 * +---+---+---+---+---+---+==============================================+
 * | VER=1 | CHLEN | CHCNT | CHCNT 2-byte lengths of compressed chunks ...|
 * +---+---+---+---+---+---+==============================================+
 */
int
get_gzip_header(FILE* stream, GZIPHeader* hdr)
{
	const char HDRSZ = 10;
	int rs = fread((void *)hdr, 1, HDRSZ, stream);
	if (rs != HDRSZ)
		return -1;
	if (memcmp(hdr, GZIP_DEFLATE_ID, 3) != 0)
		return 0;

	/*if (hdr->flg & FRESERVED)
		{ errno = EILSEQ; return -1; }*/
	if (hdr->flg & FEXTRA)
	{
		uint16_t xlen;
		if (fread((void*)&xlen, 1, 2, stream) != 2)
			return -1;
		/*
		Each subfield:
		+---+---+---+---+===============================+
		|SUB_ID |  LEN  | LEN bytes of subfield data ...|
		+---+---+---+---+===============================+
		*/
		long pos = ftell(stream);
		if (pos == -1)
			return -1;
		if (xlen < 2 + 2)
			{ errno = EILSEQ; return -1; }
		const long ENDPOS = pos + xlen;
		while (pos < ENDPOS)
		{
			char sub_id[2];
			if ((rs = fread((void*)sub_id, 1, 2, stream)) != 2)
				return -1;
			uint16_t subdataln;
			if ((rs = fread((void*)&subdataln, 1, 2, stream)) != 2)
				return -1;
			if (sub_id[0] == 'R' && sub_id[1] == 'A')
			{
				if (subdataln < 6)
					{ errno = EFAULT; return -1; }
				char xhdr[6];
				if (subdataln > xlen - (2 + 2))
					{ errno = EILSEQ; return -1; }
				if (fread((void*)xhdr, 1, 6, stream) != 6)
					return -1;
				if (*(uint16_t*)xhdr != 1)
					{ errno = ENOSYS; return -1; }
				hdr->chlen = *(uint16_t*)&xhdr[2];
				if (*(uint16_t*)&xhdr[4]*2 > subdataln-2*3)
					{ errno = EFAULT; return -1; }
				hdr->chcnt = *(uint16_t*)&xhdr[4];
				if (fread((void*)hdr->chunks, 2,
					hdr->chcnt, stream) != hdr->chcnt)
				{
					hdr->chcnt = 0;
					return -1;
				}
			}
			else
			{
				fseek(stream, subdataln - 2*2, SEEK_CUR);
			}
			pos += subdataln + 2*2;
		}
	}
	if (hdr->flg  & FNAME)
		hdr->fnamelen = skip_cstr(stream);
	if (hdr->flg  & FCOMMENT)
		hdr->commentlen = skip_cstr(stream);
	if (hdr->flg  & FHCRC)
		fseek(stream, 2, SEEK_CUR);
	if (hdr->chcnt == 0)
	{
		/* TODO: skipping to the member implemented  only for dictzip*/
		errno = ENOSYS;
		return -1;
	}
	hdr->dataoff = ftello(stream);
	size_t i, dataskip = 0;
	for(i = 0; i < hdr->chcnt; ++i)
		dataskip += hdr->chunks[i];
	fseek(stream, dataskip + 4 + EMPTY_FINISH_BLOCK_LEN, SEEK_CUR);
	if (fread((void*)&hdr->isize, 1, 4, stream) != 4)
	{
		errno = EFAULT;
		return -1;
	}
	return 1;
}

/**@brief walk through gzip members and summarize info from headers
* @return -1 on error, 1 on success*/
int
get_gzip_stat(FILE* stream,
	size_t *mem_count, size_t *chunks_count, size_t *streamsz)
{
	if (!stream)
	{
		errno = EINVAL;
		return -1;
	}
	*mem_count = 0;
	*chunks_count = 0;
	*streamsz = 0;
	GZIPHeader hdr;
	while(get_gzip_header(stream, &hdr) == 1)
	{
		*mem_count += 1;
		*chunks_count += hdr.chcnt;
		*streamsz += hdr.isize;
	}
	return 1;
}

/**@brief clear CFILE structure
* @note no memory free*/
int
clear(CFILE* cstream)
{
	cstream->stream = NULL;
	cstream->currpos = 0;
	cstream->chlen = 0;
	cstream->size = 0;
	cstream->bufoff = 0;
	cstream->bufsz = 0;
	cstream->need_close = 0;
	cstream->compression = NONE;
	memset(cstream->buf, 0, 0x10000);
	cstream->idxsz = 0;
	cstream->idx = NULL;
	cstream->init_magic = 0;
	cstream->eof = 0;
	return 0;
}

/**@brief Creates dictzip index
*
* DICTZIP index is the array of chunks offsets
* TODO: In theory, chlen must not be identical in all gzip members.
* This situation is not supported for now.
* @return On success returns 1. In that case memory for idx was
* allocated and must be freed*/
int
init_dictzip(FILE* stream, CFILE* cstream)
{
	size_t mcnt, chcnt, streamsz;
	if (stream == NULL || cstream == NULL)
		return -1;
	clear(cstream);
	get_gzip_stat(stream, &mcnt, &chcnt, &streamsz);
	if (chcnt == 0 || mcnt == 0 || streamsz == 0)
		return 0;
	GZIPHeader hdr;
	uint64_t* idx = NULL;
	fseeko(stream, 0, SEEK_SET);
	size_t i = 0;
	while(get_gzip_header(stream, &hdr) == 1)
	{
		if (hdr.chcnt == 0)
			continue;
		if (hdr.chlen == 0)
			continue;
		if (i == 0)
		{
			cstream->zst.zalloc    = NULL;
			cstream->zst.zfree     = Z_NULL;
			cstream->zst.opaque    = Z_NULL;
			cstream->zst.avail_in  = 0;
			cstream->zst.avail_out = 0;
			cstream->zst.total_in  = 0;
			cstream->zst.total_out = 0;
			cstream->zst.next_in   = 0;
			cstream->zst.next_out  = 0;
			cstream->stream = stream;
			cstream->compression = DICTZIP;
			cstream->bufsz = 0;
			cstream->bufoff = 0;
			cstream->chlen = hdr.chlen;
			cstream->idx = (char*)malloc(chcnt*8 + 8);
			memset(cstream->idx, 0, chcnt*8 + 8);
			cstream->size = streamsz;
			if (!cstream->idx)
			{
				errno = EFAULT;
				return -1;
			}
			cstream->idxsz = chcnt*8;
			idx = (uint64_t*)cstream->idx;
		}
		idx[i] = hdr.dataoff;
		++i;
		size_t j;
		for(j = 0; j < hdr.chcnt - 1; ++j)
		{
			if(i >= chcnt)
			{
				errno = EFAULT;
				free(cstream->idx);
				clear(cstream);
				return -1;
			}
			idx[i] = idx[i - 1] + hdr.chunks[j];
			++i;
		}
	}
	if (idx != NULL)
	{
		fseeko(stream, 0, SEEK_END);
		idx[i] = (uint64_t)ftello(stream);
		if (idx[i] == -1)
		{
			errno = EFAULT;
			free(cstream->idx);
			clear(cstream);
			return -1;
		}
	}
	if (cstream->stream == stream)
		return 1;
	return -1;
}

/**@brief check stream integrity
 * @return Non zero on error
 * @note This function doesn't check error flag, it checks stream
 * integrity.*/
int cferror(CFILE* stream)
{
	if (stream == NULL)
		return -1;
	if (stream->init_magic != INITIALIZED)
		return -1;
	if (stream->stream == NULL)
		return -1;
	if (ferror(stream->stream))
		return -1;
	if (stream->compression == NONE)
	{
		return 0;
	}
	else if (stream->compression == DICTZIP)
	{
		if (stream->idx == NULL)
			return -1;
		if (stream->idxsz == 0)
			return -1;
		if (stream->chlen == 0)
			return -1;
		if (stream->size == 0)
			return -1;
	}
	else
	{
		return -1;
	}
	return 0;
}

/**@brief Renew buffer, so pos will be in it
 * @return 1 on success, -1 on error*/
int
fill_buf(CFILE* cstream, off_t pos)
{
	if (cferror(cstream))
		return -1;
	if (pos >= cstream->size)
	{
		errno = EINVAL;
		cstream->eof = 1;
		return -1;
	}
	if (cstream->compression != DICTZIP)
	{
		errno = ENOSYS;
		return -1;
	}
	if (pos >= cstream->bufoff)
		if(pos - cstream->bufoff < cstream->bufsz)
			return 1;
	size_t chunk_no = pos/cstream->chlen;
	if (chunk_no > (cstream->idxsz/8 - 1))
	{
		errno = EINVAL;
		return -1;
	}
	char compressed_chunk_buf[0x10000];
	memset(compressed_chunk_buf, 0, sizeof(compressed_chunk_buf));
	off_t off_begin = ((uint64_t*)cstream->idx)[chunk_no];
	off_t off_end = ((uint64_t*)cstream->idx)[chunk_no + 1];
	if (off_end <= off_begin)
	{
		errno = EFAULT;
		return -1;
	}
	size_t compressed_chunk_len = off_end - off_begin;
	if (compressed_chunk_len > 0x10000)
	{
		errno = EFAULT;
		return -1;
	}
	fseeko(cstream->stream, off_begin , SEEK_SET);
	cstream->bufoff = chunk_no*cstream->chlen;
	int rs = fread((void *)compressed_chunk_buf, 1, compressed_chunk_len,
			cstream->stream);
	if (rs != compressed_chunk_len)
	{
		errno = EFAULT;
		return -1;
	}
	if( inflateInit2(&cstream->zst, -MAX_WBITS) != Z_OK)
	{
		errno = EFAULT;
		return -1;
	}
	cstream->zst.avail_in = rs;
	cstream->zst.avail_out = cstream->chlen;
	cstream->zst.next_in = (Bytef *)compressed_chunk_buf;
	cstream->zst.next_out = (Bytef *)cstream->buf;
	size_t old_total_out = cstream->zst.total_out;
	rs = inflate(&cstream->zst, Z_FULL_FLUSH);
	if (rs != Z_OK && rs != Z_STREAM_END)
	{
		errno = EFAULT;
		return -1;
	}
	cstream->bufsz = cstream->zst.total_out - old_total_out;
	inflateEnd(&cstream->zst);
	if (rs == Z_STREAM_END)
	{
		cstream->zst.zalloc    = NULL;
		cstream->zst.zfree     = Z_NULL;
		cstream->zst.opaque    = Z_NULL;
		cstream->zst.avail_in  = 0;
		cstream->zst.avail_out = 0;
		cstream->zst.total_in  = 0;
		cstream->zst.total_out = 0;
		cstream->zst.next_in   = 0;
		cstream->zst.next_out  = 0;
	}
	return 1;
}

size_t  getsz(FILE* file)
{
	off_t curr = ftello(file);
	if (fseeko(file, 0, SEEK_END) != 0)
		return 0;
	size_t sz = (size_t)ftello(file);
	if (fseeko(file, curr, SEEK_SET) != 0)
		return 0;
	return sz;
}

/**@brief Init CFILE* from stdio FILE*
 *
 * Scan file and make index*/
CFILE*
cfinit(FILE* stream)
{
	if (stream == NULL)
		return NULL;
	off_t initial_pos = ftello(stream);
	if (initial_pos == -1)
		return NULL;
	CFILE* cstream = (CFILE*)malloc(sizeof(CFILE));
	memset(cstream, 0, sizeof(CFILE));
	cstream->compression = get_compression(stream);
	cstream->need_close = 0;
	switch(cstream->compression)
	{
		case GZIP:
			/* first try to build dictzip index, on failure
			   fall down to gzip*/
			if (init_dictzip(stream, cstream) != 1)
			{
				/* standard GZIP is not implemented yet*/
				free(cstream);
				cstream = NULL;
				errno = ENOSYS;
			}
			break;
		case NONE:
			cstream->stream = stream;
			cstream->size = getsz(stream);
			if (cstream->size == 0 && errno != 0)
				return NULL;
			break;
		default:
			break;
	}
	if (cstream != NULL)
		cstream->init_magic = INITIALIZED;
	fseeko(stream, initial_pos, SEEK_SET);
	return cstream;
}

/**@brief Close cfile and clear resources*/
void
cfclose(CFILE** cstream)
{
	if ((*cstream))
	{
		if ((*cstream)->idx)
			free((*cstream)->idx);
		if ((*cstream)->need_close)
			if ((*cstream)->stream)
				fclose((*cstream)->stream);
		if ((*cstream)->compression == DICTZIP)
			inflateEnd(&(*cstream)->zst);
		clear((*cstream));
		free((*cstream));
	}
	(*cstream) = NULL;
}

/**@brief Check for End of file*/
int
cfeof(CFILE* cstream)
{
	if (cferror(cstream))
		return 1;
	if (cstream->compression == NONE)
		return feof(cstream->stream);
	else if (cstream->compression == DICTZIP)
		return cstream->eof;
	return 1;
}

/**@brief Move logical position
 *
 * SEEK_END is not supported*/
int
cfseek(CFILE* stream, long pos, int mode)
{
	return cfseeko(stream, pos, mode);
}

/**@brief Same as cfseek, but works with off_t*/
int
cfseeko(CFILE* stream, off_t offset, int mode)
{
	if(!stream)
		return -1;
	if (stream->compression == DICTZIP)
	{
		stream->eof = 0;
		switch (mode)
		{
			case SEEK_SET: stream->currpos = offset; break;
			case SEEK_CUR: stream->currpos += offset; break;
			case SEEK_END: stream->currpos =
			               stream->size + offset < stream->size ?
			                   stream->size + offset : stream->size;
		}
		if(stream->currpos >= stream->size)
			stream->eof = 1;
	}
	else if (stream->compression == NONE)
	{
		return fseeko(stream->stream, offset, mode);
	}
	else
	{
		errno = ENOSYS;
		return -1;
	}
	return 0;
}

/**@brief Tell current logical position*/
long
cftell(CFILE* stream)
{
	return (long)cftello(stream);
}

/**@brief Same as cftell, but works with off_t*/
off_t
cftello(CFILE* stream)
{
	if (cferror(stream))
		return -1;
	if (stream->compression == NONE)
		return ftello(stream->stream);
	else if (stream->compression == DICTZIP)
		return stream->currpos;
	errno = EINVAL;
	return -1;
}

/**@brief fread analogue*/
size_t
cfread(void* dest, size_t size, size_t count, CFILE* stream)
{
	if (size == 0 || count == 0)
		return 0;
	if (!dest || !stream)
	{
		errno = EINVAL;
		return 0;
	}
	if (stream->compression == DICTZIP)
	{
		if (stream->idxsz == 0 || stream->idx == NULL)
		{
			errno = EINVAL;
			return 0;
		}
		off_t pos = stream->currpos;
		off_t end = pos + size*count;
		end = end < stream->size ? end : stream->size;
		size_t copied = 0;
		while (pos < end)
		{
			if (fill_buf(stream, pos) != 1)
				return copied;
			if (pos < stream->bufoff
			 || pos >= stream->bufoff + stream->bufsz)
			{
				errno = EFAULT;
				return 0;
			}
			off_t copyend = end < stream->bufoff + stream->bufsz ?
					end : stream->bufoff + stream->bufsz;
			memcpy((char*)dest + copied,
				stream->buf + pos - stream->bufoff,
				copyend - pos);
			copied += copyend - pos;
			pos = copyend;
		}
		stream->currpos = pos;
		return copied;
	}
	else if (stream->compression == NONE)
	{
		if (!stream->stream)
		{
			errno = EINVAL;
			return 0;
		}
		return fread(dest, size, count, stream->stream);
	}
	else
	{
		errno = ENOSYS;
		return 0;
	}
}

/**@ fgetc analogue*/
int
cfgetc(CFILE* stream)
{
	if (cferror(stream))
	{
		errno = EINVAL;
		return EOF;
	}
	if (stream->compression == NONE)
	{
		return fgetc(stream->stream);
	}
	else if (stream->compression == DICTZIP)
	{
		if (stream->currpos == stream->size)
		{
			stream->eof = 1;
			return EOF;
		}
		if (fill_buf(stream, stream->currpos) != 1)
		{
			errno = EFAULT;
			return EOF;
		}
		return stream->buf[stream->currpos++ - stream->bufoff];
	}
	else
	{
		errno = ENOSYS;
		return EOF;
	}
}


