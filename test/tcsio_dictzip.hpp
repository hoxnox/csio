/**@author hoxnox <hoxnox@gmail.com>
 * @date 20131217 15:33:12 */

#ifndef __TCSIO_DICTZIP_HPP__
#define __TCSIO_DICTZIP_HPP__

#include "csio_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <csio.h>
#include <string>
#include <string.h>
#include <csio_config.h>

class TestCSIODictzip : public ::testing::Test
{
protected:
	void SetUp()
	{
		/*sample is specialy created. It contains 2 members.
		 * Second member has 2 chunks, second chunk contain 1
		 * element. Uncompressed file filled with zero.*/
		fname = TEST_SAMPLES_DIR;
		fname += "/file.dz";
		sample = fopen(fname.c_str(), "rb");
		ASSERT_TRUE(sample != NULL) << fname;
		csample = cfinit(sample);
		ASSERT_EQ(cferror(csample), 0) << strerror(errno);
		ASSERT_EQ(csample->compression, DICTZIP);
	}
	void TearDown()
	{
		fclose(sample);
		ASSERT_NO_FATAL_FAILURE(cfclose(&csample));
	}
	std::string fname;
	FILE* sample;
	CFILE* csample;
};

TEST_F(TestCSIODictzip, get_compression)
{
	ASSERT_EQ(get_compression(sample), GZIP);
}

TEST_F(TestCSIODictzip, get_gzip_header)
{
	GZIPHeader hdr;
	memset(&hdr, 0, sizeof(hdr));
	ASSERT_NE(get_gzip_header(sample, &hdr), -1);
	int a = 0;
	ASSERT_EQ(hdr.id1  , 0x1f);
	ASSERT_EQ(hdr.id2  , 0x8b);
	ASSERT_EQ(hdr.cm   , 0x8);
	ASSERT_EQ(hdr.flg  , 0xc);
	ASSERT_EQ(hdr.mtime, 0x52a715da);
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

TEST_F(TestCSIODictzip, get_gzip_stat)
{
	size_t mcnt = 0, chcnt = 0, sz = 0;
	ASSERT_NE(get_gzip_stat(sample, &mcnt, &chcnt, &sz), -1);
	ASSERT_EQ(chcnt, 0x7ffa + 0x0002);
	ASSERT_EQ(mcnt, 2);
	ASSERT_EQ(sz, 1910574346);
}

TEST_F(TestCSIODictzip, init_dictzip)
{
	CFILE cfile;
	memset(&cfile, 0, sizeof(cfile));
	ASSERT_EQ(init_dictzip(sample, &cfile), 1);
	ASSERT_EQ(cfile.chlen, 58315);
	ASSERT_EQ(cfile.compression, DICTZIP);
	ASSERT_TRUE(cfile.stream != NULL);
	ASSERT_TRUE(cfile.idx != NULL);
	ASSERT_EQ(cfile.idxsz, (0x7ffa + 0x0002)*8);
	ASSERT_NO_FATAL_FAILURE(free(cfile.idx));
}

TEST_F(TestCSIODictzip, cfopen_cfclose )
{
	CFILE* file = cfopen(fname.c_str(), "rb");
	ASSERT_EQ(cferror(file), 0) << strerror(errno);
	ASSERT_NO_FATAL_FAILURE(cfclose(&file));
}

TEST_F(TestCSIODictzip, fill_buf )
{
	size_t i;
	memset(csample->buf, 1, csample->chlen + 1);
	ASSERT_EQ(fill_buf(csample, 0), 1);
	ASSERT_EQ(csample->bufsz, csample->chlen);
	for(i = 0; i < csample->chlen; ++i)
		ASSERT_EQ(csample->buf[i], 0);
	ASSERT_EQ(csample->buf[i], 1);

	// last in memb
	memset(csample->buf, 1, csample->chlen + 1);
	ASSERT_EQ(fill_buf(csample, csample->chlen*0x7FFA - 1), 1); 
	ASSERT_EQ(csample->bufsz, csample->chlen);
	for(i = 0; i < csample->chlen; ++i)
		ASSERT_EQ(csample->buf[i], 0);
	ASSERT_EQ(csample->buf[i], 1);

	// first in next memb
	memset(csample->buf, 1, csample->chlen + 1);
	ASSERT_EQ(fill_buf(csample, csample->chlen*0x7FFA), 1);
	ASSERT_EQ(csample->bufsz, csample->chlen);
	for(i = 0; i < csample->chlen; ++i)
		ASSERT_EQ(csample->buf[i], 0) << i;
	ASSERT_EQ(csample->buf[i], 1);

	// the very last
	const unsigned long FILESZ = csample->size;
	memset(csample->buf, 1, csample->chlen + 1);
	ASSERT_EQ(fill_buf(csample, FILESZ - 1), 1);
	ASSERT_EQ(csample->bufsz, 1);
	ASSERT_EQ(csample->buf[0], 0);
	ASSERT_EQ(csample->buf[1], 1);

	ASSERT_EQ(fill_buf(csample, FILESZ), -1);
}


TEST_F(TestCSIODictzip, cfread)
{
	size_t i;
	size_t counter = 0;
	// cfread on the border
	cfseeko(csample, 0x7ffa*csample->chlen - csample->chlen, SEEK_SET);
	const int BUFSZ = csample->size - cftello(csample);
	char* buf = new char[BUFSZ + 1];
	memset(buf, 1, BUFSZ + 1);
	ASSERT_EQ(cfread((void*)buf, 1, BUFSZ, csample), BUFSZ);
	ASSERT_EQ(cfeof(csample), 0);
	ASSERT_EQ(buf[BUFSZ - 1] , 0);
	ASSERT_EQ(buf[BUFSZ] , 1);
	ASSERT_EQ(cfread((void*)buf, 1, 1, csample), 0);
	ASSERT_EQ(cfeof(csample), 0);
	delete [] buf;
}

TEST_F(TestCSIODictzip, cfgetc)
{
	char* buf = new char[256*1024 + 1];
	ASSERT_EQ(cfgetc(csample), 0);
	ASSERT_EQ(csample->currpos, 1);
	// cfread on the border
	cfseeko(csample, 0x7ffa*csample->chlen - 2, SEEK_SET);
	ASSERT_EQ(csample->currpos, 0x7ffa*csample->chlen - 2);
	ASSERT_EQ(cfgetc(csample), 0);
	ASSERT_EQ(csample->currpos, 0x7ffa*csample->chlen - 1);
	ASSERT_EQ(cfgetc(csample), 0);
	ASSERT_EQ(csample->currpos, 0x7ffa*csample->chlen);
	ASSERT_EQ(cfgetc(csample), 0);
	ASSERT_EQ(csample->currpos, 0x7ffa*csample->chlen + 1);
	// to the end
	cfseeko(csample, csample->size - 2, SEEK_SET);
	ASSERT_EQ(csample->currpos, csample->size - 2);
	ASSERT_EQ(cfgetc(csample), 0);
	ASSERT_EQ(csample->currpos, csample->size - 1);
	ASSERT_EQ(cfgetc(csample), 0);
	ASSERT_EQ(cfeof(csample), 0);
	ASSERT_EQ(csample->currpos, csample->size);
	ASSERT_EQ(cfgetc(csample), EOF);
	ASSERT_EQ(csample->currpos, csample->size);
	ASSERT_EQ(cfeof(csample), 1);
	delete [] buf;
}

TEST_F(TestCSIODictzip, cferror)
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

TEST_F(TestCSIODictzip, cfseek_cftell )
{
	ASSERT_EQ(cftell(csample), 0);
	cfseek(csample, 1, SEEK_SET);
	ASSERT_EQ(cftell(csample), 1);
	cfseek(csample, 1, SEEK_CUR);
	ASSERT_EQ(cftell(csample), 2);
	cfseek(csample, 0, SEEK_END);
	ASSERT_EQ(cftell(csample), 1910574346);
	cfseek(csample, -1, SEEK_END);
	ASSERT_EQ(cftell(csample), 1910574345);
	ASSERT_EQ(cfgetc(csample), 0);
	ASSERT_EQ(cfgetc(csample), EOF);
}

TEST_F(TestCSIODictzip, cfseeko_cftello)
{
	ASSERT_EQ(cftello(csample), 0);
	cfseeko(csample, 1, SEEK_SET);
	ASSERT_EQ(cftello(csample), 1);
	cfseeko(csample, 1, SEEK_CUR);
	ASSERT_EQ(cftello(csample), 2);
	cfseeko(csample, 0, SEEK_END);
	ASSERT_EQ(cftello(csample), 1910574346);
	cfseeko(csample, -1, SEEK_END);
	ASSERT_EQ(cftello(csample), 1910574345);
	ASSERT_EQ(cfgetc(csample), 0);
	ASSERT_EQ(cfgetc(csample), EOF);
}

#endif // __TCSIO_DICTZIP_HPP__

