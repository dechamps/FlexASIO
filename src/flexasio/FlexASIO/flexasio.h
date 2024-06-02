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
#include <mutex>
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
		void OutputReady();

		void ControlPanel();

	private:
		struct SampleType {
			ASIOSampleType asio;
			PaSampleFormat pa;
			size_t size;
			GUID waveSubFormat;
		};

		enum class StreamExclusivity { SHARED, EXCLUSIVE };

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
			PreparedState(FlexASIO& flexASIO, ASIOSampleRate sampleRate, ASIOBufferInfo* asioBufferInfos, long numChannels, long bufferSizeInFrames, ASIOCallbacks* callbacks);
			PreparedState(const PreparedState&) = delete;
			PreparedState(PreparedState&&) = delete;

			StreamExclusivity GetStreamExclusivity() const { return streamWithExclusivity.exclusivity;  }

			bool IsChannelActive(bool isInput, long channel) const;

			void GetLatencies(long* inputLatency, long* outputLatency);
			void Start();
			void Stop();

			void GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp);
			void OutputReady();

			void RequestReset();

		private:
			struct Buffers
			{
				Buffers(size_t bufferSetCount, size_t inputChannelCount, size_t outputChannelCount, size_t bufferSizeInFrames, size_t inputSampleSizeInBytes, size_t outputSampleSizeInBytes);
				~Buffers();
				std::byte* GetInputBuffer(size_t bufferSetIndex, size_t channelIndex) { return buffers.data() + bufferSetIndex * GetBufferSetSizeInBytes() + channelIndex * GetInputBufferSizeInBytes(); }
				std::byte* GetOutputBuffer(size_t bufferSetIndex, size_t channelIndex) { return GetInputBuffer(bufferSetIndex, inputChannelCount) + channelIndex * GetOutputBufferSizeInBytes(); }
				size_t GetBufferSetSizeInBytes() const { return buffers.size() / bufferSetCount; }
				size_t GetInputBufferSizeInBytes() const { if (buffers.empty()) return 0; return bufferSizeInFrames * inputSampleSizeInBytes; }
				size_t GetOutputBufferSizeInBytes() const { if (buffers.empty()) return 0; return bufferSizeInFrames * outputSampleSizeInBytes; }

				const size_t bufferSetCount;
				const size_t inputChannelCount;
				const size_t outputChannelCount;
				const size_t bufferSizeInFrames;
				const size_t inputSampleSizeInBytes;
				const size_t outputSampleSizeInBytes;

				// This is a giant buffer containing all ASIO buffers. It is organized as follows:
				// [ input channel 0 buffer 0 ] [ input channel 1 buffer 0 ] ... [ input channel N buffer 0 ] [ output channel 0 buffer 0 ] [ output channel 1 buffer 0 ] .. [ output channel N buffer 0 ]
				// [ input channel 0 buffer 1 ] [ input channel 1 buffer 1 ] ... [ input channel N buffer 1 ] [ output channel 0 buffer 1 ] [ output channel 1 buffer 1 ] .. [ output channel N buffer 1 ]
				// The reason why this is a giant blob is to slightly improve performance by (theroretically) improving memory locality.
				std::vector<std::byte> buffers;
			};

			class RunningState {
			public:
				RunningState(PreparedState& preparedState);
				~RunningState();

				// Note: the reason why this is not done in the constructor is to allow `PreparedState::Start()`
				// to properly set `PreparedState::runningState` before callbacks start flying. This is because
				// the ASIO host application may decide to call GetSamplePosition() or OutputReady() as soon
				// as bufferSwitch() is called without waiting for Start() to return - we don't want these calls
				// to race with `PreparedState::Start()` constructing `PreparedState::runningState`.
				void Start();

				void GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) const;
				void OutputReady();

				PaStreamCallbackResult StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags);

			private:
				enum class State { PRIMING, PRIMED, STEADYSTATE };

				struct SamplePosition {
					ASIOSamples samples = { 0 };
					ASIOTimeStamp timestamp = { 0 };
				};

				PreparedState& preparedState;
				const bool host_supports_timeinfo;
				enum class OutputReadyState { NOT_READY, READY, STOPPING };
				std::optional<std::atomic<OutputReadyState>> outputReadyState;
				State state = outputReadyState.has_value() ? State::PRIMING : State::PRIMED;
				// The index of the "unlocked" buffer (or "half-buffer", i.e. 0 or 1) that contains data not currently being processed by the ASIO host.
				long driverBufferIndex = state == State::PRIMING ? 1 : 0;
				std::atomic<SamplePosition> samplePosition;

				Win32HighResolutionTimer win32HighResolutionTimer;
				ActiveStream activeStream;
			};

			static int StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw();

			void OnConfigChange();

			FlexASIO& flexASIO;
			const ASIOSampleRate sampleRate;
			const ASIOCallbacks callbacks;

			// PortAudio buffer addresses are dynamic and are only valid for the duration of the stream callback.
			// In contrast, ASIO buffer addresses are static and are valid for as long as the stream is running.
			// Thus we need our own buffer on top of PortAudio's buffers. This doens't add any latency because buffers are copied immediately.
			Buffers buffers;
			const std::vector<ASIOBufferInfo> bufferInfos;

			struct StreamWithExclusivity final {
				Stream stream;
				StreamExclusivity exclusivity;
			};
			const StreamWithExclusivity streamWithExclusivity;

			std::optional<RunningState> runningState;
			ConfigLoader::Watcher configWatcher;
		};

		static const SampleType float32;
		static const SampleType int32;
		static const SampleType int24;
		static const SampleType int16;
		static const std::pair<std::string_view, SampleType> sampleTypes[];
		static SampleType ParseSampleType(std::string_view str);
		static SampleType WaveFormatToSampleType(const WAVEFORMATEXTENSIBLE& waveFormat);
		static SampleType SelectSampleType(PaHostApiTypeId hostApiTypeId, const Device& device, const Config::Stream& streamConfig);
		static std::string DescribeSampleType(const SampleType&);
		static DWORD SelectChannelMask(PaHostApiTypeId hostApiTypeId, const Device& device, const Config::Stream& streamConfig);

		int GetInputChannelCount() const;
		int GetOutputChannelCount() const;

		struct BufferSizes {
			long minimum;
			long maximum;
			long preferred;
			long granularity;
		};
		BufferSizes ComputeBufferSizes() const;

		long ComputeLatency(long latencyInFrames, bool output, size_t bufferSizeInFrames) const;
		long ComputeLatencyFromStream(PaStream* stream, bool output, size_t bufferSizeInFrames) const;

		template <typename Functor>
		decltype(auto) WithStreamParameters(bool inputEnabled, bool outputEnabled, double sampleRate, PaTime suggestedLatency, Functor functor) const;
		Stream OpenStream(const StreamParameters&, unsigned long framesPerBuffer, PaStreamCallback callback, void* callbackUserData) const;

		const HWND windowHandle = nullptr;
		const ConfigLoader configLoader;
		const Config& config = configLoader.Initial();

		PortAudioDebugRedirector portAudioDebugRedirector;
		PortAudioHandle portAudioHandle;

		const HostApi hostApi;
		const std::optional<Device> inputDevice;
		const std::optional<Device> outputDevice;
		const std::optional<SampleType> inputSampleType;
		const std::optional<SampleType> outputSampleType;
		const DWORD inputChannelMask;
		const DWORD outputChannelMask;

		ASIOSampleRate sampleRate = 0;
		bool sampleRateWasAccessed = false;
		bool hostSupportsOutputReady = false;

		std::optional<PreparedState> preparedState;
	};

}