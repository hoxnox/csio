#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <locale.h>
#include <csio.h>

char hex2char(unsigned char n)
{
	if(0 <= n && n <= 9)
		return n + 48;
	if(0xA <= n && n <= 0xF)
		return n - 0xA + 'a';
	return '?';
}

int hex2int(unsigned char c)
{
	if ('0' <= c && c <= '9')
		return c - '0';
	if ('a' <= c && c <= 'f')
		return c - 'a' + 10;
	if ('A' <= c && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

char* byte2str(char* begin, char* end, char* buf)
{
	int len = end - begin;
	size_t i = 0;
	for(; begin != end; ++begin)
	{
		buf[i]   = hex2char((((unsigned char)*begin)/0x10)%0x10);
		buf[i+1] = hex2char(((unsigned char)*begin)%0x10);
		i += 2;
	}
	buf[i] = 0;
	return buf;
}

int
str2bytes(char* dst, const char* src, int srclen)
{
	if (srclen == 0 || src == NULL || dst == NULL)
		return 0;
	int pos = 0;
	int rs = 0;
	for(; pos < srclen; ++pos)
	{
		if(!isxdigit(src[pos]))
			return rs;
		int c = hex2int(src[pos]);
		if (c == -1)
			return rs;
		dst[rs] = pos%2 ?  dst[rs++] + c : c*0x10;
	}
	return rs;
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		printf("Usage: <file> <bytes>\n");
		return 0;
	}

	size_t btlen = strlen(argv[2]);
	if (btlen%2 != 0 || btlen == 0)
	{
		printf("Wrong pattern length.");
		return 1;
	}
	char* target = (char*)malloc(btlen/2 + 1);
	if (!target)
	{
		fprintf(stderr, "Error memory allocation.\n");
		return 1;
	}
	int targetsz = str2bytes(target, argv[2], btlen);
	if (targetsz != (btlen%2 == 0 ? btlen/2 : btlen/2 + 1))
	{
		fprintf(stderr, "Error converting second argument,"
		                " processed %d bytes.\n", targetsz);
		return 1;
	}

	char realp[PATH_MAX];
	if (realpath(argv[1], realp) == NULL)
	{
		fprintf(stderr, "Error getting realpath of \"%s\". Message: %s\n", argv[1], strerror(errno));
		return 1;
	}
	CFILE* fin = cfopen(realp, "rb");
	if (!fin)
	{
		fprintf(stderr, "Error opening \"%s\". Message: %s\n", argv[1], strerror(errno));
		return 1;
	}
	char* buf = (char*)malloc(targetsz);
	if (!buf)
	{
		fprintf(stderr, "Error memory allocation for readbuf. Size: %d\n", targetsz);
		return 1;
	}
	int readrs = cfread(buf, 1, targetsz, fin);
	if (readrs != targetsz)
	{
		if (read == 0)
			fprintf(stderr, "Error reading file.\n");
		return 1;
	}
	if (memcmp(buf, target, targetsz) == 0)
	{
		printf("0\n");
		return 0;
	}
	while (!cfeof(fin))
	{
		int c = cfgetc(fin);
		if (c == EOF)
		{
			if (cferror(fin))
			{
				fprintf(stderr, "Error reading file.\n");
				return 1;
			}
			return 0;
		}
		size_t i;
		for (i = 0; i < targetsz - 1; ++i) // targetsz > 0 by logic
			buf[i] = buf[i+1];
		buf[targetsz - 1] =c;
		if (memcmp(buf, target, targetsz) == 0)
		{
			off_t off = cftello(fin);
			printf("%s %lu\n", realp, (unsigned long)off);
		}
	}
	return 0;
}

