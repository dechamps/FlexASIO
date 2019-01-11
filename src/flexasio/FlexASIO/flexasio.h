#pragma once

#include "config.h"

#include "portaudio.h"
#include "../FlexASIOUtil/portaudio.h"

#include <dechamps_ASIOUtil/asiosdk/asiosys.h>
#include <dechamps_ASIOUtil/asiosdk/asio.h>

#include <portaudio.h>

#include <windows.h>

#include <atomic>
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
				Buffers(size_t bufferSetCount, size_t inputChannelCount, size_t outputChannelCount, size_t bufferSizeInSamples, size_t inputSampleSize, size_t outputSampleSize);
				~Buffers();
				uint8_t* GetInputBuffer(size_t bufferSetIndex, size_t channelIndex) { return buffers.data() + bufferSetIndex * GetBufferSetSizeInBytes() + channelIndex * GetInputBufferSizeInBytes(); }
				uint8_t* GetOutputBuffer(size_t bufferSetIndex, size_t channelIndex) { return GetInputBuffer(bufferSetIndex, inputChannelCount) + channelIndex * GetOutputBufferSizeInBytes(); }
				size_t GetBufferSetSizeInBytes() const { return buffers.size() / bufferSetCount; }
				size_t GetInputBufferSizeInBytes() const { if (buffers.empty()) return 0; return bufferSizeInSamples * inputSampleSize; }
				size_t GetOutputBufferSizeInBytes() const { if (buffers.empty()) return 0; return bufferSizeInSamples * outputSampleSize; }

				const size_t bufferSetCount;
				const size_t inputChannelCount;
				const size_t outputChannelCount;
				const size_t bufferSizeInSamples;
				const size_t inputSampleSize;
				const size_t outputSampleSize;

				// This is a giant buffer containing all ASIO buffers. It is organized as follows:
				// [ input channel 0 buffer 0 ] [ input channel 1 buffer 0 ] ... [ input channel N buffer 0 ] [ output channel 0 buffer 0 ] [ output channel 1 buffer 0 ] .. [ output channel N buffer 0 ]
				// [ input channel 0 buffer 1 ] [ input channel 1 buffer 1 ] ... [ input channel N buffer 1 ] [ output channel 0 buffer 1 ] [ output channel 1 buffer 1 ] .. [ output channel N buffer 1 ]
				// The reason why this is a giant blob is to slightly improve performance by (theroretically) improving memory locality.
				std::vector<uint8_t> buffers;
			};

			class RunningState {
			public:
				RunningState(PreparedState& preparedState);

				void GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) const;

				PaStreamCallbackResult StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags);

			private:
				struct SamplePosition {
					ASIOSamples samples = { 0 };
					ASIOTimeStamp timestamp = { 0 };
				};

				class Registration {
				public:
					Registration(RunningState*& holder, RunningState& runningState) : holder(holder) {
						holder = &runningState;
					}
					~Registration() { holder = nullptr; }

				private:
					RunningState*& holder;
				};

				void Register() { preparedState.runningState = this; }
				void Unregister() { preparedState.runningState = nullptr; }

				PreparedState& preparedState;
				const bool host_supports_timeinfo;
				// The index of the "unlocked" buffer (or "half-buffer", i.e. 0 or 1) that contains data not currently being processed by the ASIO host.
				size_t our_buffer_index;
				std::atomic<SamplePosition> samplePosition;
				Win32HighResolutionTimer win32HighResolutionTimer;
				Registration registration{ preparedState.runningState, *this };
				const ActiveStream activeStream;
			};

			static int StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw();

			FlexASIO& flexASIO;
			const ASIOSampleRate sampleRate;
			const ASIOCallbacks callbacks;

			// PortAudio buffer addresses are dynamic and are only valid for the duration of the stream callback.
			// In contrast, ASIO buffer addresses are static and are valid for as long as the stream is running.
			// Thus we need our own buffer on top of PortAudio's buffers. This doens't add any latency because buffers are copied immediately.
			Buffers buffers;
			const std::vector<ASIOBufferInfo> bufferInfos;

			const Stream stream;

			// RunningState will set runningState before ownedRunningState has finished constructing.
			// This allows PreparedState to properly forward stream callbacks that might fire before RunningState construction is fully complete.
			// (See https://github.com/dechamps/FlexASIO/issues/27)
			// During steady-state operation, runningState just points to *ownedRunningState.
			RunningState* runningState = nullptr;
			std::optional<RunningState> ownedRunningState;
		};

		static const SampleType float32;
		static const SampleType int32;
		static const SampleType int24;
		static const SampleType int16;
		static const std::pair<std::string_view, SampleType> sampleTypes[];
		static SampleType ParseSampleType(std::string_view str);
		static std::optional<SampleType> WaveFormatToSampleType(const WAVEFORMATEXTENSIBLE& waveFormat);
		static SampleType SelectSampleType(const Config::Stream&, PaHostApiTypeId, const std::optional<WAVEFORMATEXTENSIBLE>&);
		static std::string DescribeSampleType(const SampleType&);

		int GetInputChannelCount() const;
		int GetOutputChannelCount() const;
		DWORD GetInputChannelMask() const;
		DWORD GetOutputChannelMask() const;

		Stream OpenStream(bool inputEnabled, bool outputEnabled, double sampleRate, unsigned long framesPerBuffer, PaStreamCallback callback, void* callbackUserData);

		const HWND windowHandle = nullptr;
		const Config config;

		PortAudioDebugRedirector portAudioDebugRedirector;
		PortAudioHandle portAudioHandle;

		const HostApi hostApi;
		const std::optional<Device> inputDevice;
		const std::optional<Device> outputDevice;
		const std::optional<WAVEFORMATEXTENSIBLE> inputFormat;
		const std::optional<WAVEFORMATEXTENSIBLE> outputFormat;
		const std::optional<SampleType> inputSampleType;
		const std::optional<SampleType> outputSampleType;

		ASIOSampleRate sampleRate = 0;
		bool sampleRateWasAccessed = false;

		std::optional<PreparedState> preparedState;
	};

}