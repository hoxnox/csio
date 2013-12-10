/**@author $username$ <$usermail$>
 * @date $date$ */

#ifndef __TCSIO_HPP__
#define __TCSIO_HPP__

#include "csio_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <csio.h>
#include <string>
#include <string.h>
#include <csio_config.h>

class TestCSIO : public ::testing::Test
{
protected:
	void SetUp()
	{
		fname = TEST_SAMPLES_DIR;
		fname += "/file.dz";
		sample = fopen(fname.c_str(), "rb");
		ASSERT_TRUE(sample != NULL) << fname;
	}
	void TearDown()
	{
		fclose(sample);
	}
	std::string fname;
	FILE* sample;
};

TEST_F(TestCSIO, get_compression)
{
	ASSERT_EQ(get_compression(sample), GZIP);
}

TEST_F(TestCSIO, get_gzip_header)
{
	GZIPHeader hdr;
	memset(&hdr, 0, sizeof(hdr));
	ASSERT_NE(get_gzip_header(sample, &hdr), -1);
	int a = 0;
	ASSERT_EQ(hdr.id1  , 0x1f);
	ASSERT_EQ(hdr.id2  , 0x8b);
	ASSERT_EQ(hdr.cm   , 0x8);
	ASSERT_EQ(hdr.flg  , 0xc);
	ASSERT_EQ(hdr.mtime, 0x52a1b806);
	ASSERT_EQ(hdr.xfl  , 0x2);
	ASSERT_EQ(hdr.os   , 0x3);
	ASSERT_EQ(hdr.chcnt, 0x7ffa);
	size_t i = 0;
	for(i = 0; i < hdr.chcnt; ++i)
		ASSERT_EQ(hdr.chunks[i], 0x4d);
	ASSERT_EQ(hdr.chlen     , 0xe3cb);
	ASSERT_EQ(hdr.dataoff   , 0x1000f);
	ASSERT_EQ(hdr.fnamelen  , 4);
	ASSERT_EQ(hdr.commentlen, 0);
	ASSERT_EQ(hdr.isize     , 0x71e0293e);
}

TEST_F(TestCSIO, get_gzip_stat)
{
	size_t mcnt = 0, chcnt = 0, sz = 0;
	ASSERT_NE(get_gzip_stat(sample, &mcnt, &chcnt, &sz), -1);
	ASSERT_EQ(chcnt, 0x7ffa + 0x33d6);
	ASSERT_EQ(mcnt, 2);
	ASSERT_EQ(sz, 2684354560);
}

TEST_F(TestCSIO, init_dictzip)
{
	CFILE cfile;
	memset(&cfile, 0, sizeof(cfile));
	ASSERT_EQ(init_dictzip(sample, &cfile), 1);
	ASSERT_EQ(cfile.chlen, 58315);
	ASSERT_EQ(cfile.compression, DICTZIP);
	ASSERT_TRUE(cfile.stream != NULL);
	ASSERT_TRUE(cfile.idx != NULL);
	ASSERT_EQ(cfile.idxsz, (0x7ffa + 0x33d6)*8);
	ASSERT_NO_FATAL_FAILURE(free(cfile.idx));
}

TEST_F(TestCSIO, cfinit_cfclose )
{
	CFILE* file = cfinit(sample);
	ASSERT_EQ(cferror(file), 0) << strerror(errno);
	ASSERT_NO_FATAL_FAILURE(cfclose(&file));
}

TEST_F(TestCSIO, cfopen_cfclose )
{
	CFILE* file = cfopen(fname.c_str(), "rb");
	ASSERT_EQ(cferror(file), 0) << strerror(errno);
	ASSERT_NO_FATAL_FAILURE(cfclose(&file));
}

TEST_F(TestCSIO, fill_buf )
{
	size_t i;
	CFILE* file = cfinit(sample);
	memset(file->buf, 1, file->chlen + 1);
	ASSERT_EQ(fill_buf(file, 0), 1);
	ASSERT_EQ(file->bufsz, file->chlen);
	for(i = 0; i < file->chlen; ++i)
		ASSERT_EQ(file->buf[i], 0);
	ASSERT_EQ(file->buf[i], 1);

	// last in memb
	memset(file->buf, 1, file->chlen + 1);
	ASSERT_EQ(fill_buf(file, file->chlen*0x7FFA + 1), 1); 
	ASSERT_EQ(file->bufsz, file->chlen);
	for(i = 0; i < file->chlen; ++i)
		ASSERT_EQ(file->buf[i], 0);
	ASSERT_EQ(file->buf[i], 1);

	// first in next memb
	memset(file->buf, 1, file->chlen + 1);
	ASSERT_EQ(fill_buf(file, file->chlen*0x7FFA + file->chlen + 1), 1);
	ASSERT_EQ(file->bufsz, file->chlen);
	for(i = 0; i < file->chlen; ++i)
		ASSERT_EQ(file->buf[i], 0);
	ASSERT_EQ(file->buf[i], 1);

	// the very last
	const unsigned long FILESZ = 2560L*1024*1024;
	memset(file->buf, 1, file->chlen + 1);
	ASSERT_EQ(fill_buf(file, FILESZ - 1), 1);
	ASSERT_EQ(file->bufsz, 56795);
	for(i = 0; i < 56795; ++i)
		ASSERT_EQ(file->buf[i], 0);
	ASSERT_EQ(file->buf[i], 1);

	ASSERT_EQ(fill_buf(file, FILESZ), -1);
}


TEST_F(TestCSIO, cfread )
{
	size_t i = 0;
	CFILE * cfile = finit(sample);
	char buf[101];
	memset(buf, 1, 101);
	ASSERT_EQ(cfread((void*)buf, 1, 100, cfile), 100);
	for(i = 0; i < 100; ++i)
		ASSERT_EQ(buf[i] , 0);
	ASSERT_EQ(buf[i] , 1);
}

TEST_F(TestCSIO, cfgetc )
{
}

TEST_F(TestCSIO, cfseek )
{

}

TEST_F(TestCSIO, cftell )
{
}

TEST_F(TestCSIO, cftello)
{
}

TEST_F(TestCSIO, cfseeko)
{
}

TEST_F(TestCSIO, cferror)
{
	ASSERT_NE(cferror(NULL), 0);
	CFILE *cfile, cfile_;
	ASSERT_NE(cferror(&cfile_), 0);
	cfile = cfinit(sample);
	ASSERT_EQ(cferror(cfile), 0);
	cfseeko(cfile, 0x10000000000L, SEEK_SET);
	ASSERT_EQ(cferror(cfile), 0);
	cfclose(&cfile);
	ASSERT_NE(cferror(cfile), 0);
}

TEST_F(TestCSIO, cfinit )
{
}

TEST_F(TestCSIO, cfclose)
{
}

TEST_F(TestCSIO, cfeof  )
{
}

#endif // __TCSIO_HPP__

