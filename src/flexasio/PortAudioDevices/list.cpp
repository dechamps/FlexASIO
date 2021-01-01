#include <portaudio.h>
#include <pa_win_wasapi.h>

#include <windows.h>
#include <stringapiset.h>

#include <iostream>
#include <string>
#include <io.h>
#include <fcntl.h>

#include <dechamps_cpputil/string.h>

#include "../FlexASIOUtil/portaudio.h"

namespace flexasio {
	namespace {

		template <typename Result>
		auto ThrowOnPaError(Result result) {
			if (result >= 0) return result;
			throw std::runtime_error(std::string("PortAudio error ") + Pa_GetErrorText(result));
		}

		void SetUTF8Mode(FILE* file, std::wstring_view label) {
			const auto fileno = _fileno(file);
			if (fileno < 0) {
				std::wcerr << "Warning: cannot get file descriptor for " << label;
				return;
			}
			// We need proper non-ASCII encoding support for printing device names - see https://github.com/dechamps/FlexASIO/issues/73
			// However, Unicode support in Windows console applications is an absolute mess.
			// _setmode(), _O_U8TEXT, and sticking to wide character I/O seems to produce the "least broken" results.
			// One thing that seems to always be broken no matter what is when using "> file.txt" in Powershell, which results in some garbage UTF-8-in-UTF-16 abomination (cmd.exe is fine, though).
			if (_setmode(fileno, _O_U8TEXT) < 0)
				std::wcerr << "Warning: cannot set " << label << " to UTF-8";
		}

		std::wstring UTF8ToWideString(std::string_view utf8) {
			const auto size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), NULL, 0);
			if (size == 0)
				throw std::runtime_error("Unable to convert UTF-8 string");
			std::wstring result(size, 0);
			if (MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), result.data(), int(result.size())) == 0)
				throw std::runtime_error("Unable to convert UTF-8 string");
			return result;
		}

		void PrintDevice(PaDeviceIndex deviceIndex) {
			std::wcout << "Device index: " << deviceIndex << std::endl;

			const auto device = Pa_GetDeviceInfo(deviceIndex);
			if (device == nullptr) throw std::runtime_error("Pa_GetDeviceInfo() returned NULL");

			std::wcout << "Device name: \"" << UTF8ToWideString(device->name) << "\"" << std::endl;
			std::wcout << "Default sample rate: " << device->defaultSampleRate << std::endl;
			std::wcout << "Input: max channel count " << device->maxInputChannels << ", default latency " << device->defaultLowInputLatency << "s (low) " << device->defaultHighInputLatency << "s (high)" << std::endl;
			std::wcout << "Output: max channel count " << device->maxOutputChannels << ", default latency " << device->defaultLowOutputLatency << "s (low) " << device->defaultHighOutputLatency << "s (high)" << std::endl;

			if (device->hostApi < 0) throw std::runtime_error("invalid hostApi index");
			const auto hostApi = Pa_GetHostApiInfo(device->hostApi);
			if (hostApi == nullptr) throw std::runtime_error("Pa_GetHostApiInfo() returned NULL");

			std::wcout << "Host API name: " << hostApi->name << std::endl;
			std::wcout << "Host API type: " << UTF8ToWideString(GetHostApiTypeIdString(hostApi->type)) << std::endl;
			if (deviceIndex == hostApi->defaultInputDevice) {
				std::wcout << "DEFAULT INPUT DEVICE for this host API" << std::endl;
			}
			if (deviceIndex == hostApi->defaultOutputDevice) {
				std::wcout << "DEFAULT OUTPUT DEVICE for this host API" << std::endl;
			}

			switch (hostApi->type) {
			case paWASAPI:
				std::wcout << "WASAPI device default format: " << UTF8ToWideString(DescribeWaveFormat(GetWasapiDeviceDefaultFormat(deviceIndex))) << std::endl;
				std::wcout << "WASAPI device mix format: " << UTF8ToWideString(DescribeWaveFormat(GetWasapiDeviceMixFormat(deviceIndex))) << std::endl;
			}
		}

		void ListDevices() {
			const PaDeviceIndex deviceCount = Pa_GetDeviceCount();

			for (PaDeviceIndex deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
				try {
					PrintDevice(deviceIndex);
				}
				catch (const std::exception& exception) {
					std::wcerr << "Error while printing device index " << deviceIndex << ": " << exception.what() << std::endl;
				}
				std::wcout << std::endl;
			}
		}

		void InitAndListDevices() {
			SetUTF8Mode(stderr, L"standard error");
			SetUTF8Mode(stdout, L"standard output");

			PortAudioDebugRedirector portAudioLogger([](std::string_view str) { std::wcerr << "[PortAudio] " << UTF8ToWideString(str) << std::endl; });

			try {
				ThrowOnPaError(Pa_Initialize());
			}
			catch (const std::exception& exception) {
				throw std::runtime_error(std::string("failed to initialize PortAudio: ") + exception.what());
			}

			ListDevices();

			try {
				ThrowOnPaError(Pa_Terminate());
			}
			catch (const std::exception& exception) {
				throw std::runtime_error(std::string("failed to terminate PortAudio: ") + exception.what());
			}
		}

	}
}

int main(int, char**) {
	try {
		::flexasio::InitAndListDevices();
	}
	catch (const std::exception& exception) {
		std::wcerr << "ERROR: " << exception.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}