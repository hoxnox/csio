/* @author nyura <nyura@concerteza.com>
 * @date 20130529 20:56:21 */

#include "ProcessManagerBase.hpp"
#include <sstream>
#include <iostream>
#include <sys/signalfd.h>
#include <sys/select.h>
#include "logging.hpp"

namespace csio {

ProcessManagerBase::ProcessManagerBase()
	: state_(STATE_NULL)
{
}

void
ProcessManagerBase::signalError(int sig, siginfo_t *si, void *ptr)
{
	void*  ErrorAddr;
	void*  Trace[16];
	int    x;
	int    TraceSize;
	char** Messages;
	std::stringstream msg;
	msg << _("ProcessManager: received sigal: ") << strsignal(sig)
			<< " (" << si->si_addr << ")" << std::endl;
#if __WORDSIZE == 64 // os type
	ErrorAddr = (void*)((ucontext_t*)ptr)->uc_mcontext.gregs[REG_RIP];
#else
	ErrorAddr = (void*)((ucontext_t*)ptr)->uc_mcontext.gregs[REG_EIP];
#endif
	TraceSize = backtrace(Trace, 16);
	Trace[1] = ErrorAddr;
	Messages = backtrace_symbols(Trace, TraceSize);
	if (Messages)
	{
		const char intend[] = "  ";
		msg << intend << _("== Backtrace ==") << std::endl;
		for (x = 1; x < TraceSize; x++)
			msg << intend << Messages[x] << std::endl;
		msg << intend << _("== End Backtrace ==");
		LOG(ERROR) << msg.str();
		free(Messages);
	}

	VLOG(2) << _("Exception occur. Hard stopping.");

	// TODO: It will be best to legally stop here, or, at least give
	// inheritances chance to do some work: handle closing,
	// destructing or smth...

	exit(2); // need restart status
}

void ProcessManagerBase::loop()
{
	int sfd = 0;
	struct signalfd_siginfo fdsi;
	sfd = signalfd(-1, &sigset_, SFD_NONBLOCK);
	if (sfd == -1)
	{
		LOG(ERROR)<<_("ProcessManager: error registering process."
		              " Can't get signalfd.");
		return;
	}
	for (;;)
	{
		{
			std::lock_guard<std::mutex> lock(state_mtx_);
			if (state_ == STATE_NULL)
				break;
		}
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(sfd, &rfds);

		int rs = select(sfd+1, &rfds, NULL, NULL, &tv);
		
		if (rs == -1)
		{
			LOG(ERROR) << _("ProcessManager: error signals wait.")
			           << _(" Message: ") << strerror(errno);
			Stop();
			break;
		}
		else if(rs == 0)
		{
			continue;
		}
		ssize_t s = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
		if (s != sizeof(struct signalfd_siginfo))
		{
			LOG(ERROR) << _("ProcessManager: error signals read.")
			           << _(" Message: ") << strerror(errno);
			Stop();
			break;
		}
		if (fdsi.ssi_signo == SIGUSR1)
		{
			// TODO: renew config
			continue;
		} else {
			LOG(INFO) << _("ProcessManager: received signal: ")
			          << strsignal(fdsi.ssi_signo)
			          << _(" STOPPING");
			Stop();
			break;
		}
	}
	VLOG(2) << _("ProcessManager: cleaning.");
	if(!doStop())
		LOG(ERROR) << _("ProcessManager: cleaning process failed.");
}

void
ProcessManagerBase::Loop()
{
	{
		std::lock_guard<std::mutex> lock(state_mtx_);
		if(state_ == STATE_RUNNING)
			return;
		else if(state_ != STATE_NULL)
			return;
		setupSignals();
		VLOG(2) << _("ProcessManager: initializing.");
		if(!doStart())
		{
			LOG(ERROR) << _("ProcessManager: initializing failed.");
			return;
		}
		state_ = STATE_RUNNING;
	}
	VLOG(2) << _("ProcessManager: starting main loop.");
	loop();
}

void
ProcessManagerBase::Stop()
{
	std::lock_guard<std::mutex> lock(state_mtx_);
	if(state_ == STATE_NULL)
		return;
	if(state_ == STATE_RUNNING)
		state_ = STATE_NULL;
	VLOG(2) << _("ProcessManager: stopping.");
}

} // namespace 

