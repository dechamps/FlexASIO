#include "portaudio.h"

#include "log.h"
#include "../FlexASIOUtil/portaudio.h"

namespace flexasio {

	void StreamDeleter::operator()(PaStream* stream) throw() {
		Log() << "Closing PortAudio stream " << stream;
		const auto error = Pa_CloseStream(stream);
		if (error != paNoError)
			Log() << "Unable to close PortAudio stream: " << Pa_GetErrorText(error);
	}

	Stream OpenStream(const PaStreamParameters *inputParameters, const PaStreamParameters *outputParameters, double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback *streamCallback, void *userData) {
		Log() << "Opening PortAudio stream with...";
		Log() << "...input parameters: " << (inputParameters == nullptr ? "none" : DescribeStreamParameters(*inputParameters));
		Log() << "...output parameters: " << (outputParameters == nullptr ? "none" : DescribeStreamParameters(*outputParameters));
		Log() << "...sample rate: " << sampleRate << " Hz";
		Log() << "...frames per buffer: " << framesPerBuffer;
		Log() << "...stream flags: " << GetStreamFlagsString(streamFlags);
		Log() << "...stream callback: " << streamCallback << " (user data " << userData << ")";
		PaStream* stream = nullptr;
		const auto error = Pa_OpenStream(&stream, inputParameters, outputParameters, sampleRate, framesPerBuffer, streamFlags, streamCallback, userData);
		if (error != paNoError) throw std::runtime_error(std::string("unable to open PortAudio stream: ") + Pa_GetErrorText(error));
		if (stream == nullptr)throw std::runtime_error("Pa_OpenStream() unexpectedly returned null");
		Log() << "PortAudio stream opened: " << stream;
		return Stream(stream);
	}

	void StreamStopper::operator()(PaStream* stream) throw() {
		Log() << "Stopping PortAudio stream " << stream;
		const auto error = Pa_StopStream(stream);
		if (error != paNoError)
			Log() << "Unable to stop PortAudio stream: " << Pa_GetErrorText(error);
	}

	ActiveStream StartStream(PaStream* const stream) {
		Log() << "Starting PortAudio stream " << stream;
		const auto error = Pa_StartStream(stream);
		if (error != paNoError) throw std::runtime_error(std::string("unable to start PortAudio stream: ") + Pa_GetErrorText(error));
		Log() << "PortAudio stream started";
		return ActiveStream(stream);
	}

}
