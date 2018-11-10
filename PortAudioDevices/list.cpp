#include <portaudio.h>
#include <pa_win_wasapi.h>

#include <windows.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>

#include <iostream>
#include <string>

#include "../FlexASIOUtil/log.h"
#include "../FlexASIOUtil/portaudio.h"
#include "../FlexASIOUtil/string.h"

namespace flexasio {
	namespace {

		template <typename Result>
		auto ThrowOnPaError(Result result) {
			if (result >= 0) return result;
			throw std::runtime_error(std::string("PortAudio error ") + Pa_GetErrorText(result));
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

		std::string GetWaveFormatTagString(WORD formatTag) {
			return EnumToString(formatTag, {
				{ WAVE_FORMAT_EXTENSIBLE, "EXTENSIBLE" },
				{ WAVE_FORMAT_MPEG, "MPEG" },
				{ WAVE_FORMAT_MPEGLAYER3, "MPEGLAYER3" },
				});
		}

		std::string GetWaveSubFormatString(const GUID& subFormat) {
			return EnumToString(subFormat, {
				{ KSDATAFORMAT_SUBTYPE_ADPCM, "ADPCM" },
				{ KSDATAFORMAT_SUBTYPE_ALAW, "A-law" },
				{ KSDATAFORMAT_SUBTYPE_DRM, "DRM" },
				{ KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS, "IEC61937 Dolby Digital Plus" },
				{ KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL, "IEC61937 Dolby Digital" },
				{ KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, "IEEE Float" },
				{ KSDATAFORMAT_SUBTYPE_MPEG, "MPEG-1" },
				{ KSDATAFORMAT_SUBTYPE_MULAW, "Mu-law" },
				{ KSDATAFORMAT_SUBTYPE_PCM, "PCM" },
				}, [](const GUID& guid) {
				char str[128];
				// Shamelessly stolen from https://stackoverflow.com/a/18555932/172594
				snprintf(str, sizeof(str), "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
				return std::string(str);
			});
		}

		std::string DescribeWaveFormat(const WAVEFORMATEXTENSIBLE& waveFormatExtensible) {
			const auto& waveFormat = waveFormatExtensible.Format;

			std::stringstream result;
			result << "format tag " << GetWaveFormatTagString(waveFormat.wFormatTag) << ", "
				<< waveFormat.nChannels << " channels, "
				<< waveFormat.nSamplesPerSec << " samples/second, "
				<< waveFormat.nAvgBytesPerSec << " average bytes/second, "
				<< waveFormat.nBlockAlign << " bytes block alignment, "
				<< waveFormat.wBitsPerSample << " bits per sample";

			if (waveFormat.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
				result << ", " << waveFormatExtensible.Samples.wValidBitsPerSample << " valid bits per sample, "
					// TODO: pretty-print channel mask
					<< waveFormatExtensible.dwChannelMask << " channel mask, "
					<< GetWaveSubFormatString(waveFormatExtensible.SubFormat) << " format";
			}

			return result.str();
		}

		void PrintWASAPIDeviceInfo(PaDeviceIndex deviceIndex) {
			WAVEFORMATEXTENSIBLE waveFormatExtensible = { 0 };
			ThrowOnPaError(PaWasapi_GetDeviceDefaultFormat(&waveFormatExtensible, sizeof(waveFormatExtensible), deviceIndex));
			std::cout << "WASAPI device default format: " << DescribeWaveFormat(waveFormatExtensible) << std::endl;
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
			case paWASAPI: PrintWASAPIDeviceInfo(deviceIndex);  break;
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