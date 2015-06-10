/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include "Config.hpp"
#include <getopt.h>
#include <logging.hpp>
#include <gettext.h>
#include <iostream>
#include <iomanip>
#include <sstream>
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
	compressors_count_ = 2;
	compression_level_ = 9;
}

inline std::string
expand_path(const std::string path)
{
	std::string result;
	char * tmp = new char[4096];
	if(tmp != NULL)
	{
		if(realpath(path.c_str(), tmp) == NULL)
		{
			LOG(ERROR) << _("Config: Error resolving path: ")
			           << "\"" << path << "\""
			           << _(" Message: ") << strerror(errno);
			return result;
		}
		result.assign(tmp);
		delete [] tmp;
	}
	return result;
}

int
Config::ParseArgs(int argc, char* argv[])
{
	std::string opt_v, opt_j, opt_o, opt_f, opt_h, opt_l;
	bool verbose = false, force = false;

	const char *sopts = "vj:l:o:fh";

	const struct option lopts[] = {
		{ "verbose", no_argument, NULL, 'v' },
		{ "threads", required_argument, NULL, 'j' },
		{ "level", required_argument, NULL, 'l' },
		{ "output", required_argument, NULL, 'o' },
		{ "force", no_argument, NULL, 'f' },

		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, '\0'}
	};

	int i, opt = 0;
	opt = getopt_long(argc, argv, sopts, lopts, &i);
	while (opt != -1)
	{
		switch (opt)
		{
			case 'v': verbose = true; break;
			case 'j': opt_j = optarg; break;
			case 'l': opt_l = optarg; break;
			case 'o': opt_o = optarg; break;
			case 'f': force = true; break;
			case 'h': PrintHelp(); return 0;
			case  -1: return -1;
			case '?': return -1;
		}
		opt = getopt_long( argc, argv, sopts, lopts, &i );
	}
	VLOG_IF(2, !opt_j.empty() && atoi(opt_j.c_str()) > 256)
		<< _("Config: Too many threads requested. Resetting to 256.");
	if (!opt_j.empty()) compressors_count_ = 
		atoi(opt_j.c_str()) < 256 ? atoi(opt_j.c_str()) : 256;
	VLOG_IF(2, !opt_l.empty() && atoi(opt_l.c_str()) > 9)
		<< _("Config: Compression level is too big. Resetting to 9.");
	if (!opt_l.empty()) compression_level_ = 
		atoi(opt_l.c_str()) < 9 ? atoi(opt_l.c_str()) : 9;
	if (!opt_o.empty()) ofname_ = opt_o;
	if (verbose) verbose_ = true;
	if (force) force_ = true;

	if (optind < argc)
		ifname_ = expand_path(argv[optind++]);
	if (ofname_.empty())
		ofname_ = ifname_ + ".dz";
	return 1;
}

template<class T>
inline std::stringstream&
append_opt(std::stringstream& ss, std::string varname, T val, bool endl = true)
{
	ss << std::setw(2) << "" << varname << " = " << std::boolalpha << val;
	if(endl)
		ss << std::endl;
	return ss;
}

inline std::vector<std::string>
split(std::string str, size_t len)
{
	std::vector<std::string> rs;
	if (str.empty() || len < 15)
		return rs;
	while(str.length() > len)
	{
		size_t splitpos = len;
		while (str[splitpos] != ' ' && splitpos > 0)
			--splitpos;
		if (splitpos == 0)
		{
			splitpos = len;
			while(str[splitpos] != ' ' && splitpos < str.length())
				++splitpos;
		}
		rs.push_back(str.substr(0, splitpos));
		str = str.substr(splitpos + 1, str.length() - splitpos);
	}
	rs.push_back(str);
	return rs;

}

template<class T>
inline std::stringstream&
append_hlp(std::stringstream& ss, std::string opt_short, std::string opt_long,
		T default_val, std::string desc)
{
	size_t totaln = 80;
	size_t optln = 14;
	size_t defln = 11;
	size_t dscln = totaln - optln - defln;
	std::stringstream opt;
	std::stringstream def;
	std::stringstream dsc;
	std::stringstream defempty;

	std::vector<std::string> desc_pieces = split(desc, dscln);
	std::vector<std::string>::iterator i = desc_pieces.begin();

	opt << "-" << opt_short << "(--" << opt_long << ")";
	def << std::boolalpha << " [" << default_val << "] ";
	defempty << std::boolalpha << " [" << "] ";
	ss << std::left << std::setfill(' ')
	   << std::setw(optln) << opt.str()
	   << std::setw(defln) << (def.str() == defempty.str() ? "" : def.str())
	   << *i++ << std::endl;
	for(; i != desc_pieces.end(); ++i)
		ss << std::setw(optln + defln) << "" << *i << std::endl;
	return ss;
}

std::string
Config::GetOptions() const
{
	std::stringstream ss;
	ss << "Options" << std::endl;
	append_opt(ss, "Input"  , IFName());
	append_opt(ss, "Output" , OFName());
	append_opt(ss, "Verbose", verbose_);
	append_opt(ss, "Threads", CompressorsCount());
	append_opt(ss, "Level"  , CompressionLevel());
	append_opt(ss, "Force"  , force_, false);
	return ss.str();
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
	std::stringstream ss;
	ss << std::endl << _("Options:") << std::endl;
	append_hlp(ss, "o", "output", "", "file name to use as output");
	append_hlp(ss, "v", "verbose", Verbose(), "make a lot of noise");
	append_hlp(ss, "j", "threads", CompressorsCount(), 
		"compressors count (tip: use number of CPU cores)");
	append_hlp(ss, "l", "level", CompressionLevel(), 
		"compression level (tip: it is fast enough for 9 here)");
	append_hlp(ss, "f", "force", Force(), 
		"ignore all warnings (rewrite output on exists)");
	append_hlp(ss, "h", "help", "", "print this message");
	std::cout << std::boolalpha << ss.str() << std::endl;
}

} // namespace

