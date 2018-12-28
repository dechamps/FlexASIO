#pragma once

#include <portaudio.h>
#include <pa_win_wasapi.h>

#include <windows.h>
#include <MMReg.h>

#include <functional>
#include <string>
#include <string_view>

namespace flexasio {

	class PortAudioDebugRedirector final {
	public:
		using Write = std::function<void(std::string_view)>;

		explicit PortAudioDebugRedirector(Write write);
		~PortAudioDebugRedirector();

	private:
		static void DebugPrint(const char*);

		static Write write;
	};

	std::string GetHostApiTypeIdString(PaHostApiTypeId hostApiTypeId);
	std::string GetSampleFormatString(PaSampleFormat sampleFormat);
	std::string GetStreamFlagsString(PaStreamFlags streamFlags);
	std::string GetWasapiFlagsString(PaWasapiFlags wasapiFlags);
	std::string GetWasapiThreadPriorityString(PaWasapiThreadPriority threadPriority);
	std::string GetWasapiStreamCategoryString(PaWasapiStreamCategory streamCategory);
	std::string GetWasapiStreamOptionString(PaWasapiStreamOption streamOption);
	std::string GetStreamCallbackFlagsString(PaStreamCallbackFlags streamCallbackFlags);

	struct HostApi {
		explicit HostApi(PaHostApiIndex index) : index(index), info(GetInfo(index)) {}

		const PaHostApiIndex index;
		const PaHostApiInfo& info;

		friend std::ostream& operator<<(std::ostream&, const HostApi&);

	private:
		static const PaHostApiInfo& GetInfo(PaHostApiIndex index);
	};

	struct Device {
		explicit Device(PaDeviceIndex index) : index(index), info(GetInfo(index)) {}

		const PaDeviceIndex index;
		const PaDeviceInfo& info;

		friend std::ostream& operator<<(std::ostream&, const Device&);

	private:
		static const PaDeviceInfo& GetInfo(PaDeviceIndex index);
	};

	WAVEFORMATEXTENSIBLE GetWasapiDeviceDefaultFormat(PaDeviceIndex index);

	std::string GetWaveFormatTagString(WORD formatTag);
	std::string GetWaveFormatChannelMaskString(DWORD channelMask);
	std::string GetWaveSubFormatString(const GUID& subFormat);
	std::string DescribeWaveFormat(const WAVEFORMATEXTENSIBLE& waveFormatExtensible);

	std::string DescribeStreamParameters(const PaStreamParameters& parameters);
	std::string DescribeStreamInfo(const PaStreamInfo& info);

	std::string DescribeStreamCallbackTimeInfo(const PaStreamCallbackTimeInfo& streamCallbackTimeInfo);

}