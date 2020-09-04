#include "flexasio.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <string_view>
#include <vector>

#include <MMReg.h>

#include <dechamps_cpputil/endian.h>
#include <dechamps_cpputil/string.h>

#include <dechamps_ASIOUtil/asio.h>

#include <dechamps_CMakeUtils/version.h>

#include "portaudio.h"
#include "pa_win_wasapi.h"

#include "log.h"

namespace flexasio {

	FlexASIO::PortAudioHandle::PortAudioHandle() {
		Log() << "Initializing PortAudio";
		PaError error = Pa_Initialize();
		if (error != paNoError)
			throw ASIOException(ASE_HWMalfunction, std::string("could not initialize PortAudio: ") + Pa_GetErrorText(error));
		Log() << "PortAudio initialization successful";
	}
	FlexASIO::PortAudioHandle::~PortAudioHandle() {
		Log() << "Terminating PortAudio";
		PaError error = Pa_Terminate();
		if (error != paNoError)
			Log() << "PortAudio termination failed with " << Pa_GetErrorText(error);
		else
			Log() << "PortAudio terminated successfully";
	}

	FlexASIO::Win32HighResolutionTimer::Win32HighResolutionTimer() {
		Log() << "Starting high resolution timer";
		timeBeginPeriod(1);
	}
	FlexASIO::Win32HighResolutionTimer::~Win32HighResolutionTimer() {
		Log() << "Stopping high resolution timer";
		timeEndPeriod(1);
	}

	DWORD FlexASIO::Win32HighResolutionTimer::GetTimeMilliseconds() const { return timeGetTime(); }

	namespace {

		std::optional<ASIOSampleRate> previousSampleRate;

		void LogPortAudioApiList() {
			const auto pa_api_count = Pa_GetHostApiCount();
			for (PaHostApiIndex pa_api_index = 0; pa_api_index < pa_api_count; ++pa_api_index) {
				Log() << "Found backend: " << HostApi(pa_api_index);
			}
		}
		void LogPortAudioDeviceList() {
			const auto deviceCount = Pa_GetDeviceCount();
			for (PaDeviceIndex deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
				Log() << "Found device: " << Device(deviceIndex);
			}
		}

		HostApi SelectDefaultHostApi() {
			Log() << "Selecting default PortAudio host API";
			// The default API used by PortAudio is MME.
			// It works, but DirectSound seems like the best default (it reports a more sensible number of channels, for example).
			// So let's try that first, and fall back to whatever the PortAudio default is if DirectSound is not available somehow.
			auto hostApiIndex = Pa_HostApiTypeIdToHostApiIndex(paDirectSound);
			if (hostApiIndex == paHostApiNotFound)
				hostApiIndex = Pa_GetDefaultHostApi();
			if (hostApiIndex < 0)
				throw std::runtime_error("Unable to get default PortAudio host API");
			return HostApi(hostApiIndex);
		}

		HostApi SelectHostApiByName(std::string_view name) {
			Log() << "Searching for a PortAudio host API named '" << name << "'";
			const auto hostApiCount = Pa_GetHostApiCount();

			for (PaHostApiIndex hostApiIndex = 0; hostApiIndex < hostApiCount; ++hostApiIndex) {
				const HostApi hostApi(hostApiIndex);
				// TODO: the comparison should be case insensitive.
				if (hostApi.info.name == name) return hostApi;
			}
			throw std::runtime_error(std::string("PortAudio host API '") + std::string(name) + "' not found");
		}

		Device SelectDeviceByName(const PaHostApiIndex hostApiIndex, const std::string_view name, const int minimumInputChannelCount, const int minimumOutputChannelCount) {
			Log() << "Searching for a PortAudio device named '" << name << "' with host API index " << hostApiIndex;
			const auto deviceCount = Pa_GetDeviceCount();

			for (PaDeviceIndex deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
				const Device device(deviceIndex);
				if (device.info.hostApi == hostApiIndex && device.info.name == name && device.info.maxInputChannels >= minimumInputChannelCount && device.info.maxOutputChannels >= minimumOutputChannelCount) return device;
			}
			throw std::runtime_error(std::string("PortAudio device '") + std::string(name) + "' not found within specified backend (minimum channel count: " + std::to_string(minimumInputChannelCount) + " input, " + std::to_string(minimumOutputChannelCount) + " output)");
		}

		std::optional<Device> SelectDevice(const PaHostApiIndex hostApiIndex, const PaDeviceIndex defaultDeviceIndex, std::optional<std::string_view> name, const int minimumInputChannelCount, const int minimumOutputChannelCount) {
			if (!name.has_value()) {
				if (defaultDeviceIndex == paNoDevice) {
					Log() << "No default device";
					return std::nullopt;
				}
				Log() << "Using default device with index " << defaultDeviceIndex;
				const Device device(defaultDeviceIndex);
				if (device.info.maxInputChannels < minimumInputChannelCount || device.info.maxOutputChannels < minimumOutputChannelCount) {
					Log() << "Cannot use default device " << device << " because we need at least " << minimumInputChannelCount << " input channels and " << minimumOutputChannelCount << " output channels";
					return std::nullopt;
				}
				return Device(defaultDeviceIndex);
			}
			if (name->empty()) {
				Log() << "Device explicitly disabled in configuration";
				return std::nullopt;
			}

			return SelectDeviceByName(hostApiIndex, *name, minimumInputChannelCount, minimumOutputChannelCount);
		}

		std::string GetPaStreamCallbackResultString(PaStreamCallbackResult result) {
			return ::dechamps_cpputil::EnumToString(result, {
				{paContinue, "paContinue"},
				{paComplete, "paComplete"},
				{paAbort, "paAbort"},
				});
		}

		std::optional<WAVEFORMATEXTENSIBLE> GetDeviceDefaultFormat(PaHostApiTypeId hostApiType, PaDeviceIndex deviceIndex) {
			if (hostApiType != paWASAPI) return std::nullopt;
			try {
				Log() << "Getting WASAPI device default format for device index " << deviceIndex;
				const auto format = GetWasapiDeviceDefaultFormat(deviceIndex);
				Log() << "WASAPI device default format for device index " << deviceIndex << ": " << DescribeWaveFormat(format);
				return format;
			}
			catch (const std::exception& exception) {
				Log() << "Error while trying to get input WASAPI default format for device index " << deviceIndex << ": " << exception.what();
				return std::nullopt;
			}
		}

		ASIOSampleRate GetDefaultSampleRate(const std::optional<Device>& inputDevice, const std::optional<Device>& outputDevice) {
			if (previousSampleRate.has_value()) {
				// Work around a REW bug. See https://github.com/dechamps/FlexASIO/issues/31
				// Another way of doing this would have been to only pick this sample rate if the application
				// didn't enquire about sample rate at createBuffers() time, but that doesn't work as well because
				// the default buffer size would be wrong.
				Log() << "Using default sample rate " << *previousSampleRate << " Hz from a previous instance of the driver";
				return *previousSampleRate;
			}

			ASIOSampleRate sampleRate = 0;
			if (inputDevice.has_value()) {
				sampleRate = (std::max)(sampleRate, inputDevice->info.defaultSampleRate);
			}
			if (outputDevice.has_value()) {
				sampleRate = (std::max)(sampleRate, outputDevice->info.defaultSampleRate);
			}
			if (sampleRate == 0) sampleRate = 44100;
			Log() << "Default sample rate: " << sampleRate;
			return sampleRate;
		}

		long Message(decltype(ASIOCallbacks::asioMessage) asioMessage, long selector, long value, void* message, double* opt) {
			Log() << "Sending message: selector = " << ::dechamps_ASIOUtil::GetASIOMessageSelectorString(selector) << ", value = " << value << ", message = " << message << ", opt = " << opt;
			const auto result = asioMessage(selector, value, message, opt);
			Log() << "Result: " << result;
			return result;
		}

		// This is purely for instrumentation - it makes it possible to see host capabilities in the log.
		// Such information could be used to inform future development (there's no point in supporting more ASIO features if host applications don't support them).
		void ProbeHostMessages(decltype(ASIOCallbacks::asioMessage) asioMessage) {
			for (const auto selector : {
				kAsioSelectorSupported, kAsioEngineVersion, kAsioResetRequest, kAsioBufferSizeChange,
				kAsioResyncRequest, kAsioLatenciesChanged, kAsioSupportsTimeInfo, kAsioSupportsTimeCode,
				kAsioMMCCommand, kAsioSupportsInputMonitor, kAsioSupportsInputGain, kAsioSupportsInputMeter,
				kAsioSupportsOutputGain, kAsioSupportsOutputMeter, kAsioOverload }) {
				Log() << "Probing for message selector: " << ::dechamps_ASIOUtil::GetASIOMessageSelectorString(selector);
				if (Message(asioMessage, kAsioSelectorSupported, selector, nullptr, nullptr) != 1) continue;

				switch (selector) {
				case kAsioEngineVersion:
					Message(asioMessage, kAsioEngineVersion, 0, nullptr, nullptr);
					break;
				}
			}
		}

		// No-op PortAudio stream callback. Useful for backends that fail to initialize without a callback, such as WDM-KS.
		int NoOpStreamCallback(const void *, void *, unsigned long, const PaStreamCallbackTimeInfo *, PaStreamCallbackFlags, void *) throw() {
			Log() << "In no-op stream callback";
			return paContinue;
		}

		long GetBufferInfosChannelCount(const ASIOBufferInfo* asioBufferInfos, const long numChannels, const bool input) {
			long result = 0;
			for (long channelIndex = 0; channelIndex < numChannels; ++channelIndex)
				if (!asioBufferInfos[channelIndex].isInput == !input)
					++result;
			return result;
		}

		void CopyFromPortAudioBuffers(const std::vector<ASIOBufferInfo>& bufferInfos, const long doubleBufferIndex, const void* const* portAudioBuffers, const size_t bufferSizeInBytes) {
			for (const auto& bufferInfo : bufferInfos)
			{
				if (!bufferInfo.isInput) continue;
				void* asioBuffer = bufferInfo.buffers[doubleBufferIndex];
				memcpy(asioBuffer, portAudioBuffers[bufferInfo.channelNum], bufferSizeInBytes);
			}
		}
		void CopyToPortAudioBuffers(const std::vector<ASIOBufferInfo>& bufferInfos, const long doubleBufferIndex, void* const* portAudioBuffers, const size_t bufferSizeInBytes) {
			for (const auto& bufferInfo : bufferInfos)
			{
				if (bufferInfo.isInput) continue;
				void* asioBuffer = bufferInfo.buffers[doubleBufferIndex];
				memcpy(portAudioBuffers[bufferInfo.channelNum], asioBuffer, bufferSizeInBytes);
			}
		}

		template <typename Enum> void IncrementEnum(Enum& value) {
			value = static_cast<Enum>(std::underlying_type_t<Enum>(value) + 1);
		}

	}

	constexpr FlexASIO::SampleType FlexASIO::float32 = { ::dechamps_cpputil::endianness == ::dechamps_cpputil::Endianness::LITTLE ? ASIOSTFloat32LSB : ASIOSTFloat32MSB, paFloat32, 4 };
	constexpr FlexASIO::SampleType FlexASIO::int32 = { ::dechamps_cpputil::endianness == ::dechamps_cpputil::Endianness::LITTLE ? ASIOSTInt32LSB : ASIOSTInt32MSB, paInt32, 4 };
	constexpr FlexASIO::SampleType FlexASIO::int24 = { ::dechamps_cpputil::endianness == ::dechamps_cpputil::Endianness::LITTLE ? ASIOSTInt24LSB : ASIOSTInt24MSB, paInt24, 3 };
	constexpr FlexASIO::SampleType FlexASIO::int16 = { ::dechamps_cpputil::endianness == ::dechamps_cpputil::Endianness::LITTLE ? ASIOSTInt16LSB : ASIOSTInt16MSB, paInt16, 2 };
	constexpr std::pair<std::string_view, FlexASIO::SampleType> FlexASIO::sampleTypes[] = {
			{"Float32", float32},
			{"Int32", int32},
			{"Int24", int24},
			{"Int16", int16},
	};

	FlexASIO::SampleType FlexASIO::ParseSampleType(const std::string_view str) {
		const auto sampleType = ::dechamps_cpputil::Find(str, sampleTypes);
		if (!sampleType.has_value()) {
			throw std::runtime_error(std::string("Invalid '") + std::string(str) + "' sample type - valid values are " + ::dechamps_cpputil::Join(sampleTypes, ", ", [](const auto& item) { return std::string("'") + std::string(item.first) + "'"; }));
		}
		return *sampleType;
	}

	std::optional<FlexASIO::SampleType> FlexASIO::WaveFormatToSampleType(const WAVEFORMATEXTENSIBLE& waveFormat) {
		if (waveFormat.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) return float32;
		if (waveFormat.SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
			for (const auto& sampleType : sampleTypes) {
				const auto bits = sampleType.second.size * 8;
				if (bits == waveFormat.Samples.wValidBitsPerSample) return sampleType.second;
				if (bits == waveFormat.Format.wBitsPerSample) return sampleType.second;
			}
		}
		return std::nullopt;
	}

	FlexASIO::SampleType FlexASIO::SelectSampleType(const Config::Stream& streamConfig, const PaHostApiTypeId hostApiTypeId, const std::optional<WAVEFORMATEXTENSIBLE>& deviceFormat) {
		if (streamConfig.sampleType.has_value()) {
			Log() << "Selecting sample type from configuration";
			return ParseSampleType(*streamConfig.sampleType);
		}
		if (hostApiTypeId == paWASAPI && streamConfig.wasapiExclusiveMode) {
			Log() << "WASAPI Exclusive mode detected";
			if (deviceFormat.has_value()) {
				Log() << "Selecting sample type from device format";
				const auto sampleType = WaveFormatToSampleType(*deviceFormat);
				if (sampleType.has_value()) return *sampleType;
			}
			Log() << "Unable to select sample type from device format, falling back";
		}
		Log() << "Selecting default sample type";
		return float32;
	}

	std::string FlexASIO::DescribeSampleType(const SampleType& sampleType) {
		return "ASIO " + ::dechamps_ASIOUtil::GetASIOSampleTypeString(sampleType.asio) + ", PortAudio " + GetSampleFormatString(sampleType.pa) + ", size " + std::to_string(sampleType.size);
	}

	FlexASIO::FlexASIO(void* sysHandle) :
		windowHandle(reinterpret_cast<decltype(windowHandle)>(sysHandle)),
		config(LoadConfig()),
	portAudioDebugRedirector([](std::string_view str) { if (IsLoggingEnabled()) Log() << "[PortAudio] " << str; }),
	hostApi([&] {
		LogPortAudioApiList();
		auto hostApi = config.backend.has_value() ? SelectHostApiByName(*config.backend) : SelectDefaultHostApi();
		Log() << "Selected backend: " << hostApi;
		LogPortAudioDeviceList();
		return hostApi;
	}()),
		inputDevice([&] {
		Log() << "Selecting input device";
		auto device = SelectDevice(hostApi.index, hostApi.info.defaultInputDevice, config.input.device, 1, 0);
		if (device.has_value()) Log() << "Selected input device: " << *device;
		else Log() << "No input device, proceeding without input";
		return device;
	}()),
		outputDevice([&] {
		Log() << "Selecting output device";
		auto device = SelectDevice(hostApi.index, hostApi.info.defaultOutputDevice, config.output.device, 0, 1);
		if (device.has_value()) Log() << "Selected output device: " << *device;
		else Log() << "No output device, proceeding without output";
		return device;
	}()),
		inputFormat(inputDevice.has_value() ? GetDeviceDefaultFormat(hostApi.info.type, inputDevice->index) : std::nullopt),
		outputFormat(outputDevice.has_value() ? GetDeviceDefaultFormat(hostApi.info.type, outputDevice->index) : std::nullopt),
		inputSampleType([&]() -> std::optional<SampleType> {
		if (!inputDevice.has_value()) return std::nullopt;
		try {
			Log() << "Selecting input sample type";
			const auto sampleType = SelectSampleType(config.input, hostApi.info.type, inputFormat);
			Log() << "Selected input sample type: " << DescribeSampleType(sampleType);
			return sampleType;
		}
		catch (const std::exception& exception) {
			throw std::runtime_error(std::string("Could not select input sample type: ") + exception.what());
		}
	}()),
		outputSampleType([&]() -> std::optional<SampleType> {
		if (!outputDevice.has_value()) return std::nullopt;
		try {
			Log() << "Selecting output sample type";
			const auto sampleType = SelectSampleType(config.output, hostApi.info.type, outputFormat);
			Log() << "Selected output sample type: " << DescribeSampleType(sampleType);
			return sampleType;
		}
		catch (const std::exception& exception) {
			throw std::runtime_error(std::string("Could not select output sample type: ") + exception.what());
		}
	}()),
		sampleRate(GetDefaultSampleRate(inputDevice, outputDevice))
	{
		Log() << "sysHandle = " << sysHandle;

		if (!inputDevice.has_value() && !outputDevice.has_value()) throw ASIOException(ASE_HWMalfunction, "No usable input nor output devices");

		Log() << "Input channel count: " << GetInputChannelCount() << " mask: " << GetWaveFormatChannelMaskString(GetInputChannelMask());
		if (inputDevice.has_value() && GetInputChannelCount() > inputDevice->info.maxInputChannels)
			Log() << "WARNING: input channel count is higher than the max channel count for this device. Input device initialization might fail.";

		Log() << "Output channel count: " << GetOutputChannelCount() << " mask: " << GetWaveFormatChannelMaskString(GetOutputChannelMask());
		if (outputDevice.has_value() && GetOutputChannelCount() > outputDevice->info.maxOutputChannels)
			Log() << "WARNING: output channel count is higher than the max channel count for this device. Output device initialization might fail.";
	}

	int FlexASIO::GetInputChannelCount() const {
		if (!inputDevice.has_value()) return 0;
		if (config.input.channels.has_value()) return *config.input.channels;
		return inputDevice->info.maxInputChannels;
	}
	int FlexASIO::GetOutputChannelCount() const {
		if (!outputDevice.has_value()) return 0;
		if (config.output.channels.has_value()) return *config.output.channels;
		return outputDevice->info.maxOutputChannels;
	}

	DWORD FlexASIO::GetInputChannelMask() const {
		if (!inputFormat.has_value()) return 0;
		if (config.input.channels.has_value()) return 0;
		return inputFormat->dwChannelMask;
	}
	DWORD FlexASIO::GetOutputChannelMask() const {
		if (!outputFormat.has_value()) return 0;
		if (config.output.channels.has_value()) return 0;
		return outputFormat->dwChannelMask;
	}

	void FlexASIO::GetBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity)
	{
		if (config.bufferSizeSamples.has_value()) {
			Log() << "Using buffer size " << *config.bufferSizeSamples << " from configuration";
			*minSize = *maxSize = *preferredSize = long(*config.bufferSizeSamples);
			*granularity = 0;
		}
		else {
			Log() << "Calculating default buffer size based on " << sampleRate << " Hz sample rate";
			// We enforce a minimum of 32 samples as applications tend to choke on extremely small buffers - see https://github.com/dechamps/FlexASIO/issues/88
			*minSize = (std::max<long>)(32,  long(sampleRate * (hostApi.info.type == paDirectSound && inputDevice.has_value() ?
				0.010 :  // Cap the min buffer size to 10 ms when using DirectSound with an input device to work around https://github.com/dechamps/FlexASIO/issues/50
				0.001    // 1 ms, there's basically no chance we'll get glitch-free streaming below this
			)));
			*maxSize = (std::max<long>)(32, long(sampleRate)); // 1 second, more would be silly
			*preferredSize = (std::max<long>)(32, long(sampleRate * 0.02)); // 20 ms
			*granularity = 1; // Don't care
		}
		Log() << "Returning: min buffer size " << *minSize << ", max buffer size " << *maxSize << ", preferred buffer size " << *preferredSize << ", granularity " << *granularity;
	}

	void FlexASIO::GetChannels(long* numInputChannels, long* numOutputChannels)
	{
		*numInputChannels = GetInputChannelCount();
		*numOutputChannels = GetOutputChannelCount();
		Log() << "Returning " << *numInputChannels << " input channels and " << *numOutputChannels << " output channels";
	}

	namespace {
		std::string getChannelName(size_t channel, DWORD channelMask)
		{
			// Search for the matching bit in channelMask
			size_t current_channel = 0;
			DWORD current_channel_speaker = 1;
			for (;;)
			{
				while ((current_channel_speaker & channelMask) == 0 && current_channel_speaker < SPEAKER_ALL)
					current_channel_speaker <<= 1;
				if (current_channel_speaker == SPEAKER_ALL)
					break;
				// Now current_channel_speaker contains the speaker for current_channel
				if (current_channel == channel)
					break;
				++current_channel;
				current_channel_speaker <<= 1;
			}

			std::stringstream channel_name;
			channel_name << channel;
			if (current_channel_speaker == SPEAKER_ALL)
				Log() << "Channel " << channel << " is outside channel mask " << channelMask;
			else
			{
				const char* pretty_name = nullptr;
				switch (current_channel_speaker)
				{
				case SPEAKER_FRONT_LEFT: pretty_name = "FL (Front Left)"; break;
				case SPEAKER_FRONT_RIGHT: pretty_name = "FR (Front Right)"; break;
				case SPEAKER_FRONT_CENTER: pretty_name = "FC (Front Center)"; break;
				case SPEAKER_LOW_FREQUENCY: pretty_name = "LFE (Low Frequency)"; break;
				case SPEAKER_BACK_LEFT: pretty_name = "BL (Back Left)"; break;
				case SPEAKER_BACK_RIGHT: pretty_name = "BR (Back Right)"; break;
				case SPEAKER_FRONT_LEFT_OF_CENTER: pretty_name = "FCL (Front Left Center)"; break;
				case SPEAKER_FRONT_RIGHT_OF_CENTER: pretty_name = "FCR (Front Right Center)"; break;
				case SPEAKER_BACK_CENTER: pretty_name = "BC (Back Center)"; break;
				case SPEAKER_SIDE_LEFT: pretty_name = "SL (Side Left)"; break;
				case SPEAKER_SIDE_RIGHT: pretty_name = "SR (Side Right)"; break;
				case SPEAKER_TOP_CENTER: pretty_name = "TC (Top Center)"; break;
				case SPEAKER_TOP_FRONT_LEFT: pretty_name = "TFL (Top Front Left)"; break;
				case SPEAKER_TOP_FRONT_CENTER: pretty_name = "TFC (Top Front Center)"; break;
				case SPEAKER_TOP_FRONT_RIGHT: pretty_name = "TFR (Top Front Right)"; break;
				case SPEAKER_TOP_BACK_LEFT: pretty_name = "TBL (Top Back left)"; break;
				case SPEAKER_TOP_BACK_CENTER: pretty_name = "TBC (Top Back Center)"; break;
				case SPEAKER_TOP_BACK_RIGHT: pretty_name = "TBR (Top Back Right)"; break;
				}
				if (!pretty_name)
					Log() << "Speaker " << current_channel_speaker << " is unknown";
				else
					channel_name << " " << pretty_name;
			}
			return channel_name.str();
		}
	}

	void FlexASIO::GetChannelInfo(ASIOChannelInfo* info)
	{
		Log() << "CFlexASIO::getChannelInfo()";

		Log() << "Channel info requested for " << (info->isInput ? "input" : "output") << " channel " << info->channel;
		if (info->isInput)
		{
			if (info->channel < 0 || info->channel >= GetInputChannelCount()) throw ASIOException(ASE_InvalidParameter, "no such input channel");
		}
		else
		{
			if (info->channel < 0 || info->channel >= GetOutputChannelCount()) throw ASIOException(ASE_InvalidParameter, "no such output channel");
		}

		info->isActive = preparedState.has_value() && preparedState->IsChannelActive(info->isInput, info->channel);
		info->channelGroup = 0;
		info->type = info->isInput ? inputSampleType->asio : outputSampleType->asio;
		std::stringstream channel_string;
		channel_string << (info->isInput ? "IN" : "OUT") << " " << getChannelName(info->channel, info->isInput ? GetInputChannelMask() : GetOutputChannelMask());
		strcpy_s(info->name, 32, channel_string.str().c_str());
		Log() << "Returning: " << info->name << ", " << (info->isActive ? "active" : "inactive") << ", group " << info->channelGroup << ", type " << ::dechamps_ASIOUtil::GetASIOSampleTypeString(info->type);
	}

	FlexASIO::OpenStreamResult FlexASIO::OpenStream(bool inputEnabled, bool outputEnabled, double sampleRate, unsigned long framesPerBuffer, PaStreamCallback callback, void* callbackUserData)
	{
		Log() << "CFlexASIO::OpenStream(inputEnabled = " << inputEnabled << ", outputEnabled = " << outputEnabled << ", sampleRate = " << sampleRate << ", framesPerBuffer = " << framesPerBuffer << ", callback = " << callback << ", callbackUserData = " << callbackUserData;
		OpenStreamResult result;
		result.exclusive = hostApi.info.type == paWDMKS;

		PaStreamParameters common_parameters = { 0 };
		common_parameters.sampleFormat = paNonInterleaved;
		common_parameters.hostApiSpecificStreamInfo = NULL;
		common_parameters.suggestedLatency = 3 * framesPerBuffer / sampleRate;

		PaWasapiStreamInfo common_wasapi_stream_info = { 0 };
		if (hostApi.info.type == paWASAPI) {
			common_wasapi_stream_info.size = sizeof(common_wasapi_stream_info);
			common_wasapi_stream_info.hostApiType = paWASAPI;
			common_wasapi_stream_info.version = 1;
			common_wasapi_stream_info.flags = 0;
		}

		PaStreamParameters input_parameters = common_parameters;
		PaWasapiStreamInfo input_wasapi_stream_info = common_wasapi_stream_info;
		if (inputEnabled)
		{
			input_parameters.device = inputDevice->index;
			input_parameters.channelCount = GetInputChannelCount();
			input_parameters.sampleFormat |= inputSampleType->pa;
			if (config.input.suggestedLatencySeconds.has_value()) input_parameters.suggestedLatency = *config.input.suggestedLatencySeconds;
			if (hostApi.info.type == paWASAPI)
			{
				const auto inputChannelMask = GetInputChannelMask();
				if (inputChannelMask != 0)
				{
					input_wasapi_stream_info.flags |= paWinWasapiUseChannelMask;
					input_wasapi_stream_info.channelMask = inputChannelMask;
				}
				Log() << "Using " << (config.input.wasapiExclusiveMode ? "exclusive" : "shared") << " mode for input WASAPI stream";
				if (config.input.wasapiExclusiveMode) {
					input_wasapi_stream_info.flags |= paWinWasapiExclusive;
					result.exclusive = true;
				}
				Log() << (config.input.wasapiAutoConvert ? "Enabling" : "Disabling") << " auto-conversion for input WASAPI stream";
				if (config.input.wasapiAutoConvert) {
					input_wasapi_stream_info.flags |= paWinWasapiAutoConvert;
				}
				input_parameters.hostApiSpecificStreamInfo = &input_wasapi_stream_info;
			}
		}

		PaStreamParameters output_parameters = common_parameters;
		PaWasapiStreamInfo output_wasapi_stream_info = common_wasapi_stream_info;
		if (outputEnabled)
		{
			output_parameters.device = outputDevice->index;
			output_parameters.channelCount = GetOutputChannelCount();
			output_parameters.sampleFormat |= outputSampleType->pa;
			if (config.output.suggestedLatencySeconds.has_value()) output_parameters.suggestedLatency = *config.output.suggestedLatencySeconds;
			if (hostApi.info.type == paWASAPI)
			{
				const auto outputChannelMask = GetOutputChannelMask();
				if (outputChannelMask != 0)
				{
					output_wasapi_stream_info.flags |= paWinWasapiUseChannelMask;
					output_wasapi_stream_info.channelMask = outputChannelMask;
				}
				Log() << "Using " << (config.output.wasapiExclusiveMode ? "exclusive" : "shared") << " mode for output WASAPI stream";
				if (config.output.wasapiExclusiveMode) {
					output_wasapi_stream_info.flags |= paWinWasapiExclusive;
					result.exclusive = true;
				}
				Log() << (config.output.wasapiAutoConvert ? "Enabling" : "Disabling") << " auto-conversion for output WASAPI stream";
				if (config.output.wasapiAutoConvert) {
					output_wasapi_stream_info.flags |= paWinWasapiAutoConvert;
				}
				output_parameters.hostApiSpecificStreamInfo = &output_wasapi_stream_info;
			}
		}

		result.stream = flexasio::OpenStream(
			inputEnabled ? &input_parameters : NULL,
			outputEnabled ? &output_parameters : NULL,
			sampleRate, framesPerBuffer, paPrimeOutputBuffersUsingStreamCallback, callback, callbackUserData);
		if (result.stream != nullptr) {
			const auto streamInfo = Pa_GetStreamInfo(result.stream.get());
			if (streamInfo == nullptr) {
				Log() << "Unable to get stream info";
			}
			else {
				Log() << "Stream info: " << DescribeStreamInfo(*streamInfo);
			}
		}
		return result;
	}

	bool FlexASIO::CanSampleRate(ASIOSampleRate sampleRate)
	{
		Log() << "Checking for sample rate: " << sampleRate;

		if (preparedState.has_value() && preparedState->IsExclusive()) {
			// Some applications will call canSampleRate() while the stream is running. If the stream is exclusive our probes will fail.
			// In that case we always say "yes" - always saying "no" confuses applications. See https://github.com/dechamps/FlexASIO/issues/66
			Log() << "Faking sample rate " << sampleRate << " as available because an exclusive stream is currently running";
			return true;
		}

		// We do not know whether the host application intends to use only input channels, only output channels, or both.
		// This logic ensures the driver is usable for all three use cases.
		bool available = false;
		if (inputDevice.has_value())
			try {
				Log() << "Checking if input supports this sample rate";
				OpenStream(true, false, sampleRate, paFramesPerBufferUnspecified, NoOpStreamCallback, nullptr);
				Log() << "Input supports this sample rate";
				available = true;
			}
			catch (const std::exception& exception) {
				Log() << "Input does not support this sample rate: " << exception.what();
			}
		if (outputDevice.has_value())
			try {
				Log() << "Checking if output supports this sample rate";
				OpenStream(false, true, sampleRate, paFramesPerBufferUnspecified, NoOpStreamCallback, nullptr);
				Log() << "Output supports this sample rate";
				available = true;
			}
			catch (const std::exception& exception) {
				Log() << "Output does not support this sample rate: " << exception.what();
			}

		Log() << "Sample rate " << sampleRate << " is " << (available ? "available" : "unavailable");
		return available;
	}

	void FlexASIO::GetSampleRate(ASIOSampleRate* sampleRateResult)
	{
		sampleRateWasAccessed = true;
		previousSampleRate = sampleRate;
		*sampleRateResult = sampleRate;
		Log() << "Returning sample rate: " << *sampleRateResult;
	}

	void FlexASIO::SetSampleRate(ASIOSampleRate requestedSampleRate)
	{
		Log() << "Request to set sample rate: " << requestedSampleRate;

		if (!(requestedSampleRate > 0 && requestedSampleRate < (std::numeric_limits<ASIOSampleRate>::max)())) {
			throw ASIOException(ASE_InvalidParameter, "setSampleRate() called with an invalid sample rate");
		}

		sampleRateWasAccessed = true;
		previousSampleRate = requestedSampleRate;

		if (requestedSampleRate == sampleRate) {
			Log() << "Requested sampled rate is equal to current sample rate";
			return;
		}

		sampleRate = requestedSampleRate;
		if (preparedState.has_value())
		{
			Log() << "Sending a reset request to the host as it's not possible to change sample rate while streaming";
			preparedState->RequestReset();
		}
	}

	void FlexASIO::CreateBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) {
		Log() << "Request to create buffers for " << numChannels << " channels, size " << bufferSize << " samples";
		if (numChannels < 1 || bufferSize < 1 || callbacks == nullptr || callbacks->bufferSwitch == nullptr)
			throw ASIOException(ASE_InvalidParameter, "invalid createBuffer() parameters");

		if (preparedState.has_value()) {
			throw ASIOException(ASE_InvalidMode, "createBuffers() called multiple times");
		}

		if (!sampleRateWasAccessed) {
			// See https://github.com/dechamps/FlexASIO/issues/31
			Log() << "WARNING: ASIO host application never enquired about sample rate, and therefore cannot know we are running at " << sampleRate << " Hz!";
		}

		preparedState.emplace(*this, sampleRate, bufferInfos, numChannels, bufferSize, callbacks);
	}

	FlexASIO::PreparedState::Buffers::Buffers(size_t bufferSetCount, size_t inputChannelCount, size_t outputChannelCount, size_t bufferSizeInFrames, size_t inputSampleSizeInBytes, size_t outputSampleSizeInBytes) :
		bufferSetCount(bufferSetCount), inputChannelCount(inputChannelCount), outputChannelCount(outputChannelCount), bufferSizeInFrames(bufferSizeInFrames), inputSampleSizeInBytes(inputSampleSizeInBytes), outputSampleSizeInBytes(outputSampleSizeInBytes),
		buffers(bufferSetCount * bufferSizeInFrames * (inputChannelCount * inputSampleSizeInBytes + outputChannelCount * outputSampleSizeInBytes)) {
		Log() << "Allocated "
			<< bufferSetCount << " buffer sets, "
			<< inputChannelCount << "/" << outputChannelCount << " (I/O) channels per buffer set, "
			<< bufferSizeInFrames << " samples per channel, "
			<< inputSampleSizeInBytes << "/" << outputSampleSizeInBytes << " (I/O) bytes per sample, memory range: "
			<< static_cast<const void*>(buffers.data()) << "-" << static_cast<const void*>(buffers.data() + buffers.size());
	}

	FlexASIO::PreparedState::Buffers::~Buffers() {
		Log() << "Destroying buffers";
	}

	FlexASIO::PreparedState::PreparedState(FlexASIO& flexASIO, ASIOSampleRate sampleRate, ASIOBufferInfo* asioBufferInfos, long numChannels, long bufferSizeInFrames, ASIOCallbacks* callbacks) :
		flexASIO(flexASIO), sampleRate(sampleRate), callbacks(*callbacks),
		buffers(
			2,
			GetBufferInfosChannelCount(asioBufferInfos, numChannels, true), GetBufferInfosChannelCount(asioBufferInfos, numChannels, false),
			bufferSizeInFrames,
			flexASIO.inputSampleType.has_value() ? flexASIO.inputSampleType->size : 0, flexASIO.outputSampleType.has_value() ? flexASIO.outputSampleType->size : 0),
		bufferInfos([&] {
		std::vector<ASIOBufferInfo> bufferInfos;
		bufferInfos.reserve(numChannels);
		size_t nextBuffersInputChannelIndex = 0;
		size_t nextBuffersOutputChannelIndex = 0;
		for (long channelIndex = 0; channelIndex < numChannels; ++channelIndex)
		{
			ASIOBufferInfo& asioBufferInfo = asioBufferInfos[channelIndex];
			if (asioBufferInfo.isInput)
			{
				if (asioBufferInfo.channelNum < 0 || asioBufferInfo.channelNum >= flexASIO.GetInputChannelCount())
					throw ASIOException(ASE_InvalidParameter, "out of bounds input channel in createBuffers() buffer info");
			}
			else
			{
				if (asioBufferInfo.channelNum < 0 || asioBufferInfo.channelNum >= flexASIO.GetOutputChannelCount())
					throw ASIOException(ASE_InvalidParameter, "out of bounds output channel in createBuffers() buffer info");
			}
			const auto getBuffer = asioBufferInfo.isInput ? &Buffers::GetInputBuffer : &Buffers::GetOutputBuffer;
			auto& nextBuffersChannelIndex = asioBufferInfo.isInput ? nextBuffersInputChannelIndex : nextBuffersOutputChannelIndex;
			const auto bufferSizeInBytes = asioBufferInfo.isInput ? buffers.GetInputBufferSizeInBytes() : buffers.GetOutputBufferSizeInBytes();

			uint8_t* first_half = (buffers.*getBuffer)(0, nextBuffersChannelIndex);
			uint8_t* second_half = (buffers.*getBuffer)(1, nextBuffersChannelIndex);
			++nextBuffersChannelIndex;
			asioBufferInfo.buffers[0] = first_half;
			asioBufferInfo.buffers[1] = second_half;
			Log() << "ASIO buffer #" << channelIndex << " is " << (asioBufferInfo.isInput ? "input" : "output") << " channel " << asioBufferInfo.channelNum
				<< " - first half: " << static_cast<const void*>(first_half) << "-" << static_cast<const void*>(first_half + bufferSizeInBytes)
				<< " - second half: " << static_cast<const void*>(second_half) << "-" << static_cast<const void*>(second_half + bufferSizeInBytes);
			bufferInfos.push_back(asioBufferInfo);
		}
		return bufferInfos;
	}()), openStreamResult(flexASIO.OpenStream(buffers.inputChannelCount > 0, buffers.outputChannelCount > 0, sampleRate, unsigned long(bufferSizeInFrames), &PreparedState::StreamCallback, this)) {
		if (callbacks->asioMessage) ProbeHostMessages(callbacks->asioMessage);
	}

	bool FlexASIO::PreparedState::IsChannelActive(bool isInput, long channel) const {
		for (const auto& buffersInfo : bufferInfos)
			if (!!buffersInfo.isInput == !!isInput && buffersInfo.channelNum == channel)
				return true;
		return false;
	}

	void FlexASIO::DisposeBuffers()
	{
		if (!preparedState.has_value()) throw ASIOException(ASE_InvalidMode, "disposeBuffers() called before createBuffers()");
		preparedState.reset();
	}

	void FlexASIO::GetLatencies(long* inputLatency, long* outputLatency) {
		if (!preparedState.has_value()) throw ASIOException(ASE_InvalidMode, "getLatencies() called before createBuffers()");
		return preparedState->GetLatencies(inputLatency, outputLatency);
	}

	void FlexASIO::PreparedState::GetLatencies(long* inputLatency, long* outputLatency)
	{
		const PaStreamInfo* stream_info = Pa_GetStreamInfo(openStreamResult.stream.get());
		if (!stream_info) throw ASIOException(ASE_HWMalfunction, "unable to get stream info");

		// See https://github.com/dechamps/FlexASIO/issues/10.
		// The latency that PortAudio reports appears to take the buffer size into account already.
		*inputLatency = (long)(stream_info->inputLatency * sampleRate);
		*outputLatency = (long)(stream_info->outputLatency * sampleRate);

		if (!flexASIO.hostSupportsOutputReady) {
			Log() << buffers.bufferSizeInFrames << " samples added to output latency due to the ASIO Host Application not supporting OutputReady";
			*outputLatency += long(buffers.bufferSizeInFrames);
		}

		Log() << "Returning input latency of " << *inputLatency << " samples and output latency of " << *outputLatency << " samples";
	}

	void FlexASIO::Start() {
		if (!preparedState.has_value()) throw ASIOException(ASE_InvalidMode, "start() called before createBuffers()");
		return preparedState->Start();
	}

	void FlexASIO::PreparedState::Start()
	{
		if (runningState != nullptr) throw ASIOException(ASE_InvalidMode, "start() called twice");
		ownedRunningState.emplace(*this);
	}

	FlexASIO::PreparedState::RunningState::RunningState(PreparedState& preparedState) :
		preparedState(preparedState),
		host_supports_timeinfo([&] {
		Log() << "Checking if the host supports time info";
		const bool result = preparedState.callbacks.asioMessage &&
			Message(preparedState.callbacks.asioMessage, kAsioSelectorSupported, kAsioSupportsTimeInfo, NULL, NULL) == 1 &&
			Message(preparedState.callbacks.asioMessage, kAsioSupportsTimeInfo, 0, NULL, NULL) == 1;
		Log() << "The host " << (result ? "supports" : "does not support") << " time info";
		return result;
	}()),
		hostSupportsOutputReady(preparedState.flexASIO.hostSupportsOutputReady),
		activeStream(preparedState.openStreamResult.stream.get()) {}

	void FlexASIO::Stop() {
		if (!preparedState.has_value()) throw ASIOException(ASE_InvalidMode, "stop() called before createBuffers()");
		return preparedState->Stop();
	}

	void FlexASIO::PreparedState::Stop()
	{
		if (runningState == nullptr) throw ASIOException(ASE_InvalidMode, "stop() called before start()");
		ownedRunningState.reset();
	}

	int FlexASIO::PreparedState::StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw() {
		if (IsLoggingEnabled()) Log() << "--- ENTERING STREAM CALLBACK";
		PaStreamCallbackResult result = paContinue;
		try {
			auto& preparedState = *static_cast<PreparedState*>(userData);
			if (preparedState.runningState == nullptr) {
				throw std::runtime_error("PortAudio stream callback fired in non-started state");
			}
			result = preparedState.runningState->StreamCallback(input, output, frameCount, timeInfo, statusFlags);
		}
		catch (const std::exception& exception) {
			if (IsLoggingEnabled()) Log() << "Caught exception in stream callback: " << exception.what();
		}
		catch (...) {
			if (IsLoggingEnabled()) Log() << "Caught unknown exception in stream callback";
		}
		if (IsLoggingEnabled()) Log() << "--- EXITING STREAM CALLBACK (" << GetPaStreamCallbackResultString(result) << ")";
		return result;
	}

	PaStreamCallbackResult FlexASIO::PreparedState::RunningState::StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags)
	{
		auto currentSamplePosition = samplePosition.load();
		currentSamplePosition.timestamp = ::dechamps_ASIOUtil::Int64ToASIO<ASIOTimeStamp>(((long long int) win32HighResolutionTimer.GetTimeMilliseconds()) * 1000000);
		if (state == State::STEADYSTATE) currentSamplePosition.samples = ::dechamps_ASIOUtil::Int64ToASIO<ASIOSamples>(::dechamps_ASIOUtil::ASIOToInt64(currentSamplePosition.samples) + frameCount);
		samplePosition.store(currentSamplePosition);
		if (IsLoggingEnabled()) Log() << "Updated sample position: timestamp " << ::dechamps_ASIOUtil::ASIOToInt64(currentSamplePosition.timestamp) << ", " << ::dechamps_ASIOUtil::ASIOToInt64(currentSamplePosition.samples) << " samples";

		if (IsLoggingEnabled()) Log() << "PortAudio stream callback with input " << input << ", output "
			<< output << ", "
			<< frameCount << " frames, time info ("
			<< (timeInfo == nullptr ? "none" : DescribeStreamCallbackTimeInfo(*timeInfo)) << "), flags "
			<< GetStreamCallbackFlagsString(statusFlags);

		if (frameCount != preparedState.buffers.bufferSizeInFrames)
		{
			if (IsLoggingEnabled()) Log() << "Expected " << preparedState.buffers.bufferSizeInFrames << " frames, got " << frameCount << " instead, aborting";
			return paContinue;
		}

		if (statusFlags & paInputOverflow && IsLoggingEnabled())
			Log() << "INPUT OVERFLOW detected (some input data was discarded)";
		if (statusFlags & paInputUnderflow && IsLoggingEnabled())
			Log() << "INPUT UNDERFLOW detected (gaps were inserted in the input)";
		if (statusFlags & paOutputOverflow && IsLoggingEnabled())
			Log() << "OUTPUT OVERFLOW detected (some output data was discarded)";
		if (statusFlags & paOutputUnderflow && IsLoggingEnabled())
			Log() << "OUTPUT UNDERFLOW detected (gaps were inserted in the output)";

		const auto inputSampleSizeInBytes = preparedState.buffers.inputSampleSizeInBytes;
		const auto outputSampleSizeInBytes = preparedState.buffers.outputSampleSizeInBytes;
		const void* const* input_samples = static_cast<const void* const*>(input);
		void* const* output_samples = static_cast<void* const*>(output);

		if (output_samples) {
			for (int output_channel_index = 0; output_channel_index < preparedState.flexASIO.GetOutputChannelCount(); ++output_channel_index)
				memset(output_samples[output_channel_index], 0, frameCount * outputSampleSizeInBytes);
		}

		// Some backends (e.g. WASAPI) issue the first stream callback from within Pa_StartStream().
		// This is problematic because some host applications (e.g. foo_out_asio) wait for Start() to finish
		// before returning from bufferSwitch(), resulting in a deadlock. See: https://github.com/dechamps/FlexASIO/issues/60
		// To work around this problem, unblock Start() if it is still running by the time we reach this point.
		// One possible downside of this approach is that we might not report Start() errors properly if they occur
		// after the first stream callback fires.
		if (state == InitialState()) activeStream.EndWaitForStartOutcome();

		// See dechamps_ASIOUtil/BUFFERS.md for the gory details of how ASIO buffer management works.

		if (state != State::PRIMING) {
			if (IsLoggingEnabled()) Log() << "Transferring input buffers from PortAudio to ASIO buffer index #" << driverBufferIndex;
			CopyFromPortAudioBuffers(preparedState.bufferInfos, driverBufferIndex, input_samples, frameCount * inputSampleSizeInBytes);

			if (!host_supports_timeinfo)
			{
				if (IsLoggingEnabled()) Log() << "Firing ASIO bufferSwitch() callback with buffer index: " << driverBufferIndex;
				preparedState.callbacks.bufferSwitch(driverBufferIndex, ASIOTrue);
				if (IsLoggingEnabled()) Log() << "bufferSwitch() complete";
			}
			else
			{
				ASIOTime time = { 0 };
				time.timeInfo.flags = kSystemTimeValid | kSamplePositionValid | kSampleRateValid;
				time.timeInfo.samplePosition = currentSamplePosition.samples;
				time.timeInfo.systemTime = currentSamplePosition.timestamp;
				time.timeInfo.sampleRate = preparedState.sampleRate;
				if (IsLoggingEnabled()) Log() << "Firing ASIO bufferSwitchTimeInfo() callback with buffer index: " << driverBufferIndex << ", time info: (" << ::dechamps_ASIOUtil::DescribeASIOTime(time) << ")";
				const auto timeResult = preparedState.callbacks.bufferSwitchTimeInfo(&time, driverBufferIndex, ASIOTrue);
				if (IsLoggingEnabled()) Log() << "bufferSwitchTimeInfo() complete, returned time info: " << (timeResult == nullptr ? "none" : ::dechamps_ASIOUtil::DescribeASIOTime(*timeResult));
			}
		}

		if (!hostSupportsOutputReady) {
			driverBufferIndex = (driverBufferIndex + 1) % 2;
		}
		else {
			std::unique_lock outputReadyLock(outputReadyMutex);
			if (!outputReady) {
				if (IsLoggingEnabled()) Log() << "Waiting for the ASIO Host Application to signal OutputReady";
				outputReadyCondition.wait(outputReadyLock, [&] { return outputReady; });
				outputReady = false;
			}
		}

		if (IsLoggingEnabled()) Log() << "Transferring output buffers from buffer index #" << driverBufferIndex << " to PortAudio";
		CopyToPortAudioBuffers(preparedState.bufferInfos, driverBufferIndex, output_samples, frameCount * outputSampleSizeInBytes);

		if (hostSupportsOutputReady) driverBufferIndex = (driverBufferIndex + 1) % 2;

		if (state != State::STEADYSTATE) IncrementEnum(state);
		return paContinue;
	}

	void FlexASIO::GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) {
		if (!preparedState.has_value()) throw ASIOException(ASE_InvalidMode, "getSamplePosition() called before createBuffers()");
		return preparedState->GetSamplePosition(sPos, tStamp);
	}

	void FlexASIO::PreparedState::GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp)
	{
		if (runningState == nullptr) throw ASIOException(ASE_InvalidMode, "getSamplePosition() called before start()");
		return runningState->GetSamplePosition(sPos, tStamp);
	}

	void FlexASIO::PreparedState::RunningState::GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) const
	{
		const auto currentSamplePosition = samplePosition.load();
		*sPos = currentSamplePosition.samples;
		*tStamp = currentSamplePosition.timestamp;
		if (IsLoggingEnabled()) Log() << "Returning: sample position " << ::dechamps_ASIOUtil::ASIOToInt64(*sPos) << ", timestamp " << ::dechamps_ASIOUtil::ASIOToInt64(*tStamp);
	}

	void FlexASIO::OutputReady() {
		if (!hostSupportsOutputReady) {
			Log() << "Host supports OutputReady";
			hostSupportsOutputReady = true;
		}
		if (preparedState.has_value()) preparedState->OutputReady();
	}

	void FlexASIO::PreparedState::OutputReady() {
		if (runningState != nullptr) runningState->OutputReady();
	}

	void FlexASIO::PreparedState::RunningState::OutputReady() {
		{
			std::scoped_lock outputReadyLock(outputReadyMutex);
			outputReady = true;
		}
		outputReadyCondition.notify_all();
	}

	void FlexASIO::PreparedState::RequestReset() {
		if (!callbacks.asioMessage || Message(callbacks.asioMessage, kAsioSelectorSupported, kAsioResetRequest, nullptr, nullptr) != 1)
			throw ASIOException(ASE_InvalidMode, "reset requests are not supported");
		Message(callbacks.asioMessage, kAsioResetRequest, 0, NULL, NULL);
	}

	void FlexASIO::ControlPanel() {
		const auto url = std::string("https://github.com/dechamps/FlexASIO/blob/") + ::dechamps_CMakeUtils_gitDescription + "/CONFIGURATION.md";
		Log() << "Opening URL: " << url;
		const auto result = ShellExecuteA(windowHandle, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
		Log() << "ShellExecuteA() result: " << result;
	}

}

