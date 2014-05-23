/* UPA 7.6 interactive provider.
 */

#include "hitsuji.hh"

/* VA leaks a dependency upon _MAX_PATH */
#include <cstdlib>

/* Multimedia high resolution timers */
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib, "winmm")

#include "chromium/chromium_switches.hh"
#include "chromium/command_line.hh"
#include "chromium/logging.hh"
#include "chromium/logging_win.hh"

// {E2FD9547-BF66-4349-A0E3-213DACE4D667}
DEFINE_GUID(kLogProvider, 
0xe2fd9547, 0xbf66, 0x4349, 0xa0, 0xe3, 0x21, 0x3d, 0xac, 0xe4, 0xd6, 0x67);

namespace { /* anonymous */

class env_t
{
public:
	env_t (int argc, const char* argv[])
	{
/* startup from clean string */
		CommandLine::Init (argc, argv);
		std::string log_path = GetLogFileName();
/* forward onto logging */
		logging::InitLogging(
			log_path.c_str(),
			DetermineLogMode (*CommandLine::ForCurrentProcess()),
			logging::DONT_LOCK_LOG_FILE,
			logging::APPEND_TO_OLD_LOG_FILE,
			logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS
			);
#ifndef USE_ETW_LOGGING
		logging::SetLogMessageHandler (log_handler);
#else
		logging::LogEventProvider::Initialize (kLogProvider);
#endif
	}

protected:
	std::string GetLogFileName() {
		const std::string log_filename ("/Hitsuji.log");
		return log_filename;
	}

	logging::LoggingDestination DetermineLogMode (const CommandLine& command_line) {
#ifdef NDEBUG
		const logging::LoggingDestination kDefaultLoggingMode = logging::LOG_NONE;
#else
		const logging::LoggingDestination kDefaultLoggingMode = logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
#endif
		logging::LoggingDestination log_mode;
// Let --enable-logging=file force Vhayu and file logging, particularly useful for
// non-debug builds where otherwise you can't get logs on fault at all.
		if (command_line.GetSwitchValueASCII (switches::kEnableLogging) == "file")
			log_mode = logging::LOG_ONLY_TO_FILE;
		else
			log_mode = kDefaultLoggingMode;
		return log_mode;
	}

	static bool log_handler (int severity, const char* file, int line, size_t message_start, const std::string& str)
	{
		fprintf (stdout, "%s", str.c_str());
		fflush (stdout);
		return true;
	}
};

class timecaps_t
{
	UINT wTimerRes;
public:
	timecaps_t (unsigned resolution_ms) :
		wTimerRes (0)
	{
		TIMECAPS tc;
		if (MMSYSERR_NOERROR == timeGetDevCaps (&tc, sizeof (TIMECAPS))) {
			wTimerRes = min (max (tc.wPeriodMin, resolution_ms), tc.wPeriodMax);
			if (TIMERR_NOCANDO == timeBeginPeriod (wTimerRes)) {
				LOG(WARNING) << "Minimum timer resolution " << wTimerRes << "ms is out of range.";
				wTimerRes = 0;
			}
		} else {
			LOG(WARNING) << "Failed to query timer device resolution.";
		}
	}

	~timecaps_t()
	{
		if (wTimerRes > 0)
			timeEndPeriod (wTimerRes);
	}
};

} /* anonymous namespace */

int
main (
	int		argc,
	const char*	argv[]
	)
{
#ifdef _MSC_VER
/* Suppress abort message. */
	_set_abort_behavior (0, ~0);
#endif

	env_t env (argc, argv);
	timecaps_t timecaps (1 /* ms */);

	hitsuji::hitsuji_t hitsuji;
	return hitsuji.Run();
}

/* eof */