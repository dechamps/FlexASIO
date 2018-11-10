#include "portaudio.h"

#include <iostream>
#include <string>

#include "../FlexASIOUtil/log.h"
#include "../FlexASIOUtil/portaudio.h"

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

			std::cout << std::endl;
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
			}
		}

		void InitAndListDevices() {
			PortAudioLogger portAudioLogger;

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