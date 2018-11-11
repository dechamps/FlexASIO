#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <functional>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>
#include <cassert>
#include <sstream>

#include "..\ASIOSDK2.3.1\host\ginclude.h"
#include "..\ASIOSDK2.3.1\common\asio.h"
#include "..\FlexASIO\cflexasio.h"
#include "..\FlexASIOUtil\asio.h"
#include "..\FlexASIOUtil\find.h"
#include "..\FlexASIOUtil\string.h"

// The global ASIO driver pointer that the ASIO host library internally uses.
extern IASIO* theAsioDriver;

namespace flexasio {
	namespace {

		template <typename FunctionPointer> struct function_pointer_traits;
		template <typename ReturnValue, typename... Args> struct function_pointer_traits<ReturnValue(*)(Args...)> {
			using function = std::function<ReturnValue(Args...)>;
		};

		std::string GetASIOSampleTypeString(ASIOSampleType sampleType) {
			return EnumToString(sampleType, {
				{ASIOSTInt16MSB, "ASIOSTInt16MSB"},
				{ASIOSTInt24MSB, "ASIOSTInt24MSB"},
				{ASIOSTInt32MSB, "ASIOSTInt32MSB"},
				{ASIOSTFloat32MSB, "ASIOSTFloat32MSB"},
				{ASIOSTFloat64MSB, "ASIOSTFloat64MSB"},
				{ASIOSTInt32MSB16, "ASIOSTInt32MSB16"},
				{ASIOSTInt32MSB18, "ASIOSTInt32MSB18"},
				{ASIOSTInt32MSB20, "ASIOSTInt32MSB20"},
				{ASIOSTInt32MSB24, "ASIOSTInt32MSB24"},
				{ASIOSTInt16LSB, "ASIOSTInt16LSB"},
				{ASIOSTInt24LSB, "ASIOSTInt24LSB"},
				{ASIOSTInt32LSB, "ASIOSTInt32LSB"},
				{ASIOSTFloat32LSB, "ASIOSTFloat32LSB"},
				{ASIOSTFloat64LSB, "ASIOSTFloat64LSB"},
				{ASIOSTInt32LSB16, "ASIOSTInt32LSB16"},
				{ASIOSTInt32LSB18, "ASIOSTInt32LSB18"},
				{ASIOSTInt32LSB20, "ASIOSTInt32LSB20"},
				{ASIOSTInt32LSB24, "ASIOSTInt32LSB24"},
				{ASIOSTDSDInt8LSB1, "ASIOSTDSDInt8LSB1"},
				{ASIOSTDSDInt8MSB1, "ASIOSTDSDInt8MSB1"},
				{ASIOSTDSDInt8NER8, "ASIOSTDSDInt8NER8"},
				});
		}

		std::string GetASIOMessageSelectorString(long selector) {
			return EnumToString(selector, {
				{kAsioSelectorSupported, "kAsioSelectorSupported"},
				{kAsioEngineVersion, "kAsioEngineVersion"},
				{kAsioResetRequest, "kAsioResetRequest"},
				{kAsioBufferSizeChange, "kAsioBufferSizeChange"},
				{kAsioResyncRequest, "kAsioResyncRequest"},
				{kAsioLatenciesChanged, "kAsioLatenciesChanged"},
				{kAsioSupportsTimeInfo, "kAsioSupportsTimeInfo"},
				{kAsioSupportsTimeCode, "kAsioSupportsTimeCode"},
				{kAsioMMCCommand, "kAsioMMCCommand"},
				{kAsioSupportsInputMonitor, "kAsioSupportsInputMonitor"},
				{kAsioSupportsInputGain, "kAsioSupportsInputGain"},
				{kAsioSupportsInputMeter, "kAsioSupportsInputMeter"},
				{kAsioSupportsOutputGain, "kAsioSupportsOutputGain"},
				{kAsioSupportsOutputMeter, "kAsioSupportsOutputMeter"},
				{kAsioOverload, "kAsioOverload"},
				});
		}

		ASIOError PrintError(ASIOError error) {
			std::cout << "-> " << GetASIOErrorString(error) << std::endl;
			return error;
		}

		std::optional<ASIODriverInfo> Init() {
			ASIODriverInfo asioDriverInfo = { 0 };
			asioDriverInfo.asioVersion = 2;
			std::cout << "ASIOInit(asioVersion = " << asioDriverInfo.asioVersion << ")" << std::endl;
			const auto initError = PrintError(ASIOInit(&asioDriverInfo));
			std::cout << "asioVersion = " << asioDriverInfo.asioVersion << " driverVersion = " << asioDriverInfo.asioVersion << " name = " << asioDriverInfo.name << " errorMessage = " << asioDriverInfo.errorMessage << " sysRef = " << asioDriverInfo.sysRef << std::endl;
			if (initError != ASE_OK) return std::nullopt;
			return asioDriverInfo;
		}

		std::pair<long, long> GetChannels() {
			std::cout << "ASIOGetChannels()" << std::endl;
			long numInputChannels, numOutputChannels;
			const auto error = PrintError(ASIOGetChannels(&numInputChannels, &numOutputChannels));
			if (error != ASE_OK) return { 0, 0 };
			std::cout << "Channel count: " << numInputChannels << " input, " << numOutputChannels << " output" << std::endl;
			return { numInputChannels, numOutputChannels };
		}

		struct BufferSize {
			long min = LONG_MIN;
			long max = LONG_MIN;
			long preferred = LONG_MIN;
			long granularity = LONG_MIN;
		};

		std::optional<BufferSize> GetBufferSize() {
			std::cout << "ASIOGetBufferSize()" << std::endl;
			BufferSize bufferSize;
			const auto error = PrintError(ASIOGetBufferSize(&bufferSize.min, &bufferSize.max, &bufferSize.preferred, &bufferSize.granularity));
			if (error != ASE_OK) return std::nullopt;
			std::cout << "Buffer size: min " << bufferSize.min << " max " << bufferSize.max << " preferred " << bufferSize.preferred << " granularity " << bufferSize.granularity << std::endl;
			return bufferSize;
		}

		std::optional<ASIOSampleRate> GetSampleRate() {
			std::cout << "ASIOGetSampleRate()" << std::endl;
			ASIOSampleRate sampleRate = NAN;
			const auto error = PrintError(ASIOGetSampleRate(&sampleRate));
			if (error != ASE_OK) return std::nullopt;
			std::cout << "Sample rate: " << sampleRate << std::endl;
			return sampleRate;
		}

		bool CanSampleRate(ASIOSampleRate sampleRate) {
			std::cout << "ASIOCanSampleRate(" << sampleRate << ")" << std::endl;
			return PrintError(ASIOCanSampleRate(sampleRate)) == ASE_OK;
		}

		bool SetSampleRate(ASIOSampleRate sampleRate) {
			std::cout << "ASIOSetSampleRate(" << sampleRate << ")" << std::endl;
			return PrintError(ASIOSetSampleRate(sampleRate)) == ASE_OK;
		}

		bool OutputReady() {
			std::cout << "ASIOOutputReady()" << std::endl;
			return PrintError(ASIOOutputReady()) == ASE_OK;
		}

		std::optional<ASIOChannelInfo> GetChannelInfo(long channel, ASIOBool isInput) {
			std::cout << "ASIOGetChannelInfo(channel = " << channel << " isInput = " << isInput << ")" << std::endl;
			ASIOChannelInfo channelInfo;
			channelInfo.channel = channel;
			channelInfo.isInput = isInput;
			if (PrintError(ASIOGetChannelInfo(&channelInfo)) != ASE_OK) return std::nullopt;
			std::cout << "isActive = " << channelInfo.isActive << " channelGroup = " << channelInfo.channelGroup << " type = " << GetASIOSampleTypeString(channelInfo.type) << " name = " << channelInfo.name << std::endl;
			return channelInfo;
		}

		void GetAllChannelInfo(std::pair<long, long> ioChannelCounts) {
			for (long inputChannel = 0; inputChannel < ioChannelCounts.first; ++inputChannel) GetChannelInfo(inputChannel, true);
			for (long outputChannel = 0; outputChannel < ioChannelCounts.second; ++outputChannel) GetChannelInfo(outputChannel, false);
		}

		struct Buffers {
			Buffers() = default;
			explicit Buffers(std::vector<ASIOBufferInfo> info) : info(std::move(info)) {}
			Buffers(const Buffers&) = delete;
			Buffers(Buffers&&) = default;
			~Buffers() {
				if (info.size() == 0) return;
				std::cout << std::endl;
				std::cout << "ASIODisposeBuffers()" << std::endl;
				PrintError(ASIODisposeBuffers());
			}

			std::vector<ASIOBufferInfo> info;
		};

		// TODO: we should also test with not all channels active.
		Buffers CreateBuffers(std::pair<long, long> ioChannelCounts, long bufferSize, ASIOCallbacks callbacks) {
			std::vector<ASIOBufferInfo> bufferInfos;
			for (long inputChannel = 0; inputChannel < ioChannelCounts.first; ++inputChannel) {
				auto& bufferInfo = bufferInfos.emplace_back();
				bufferInfo.isInput = true;
				bufferInfo.channelNum = inputChannel;
			}
			for (long outputChannel = 0; outputChannel < ioChannelCounts.second; ++outputChannel) {
				auto& bufferInfo = bufferInfos.emplace_back();
				bufferInfo.isInput = false;
				bufferInfo.channelNum = outputChannel;
			}

			std::cout << "ASIOCreateBuffers(";
			for (const auto& bufferInfo : bufferInfos) {
				std::cout << "isInput = " << bufferInfo.isInput << " channelNum = " << bufferInfo.channelNum << " ";
			}
			std::cout << ", bufferSize = " << bufferSize << ", bufferSwitch = " << (void*)(callbacks.bufferSwitch) << " sampleRateDidChange = " << (void*)(callbacks.sampleRateDidChange) << " asioMessage = " << (void*)(callbacks.asioMessage) << " bufferSwitchTimeInfo = " << (void*)(callbacks.bufferSwitchTimeInfo) << ")" << std::endl;

			if (PrintError(ASIOCreateBuffers(bufferInfos.data(), long(bufferInfos.size()), bufferSize, &callbacks)) != ASE_OK) return {};
			return Buffers(bufferInfos);
		}

		void GetLatencies() {
			long inputLatency = LONG_MIN, outputLatency = LONG_MIN;
			std::cout << "ASIOGetLatencies()" << std::endl;
			if (PrintError(ASIOGetLatencies(&inputLatency, &outputLatency)) != ASE_OK) return;
			std::cout << "Latencies: input " << inputLatency << " samples, output " << outputLatency << " samples" << std::endl;
		}

		bool Start() {
			std::cout << "ASIOStart()" << std::endl;
			return PrintError(ASIOStart()) == ASE_OK;
		}

		bool Stop() {
			std::cout << "ASIOStop()" << std::endl;
			return PrintError(ASIOStop()) == ASE_OK;
		}

		void GetSamplePosition() {
			std::cout << "ASIOGetSamplePosition()" << std::endl;
			ASIOSamples samples;
			ASIOTimeStamp timeStamp;
			if (PrintError(ASIOGetSamplePosition(&samples, &timeStamp)) != ASE_OK) return;
			std::cout << "Sample position: " << ASIOToInt64(samples) << " timestamp: " << ASIOToInt64(timeStamp) << std::endl;
		}

		using ASIOMessageHandler = decltype(ASIOCallbacks::asioMessage);

		long HandleSelectorSupportedMessage(long, long value, void*, double*);

		long HandleSupportsTimeInfoMessage(long, long, void*, double*) { return 1; }

		constexpr std::pair<long, ASIOMessageHandler> message_selector_handlers[] = {
				{kAsioSelectorSupported, HandleSelectorSupportedMessage},
				{kAsioSupportsTimeInfo, HandleSupportsTimeInfoMessage},
		};

		long HandleSelectorSupportedMessage(long, long value, void*, double*) {
			std::cout << "Being queried for message selector " << GetASIOMessageSelectorString(value) << std::endl;
			return Find(value, message_selector_handlers).has_value() ? 1 : 0;
		}

		long HandleASIOMessage(long selector, long value, void* message, double* opt) {
			const auto handler = Find(selector, message_selector_handlers);
			if (!handler.has_value()) return 0;
			return (*handler)(selector, value, message, opt);
		}

		// Allows the use of capturing lambdas for ASIO callbacks, even though ASIO doesn't provide any mechanism to pass user context to callbacks.
		// This works by assuming that we will only use one set of callbacks at a time, such that we can use global state as a side channel.
		struct Callbacks {
			Callbacks() {
				assert(global == nullptr);
				global = this;
			}
			~Callbacks() {
				assert(global == this);
				global = nullptr;
			}

			function_pointer_traits<decltype(ASIOCallbacks::bufferSwitch)>::function bufferSwitch;
			function_pointer_traits<decltype(ASIOCallbacks::sampleRateDidChange)>::function sampleRateDidChange;
			function_pointer_traits<decltype(ASIOCallbacks::asioMessage)>::function asioMessage;
			function_pointer_traits<decltype(ASIOCallbacks::bufferSwitchTimeInfo)>::function bufferSwitchTimeInfo;

			ASIOCallbacks GetASIOCallbacks() const {
				ASIOCallbacks callbacks;
				callbacks.bufferSwitch = GetASIOCallback<&Callbacks::bufferSwitch>();
				callbacks.sampleRateDidChange = GetASIOCallback<&Callbacks::sampleRateDidChange>();
				callbacks.asioMessage = GetASIOCallback<&Callbacks::asioMessage>();
				callbacks.bufferSwitchTimeInfo = GetASIOCallback<&Callbacks::bufferSwitchTimeInfo>();
				return callbacks;
			}

		private:
			template <auto memberFunction> auto GetASIOCallback() const {
				return [](auto... args) {
					assert(global != nullptr);
					return (global->*memberFunction)(args...);
				};
			}

			static Callbacks* global;
		};

		Callbacks* Callbacks::global = nullptr;

		bool Run() {
			if (!Init()) return false;

			std::cout << std::endl;

			const auto ioChannelCounts = GetChannels();
			if (ioChannelCounts.first == 0 && ioChannelCounts.second == 0) return false;

			std::cout << std::endl;

			const auto bufferSize = GetBufferSize();
			if (!bufferSize.has_value()) return false;

			std::cout << std::endl;

			GetSampleRate();

			std::cout << std::endl;

			for (const auto sampleRate : { 44100, 96000, 192000, 48000 }) {
				if (!(CanSampleRate(sampleRate) && SetSampleRate(sampleRate) && GetSampleRate() == sampleRate) && sampleRate == 48000) return false;
			}

			std::cout << std::endl;

			OutputReady();

			std::cout << std::endl;

			GetAllChannelInfo(ioChannelCounts);

			std::cout << std::endl;

			std::mutex bufferSwitchCountMutex;
			std::condition_variable bufferSwitchCountCondition;
			size_t bufferSwitchCount = 0;
			const auto incrementBufferSwitchCount = [&] {
				{
					std::scoped_lock bufferSwitchCountLock(bufferSwitchCountMutex);
					++bufferSwitchCount;
				}
				bufferSwitchCountCondition.notify_all();
			};

			Callbacks callbacks;
			callbacks.bufferSwitch = [&](long doubleBufferIndex, ASIOBool directProcess) {
				std::cout << "bufferSwitch(doubleBufferIndex = " << doubleBufferIndex << ", directProcess = " << directProcess << ")" << std::endl;
				GetSamplePosition();
				std::cout << "<-" << std::endl;
				incrementBufferSwitchCount();
			};
			callbacks.sampleRateDidChange = [&](ASIOSampleRate sampleRate) {
				std::cout << "sampleRateDidChange(" << sampleRate << ")" << std::endl;
				std::cout << "<-" << std::endl;
			};
			callbacks.asioMessage = [&](long selector, long value, void* message, double* opt) {
				std::cout << "asioMessage(selector = " << GetASIOMessageSelectorString(selector) << ", value = " << value << ", message = " << message << ", opt = " << opt << ")" << std::endl;
				const auto result = HandleASIOMessage(selector, value, message, opt);
				std::cout << "<- " << result << std::endl;
				return result;
			};
			callbacks.bufferSwitchTimeInfo = [&](ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess) {
				std::cout << "bufferSwitchTimeInfo(params = (" << (params == nullptr ? "none" : DescribeASIOTime(*params)) << "), doubleBufferIndex = " << doubleBufferIndex << ", directProcess = " << directProcess << ")" << std::endl;
				GetSamplePosition();
				std::cout << "<- nullptr" << std::endl;
				incrementBufferSwitchCount();
				return nullptr;
			};

			const auto buffers = CreateBuffers(ioChannelCounts, bufferSize->preferred, callbacks.GetASIOCallbacks());
			if (buffers.info.size() == 0) return false;

			std::cout << std::endl;

			GetSampleRate();
			GetAllChannelInfo(ioChannelCounts);

			std::cout << std::endl;

			GetLatencies();

			std::cout << std::endl;

			if (!Start()) return false;

			std::cout << std::endl;

			constexpr size_t bufferSwitchCountThreshold = 10;
			std::cout << "Now waiting for " << bufferSwitchCountThreshold << " buffer switches..." << std::endl;
			std::cout << std::endl;

			{
				std::unique_lock bufferSwitchCountLock(bufferSwitchCountMutex);
				bufferSwitchCountCondition.wait(bufferSwitchCountLock, [&] { return bufferSwitchCount >= bufferSwitchCountThreshold;  });
			}

			std::cout << std::endl;
			std::cout << "Reached " << bufferSwitchCountThreshold << " buffer switches, stopping" << std::endl;

			if (!Stop()) return false;

			// Note: we don't call ASIOExit() because it gets confused by our driver setup trickery (see InitAndRun()).
			// That said, this doesn't really matter because ASIOExit() is basically a no-op in our case, anyway.
			return true;
		}

		bool InitAndRun() {
			// This basically does an end run around the ASIO host library driver loading system, simulating what loadAsioDriver() does.
			// This allows us to trick the ASIO host library into using a specific instance of an ASIO driver (the one this program is linked against),
			// as opposed to whatever ASIO driver might be currently installed on the system.
			theAsioDriver = CreateFlexASIO();

			const bool result = Run();

			// There are cases in which the ASIO host library will nullify the driver pointer.
			// For example, it does that if the driver fails to initialize.
			// (Sadly the ASIO host library won't call Release() in that case, because memory leaks are fun!)
			if (theAsioDriver != nullptr) {
				ReleaseFlexASIO(theAsioDriver);
				theAsioDriver = nullptr;
			}

			return result;
		}

	}
}

int main(int, char**) {
	if (!::flexasio::InitAndRun()) return 1;
	return 0;
}