#pragma once

#include <portaudio.h>

#include <memory>
#include <future>
#include <thread>

namespace flexasio {

	struct StreamDeleter {
		void operator()(PaStream*) throw();
	};
	using Stream = std::unique_ptr<PaStream, StreamDeleter>;
	Stream OpenStream(const PaStreamParameters *inputParameters, const PaStreamParameters *outputParameters, double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback *streamCallback, void *userData);

	class ActiveStream {
	public:
		ActiveStream(PaStream*);
		~ActiveStream();

		void EndWaitForStartOutcome();

	private:
		class StartThread {
		public:
			template <typename... Args> StartThread(Args&&... args) : thread(std::forward<Args>(args)...) {}
			~StartThread();

		private:
			std::thread thread;
		};

		PaStream* const stream;

		std::promise<void> promisedOutcome;
		StartThread startThread;
	};

}
