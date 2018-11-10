#pragma once

#include "config.h"

#include "../FlexASIOUtil/portaudio.h"

#include "..\ASIOSDK2.3.1\common\asiosys.h"
#include "..\ASIOSDK2.3.1\common\asio.h"

#include <portaudio.h>

#include <windows.h>

#include <optional>
#include <stdexcept>
#include <vector>

namespace flexasio {

	class ASIOException : public std::runtime_error {
	public:
		template <typename... Args> ASIOException(ASIOError asioError, Args&&... args) : asioError(asioError), std::runtime_error(std::forward<Args>(args)...) {}
		ASIOError GetASIOError() const { return asioError; }

	private:
		ASIOError asioError;
	};

	class FlexASIO final {
	public:
		FlexASIO(void* sysHandle);
		~FlexASIO();

		void GetChannels(long* numInputChannels, long* numOutputChannels);
		void GetChannelInfo(ASIOChannelInfo* info);
		bool CanSampleRate(ASIOSampleRate sampleRate);
		void SetSampleRate(ASIOSampleRate sampleRate);
		void GetSampleRate(ASIOSampleRate* sampleRate);

		void CreateBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks);
		void DisposeBuffers();
		void GetLatencies(long* inputLatency, long* outputLatency);

		void Start();
		void Stop();
		void GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp);

		void ControlPanel();

	private:
		using Sample = float;

		class PortAudioHandle {
		public:
			PortAudioHandle();
			PortAudioHandle(const PortAudioHandle&) = delete;
			PortAudioHandle(const PortAudioHandle&&) = delete;
			~PortAudioHandle();
		};

		class Win32HighResolutionTimer {
		public:
			Win32HighResolutionTimer();
			Win32HighResolutionTimer(const Win32HighResolutionTimer&) = delete;
			Win32HighResolutionTimer(Win32HighResolutionTimer&&) = delete;
			~Win32HighResolutionTimer();
			DWORD GetTimeMilliseconds() const;
		};

		struct Buffers
		{
			Buffers(size_t buffer_count, size_t channel_count, size_t buffer_size) :
				buffer_count(buffer_count), channel_count(channel_count), buffer_size(buffer_size),
				buffers(new Sample[getSize()]()) { }
			~Buffers() { delete[] buffers; }
			Sample* getBuffer(size_t buffer, size_t channel) { return buffers + buffer * channel_count * buffer_size + channel * buffer_size; }
			size_t getSize() { return buffer_count * channel_count * buffer_size; }

			const size_t buffer_count;
			const size_t channel_count;
			const size_t buffer_size;

			// This is a giant buffer containing all ASIO buffers. It is organized as follows:
			// [ input channel 0 buffer 0 ] [ input channel 1 buffer 0 ] ... [ input channel N buffer 0 ] [ output channel 0 buffer 0 ] [ output channel 1 buffer 0 ] .. [ output channel N buffer 0 ]
			// [ input channel 0 buffer 1 ] [ input channel 1 buffer 1 ] ... [ input channel N buffer 1 ] [ output channel 0 buffer 1 ] [ output channel 1 buffer 1 ] .. [ output channel N buffer 1 ]
			// The reason why this is a giant blob is to slightly improve performance by (theroretically) improving memory locality.
			Sample* const buffers;
		};

		PaError OpenStream(PaStream**, double sampleRate, unsigned long framesPerBuffer);
		static int StaticStreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw();
		PaStreamCallbackResult StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags);

		const HWND windowHandle = nullptr;
		const Config config;

		PortAudioLogger portAudioLogger;
		PortAudioHandle portAudioHandle;

		const HostApi hostApi;
		const std::optional<Device> inputDevice;
		const std::optional<Device> outputDevice;

		long input_channel_count;
		long output_channel_count;
		// WAVEFORMATEXTENSIBLE channel masks. Not always available.
		DWORD input_channel_mask = 0;
		DWORD output_channel_mask = 0;

		ASIOSampleRate sample_rate = 0;

		// PortAudio buffer addresses are dynamic and are only valid for the duration of the stream callback.
		// In contrast, ASIO buffer addresses are static and are valid for as long as the stream is running.
		// Thus we need our own buffer on top of PortAudio's buffers. This doens't add any latency because buffers are copied immediately.
		std::unique_ptr<Buffers> buffers;
		std::vector<ASIOBufferInfo> buffers_info;
		ASIOCallbacks callbacks;

		PaStream* stream = nullptr;
		bool host_supports_timeinfo;
		// The index of the "unlocked" buffer (or "half-buffer", i.e. 0 or 1) that contains data not currently being processed by the ASIO host.
		size_t our_buffer_index;
		ASIOSamples position;
		ASIOTimeStamp position_timestamp;
		bool started = false;
		std::optional<Win32HighResolutionTimer> win32HighResolutionTimer;
	};

}