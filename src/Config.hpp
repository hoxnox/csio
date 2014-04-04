/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <string>

namespace csio {

class Config
{
public:
	Config()
	{
		SetDefaults();
	}
	~Config();
	int ParseArgs(int argc, char* argv[]);
	void SetDefaults();

	std::string GetOptions() const;
	void        PrintInfo() const;
	void        PrintHelp() const;

	bool        Force()            const { return force_; }
	int         Verbose()          const { return verbose_; }
	std::string IFName()           const { return ifname_; }
	std::string OFName()           const { return ofname_; }
	int         CompressionLevel() const { return compression_level_; }
	int         CompressorsCount() const { return compressors_count_; }

private:
	bool        force_;
	int         verbose_;
	std::string ifname_;
	std::string ofname_;
	int         compression_level_;
	int         compressors_count_;
};

} // namespace

#endif // __CONFIG_HPP__

