#include "portaudio.h"

#include "log.h"
#include "../FlexASIOUtil/portaudio.h"

namespace flexasio {

	namespace {

		void LogStreamParameters(const StreamParameters& streamParameters) {
			Log() << "...input parameters: " << (streamParameters.inputParameters == nullptr ? "none" : DescribeStreamParameters(*streamParameters.inputParameters));
			Log() << "...output parameters: " << (streamParameters.outputParameters == nullptr ? "none" : DescribeStreamParameters(*streamParameters.outputParameters));
			Log() << "...sample rate: " << streamParameters.sampleRate << " Hz";
		}

	}

	void CheckFormatSupported(const StreamParameters& streamParameters) {
		Log() << "Checking that PortAudio supports format with...";
		LogStreamParameters(streamParameters);
		const auto error = Pa_IsFormatSupported(streamParameters.inputParameters, streamParameters.outputParameters, streamParameters.sampleRate);
		if (error != paFormatIsSupported) throw std::runtime_error(std::string("PortAudio does not support format: ") + Pa_GetErrorText(error));
		Log() << "Format is supported";
	}

	void StreamDeleter::operator()(PaStream* stream) throw() {
		Log() << "Closing PortAudio stream " << stream;
		const auto error = Pa_CloseStream(stream);
		if (error != paNoError)
			Log() << "Unable to close PortAudio stream: " << Pa_GetErrorText(error);
	}

	Stream OpenStream(const StreamParameters& streamParameters, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback *streamCallback, void *userData) {
		Log() << "Opening PortAudio stream with...";
		LogStreamParameters(streamParameters);
		Log() << "...frames per buffer: " << framesPerBuffer;
		Log() << "...stream flags: " << GetStreamFlagsString(streamFlags);
		Log() << "...stream callback: " << streamCallback << " (user data " << userData << ")";
		PaStream* stream = nullptr;
		const auto error = Pa_OpenStream(&stream, streamParameters.inputParameters, streamParameters.outputParameters, streamParameters.sampleRate, framesPerBuffer, streamFlags, streamCallback, userData);
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
