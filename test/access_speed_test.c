/**@author hoxnox <hoxnox@gmail.com>
 * @date 20141224 14:53:36 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/time.h>
#include <stdint.h>
#include <csio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef struct
{
	int       min_read_count;
	int       max_read_count;
	int       min_read_block;
	int       max_read_block;
	char      filename[256];
	char      cfilename[256];
} Config;

typedef struct
{
	size_t offset;
	size_t blocksz;
} ReadInstruction;

int cfg_parse_args(int argc, char* argv[], Config* cfg)
{
	if (argc >= 3)
	{
		snprintf(cfg->filename, sizeof(cfg->filename), "%s", argv[1]);
		snprintf(cfg->cfilename, sizeof(cfg->cfilename), "%s", argv[2]);
	}
	return 0;
}

void cfg_set_defaults(Config* cfg)
{
	cfg->max_read_count = 1000;
	cfg->min_read_count = 900;
	cfg->min_read_block = 10;
	cfg->max_read_block = 10000;
	snprintf(cfg->filename, sizeof(cfg->filename), "/tmp/random.file");
	snprintf(cfg->cfilename, sizeof(cfg->cfilename), "/tmp/random.file.dz");
}

void timersub(struct timeval *a, struct timeval *b, struct timeval *res)
{
	res->tv_sec = a->tv_sec - b->tv_sec;
	res->tv_usec = a->tv_usec - b->tv_usec;
	if (res->tv_usec < 0)
	{
	   --res->tv_sec;
	   res->tv_usec += 1000000;
	}
}

void
perform_stdio_reads(FILE* file, ReadInstruction* readmap, size_t readmapsz, char* readbuf)
{
	for (size_t i = 0; i < readmapsz; ++i)
	{
		fseek(file, readmap[i].offset, SEEK_SET);
		if (fread(readbuf, readmap[i].blocksz, 1, file) != 1)
		{
			printf("Error reading block of %lu bytes from offset %lu\n",
					readmap[i].blocksz, readmap[i].offset);
			break;
		}
	}
}

void
perform_csio_reads(CFILE* file, ReadInstruction* readmap, size_t readmapsz, char* readbuf)
{
	for (size_t i = 0; i < readmapsz; ++i)
	{
		cfseek(file, readmap[i].offset, SEEK_SET);
		if (cfread(readbuf, readmap[i].blocksz, 1, file) != 1)
		{
			printf("Error reading block of %lu bytes from offset %lu\n",
					readmap[i].blocksz, readmap[i].offset);
			break;
		}
	}
}

void clear_cache()
{
	int fd;
	char* data = "3";
	
	fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
	if (write(fd, data, sizeof(char)) != 1)
	{
		printf("Error cache clearing\n");
		exit(1);
	}
	close(fd);
}

int
main(int argc, char* argv[])
{
	Config cfg;
	cfg_set_defaults(&cfg);
	if (cfg_parse_args(argc, argv, &cfg) != 0)
		return 0;
	FILE* file = fopen(cfg.filename, "rb");
	CFILE* cfile = cfopen(cfg.cfilename, "rb");
	if (!file || !cfile)
	{
		printf("Error opening %s or %s\n", cfg.filename, cfg.cfilename);
		return 1;
	}
	fseek(file, 0, SEEK_END);
	cfseek(cfile, 0, SEEK_END);
	int filesz  = ftell(file);
	int cfilesz  = cftell(cfile);
	if (filesz < cfg.max_read_block)
	{
		printf("File is too small (%d) for selected max_read_block (%d).",
				filesz, cfg.max_read_block);
		return 1;
	}
	if (cfilesz != filesz)
	{
		printf("compressed and regular file sizes differs: %d != %d",
				cfilesz, filesz);
		return 1;
	}
	srand(time(NULL));
	int readmapsz = rand()%(cfg.max_read_count - cfg.min_read_count) + cfg.min_read_count;
	ReadInstruction* readmap = (ReadInstruction*)malloc(readmapsz*sizeof(ReadInstruction));
	char* readbuf = (char*)malloc(cfg.max_read_block);
	if (!readmap)
	{
		printf("Error allocating readmap");
		return 1;
	}
	for (size_t i = 0; i < readmapsz; ++i)
	{
		readmap[i].blocksz = rand()%(cfg.max_read_block - cfg.min_read_block) + cfg.min_read_block;
		readmap[i].offset  = rand()%(filesz - readmap[i].blocksz);
	}
	printf("Generated readmap, size: %d, block sizes from %d to %d, file size: %d\n",
			readmapsz, cfg.min_read_block, cfg.max_read_block, filesz);
	struct timeval start_tv, end_tv, elapsed_tv;
	uint64_t elapsed, celapsed = 0;

	clear_cache();
	printf("Perform stdio reads.\n");
	gettimeofday(&start_tv, NULL);
	perform_stdio_reads(file, readmap, readmapsz, readbuf);
	gettimeofday(&end_tv, NULL);
	timersub(&end_tv, &start_tv, &elapsed_tv);
	elapsed = elapsed_tv.tv_sec*1000000 + elapsed_tv.tv_usec;
	printf("Elapsed: %lu usec\n", elapsed);

	clear_cache();
	printf("Perform csio reads.\n");
	gettimeofday(&start_tv, NULL);
	perform_csio_reads(cfile, readmap, readmapsz, readbuf);
	gettimeofday(&end_tv, NULL);
	timersub(&end_tv, &start_tv, &elapsed_tv);
	celapsed = elapsed_tv.tv_sec*1000000 + elapsed_tv.tv_usec;
	printf("Elapsed: %lu usec\n", celapsed);

	if (celapsed >= elapsed)
		printf("csio is SLOWER in %5.5f times\n", (double)celapsed/elapsed);
	else
		printf("csio is FASTER in %5.5f times\n", (double)elapsed/celapsed);

	free(readbuf);
	free(readmap);
	fclose(file);
	return 0;
}

