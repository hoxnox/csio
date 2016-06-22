#include <csio.h>
#include <iostream>
#include <cstdio>
#include <string>
#include <fstream>

std::string
prepare_sample_file()
{
	const char sample[] = {
		0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20,
		0x77, 0x6F, 0x72, 0x6C, 0x64, 0x21
	};
	std::string fname = std::tmpnam(NULL);
	std::basic_fstream<char> ofile(fname.c_str(), std::ios::binary | std::ios::out);
	if (!ofile.is_open())
		return std::string();
	ofile.write(sample, sizeof(sample));
	return fname;
}

int main(int argc, char * argv[])
{
	std::string fname = prepare_sample_file();
	if (fname.empty())
		return -1;
	CFILE* cfile = cfopen(fname.c_str(), "r");
	char buf[20];
	if (cfread(buf, 1, 13, cfile) != 13)
		return -1;
	if (std::string(buf) != "Hello, world!")
		return -1;
	return 0;
}

