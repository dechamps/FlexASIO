/*

	Copyright (C) 2014 Etienne Dechamps (e-t172) <etienne@edechamps.fr>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

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

		const PaSampleFormat portaudio_sample_format = paFloat32;
		const ASIOSampleType asio_sample_type = ASIOSTFloat32LSB;

		void LogPortAudioApiList() {
			const auto pa_api_count = Pa_GetHostApiCount();
			for (PaHostApiIndex pa_api_index = 0; pa_api_index < pa_api_count; ++pa_api_index) {
				Log() << "Found backend: " << HostApi(pa_api_index);
			}
		}
		void LogPortAudioDeviceList() {
			const auto deviceCount = Pa_GetDeviceCount();
			for (PaDeviceIndex deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
				const auto device = Pa_GetDeviceInfo(deviceIndex);
				if (device == nullptr) {
					Log() << "Unable to get info for PortAudio device at index " << deviceIndex;
					continue;
				}
				Log() << "PortAudio device at index " << deviceIndex << ": '" << device->name << "', host API " << device->hostApi;
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

		const std::optional<PaDeviceIndex> SelectPortAudioDeviceByName(const PaHostApiIndex hostApiIndex, const std::string_view name) {
			Log() << "Searching for a PortAudio device named '" << name << "' with host API index " << hostApiIndex;
			const auto deviceCount = Pa_GetDeviceCount();

			for (PaDeviceIndex deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
				const auto device = Pa_GetDeviceInfo(deviceIndex);
				if (device == nullptr) {
					Log() << "Unable to get PortAudio device info for device index " << deviceIndex;
					continue;
				}
				if (device->hostApi == hostApiIndex && device->name == name) return deviceIndex;
			}
			Log() << "Unable to find requested PortAudio device";
			return std::nullopt;
		}

		const std::optional<PaDeviceIndex> SelectPortAudioDevice(const PaHostApiIndex hostApiIndex, const PaDeviceIndex defaultDevice, std::optional<std::string_view> name) {
			if (!name.has_value()) {
				Log() << "Using default device with index " << defaultDevice;
				return defaultDevice;
			}
			if (name->empty()) return paNoDevice;

			return SelectPortAudioDeviceByName(hostApiIndex, *name);
		}

		std::string GetPaStreamCallbackResultString(PaStreamCallbackResult result) {
			return EnumToString(result, {
				{paContinue, "paContinue"},
				{paComplete, "paComplete"},
				{paAbort, "paAbort"},
				});
		}

	}

	FlexASIO::FlexASIO(void* sysHandle) :
		windowHandle(reinterpret_cast<decltype(windowHandle)>(sysHandle)),
		config([&] {
		const auto config = LoadConfig();
		if (!config.has_value()) throw ASIOException(ASE_HWMalfunction, "could not load FlexASIO configuration. See FlexASIO log for details.");
		return *config;
	}()), hostApi([&] {
		LogPortAudioApiList();
		auto hostApi = config.backend.has_value() ? SelectHostApiByName(*config.backend) : SelectDefaultHostApi();
		Log() << "Selected backend: " << hostApi;
		return hostApi;
	}())
	{
		Log() << "sysHandle = " << sysHandle;

		LogPortAudioDeviceList();
		sample_rate = 0;

		Log() << "Selecting input device";
		{
			const auto optionalInputDeviceIndex = SelectPortAudioDevice(hostApi.index, hostApi.info.defaultInputDevice, config.input.device);
			if (!optionalInputDeviceIndex.has_value()) throw ASIOException(ASE_HWMalfunction, "unable to select input device");
			input_device_index = *optionalInputDeviceIndex;
		}
		if (input_device_index != paNoDevice) {
			input_device_info = Pa_GetDeviceInfo(input_device_index);
			if (!input_device_info) throw ASIOException(ASE_HWMalfunction, "unable to get input device info");
		}

		if (input_device_info == nullptr) {
			Log() << "No input device, proceeding without input";
		}
		else {
			Log() << "Selected input device: " << input_device_info->name;
			input_channel_count = input_device_info->maxInputChannels;
			if (input_channel_count <= 0) throw ASIOException(ASE_HWMalfunction, "selected input device doesn't have any input channels (did you mean to select it as an output device?)");
			sample_rate = (std::max)(input_device_info->defaultSampleRate, sample_rate);
		}

		Log() << "Selecting output device";
		{
			const auto optionalOutputDeviceIndex = SelectPortAudioDevice(hostApi.index, hostApi.info.defaultOutputDevice, config.output.device);
			if (!optionalOutputDeviceIndex.has_value()) throw ASIOException(ASE_HWMalfunction, "unable to select output device");
			output_device_index = *optionalOutputDeviceIndex;
		}
		if (output_device_index != paNoDevice) {
			output_device_info = Pa_GetDeviceInfo(output_device_index);
			if (!output_device_info) throw ASIOException(ASE_HWMalfunction, "unable to get output device info");
		}

		if (output_device_info == nullptr) {
			Log() << "No output device, proceeding without output";
		}
		else {
			Log() << "Selected output device: " << output_device_info->name;
			output_channel_count = output_device_info->maxOutputChannels;
			if (output_channel_count <= 0) throw ASIOException(ASE_HWMalfunction, "selected output device doesn't have any input channels (did you mean to select it as an input device?)");
			sample_rate = (std::max)(output_device_info->defaultSampleRate, sample_rate);
		}

		if (input_device_info == nullptr && output_device_info == nullptr) throw ASIOException(ASE_HWMalfunction, "No usable input nor output devices");

		if (hostApi.info.type == paWASAPI)
		{
			// PortAudio has some WASAPI-specific goodies to make us smarter.
			if (input_device_index != paNoDevice) {
				WAVEFORMATEXTENSIBLE input_waveformat;
				PaError error = PaWasapi_GetDeviceDefaultFormat(&input_waveformat, sizeof(input_waveformat), input_device_index);
				if (error <= 0)
					Log() << "Unable to get WASAPI default format for input device";
				else
				{
					input_channel_count = input_waveformat.Format.nChannels;
					input_channel_mask = input_waveformat.dwChannelMask;
				}
			}

			if (output_device_index != paNoDevice) {
				WAVEFORMATEXTENSIBLE output_waveformat;
				const auto error = PaWasapi_GetDeviceDefaultFormat(&output_waveformat, sizeof(output_waveformat), output_device_index);
				if (error <= 0)
					Log() << "Unable to get WASAPI default format for output device";
				else
				{
					output_channel_count = output_waveformat.Format.nChannels;
					output_channel_mask = output_waveformat.dwChannelMask;
				}
			}
		}

		if (sample_rate == 0)
			sample_rate = 44100;

		Log() << "Initialized successfully";
	}

	FlexASIO::~FlexASIO()
	{
		if (started)
			Stop();
		if (buffers)
			DisposeBuffers();
	}

	void FlexASIO::GetChannels(long* numInputChannels, long* numOutputChannels)
	{
		*numInputChannels = input_channel_count;
		*numOutputChannels = output_channel_count;
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
			if (info->channel < 0 || info->channel >= input_channel_count) throw ASIOException(ASE_InvalidParameter, "no such input channel");
		}
		else
		{
			if (info->channel < 0 || info->channel >= output_channel_count) throw ASIOException(ASE_InvalidParameter, "no such output channel");
		}

		info->isActive = false;
		for (std::vector<ASIOBufferInfo>::const_iterator buffers_info_it = buffers_info.begin(); buffers_info_it != buffers_info.end(); ++buffers_info_it)
			if (buffers_info_it->isInput == info->isInput && buffers_info_it->channelNum == info->channel)
			{
				info->isActive = true;
				break;
			}

		info->channelGroup = 0;
		info->type = asio_sample_type;
		std::stringstream channel_string;
		channel_string << (info->isInput ? "IN" : "OUT") << " " << getChannelName(info->channel, info->isInput ? input_channel_mask : output_channel_mask);
		strcpy_s(info->name, 32, channel_string.str().c_str());
		Log() << "Returning: " << info->name << ", " << (info->isActive ? "active" : "inactive") << ", group " << info->channelGroup << ", type " << info->type;
	}

	PaError FlexASIO::OpenStream(PaStream** stream, double sampleRate, unsigned long framesPerBuffer)
	{
		Log() << "CFlexASIO::OpenStream(" << sampleRate << ", " << framesPerBuffer << ")";

		PaStreamParameters common_parameters = { 0 };
		common_parameters.sampleFormat = portaudio_sample_format | paNonInterleaved;
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
		if (input_device_info)
		{
			input_parameters.device = input_device_index;
			input_parameters.channelCount = input_channel_count;
			input_parameters.suggestedLatency = input_device_info->defaultLowInputLatency;
			if (hostApi.info.type == paWASAPI)
			{
				if (input_channel_mask != 0)
				{
					input_wasapi_stream_info.flags |= paWinWasapiUseChannelMask;
					input_wasapi_stream_info.channelMask = input_channel_mask;
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
		if (output_device_info)
		{
			output_parameters.device = output_device_index;
			output_parameters.channelCount = output_channel_count;
			output_parameters.suggestedLatency = output_device_info->defaultLowOutputLatency;
			if (hostApi.info.type == paWASAPI)
			{
				if (output_channel_mask != 0)
				{
					output_wasapi_stream_info.flags |= paWinWasapiUseChannelMask;
					output_wasapi_stream_info.channelMask = output_channel_mask;
				}
				Log() << "Using " << (config.output.wasapiExclusiveMode ? "exclusive" : "shared") << " mode for output WASAPI stream";
				if (config.output.wasapiExclusiveMode) {
					output_wasapi_stream_info.flags |= paWinWasapiExclusive;
				}
				output_parameters.hostApiSpecificStreamInfo = &output_wasapi_stream_info;
			}
		}

		return Pa_OpenStream(
			stream,
			input_device_info ? &input_parameters : NULL,
			output_device_info ? &output_parameters : NULL,
			sampleRate, framesPerBuffer, paNoFlag, &FlexASIO::StaticStreamCallback, this);
	}

	bool FlexASIO::CanSampleRate(ASIOSampleRate sampleRate)
	{
		Log() << "Checking for sample rate: " << sampleRate;

		PaStream* temp_stream;
		PaError error = OpenStream(&temp_stream, sampleRate, paFramesPerBufferUnspecified);
		if (error != paNoError)
		{
			Log() << "Cannot do this sample rate: " << Pa_GetErrorText(error);
			return false;
		}

		Log() << "Sample rate is available";
		Pa_CloseStream(temp_stream);
		return true;
	}

	void FlexASIO::GetSampleRate(ASIOSampleRate* sampleRate)
	{
		*sampleRate = sample_rate;
		Log() << "Returning sample rate: " << *sampleRate;
	}

	void FlexASIO::SetSampleRate(ASIOSampleRate sampleRate)
	{
		Log() << "Request to set sample rate: " << sampleRate;
		if (buffers)
		{
			if (!callbacks.asioMessage)
				throw ASIOException(ASE_InvalidMode, "changing the sample rate after createBuffers() is not supported");
			Log() << "Sending a reset request to the host as it's not possible to change sample rate when streaming";
			callbacks.asioMessage(kAsioResetRequest, 0, NULL, NULL);
		}
		sample_rate = sampleRate;
	}

	void FlexASIO::CreateBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks)
	{
		Log() << "Request to create buffers for " << numChannels << ", size " << bufferSize << " bytes";
		if (numChannels < 1 || bufferSize < 1 || !callbacks || !callbacks->bufferSwitch)
			throw ASIOException(ASE_InvalidParameter, "invalid parameters to createBuffers()");

		if (buffers) throw ASIOException(ASE_InvalidMode, "createBuffers() called multiple times");

		buffers_info.reserve(numChannels);
		std::unique_ptr<Buffers> temp_buffers(new Buffers(2, numChannels, bufferSize));
		Log() << "Buffers instantiated, memory range : " << temp_buffers->buffers << "-" << temp_buffers->buffers + temp_buffers->getSize();
		for (long channel_index = 0; channel_index < numChannels; ++channel_index)
		{
			ASIOBufferInfo& buffer_info = bufferInfos[channel_index];
			if (buffer_info.isInput)
			{
				if (buffer_info.channelNum < 0 || buffer_info.channelNum >= input_channel_count)
					throw ASIOException(ASE_InvalidParameter, "out of bounds input channel in createBuffers() buffer info");
			}
			else
			{
				if (buffer_info.channelNum < 0 || buffer_info.channelNum >= output_channel_count)
					throw ASIOException(ASE_InvalidParameter, "out of bounds output channel in createBuffers() buffer info");
			}

			Sample* first_half = temp_buffers->getBuffer(0, channel_index);
			Sample* second_half = temp_buffers->getBuffer(1, channel_index);
			buffer_info.buffers[0] = static_cast<void*>(first_half);
			buffer_info.buffers[1] = static_cast<void*>(second_half);
			Log() << "ASIO buffer #" << channel_index << " is " << (buffer_info.isInput ? "input" : "output") << " channel " << buffer_info.channelNum
				<< " - first half: " << first_half << "-" << first_half + bufferSize << " - second half: " << second_half << "-" << second_half + bufferSize;
			buffers_info.push_back(buffer_info);
		}


		Log() << "Opening PortAudio stream";
		if (sample_rate == 0)
		{
			sample_rate = 44100;
			Log() << "The sample rate was never specified, using " << sample_rate << " as fallback";
		}
		PaStream* temp_stream;
		PaError error = OpenStream(&temp_stream, sample_rate, unsigned long(temp_buffers->buffer_size));
		if (error != paNoError)
			throw ASIOException(ASE_HWMalfunction, std::string("Unable to open PortAudio stream: ") + Pa_GetErrorText(error));

		buffers = std::move(temp_buffers);
		stream = temp_stream;
		this->callbacks = *callbacks;
	}

	void FlexASIO::DisposeBuffers()
	{
		if (!buffers) throw ASIOException(ASE_InvalidMode, "disposeBuffers() called before createBuffers()");
		if (started) throw ASIOException(ASE_InvalidMode, "disposeBuffers() called before stop()");

		Log() << "Closing PortAudio stream";
		PaError error = Pa_CloseStream(stream);
		if (error != paNoError)
			throw ASIOException(ASE_HWMalfunction, std::string("unable to close PortAudio stream: ") + Pa_GetErrorText(error));

		stream = NULL;
		buffers.reset();
		buffers_info.clear();
	}

	void FlexASIO::GetLatencies(long* inputLatency, long* outputLatency)
	{
		if (!stream) throw ASIOException(ASE_InvalidMode, "getLatencies() called before createBuffers()");

		const PaStreamInfo* stream_info = Pa_GetStreamInfo(stream);
		if (!stream_info) throw ASIOException(ASE_HWMalfunction, "unable to get stream info");

		// See https://github.com/dechamps/FlexASIO/issues/10.
		// The latency that PortAudio reports appears to take the buffer size into account already.
		*inputLatency = (long)(stream_info->inputLatency * sample_rate);
		*outputLatency = (long)(stream_info->outputLatency * sample_rate);
		Log() << "Returning input latency of " << *inputLatency << " samples and output latency of " << *outputLatency << " samples";
	}

	void FlexASIO::Start()
	{
		if (!buffers) throw ASIOException(ASE_InvalidMode, "start() called before createBuffers()");
		if (started) throw ASIOException(ASE_InvalidMode, "start() called twice");

		host_supports_timeinfo = callbacks.asioMessage &&
			callbacks.asioMessage(kAsioSelectorSupported, kAsioSupportsTimeInfo, NULL, NULL) == 1 &&
			callbacks.asioMessage(kAsioSupportsTimeInfo, 0, NULL, NULL) == 1;
		if (host_supports_timeinfo)
			Log() << "The host supports time info";

		Log() << "Starting stream";
		win32HighResolutionTimer.emplace();
		started = true;
		our_buffer_index = 0;
		position = Int64ToASIO<ASIOSamples>(0);
		position_timestamp = Int64ToASIO<ASIOTimeStamp>(((long long int) win32HighResolutionTimer->GetTimeMilliseconds()) * 1000000);
		PaError error = Pa_StartStream(stream);
		if (error != paNoError) throw ASIOException(ASE_HWMalfunction, std::string("unable to start PortAudio stream: ") + Pa_GetErrorText(error));

		Log() << "Started successfully";
	}

	void FlexASIO::Stop()
	{
		if (!started) throw ASIOException(ASE_InvalidMode, "stop() called before start()");

		Log() << "Stopping stream";
		PaError error = Pa_StopStream(stream);
		if (error != paNoError) throw ASIOException(ASE_HWMalfunction, std::string("unable to stop PortAudio stream: ") + Pa_GetErrorText(error));

		started = false;
		win32HighResolutionTimer.reset();
		Log() << "Stopped successfully";
	}

	int FlexASIO::StaticStreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw() {
		Log() << "--- ENTERING STREAM CALLBACK";
		PaStreamCallbackResult result = paContinue;
		try {
			result = static_cast<FlexASIO*>(userData)->StreamCallback(input, output, frameCount, timeInfo, statusFlags);
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

	PaStreamCallbackResult FlexASIO::StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags)
	{
		Log() << "Running stream callback with " << frameCount << " frames";
		if (!started)
		{
			Log() << "Ignoring callback as stream is not started";
			return paContinue;
		}
		if (frameCount != buffers->buffer_size)
		{
			Log() << "Expected " << buffers->buffer_size << " frames, got " << frameCount << " instead, aborting";
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

		const Sample* const* input_samples = static_cast<const Sample* const*>(input);
		Sample* const* output_samples = static_cast<Sample* const*>(output);

		for (int output_channel_index = 0; output_channel_index < output_channel_count; ++output_channel_index)
			memset(output_samples[output_channel_index], 0, frameCount * sizeof(Sample));

		size_t locked_buffer_index = (our_buffer_index + 1) % 2; // The host is currently busy with locked_buffer_index and is not touching our_buffer_index.
		Log() << "Transferring between PortAudio and buffer #" << our_buffer_index;
		for (std::vector<ASIOBufferInfo>::const_iterator buffers_info_it = buffers_info.begin(); buffers_info_it != buffers_info.end(); ++buffers_info_it)
		{
			Sample* buffer = reinterpret_cast<Sample*>(buffers_info_it->buffers[our_buffer_index]);
			if (buffers_info_it->isInput)
				memcpy(buffer, input_samples[buffers_info_it->channelNum], frameCount * sizeof(Sample));
			else
				memcpy(output_samples[buffers_info_it->channelNum], buffer, frameCount * sizeof(Sample));
		}

		if (!host_supports_timeinfo)
		{
			Log() << "Firing ASIO bufferSwitch() callback";
			callbacks.bufferSwitch(long(our_buffer_index), ASIOFalse);
		}
		else
		{
			ASIOTime time = { 0 };
			time.timeInfo.flags = kSystemTimeValid | kSamplePositionValid | kSampleRateValid;
			time.timeInfo.samplePosition = position;
			time.timeInfo.systemTime = position_timestamp;
			time.timeInfo.sampleRate = sample_rate;
			Log() << "Firing ASIO bufferSwitchTimeInfo() callback with samplePosition " << ASIOToInt64(time.timeInfo.samplePosition) << ", systemTime " << ASIOToInt64(time.timeInfo.systemTime);
			callbacks.bufferSwitchTimeInfo(&time, long(our_buffer_index), ASIOFalse);
		}
		std::swap(locked_buffer_index, our_buffer_index);
		position = Int64ToASIO<ASIOSamples>(ASIOToInt64(position) + frameCount);
		position_timestamp = Int64ToASIO<ASIOTimeStamp>(((long long int) win32HighResolutionTimer->GetTimeMilliseconds()) * 1000000);

		return paContinue;
	}

	void FlexASIO::GetSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp)
	{
		if (!started) throw ASIOException(ASE_InvalidMode, "getSamplePosition() called before start()");

		*sPos = position;
		*tStamp = position_timestamp;
		Log() << "Returning: sample position " << ASIOToInt64(position) << ", timestamp " << ASIOToInt64(position_timestamp);
	}

	void FlexASIO::ControlPanel() {
		const auto url = std::string("https://github.com/dechamps/FlexASIO/blob/") + gitstr + "/CONFIGURATION.md";
		Log() << "Opening URL: " << url;
		const auto result = ShellExecuteA(windowHandle, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
		Log() << "ShellExecuteA() result: " << result;
	}

}

