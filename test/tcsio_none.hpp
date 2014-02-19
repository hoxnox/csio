/**@author hoxnox <hoxnox@gmail.com>
 * @date 20131217 15:33:12 */

#ifndef __TCSIO_NONE_HPP__
#define __TCSIO_NONE_HPP__

#include "csio_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <csio.h>
#include <string>
#include <string.h>

class TestCSIONone : public ::testing::Test
{
protected:
	void SetUp()
	{
		fname = TEST_SAMPLES_DIR;
		fname += "/file";
		sample = fopen(fname.c_str(), "rb");
		ASSERT_TRUE(sample != NULL) << fname;
		csample = cfinit(sample);
		ASSERT_EQ(cferror(csample), 0) << strerror(errno);
		ASSERT_EQ(csample->compression, NONE);
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

TEST_F(TestCSIONone, additional_fields)
{
	ASSERT_EQ(csample->size, 256);
}

TEST_F(TestCSIONone, get_compression)
{
	ASSERT_EQ(get_compression(sample), NONE);
}

TEST_F(TestCSIONone, cfopen_cfclose )
{
	CFILE* file = cfopen(fname.c_str(), "rb");
	ASSERT_EQ(file->compression, NONE);
	ASSERT_EQ(cferror(file), 0) << strerror(errno);
	ASSERT_NO_FATAL_FAILURE(cfclose(&file));
}

TEST_F(TestCSIONone, cfread )
{
	char buf[256+1];
	memset(buf, 1, 256+1);
	ASSERT_EQ(cfread((void*)buf, 1, 257, csample), 256);
	ASSERT_EQ(buf[256-1] , 0);
	ASSERT_EQ(buf[256] , 1);
	ASSERT_EQ(cfeof(csample) , 1);
	ASSERT_EQ(cfgetc(csample), EOF);
}

TEST_F(TestCSIONone, cfgetc )
{
	for(size_t i = 0; i < 256; ++i)
	{
		ASSERT_EQ(cfgetc(csample), 0) << i;
		ASSERT_EQ(cfeof(csample) , 0);
	}
	ASSERT_EQ(cfgetc(csample), EOF);
	ASSERT_EQ(cfeof(csample) , 1);
}


TEST_F(TestCSIONone, cfseek_cftell )
{
	ASSERT_EQ(cftell(csample), 0);
	cfseek(csample, 1, SEEK_SET);
	ASSERT_EQ(cftell(csample), 1);
	cfseek(csample, 1, SEEK_CUR);
	ASSERT_EQ(cftell(csample), 2);
	cfseek(csample, 0, SEEK_END);
	ASSERT_EQ(cftell(csample), 256);
	cfseek(csample, -1, SEEK_END);
	ASSERT_EQ(cftell(csample), 255);
	ASSERT_EQ(cfgetc(csample), 0);
	ASSERT_EQ(cfgetc(csample), EOF);
}

TEST_F(TestCSIONone, cfseeko_cftello)
{
	ASSERT_EQ(cftello(csample), 0);
	cfseeko(csample, 1, SEEK_SET);
	ASSERT_EQ(cftello(csample), 1);
	cfseeko(csample, 1, SEEK_CUR);
	ASSERT_EQ(cftello(csample), 2);
	cfseeko(csample, 0, SEEK_END);
	ASSERT_EQ(cftello(csample), 256);
	cfseeko(csample, -1, SEEK_END);
	ASSERT_EQ(cftello(csample), 255);
	ASSERT_EQ(cfgetc(csample), 0);
	ASSERT_EQ(cfgetc(csample), EOF);
}


TEST_F(TestCSIONone, cferror)
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

TEST_F(TestCSIONone, FILE_is_opened_on_cfclose)
{
	ASSERT_NE(cfgetc(csample), EOF);
	cfclose(&csample);
	ASSERT_NE(fgetc(sample), EOF);
	ASSERT_TRUE(csample == NULL);
}

#endif // __TCSIO_NONE_HPP__

