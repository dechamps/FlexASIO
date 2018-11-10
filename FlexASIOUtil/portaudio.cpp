#include "portaudio.h"

#include <cctype>
#include <mutex>
#include <string_view>

#include "log.h"

// From pa_debugprint.h. The PortAudio DLL exports this function, but sadly it is not exposed in a public header file.
extern "C" {
	typedef void(*PaUtilLogCallback) (const char *log);
	extern void PaUtil_SetDebugPrintFunction(PaUtilLogCallback cb);
}

namespace flexasio {

	namespace {
		std::mutex loggerMutex;
		size_t loggerReferenceCount;

		void DebugPrint(const char* log) {
			std::string_view logline(log);
			while (!logline.empty() && isspace(logline.back())) logline.remove_suffix(1);
			Log() << "[PortAudio] " << logline;
		}
	}

	PortAudioLogger::PortAudioLogger() {
		std::scoped_lock loggerLock(loggerMutex);
		if (loggerReferenceCount++ > 0) return;
		Log() << "Enabling PortAudio debug output redirection";
		PaUtil_SetDebugPrintFunction(DebugPrint);
	}

	PortAudioLogger::~PortAudioLogger() {
		std::scoped_lock loggerLock(loggerMutex);
		if (--loggerReferenceCount > 0) return;
		Log() << "Disabling PortAudio debug output redirection";
		PaUtil_SetDebugPrintFunction(NULL);
	}

}
