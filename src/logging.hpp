#include <easylogging++.h>

#ifndef __LOGGING_HPP__
#define __LOGGING_HPP__

namespace csio {

#define ELPP_THREAD_SAFE
#define ELPP_FORCE_USE_STD_THREAD
#define INIT_LOGGING INITIALIZE_EASYLOGGINGPP

using namespace el;

inline void InitLogging(int verbose_level = 1)
{
	Loggers::setVerboseLevel(verbose_level < 4 ? verbose_level : 1);
//	verbose_level = ;
//	FLAGS_logtostderr = true;
//#ifdef NDEBUG
//	FLAGS_log_prefix = false;
//#else
//	FLAGS_log_prefix = true;
//#endif // _NDEBUG
//	FLAGS_v = verbose_level;
//	google::InitGoogleLogging("dzip");
}

} // namespace

#endif // __LOGGING_HPP__
