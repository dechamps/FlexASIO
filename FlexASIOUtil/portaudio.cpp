#include "portaudio.h"

#include <cctype>
#include <mutex>
#include <stdexcept>
#include <string_view>

#include "log.h"
#include "string.h"

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
		Log() << "PortAudio version: " << Pa_GetVersionText();

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

	std::string GetHostApiTypeIdString(PaHostApiTypeId hostApiTypeId) {
		return EnumToString(hostApiTypeId, {
			{ paInDevelopment, "In development" },
			{ paDirectSound, "DirectSound" },
			{ paMME, "MME" },
			{ paASIO, "ASIO" },
			{ paSoundManager, "SoundManager" },
			{ paCoreAudio, "CoreAudio" },
			{ paOSS, "OSS" },
			{ paALSA, "ALSA" },
			{ paAL, "AL" },
			{ paBeOS, "BeOS" },
			{ paWDMKS, "WDMKS" },
			{ paJACK, "JACK" },
			{ paWASAPI, "WASAPI" },
			{ paAudioScienceHPI, "AudioScienceHPI" },
			});
	}

	std::ostream& operator<<(std::ostream& os, const HostApi& hostApi) {
		os << "PortAudio host API index " << hostApi.index
			<< " (name: '" << hostApi.info.name
			<< "', type: " << GetHostApiTypeIdString(hostApi.info.type)
			<< ", default input device: " << hostApi.info.defaultInputDevice
			<< ", default output device: " << hostApi.info.defaultOutputDevice << ")";
		return os;
	}

	const PaHostApiInfo& HostApi::GetInfo(PaHostApiIndex index) {
		const auto info = Pa_GetHostApiInfo(index);
		if (info == nullptr) throw std::runtime_error(std::string("Unable to get host API info for host API index ") + std::to_string(index));
		return *info;
	}

	std::ostream& operator<<(std::ostream& os, const Device& device) {
		os << "PortAudio device index " << device.index
			<< " (name: '" << device.info.name
			<< "', host API: " << device.info.hostApi
			<< ", default sample rate: " << device.info.defaultSampleRate
			<< ", max input channels: " << device.info.maxInputChannels
			<< ", max output channels: " << device.info.maxOutputChannels
			<< ", input latency: " << device.info.defaultLowInputLatency << " (low) " << device.info.defaultHighInputLatency << " (high)"
			<< ", output latency: " << device.info.defaultLowOutputLatency << " (low) " << device.info.defaultHighOutputLatency << " (high)" << ")";
		return os;
	}

	const PaDeviceInfo& Device::GetInfo(PaDeviceIndex index) {
		const auto info = Pa_GetDeviceInfo(index);
		if (info == nullptr) throw std::runtime_error(std::string("Unable to get device info for device index ") + std::to_string(index));
		return *info;
	}

}
