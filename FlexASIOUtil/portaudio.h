#pragma once

#include <portaudio.h>

#include <windows.h>
#include <MMReg.h>

#include <string>

namespace flexasio {

	class PortAudioLogger final {
	public:
		PortAudioLogger();
		~PortAudioLogger();
	};

	std::string GetHostApiTypeIdString(PaHostApiTypeId hostApiTypeId);
	std::string GetSampleFormatString(PaSampleFormat sampleFormat);

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

	std::string DescribeWaveFormat(const WAVEFORMATEXTENSIBLE& waveFormatExtensible);

	std::string DescribeStreamParameters(const PaStreamParameters& parameters);

	struct StreamDeleter {
		void operator()(PaStream*) throw();
	};
	using Stream = std::unique_ptr<PaStream, StreamDeleter>;
	Stream OpenStream(const PaStreamParameters *inputParameters, const PaStreamParameters *outputParameters, double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback *streamCallback, void *userData);

	struct StreamStopper {
		void operator()(PaStream*) throw();
	};
	using ActiveStream = std::unique_ptr<PaStream, StreamStopper>;
	ActiveStream StartStream(PaStream*);

}