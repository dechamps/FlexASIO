#include <portaudio.h>
#include <pa_win_wasapi.h>

#include <windows.h>

#include <iostream>
#include <string>

#include "../FlexASIOUtil/portaudio.h"
#include "../FlexASIOUtil/string.h"

namespace flexasio {
	namespace {

		template <typename Result>
		auto ThrowOnPaError(Result result) {
			if (result >= 0) return result;
			throw std::runtime_error(std::string("PortAudio error ") + Pa_GetErrorText(result));
		}

		void PrintDevice(PaDeviceIndex deviceIndex) {
			std::cout << "Device index: " << deviceIndex << std::endl;

			const auto device = Pa_GetDeviceInfo(deviceIndex);
			if (device == nullptr) throw std::runtime_error("Pa_GetDeviceInfo() returned NULL");

			std::cout << "Device name: " << device->name << std::endl;
			std::cout << "Default sample rate: " << device->defaultSampleRate << std::endl;
			std::cout << "Input: max channel count " << device->maxInputChannels << ", default latency " << device->defaultLowInputLatency << "s (low) " << device->defaultHighInputLatency << "s (high)" << std::endl;
			std::cout << "Output: max channel count " << device->maxOutputChannels << ", default latency " << device->defaultLowOutputLatency << "s (low) " << device->defaultHighOutputLatency << "s (high)" << std::endl;

			if (device->hostApi < 0) throw std::runtime_error("invalid hostApi index");
			const auto hostApi = Pa_GetHostApiInfo(device->hostApi);
			if (hostApi == nullptr) throw std::runtime_error("Pa_GetHostApiInfo() returned NULL");

			std::cout << "Host API name: " << hostApi->name << std::endl;
			std::cout << "Host API type: " << GetHostApiTypeIdString(hostApi->type) << std::endl;
			if (deviceIndex == hostApi->defaultInputDevice) {
				std::cout << "DEFAULT INPUT DEVICE for this host API" << std::endl;
			}
			if (deviceIndex == hostApi->defaultOutputDevice) {
				std::cout << "DEFAULT OUTPUT DEVICE for this host API" << std::endl;
			}

			switch (hostApi->type) {
			case paWASAPI:
				std::cout << "WASAPI device default format: " << DescribeWaveFormat(GetWasapiDeviceDefaultFormat(deviceIndex)) << std::endl;
			}
		}

		void ListDevices() {
			const PaDeviceIndex deviceCount = Pa_GetDeviceCount();

			for (PaDeviceIndex deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
				try {
					PrintDevice(deviceIndex);
				}
				catch (const std::exception& exception) {
					std::cerr << "Error while printing device index " << deviceIndex << ": " << exception.what() << std::endl;
				}
				std::cout << std::endl;
			}
		}

		void InitAndListDevices() {
			PortAudioDebugRedirector portAudioLogger([](std::string_view str) { std::cerr << "[PortAudio] " << str << std::endl; });

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
		std::cerr << "ERROR: " << exception.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}