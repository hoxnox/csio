/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140324 20:20:30
 *
 * DZip compression utility.
 * You can use regular gunzip to extract files.*/

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <memory.h>
#include <csio_config.h>
#include <string.h>
#include <errno.h>

#include <libintl.h>
#include "gettext.h"
#include "csio.h"
#include "dzip.h"
#include "cqueue.h"

enum {LESS, MORE};

void print_help(int mode)
{
	printf( _("Usage: dzip [ <options> ] [ <name> ... ]\n") );
	printf( _("DZip compress utility\n") );
	if (mode != LESS)
	{
		printf( "\n" );
		printf( _("Options:\n"
		          "\t""-j, --jobs    parallel to jobs\n"
		          "\t""-h, --help    print this message\n"
		) );
	}
	printf("\n");
	printf( _("(c) 2014 concerteza <nyura@concerteza.ru>\n"
	          "Version: %d.%d.%d\n"), csio_VERSION_MAJOR,
	                                  csio_VERSION_MINOR,
	                                  csio_VERSION_PATCH );
}

typedef struct {
	size_t jobs;
	int force;
} Config;

int parse_args(Config* cfg, int argc, char* argv[])
{
	int c;
	int digit_optind = 0;
	while (1)
	{
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
//		    {"jobs",  required_argument, 0, 'j' },
		    {"force", no_argument,       0, 'f' },
		    {"help",  no_argument,       0, 'h' },
		    {0,       0,                 0, 0   }
		};

		c = getopt_long(argc, argv, "fh",
			long_options, &option_index);
		if (c == -1)
		    break;

		switch (c) {
		case 'h':
			print_help(MORE);
			return 0;
		    break;

		case 'f':
		    cfg->force = 1;
		    break;

		    /* not implemented yet
		case 'j':
		    cfg->jobs = atoi(optarg);
		    break;
		    */

		default:
#			ifndef NDEBUG
			printf("getopt returned code 0%o(%c)\n", c, c);
#			endif
			return -1;
		}
	}
	return optind;
}

int file_exists(const char* fname)
{
	int rs = 0;
	FILE* file = fopen(fname, "r");
	if (file)
		rs = 1;
	fclose(file);
	return rs;
}

struct WriterContext
{
	FILE* ofile;
	FileChunksQueue* to_write;
};

void*
writer(void* wr_ctxt)
{
	if (!wr_ctxt)
		return NULL;
	FileChunksQueue* to_write = ((struct WriterContext*)wr_ctxt)->to_write;
	FILE* ofile = ((struct WriterContext*)wr_ctxt)->ofile;
	while(1)
	{
		FileChunk chunk = pop_front_chunks_queue(to_write);
		if (is_file_chunk_empty(&chunk))
		{
			struct timespec tm = default_tm;
			pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
			int rs = pthread_cond_timedwait(&to_write->can_pop, &mtx, &tm);
			if (rs != 0)
			{
				if (errno == ETIMEDOUT)
				{
					if(stop == 1)
						break;
					else
						continue;
				}
				fprintf(stderr, "Error on writer: %s", strerror(errno));
				break;
			}
			continue;
		}
	}
	return NULL;
}

int compress_file(FILE* ifile, FILE* ofile, char threads)
{
	char ibuf[CHUNK_SIZE];
	FileChunksQueue to_compress;
	if (init_chunks_queue(&to_compress) != 0)
		return -1;
	FileChunksQueue to_write;
	if (init_chunks_queue(&to_write) != 0)
		return -1;
	pthread_t twriter;
	struct WriterContext ctxt;
	ctxt.ofile = ofile;
	ctxt.to_write = &to_write;
	if (pthread_create(&twriter, NULL, writer, &ctxt) != 0)
	{
		fprintf(stderr, "Error creating writer thread. Message: %s",
				strerror(errno));
		return -1;
	}
	while (!feof(ifile))
	{
		// filling to_compress
	}
	free_chunks_queue(&to_compress);
	free_chunks_queue(&to_write);
	return 0;
}

#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif

int
compress_chunk(FileChunk* chunk, int flush_mode, size_t datasz)
{
	if (!chunk || datasz >CHUNK_SIZE 
	 || (flush_mode != Z_FULL_FLUSH && flush_mode != Z_FINISH))
	{
		return -1;
	}
	z_stream zst;
	zst.zalloc = Z_NULL;
	zst.zfree = Z_NULL;
	zst.opaque = Z_NULL;
	memset(&zst, 0, sizeof(z_stream));
	if (deflateInit2(&zst, compress_level, Z_DEFLATED, -MAX_WBITS,
		DEF_MEM_LEVEL, 0) != Z_OK)
	{
		return -1;
	}
	size_t zdatasz = sizeof(chunk->zdata);
	zst.next_in = (Bytef*)chunk->data;
	zst.avail_in = datasz;
	zst.next_out = (Bytef*)chunk->zdata;
	zst.avail_out = zdatasz;
	int rs = deflate(&zst, Z_NO_FLUSH);
	if (rs != Z_OK || zst.avail_in != 0)
	{
		deflateEnd(&zst);
		return -1;
	}
	size_t writtensz = zdatasz - zst.avail_out;
	zst.avail_in = 0;
	zst.next_out = (Bytef*)(chunk->zdata + writtensz);
	rs = deflate(&zst, flush_mode);
	if (rs != Z_OK)
	{
		deflateEnd(&zst);
		return -1;
	}
	deflateEnd(&zst);
	return 0;
}

int
main(int argc, char* argv[])
{
	Config cfg;
	memset(&cfg, 0, sizeof(cfg));
	if (argc < 2)
	{
		print_help(LESS);
		return 0;
	}
	int rs = parse_args(&cfg, argc, argv);
	if (rs <= 0)
		return 0;

	for(size_t i = rs; i < argc; ++i)
	{
		FILE* ifile = fopen(argv[i], "rb");
		if (!ifile)
		{
			fprintf(stderr, _("Error opening %s\n"), argv[i]);
			continue;
		}
		char* newfname = malloc(strlen(argv[i]) + 4);
		memset(newfname, 0, strlen(argv[i]) + 4);
		memcpy(newfname, argv[i], strlen(argv[i]));
		memcpy(newfname + strlen(argv[i]), ".dz", 3);
		if (file_exists(newfname) && !cfg.force)
		{
			printf( _("File \"%s\" exists."
			          " Use force to overwrite.\n"), argv[i]);
			fclose(ifile);
			free(newfname);
			continue;
		}
		FILE* ofile = fopen(newfname, "w");
		if (!ofile)
		{
			printf( _("Error opening \"%s\" for writing.")
					, newfname);
			fclose(ifile);
			free(newfname);
			continue;
		}
		if (!compress_file(ifile, ofile, cfg.jobs))
			printf( _("Error compressing \"%s\"."), argv[i]);
		fclose(ifile);
		fclose(ofile);
		free(newfname);
	}
	return 0;
}

