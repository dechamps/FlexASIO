#include "portaudio.h"

#include "log.h"
#include "../FlexASIOUtil/portaudio.h"

#include <Objbase.h>
#include <comdef.h>

namespace flexasio {

	namespace {

		class COMInitializer {
		public:
			COMInitializer() {
				::_com_util::CheckError(CoInitializeEx(NULL, COINIT_MULTITHREADED));
			}
			~COMInitializer() {
				CoUninitialize();
			}
		};

	}

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

	ActiveStream::ActiveStream(PaStream* stream) : stream(stream), startThread([this] {
		std::exception_ptr exception;
		try {
			Log() << "Starting PortAudio stream " << this->stream;
			{
				COMInitializer comInitializer;  // Required because WASAPI Pa_StartStream() calls into the COM library
				const auto error = Pa_StartStream(this->stream);
				if (error != paNoError) throw std::runtime_error(std::string("unable to start PortAudio stream: ") + Pa_GetErrorText(error));
			}
			Log() << "PortAudio stream started";
		}
		catch (...) {
		  exception = std::current_exception();
		}
		Log() << "Setting start outcome";
		try {
			if (exception)
				promisedOutcome.set_exception(exception);
			else
				promisedOutcome.set_value();
		}
		catch (const std::future_error& future_error) {
			if (future_error.code() != std::future_errc::promise_already_satisfied) throw;
			Log() << "Start outcome already set";
			return;
		}
		Log() << "Start outcome set";
		}) {
		Log() << "Waiting for start outcome";
		promisedOutcome.get_future().get();
		Log() << "Start outcome is OK";
	}

	ActiveStream::StartThread::~StartThread() {
		if (thread.joinable()) {
			Log() << "Waiting for start thread to finish";
			thread.join();
			Log() << "Start thread finished";
		}
	}

	void ActiveStream::EndWaitForStartOutcome() {
		Log() << "Ending wait for outcome";
		try {
			promisedOutcome.set_value();
		}
		catch (const std::future_error& future_error) {
			if (future_error.code() != std::future_errc::promise_already_satisfied) throw;
			Log() << "Start outcome already set";
			return;
		}
		Log() << "Outcome wait ended";
	}

	ActiveStream::~ActiveStream() {
		Log() << "Stopping PortAudio stream " << stream;
		const auto error = Pa_StopStream(stream);
		if (error != paNoError)
			Log() << "Unable to stop PortAudio stream: " << Pa_GetErrorText(error);
	}

}
