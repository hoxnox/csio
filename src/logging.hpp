#include <glog/logging.h>

#ifndef __LOGGING_HPP__
#define __LOGGING_HPP__

namespace csio {

inline void InitLogging(int verbose_level = 1)
{
	verbose_level = (verbose_level < 4 ? verbose_level : 1);
	FLAGS_logtostderr = true;
#ifdef NDEBUG
	FLAGS_log_prefix = false;
#else
	FLAGS_log_prefix = true;
#endif // _NDEBUG
	FLAGS_v = verbose_level;
	google::InitGoogleLogging("dzip");
}

} // namespace

#endif // __LOGGING_HPP__
