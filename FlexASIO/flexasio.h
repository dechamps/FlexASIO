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

		class PreparedState {
		public:
			PreparedState(FlexASIO& flexASIO, ASIOSampleRate sampleRate, ASIOBufferInfo* asioBufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks);
			PreparedState(const PreparedState&) = delete;
			PreparedState(PreparedState&&) = delete;

			bool IsChannelActive(bool isInput, long channel) const;

			void GetLatencies(long* inputLatency, long* outputLatency);
			void Start();
			void Stop();

			void GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp);

			void RequestReset();

		private:
			struct Buffers
			{
				Buffers(size_t buffer_count, size_t channel_count, size_t buffer_size);
				Buffers(const Buffers&) = delete;
				Buffers(Buffers&&) = delete;
				~Buffers();
				Sample* getBuffer(size_t buffer, size_t channel) const { return buffers + buffer * channelCount * bufferSize + channel * bufferSize; }
				size_t getSize() const { return bufferCount * channelCount * bufferSize; }

				const size_t bufferCount;
				const size_t channelCount;
				const size_t bufferSize;

				// This is a giant buffer containing all ASIO buffers. It is organized as follows:
				// [ input channel 0 buffer 0 ] [ input channel 1 buffer 0 ] ... [ input channel N buffer 0 ] [ output channel 0 buffer 0 ] [ output channel 1 buffer 0 ] .. [ output channel N buffer 0 ]
				// [ input channel 0 buffer 1 ] [ input channel 1 buffer 1 ] ... [ input channel N buffer 1 ] [ output channel 0 buffer 1 ] [ output channel 1 buffer 1 ] .. [ output channel N buffer 1 ]
				// The reason why this is a giant blob is to slightly improve performance by (theroretically) improving memory locality.
				Sample* const buffers;
			};

			class RunningState {
			public:
				RunningState(PreparedState& preparedState);

				void GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp);

				PaStreamCallbackResult StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags);

			private:
				const PreparedState& preparedState;
				const bool host_supports_timeinfo;
				// The index of the "unlocked" buffer (or "half-buffer", i.e. 0 or 1) that contains data not currently being processed by the ASIO host.
				size_t our_buffer_index;
				ASIOSamples position;
				ASIOTimeStamp position_timestamp;
				Win32HighResolutionTimer win32HighResolutionTimer;
				const ActiveStream activeStream;
			};

			static int StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw();

			FlexASIO& flexASIO;
			const ASIOSampleRate sampleRate;
			const ASIOCallbacks callbacks;

			// PortAudio buffer addresses are dynamic and are only valid for the duration of the stream callback.
			// In contrast, ASIO buffer addresses are static and are valid for as long as the stream is running.
			// Thus we need our own buffer on top of PortAudio's buffers. This doens't add any latency because buffers are copied immediately.
			const Buffers buffers;
			const std::vector<ASIOBufferInfo> bufferInfos;

			const Stream stream;

			std::optional<RunningState> runningState;
		};

		int GetInputChannelCount() const { return inputDevice.has_value() ? inputDevice->info.maxInputChannels : 0; }
		int GetOutputChannelCount() const { return outputDevice.has_value() ? outputDevice->info.maxOutputChannels : 0; }
		DWORD GetInputChannelMask() const { return inputFormat.has_value() ? inputFormat->dwChannelMask : 0; }
		DWORD GetOutputChannelMask() const { return outputFormat.has_value() ? outputFormat->dwChannelMask : 0; }

		Stream OpenStream(double sampleRate, unsigned long framesPerBuffer, PaStreamCallback callback, void* callbackUserData);

		const HWND windowHandle = nullptr;
		const Config config;

		PortAudioLogger portAudioLogger;
		PortAudioHandle portAudioHandle;

		const HostApi hostApi;
		const std::optional<Device> inputDevice;
		const std::optional<Device> outputDevice;
		const std::optional<WAVEFORMATEXTENSIBLE> inputFormat;
		const std::optional<WAVEFORMATEXTENSIBLE> outputFormat;

		ASIOSampleRate sample_rate = 0;

		std::optional<PreparedState> bufferState;
	};

}