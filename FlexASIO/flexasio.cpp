#include "flexasio.h"

#include <cassert>
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <string_view>
#include <vector>

#include <MMReg.h>

#include "portaudio.h"
#include "pa_win_wasapi.h"

#include "../FlexASIOUtil/endian.h"
#include "../FlexASIOUtil/log.h"
#include "../FlexASIOUtil/version.h"
#include "../FlexASIOUtil/asio.h"
#include "../FlexASIOUtil/string.h"

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

		Device SelectDeviceByName(const PaHostApiIndex hostApiIndex, const std::string_view name) {
			Log() << "Searching for a PortAudio device named '" << name << "' with host API index " << hostApiIndex;
			const auto deviceCount = Pa_GetDeviceCount();

			for (PaDeviceIndex deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
				const Device device(deviceIndex);
				if (device.info.hostApi == hostApiIndex && device.info.name == name) return device;
			}
			throw std::runtime_error(std::string("PortAudio device '") + std::string(name) + "' not found within specified backend");
		}

		std::optional<Device> SelectDevice(const PaHostApiIndex hostApiIndex, const PaDeviceIndex defaultDeviceIndex, std::optional<std::string_view> name) {
			if (!name.has_value()) {
				Log() << "Using default device with index " << defaultDeviceIndex;
				return Device(defaultDeviceIndex);
			}
			if (name->empty()) {
				Log() << "Device explicitly disabled in configuration";
				return std::nullopt;
			}

			return SelectDeviceByName(hostApiIndex, *name);
		}

		std::string GetPaStreamCallbackResultString(PaStreamCallbackResult result) {
			return EnumToString(result, {
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
			Log() << "Sending message: selector = " << GetASIOMessageSelectorString(selector) << ", value = " << value << ", message = " << message << ", opt = " << opt;
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
				Log() << "Probing for message selector: " << GetASIOMessageSelectorString(selector);
				if (Message(asioMessage, kAsioSelectorSupported, selector, nullptr, nullptr) != 1) continue;

				switch (selector) {
				case kAsioEngineVersion:
					Message(asioMessage, kAsioEngineVersion, 0, nullptr, nullptr);
					break;
				}
			}
		}

		// No-op PortAudio stream callback. Useful for backends that fail to initialize without a callback, such as WDM-KS.
		int NoOpStreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw() {
			Log() << "In no-op stream callback";
			return paContinue;
		}

	}

	const FlexASIO::SampleType FlexASIO::float32 = { GetEndianness() == Endianness::LITTLE ? ASIOSTFloat32LSB : ASIOSTFloat32MSB, paFloat32, 4 };
	const FlexASIO::SampleType FlexASIO::int32 = { GetEndianness() == Endianness::LITTLE ? ASIOSTInt32LSB : ASIOSTInt32MSB, paInt32, 4 };
	const FlexASIO::SampleType FlexASIO::int24 = { GetEndianness() == Endianness::LITTLE ? ASIOSTInt24LSB : ASIOSTInt24MSB, paInt24, 3 };
	const FlexASIO::SampleType FlexASIO::int16 = { GetEndianness() == Endianness::LITTLE ? ASIOSTInt16LSB : ASIOSTInt16MSB, paInt16, 2 };

	FlexASIO::SampleType FlexASIO::ParseSampleType(const std::string_view str) {
		static const bool bigEndian = GetEndianness() == Endianness::BIG;
		static const std::vector<std::pair<std::string_view, SampleType>> sampleTypes = {
			{"Float32", float32},
			{"Int32", int32},
			{"Int24", int24},
			{"Int16", int16},
		};
		const auto sampleType = Find(str, sampleTypes);
		if (!sampleType.has_value()) {
			throw std::runtime_error(std::string("Invalid '") + std::string(str) + "' sample type - valid values are " + Join(sampleTypes, ", ", [](const auto& item) { return std::string("'") + std::string(item.first) + "'"; }));
		}
		return *sampleType;
	}

	FlexASIO::FlexASIO(void* sysHandle) :
		windowHandle(reinterpret_cast<decltype(windowHandle)>(sysHandle)),
		config([&] {
		const auto config = LoadConfig();
		if (!config.has_value()) throw ASIOException(ASE_HWMalfunction, "could not load FlexASIO configuration. See FlexASIO log for details.");
		return *config;
	}()), sampleType([&] {
		const auto sampleType = config.sampleType.has_value() ? ParseSampleType(*config.sampleType) : float32;
		Log() << "Selected sample type: ASIO " << GetASIOSampleTypeString(sampleType.asio) << ", PortAudio " << GetSampleFormatString(sampleType.pa) << ", size " << sampleType.size << " bytes";
		return sampleType;
	}()), hostApi([&] {
		LogPortAudioApiList();
		auto hostApi = config.backend.has_value() ? SelectHostApiByName(*config.backend) : SelectDefaultHostApi();
		Log() << "Selected backend: " << hostApi;
		LogPortAudioDeviceList();
		return hostApi;
	}()),
		inputDevice([&] {
		Log() << "Selecting input device";
		auto device = SelectDevice(hostApi.index, hostApi.info.defaultInputDevice, config.input.device);
		if (device.has_value()) {
			Log() << "Selected input device: " << *device;
		}
		else {
			Log() << "No input device, proceeding without input";
		}
		return device;
	}()),
		outputDevice([&] {
		Log() << "Selecting output device";
		auto device = SelectDevice(hostApi.index, hostApi.info.defaultOutputDevice, config.output.device);
		if (device.has_value()) {
			Log() << "Selected output device: " << *device;
		}
		else {
			Log() << "No output device, proceeding without output";
		}
		return device;
	}()),
		inputFormat(inputDevice.has_value() ? GetDeviceDefaultFormat(hostApi.info.type, inputDevice->index) : std::nullopt),
		outputFormat(outputDevice.has_value() ? GetDeviceDefaultFormat(hostApi.info.type, outputDevice->index) : std::nullopt),
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
			*minSize = long(sampleRate * 0.001); // 1 ms, there's basically no chance we'll get glitch-free streaming below this
			*maxSize = long(sampleRate); // 1 second, more would be silly
			*preferredSize = long(sampleRate * 0.02); // typical - 20 ms
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
		info->type = sampleType.asio;
		std::stringstream channel_string;
		channel_string << (info->isInput ? "IN" : "OUT") << " " << getChannelName(info->channel, info->isInput ? GetInputChannelMask() : GetOutputChannelMask());
		strcpy_s(info->name, 32, channel_string.str().c_str());
		Log() << "Returning: " << info->name << ", " << (info->isActive ? "active" : "inactive") << ", group " << info->channelGroup << ", type " << GetASIOSampleTypeString(info->type);
	}

	Stream FlexASIO::OpenStream(bool inputEnabled, bool outputEnabled, double sampleRate, unsigned long framesPerBuffer, PaStreamCallback callback, void* callbackUserData)
	{
		Log() << "CFlexASIO::OpenStream(inputEnabled = " << inputEnabled << ", outputEnabled = " << outputEnabled << ", sampleRate = " << sampleRate << ", framesPerBuffer = " << framesPerBuffer << ", callback = " << callback << ", callbackUserData = " << callbackUserData;

		inputEnabled = inputEnabled && inputDevice.has_value();
		outputEnabled = outputEnabled && outputDevice.has_value();

		auto defaultSuggestedLatency = double(framesPerBuffer) / sampleRate;

		PaStreamParameters common_parameters = { 0 };
		common_parameters.sampleFormat = sampleType.pa | paNonInterleaved;
		common_parameters.hostApiSpecificStreamInfo = NULL;

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
			input_parameters.suggestedLatency = config.input.suggestedLatencySeconds.has_value() ? *config.input.suggestedLatencySeconds : defaultSuggestedLatency;
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
			output_parameters.suggestedLatency = config.output.suggestedLatencySeconds.has_value() ? *config.output.suggestedLatencySeconds : defaultSuggestedLatency;
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
				}
				output_parameters.hostApiSpecificStreamInfo = &output_wasapi_stream_info;
			}
		}

		return flexasio::OpenStream(
			inputEnabled ? &input_parameters : NULL,
			outputEnabled ? &output_parameters : NULL,
			sampleRate, framesPerBuffer, paNoFlag, callback, callbackUserData);
	}

	bool FlexASIO::CanSampleRate(ASIOSampleRate sampleRate)
	{
		Log() << "Checking for sample rate: " << sampleRate;

		// We do not know whether the host application intends to use only input channels, only output channels, or both.
		// This logic ensures the driver is usable for all three use cases.
		bool available = false;
		try {
			Log() << "Checking if input supports this sample rate";
			OpenStream(true, false, sampleRate, paFramesPerBufferUnspecified, NoOpStreamCallback, nullptr);
			Log() << "Input supports this sample rate";
			available = true;
		}
		catch (const std::exception& exception) {
			Log() << "Input does not support this sample rate: " << exception.what();
		}
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
		*sampleRateResult = sampleRate;
		Log() << "Returning sample rate: " << *sampleRateResult;
	}

	void FlexASIO::SetSampleRate(ASIOSampleRate requestedSampleRate)
	{
		Log() << "Request to set sample rate: " << requestedSampleRate;

		if (!(requestedSampleRate > 0 && requestedSampleRate < (std::numeric_limits<ASIOSampleRate>::max)())) {
			throw ASIOException(ASE_InvalidParameter, "setSampleRate() called with an invalid sample rate");
		}

		if (requestedSampleRate == sampleRate) {
			Log() << "Requested sampled rate is equal to current sample rate";
			return;
		}

		if (preparedState.has_value())
		{
			Log() << "Sending a reset request to the host as it's not possible to change sample rate while streaming";
			preparedState->RequestReset();
		}
		sampleRate = requestedSampleRate;
	}

	void FlexASIO::CreateBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) {
		Log() << "Request to create buffers for " << numChannels << " channels, size " << bufferSize << " samples";
		if (numChannels < 1 || bufferSize < 1 || callbacks == nullptr || callbacks->bufferSwitch == nullptr)
			throw ASIOException(ASE_InvalidParameter, "invalid createBuffer() parameters");

		if (preparedState.has_value()) {
			throw ASIOException(ASE_InvalidMode, "createBuffers() called multiple times");
		}

		preparedState.emplace(*this, sampleRate, bufferInfos, numChannels, bufferSize, callbacks);
	}

	FlexASIO::PreparedState::Buffers::Buffers(size_t bufferCount, size_t channelCount, size_t bufferSizeInSamples, size_t sampleSize) :
		bufferCount(bufferCount), channelCount(channelCount), bufferSizeInSamples(bufferSizeInSamples), sampleSize(sampleSize), buffers(new uint8_t[getSizeInBytes()]()) {
		Log() << "Allocated " << bufferCount << " buffers, " << channelCount << " channels per buffer, " << bufferSizeInSamples << " samples per channel, " << sampleSize << " bytes per sample, memory range: " << static_cast<const void*>(buffers) << "-" << static_cast<const void*>(buffers + getSizeInBytes());
	}

	FlexASIO::PreparedState::Buffers::~Buffers() {
		Log() << "Destroying buffers";
		delete[] buffers;
	}

	FlexASIO::PreparedState::PreparedState(FlexASIO& flexASIO, ASIOSampleRate sampleRate, ASIOBufferInfo* asioBufferInfos, long numChannels, long bufferSizeInSamples, ASIOCallbacks* callbacks) :
		flexASIO(flexASIO), sampleRate(sampleRate), callbacks(*callbacks), buffers(2, numChannels, bufferSizeInSamples, flexASIO.sampleType.size), bufferInfos([&] {
		const auto bufferSizeInBytes = bufferSizeInSamples * flexASIO.sampleType.size;
		std::vector<ASIOBufferInfo> bufferInfos;
		bufferInfos.reserve(numChannels);
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

			uint8_t* first_half = buffers.getBuffer(0, channelIndex);
			uint8_t* second_half = buffers.getBuffer(1, channelIndex);
			asioBufferInfo.buffers[0] = first_half;
			asioBufferInfo.buffers[1] = second_half;
			Log() << "ASIO buffer #" << channelIndex << " is " << (asioBufferInfo.isInput ? "input" : "output") << " channel " << asioBufferInfo.channelNum
				<< " - first half: " << static_cast<const void*>(first_half) << "-" << static_cast<const void*>(first_half + bufferSizeInBytes)
				<< " - second half: " << static_cast<const void*>(second_half) << "-" << static_cast<const void*>(second_half + bufferSizeInBytes);
			bufferInfos.push_back(asioBufferInfo);
		}
		return bufferInfos;
	}()), stream(flexASIO.OpenStream(IsInputEnabled(), IsOutputEnabled(), sampleRate, unsigned long(buffers.bufferSizeInSamples), &PreparedState::StreamCallback, this)) {
		if (callbacks->asioMessage) ProbeHostMessages(callbacks->asioMessage);
	}

	bool FlexASIO::PreparedState::IsChannelActive(bool isInput, long channel) const {
		for (const auto& buffersInfo : bufferInfos)
			if (!!buffersInfo.isInput == !!isInput && buffersInfo.channelNum == channel)
				return true;
		return false;
	}

	bool FlexASIO::PreparedState::IsInputEnabled() const {
		for (const auto& buffersInfo : bufferInfos)
			if (buffersInfo.isInput)
				return true;
		return false;
	}
	bool FlexASIO::PreparedState::IsOutputEnabled() const {
		for (const auto& buffersInfo : bufferInfos)
			if (!buffersInfo.isInput)
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
		const PaStreamInfo* stream_info = Pa_GetStreamInfo(stream.get());
		if (!stream_info) throw ASIOException(ASE_HWMalfunction, "unable to get stream info");

		// See https://github.com/dechamps/FlexASIO/issues/10.
		// The latency that PortAudio reports appears to take the buffer size into account already.
		*inputLatency = (long)(stream_info->inputLatency * sampleRate);
		*outputLatency = (long)(stream_info->outputLatency * sampleRate);
		Log() << "Returning input latency of " << *inputLatency << " samples and output latency of " << *outputLatency << " samples";
	}

	void FlexASIO::Start() {
		if (!preparedState.has_value()) throw ASIOException(ASE_InvalidMode, "start() called before createBuffers()");
		return preparedState->Start();
	}

	void FlexASIO::PreparedState::Start()
	{
		if (runningState.has_value()) throw ASIOException(ASE_InvalidMode, "start() called twice");
		runningState.emplace(*this);
	}

	FlexASIO::PreparedState::RunningState::RunningState(PreparedState& preparedState) :
		preparedState(preparedState),
		our_buffer_index(0),
		position(Int64ToASIO<ASIOSamples>(0)),
		position_timestamp(Int64ToASIO<ASIOTimeStamp>(((long long int) win32HighResolutionTimer.GetTimeMilliseconds()) * 1000000)),
		host_supports_timeinfo([&] {
		Log() << "Checking if the host supports time info";
		const bool result = preparedState.callbacks.asioMessage &&
			Message(preparedState.callbacks.asioMessage, kAsioSelectorSupported, kAsioSupportsTimeInfo, NULL, NULL) == 1 &&
			Message(preparedState.callbacks.asioMessage, kAsioSupportsTimeInfo, 0, NULL, NULL) == 1;
		Log() << "The host " << (result ? "supports" : "does not support") << " time info";
		return result;
	}()),
		activeStream(StartStream(preparedState.stream.get())) {}

	void FlexASIO::Stop() {
		if (!preparedState.has_value()) throw ASIOException(ASE_InvalidMode, "stop() called before createBuffers()");
		return preparedState->Stop();
	}

	void FlexASIO::PreparedState::Stop()
	{
		if (!runningState.has_value()) throw ASIOException(ASE_InvalidMode, "stop() called before start()");
		runningState.reset();
	}

	int FlexASIO::PreparedState::StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw() {
		Log() << "--- ENTERING STREAM CALLBACK";
		PaStreamCallbackResult result = paContinue;
		try {
			auto& preparedState = *static_cast<PreparedState*>(userData);
			if (!preparedState.runningState.has_value()) {
				throw std::runtime_error("PortAudio stream callback fired in non-started state");
			}
			result = preparedState.runningState->StreamCallback(input, output, frameCount, timeInfo, statusFlags);
		}
		catch (const std::exception& exception) {
			Log() << "Caught exception in stream callback: " << exception.what();
		}
		catch (...) {
			Log() << "Caught unknown exception in stream callback";
		}
		Log() << "--- EXITING STREAM CALLBACK (" << GetPaStreamCallbackResultString(result) << ")";
		return result;
	}

	PaStreamCallbackResult FlexASIO::PreparedState::RunningState::StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags)
	{
		Log() << "PortAudio stream callback with input " << input << ", output "
			<< output << ", "
			<< frameCount << " frames, time info ("
			<< (timeInfo == nullptr ? "none" : DescribeStreamCallbackTimeInfo(*timeInfo)) << "), flags "
			<< GetStreamCallbackFlagsString(statusFlags);

		if (frameCount != preparedState.buffers.bufferSizeInSamples)
		{
			Log() << "Expected " << preparedState.buffers.bufferSizeInSamples << " frames, got " << frameCount << " instead, aborting";
			return paContinue;
		}

		if (statusFlags & paInputOverflow)
			Log() << "INPUT OVERFLOW detected (some input data was discarded)";
		if (statusFlags & paInputUnderflow)
			Log() << "INPUT UNDERFLOW detected (gaps were inserted in the input)";
		if (statusFlags & paOutputOverflow)
			Log() << "OUTPUT OVERFLOW detected (some output data was discarded)";
		if (statusFlags & paOutputUnderflow)
			Log() << "OUTPUT UNDERFLOW detected (gaps were inserted in the output)";

		const auto sampleSize = preparedState.buffers.sampleSize;
		const void* const* input_samples = static_cast<const void* const*>(input);
		void* const* output_samples = static_cast<void* const*>(output);

		if (output_samples) {
			for (int output_channel_index = 0; output_channel_index < preparedState.flexASIO.GetOutputChannelCount(); ++output_channel_index)
				memset(output_samples[output_channel_index], 0, frameCount * sampleSize);
		}

		size_t locked_buffer_index = (our_buffer_index + 1) % 2; // The host is currently busy with locked_buffer_index and is not touching our_buffer_index.
		Log() << "Transferring between PortAudio and buffer #" << our_buffer_index;
		for (const auto& bufferInfo : preparedState.bufferInfos)
		{
			void* buffer = bufferInfo.buffers[our_buffer_index];
			if (bufferInfo.isInput)
				memcpy(buffer, input_samples[bufferInfo.channelNum], frameCount * sampleSize);
			else
				memcpy(output_samples[bufferInfo.channelNum], buffer, frameCount * sampleSize);
		}

		if (!host_supports_timeinfo)
		{
			Log() << "Firing ASIO bufferSwitch() callback with buffer index: " << our_buffer_index;
			preparedState.callbacks.bufferSwitch(long(our_buffer_index), ASIOFalse);
			Log() << "bufferSwitch() complete";
		}
		else
		{
			ASIOTime time = { 0 };
			time.timeInfo.flags = kSystemTimeValid | kSamplePositionValid | kSampleRateValid;
			time.timeInfo.samplePosition = position;
			time.timeInfo.systemTime = position_timestamp;
			time.timeInfo.sampleRate = preparedState.sampleRate;
			Log() << "Firing ASIO bufferSwitchTimeInfo() callback with buffer index: " << our_buffer_index << ", time info: (" << DescribeASIOTime(time) << ")";
			const auto timeResult = preparedState.callbacks.bufferSwitchTimeInfo(&time, long(our_buffer_index), ASIOFalse);
			Log() << "bufferSwitchTimeInfo() complete, returned time info: " << (timeResult == nullptr ? "none" : DescribeASIOTime(*timeResult));
		}

		std::swap(locked_buffer_index, our_buffer_index);
		position = Int64ToASIO<ASIOSamples>(ASIOToInt64(position) + frameCount);
		position_timestamp = Int64ToASIO<ASIOTimeStamp>(((long long int) win32HighResolutionTimer.GetTimeMilliseconds()) * 1000000);
		Log() << "Updated buffer index: " << our_buffer_index << ", position: " << ASIOToInt64(position) << ", timestamp: " << ASIOToInt64(position_timestamp);

		return paContinue;
	}

	void FlexASIO::GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) {
		if (!preparedState.has_value()) throw ASIOException(ASE_InvalidMode, "getSamplePosition() called before createBuffers()");
		return preparedState->GetSamplePosition(sPos, tStamp);
	}

	void FlexASIO::PreparedState::GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp)
	{
		if (!runningState.has_value()) throw ASIOException(ASE_InvalidMode, "getSamplePosition() called before start()");
		return runningState->GetSamplePosition(sPos, tStamp);
	}

	void FlexASIO::PreparedState::RunningState::GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp)
	{
		*sPos = position;
		*tStamp = position_timestamp;
		Log() << "Returning: sample position " << ASIOToInt64(position) << ", timestamp " << ASIOToInt64(position_timestamp);
	}

	void FlexASIO::PreparedState::RequestReset() {
		if (!callbacks.asioMessage || Message(callbacks.asioMessage, kAsioSelectorSupported, kAsioResetRequest, nullptr, nullptr) != 1)
			throw ASIOException(ASE_InvalidMode, "reset requests are not supported");
		Message(callbacks.asioMessage, kAsioResetRequest, 0, NULL, NULL);
	}

	void FlexASIO::ControlPanel() {
		const auto url = std::string("https://github.com/dechamps/FlexASIO/blob/") + gitstr + "/CONFIGURATION.md";
		Log() << "Opening URL: " << url;
		const auto result = ShellExecuteA(windowHandle, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
		Log() << "ShellExecuteA() result: " << result;
	}

}

