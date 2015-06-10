/* @author nyura <nyura@concerteza.com>
 * @date 20130529 20:56:21 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <execinfo.h>
#include <unistd.h>
#include <errno.h>
#include <wait.h>
#include <gettext.h>
#include <chrono>
#include <mutex>
#include <thread>
//#include <utils/Utils.h>

namespace csio {

class ProcessManagerBase
{
public:
	enum State
	{
		STATE_NULL      = 0,
		STATE_RUNNING   = 2,
	};
	ProcessManagerBase();
	virtual ~ProcessManagerBase() {};
	void            Dispatch();
	void            Loop();
	void            Stop();
protected:
	virtual bool    doStart() = 0;
	virtual bool    doStop() = 0;
private:
	void            loop();
	void            setupSignals();
	static void*    start_loop(void* process_manager);
	static void     signalError(int sig, siginfo_t *si, void *ptr);
	State           state_;
	std::mutex      state_mtx_;
	sigset_t        sigset_;
	std::thread*    thread_;
};

////////////////////////////////////////////////////////////////////////
// inline

inline void
ProcessManagerBase::setupSignals()
{
	struct sigaction sigact;
	sigact.sa_flags = SA_SIGINFO;
	sigact.sa_sigaction = ProcessManagerBase::signalError;
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGFPE, &sigact, 0);
	sigaction(SIGILL, &sigact, 0);
	sigaction(SIGSEGV, &sigact, 0);
	sigaction(SIGBUS, &sigact, 0);

	sigemptyset(&sigset_);
	sigaddset(&sigset_, SIGQUIT);
	sigaddset(&sigset_, SIGINT);
	sigaddset(&sigset_, SIGTERM);
	sigaddset(&sigset_, SIGUSR1);
	sigaddset(&sigset_, SIGALRM);
	sigprocmask(SIG_BLOCK, &sigset_, NULL);
}

} // namespace

