/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140404 19:40:10 */

#include <CompressManager.hpp>
#include <iostream>
#include <logging.hpp>
#include <string>
#include <sstream>

using namespace csio;

INIT_LOGGING

int
main(int argc, char * argv[])
{
	std::ios::sync_with_stdio(false);
	Config cfg;
	if(argc < 2)
	{
		cfg.PrintInfo();
		return 0;
	}
	if (cfg.ParseArgs(argc, argv) <= 0)
		return 0;
	InitLogging(cfg.Verbose() ? 2 : 1);

	CompressManager cmprs(cfg);
	cmprs.Loop();
	return 0;
}

