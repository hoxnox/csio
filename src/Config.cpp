/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include "Config.hpp"
#include <getopt.h>
#include <logging.hpp>
#include <gettext.h>
#include <iostream>
#include <csio.h>

namespace csio {

Config::~Config()
{
}

void
Config::SetDefaults()
{
	force_ = false;
	verbose_ = false;
	compressors_count_ = 10;
	compression_level_ = 9;
}

inline std::string
expand_path(const std::string path)
{
	std::string copy(path);
	char * tmp = new char[4096];
	if(tmp != NULL)
	{
		if(realpath(path.c_str(), tmp) == NULL)
		{
			LOG(ERROR) << _("Error resolving \"") << path << "\""
			           << _("Message: ") << strerror(errno);
			copy.assign(tmp);
		}
		delete [] tmp;
	}
	return copy;
}

int
Config::ParseArgs(int argc, char* argv[])
{
	std::string opt_v, opt_j, opt_o, opt_f, opt_h;
	bool verbose = false, force = false;

	const char *sopts = "vj:o:fh";

	const struct option lopts[] = {
		{ "verbose", no_argument, NULL, 'v' },
		{ "threads", required_argument, NULL, 'j' },
		{ "output", required_argument, NULL, 'o' },
		{ "force", no_argument, NULL, 'f' },

		{ "help", no_argument, NULL, 'h' },
		{NULL, no_argument, NULL, '\0'}
	};

	int i, opt = 0;
	opt = getopt_long(argc, argv, sopts, lopts, &i);
	while (opt != -1)
	{
		switch (opt)
		{
			case 'v': verbose = true; break;
			case 'j': opt_j = optarg; break;
			case 'o': opt_o = optarg; break;
			case 'f': force = true; break;
			case 'h': PrintHelp(); return 0;
			case  -1: return -1;
			case '?': return -1;
		}
		opt = getopt_long( argc, argv, sopts, lopts, &i );
	}
	if (!opt_j.empty()) compressors_count_ = 
		atoi(opt_j.c_str()) < 256 ? atoi(opt_j.c_str()) : 256;
	if (!opt_o.empty()) ofname_ = expand_path(opt_o);
	if (verbose) verbose_ = true;
	if (force) force_ = true;

	if (optind < argc)
		ifname_ = argv[optind++];
	if (ofname_.empty())
		ofname_ = ifname_ + ".dz";
	return 1;
}

std::string
Config::GetOptions() const
{
	return "Options:"
	       "	TODO: options desc";
}

void
Config::PrintInfo() const
{
	std::cout << _("dzip compression utility (csio ver. ")
	          << csio_VERSION_MAJOR << "."
	          << csio_VERSION_MINOR << "."
	          << csio_VERSION_PATCH << ")" << std::endl
	          << _("Usage: dzip [options] <filename>") << std::endl;
}

void
Config::PrintHelp() const
{
	PrintInfo();
	std::cout << _("Options:") << std::endl
	          << _("	TODO: options help") << std::endl;
}

} // namespace

