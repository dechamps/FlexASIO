#pragma once

#include <portaudio.h>

#include <memory>

namespace flexasio {

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
