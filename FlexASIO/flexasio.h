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

		void GetBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity);
		void GetChannels(long* numInputChannels, long* numOutputChannels);
		void GetChannelInfo(ASIOChannelInfo* info);
		bool CanSampleRate(ASIOSampleRate sampleRate);
		void SetSampleRate(ASIOSampleRate requestedSampleRate);
		void GetSampleRate(ASIOSampleRate* sampleRateResult);

		void CreateBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks);
		void DisposeBuffers();

		void GetLatencies(long* inputLatency, long* outputLatency);
		void Start();
		void Stop();
		void GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp);

		void ControlPanel();

	private:
		struct SampleType {
			ASIOSampleType asio;
			PaSampleFormat pa;
			size_t size;
		};

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
			PreparedState(FlexASIO& flexASIO, ASIOSampleRate sampleRate, ASIOBufferInfo* asioBufferInfos, long numChannels, long bufferSizeInSamples, ASIOCallbacks* callbacks);
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
				Buffers(size_t buffer_count, size_t channel_count, size_t buffer_size, size_t sample_size);
				Buffers(const Buffers&) = delete;
				Buffers(Buffers&&) = delete;
				~Buffers();
				uint8_t* getBuffer(size_t buffer, size_t channel) const { return buffers + buffer * channelCount * bufferSizeInSamples * sampleSize + channel * bufferSizeInSamples * sampleSize; }
				size_t getSizeInSamples() const { return bufferCount * channelCount * bufferSizeInSamples; }
				size_t getSizeInBytes() const { return getSizeInSamples() * sampleSize; }

				const size_t bufferCount;
				const size_t channelCount;
				const size_t bufferSizeInSamples;
				const size_t sampleSize;

				// This is a giant buffer containing all ASIO buffers. It is organized as follows:
				// [ input channel 0 buffer 0 ] [ input channel 1 buffer 0 ] ... [ input channel N buffer 0 ] [ output channel 0 buffer 0 ] [ output channel 1 buffer 0 ] .. [ output channel N buffer 0 ]
				// [ input channel 0 buffer 1 ] [ input channel 1 buffer 1 ] ... [ input channel N buffer 1 ] [ output channel 0 buffer 1 ] [ output channel 1 buffer 1 ] .. [ output channel N buffer 1 ]
				// The reason why this is a giant blob is to slightly improve performance by (theroretically) improving memory locality.
				uint8_t* const buffers;
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

			bool IsInputEnabled() const;
			bool IsOutputEnabled() const;

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

		static const SampleType float32;
		static const SampleType int32;
		static const SampleType int24;
		static const SampleType int16;
		static SampleType ParseSampleType(std::string_view str);

		int GetInputChannelCount() const;
		int GetOutputChannelCount() const;
		DWORD GetInputChannelMask() const;
		DWORD GetOutputChannelMask() const;

		Stream OpenStream(bool inputEnabled, bool outputEnabled, double sampleRate, unsigned long framesPerBuffer, PaStreamCallback callback, void* callbackUserData);

		const HWND windowHandle = nullptr;
		const Config config;
		const SampleType sampleType;

		PortAudioLogger portAudioLogger;
		PortAudioHandle portAudioHandle;

		const HostApi hostApi;
		const std::optional<Device> inputDevice;
		const std::optional<Device> outputDevice;
		const std::optional<WAVEFORMATEXTENSIBLE> inputFormat;
		const std::optional<WAVEFORMATEXTENSIBLE> outputFormat;

		ASIOSampleRate sampleRate = 0;

		std::optional<PreparedState> preparedState;
	};

}