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
#include <cctype>
#include <vector>

#include <atlbase.h>
#include <atlcom.h>

#include <MMReg.h>

#include "portaudio.h"
#include "pa_win_wasapi.h"

#include "..\ASIOSDK2.3.1\common\iasiodrv.h"

#include "flexasio.rc.h"
#include "config.h"
#include "log.h"
#include "flexasio_h.h"
#include "../FlexASIOUtil/version.h"
#include "../FlexASIOUtil/asio.h"
#include "../FlexASIOUtil/string.h"

// From pa_debugprint.h. The PortAudio DLL exports this function, but sadly it is not exposed in a public header file.
extern "C" {
	typedef void(*PaUtilLogCallback) (const char *log);
	extern void PaUtil_SetDebugPrintFunction(PaUtilLogCallback cb);
}

// Provide a definition for the ::CFlexASIO class declaration that the MIDL compiler generated.
// The actual implementation is in a derived class in an anonymous namespace, as it should be.
//
// Note: ASIO doesn't use COM properly, and doesn't define a proper interface.
// Instead, it uses the CLSID to create an instance and then blindfully casts it to IASIO, giving the finger to QueryInterface() and to sensible COM design in general.
// Of course, since this is a blind cast, the order of inheritance below becomes critical: if IASIO is not first, the cast is likely to produce a wrong vtable offset, crashing the whole thing. What a nice design.
class CFlexASIO : public IASIO, public IFlexASIO {};

namespace flexasio {
	namespace {

		class PortAudioLogger final {
		public:
			PortAudioLogger() {
				std::scoped_lock lock(mutex);
				if (referenceCount++ > 0) return;
				Log() << "Enabling PortAudio debug output redirection";
				PaUtil_SetDebugPrintFunction(DebugPrint);
			}

			~PortAudioLogger() {
				std::scoped_lock lock(mutex);
				if (--referenceCount > 0) return;
				Log() << "Disabling PortAudio debug output redirection";
				PaUtil_SetDebugPrintFunction(NULL);
			}

		private:
			static void DebugPrint(const char* log) {
				std::string_view logline(log);
				while (!logline.empty() && isspace(logline.back())) logline.remove_suffix(1);
				Log() << "[PortAudio] " << logline;
			}

			static std::mutex mutex;
			static size_t referenceCount;
		};

		std::mutex PortAudioLogger::mutex;
		size_t PortAudioLogger::referenceCount = 0;

		class Win32HighResolutionTimer {
		public:
			Win32HighResolutionTimer() {
				Log() << "Starting high resolution timer";
				timeBeginPeriod(1);
			}
			Win32HighResolutionTimer(const Win32HighResolutionTimer&) = delete;
			Win32HighResolutionTimer(Win32HighResolutionTimer&&) = delete;
			~Win32HighResolutionTimer() {
				Log() << "Stopping high resolution timer";
				timeEndPeriod(1);
			}

			DWORD GetTimeMilliseconds() const { return timeGetTime(); }
		};

		const PaSampleFormat portaudio_sample_format = paFloat32;
		const ASIOSampleType asio_sample_type = ASIOSTFloat32LSB;
		typedef float Sample;

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

		class CFlexASIO :
			public ::CFlexASIO,
			public CComObjectRootEx<CComMultiThreadModel>,
			public CComCoClass<CFlexASIO, &__uuidof(::CFlexASIO)>
		{
			BEGIN_COM_MAP(CFlexASIO)
				COM_INTERFACE_ENTRY(IFlexASIO)

				// To add insult to injury, ASIO mistakes the CLSID for an IID when calling CoCreateInstance(). Yuck.
				COM_INTERFACE_ENTRY(::CFlexASIO)

				// IASIO doesn't have an IID (see above), which is why it doesn't appear here.
			END_COM_MAP()

			DECLARE_REGISTRY_RESOURCEID(IDR_FLEXASIO)

		public:
			CFlexASIO() throw();
			~CFlexASIO() throw();

			// IASIO implementation

			ASIOBool init(void* sysHandle) throw() final;
			void getDriverName(char* name) throw() final { Log() << "CFlexASIO::getDriverName()"; strcpy_s(name, 32, "FlexASIO"); }
			long getDriverVersion() throw() final { Log() << "CFlexASIO::getDriverVersion()"; return 0; }
			void getErrorMessage(char* string) throw() final { Log() << "CFlexASIO::getErrorMessage()"; strcpy_s(string, 124, init_error.c_str()); }

			ASIOError getClockSources(ASIOClockSource* clocks, long* numSources) throw() final;
			ASIOError setClockSource(long reference) throw() final;
			ASIOError getChannels(long* numInputChannels, long* numOutputChannels) throw() final;
			ASIOError getChannelInfo(ASIOChannelInfo* info) throw() final;
			ASIOError getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) throw() final;
			ASIOError canSampleRate(ASIOSampleRate sampleRate) throw() final;
			ASIOError setSampleRate(ASIOSampleRate sampleRate) throw() final;
			ASIOError getSampleRate(ASIOSampleRate* sampleRate) throw() final;

			ASIOError createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) throw() final;
			ASIOError disposeBuffers() throw() final;
			ASIOError getLatencies(long* inputLatency, long* outputLatency) throw() final;

			ASIOError start() throw() final;
			ASIOError stop() throw() final;
			ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) throw() final;

			// Not implemented
			ASIOError controlPanel() throw() final { Log() << "CFlexASIO::controlPanel()"; return ASE_NotPresent; }
			ASIOError future(long selector, void *opt) throw() final { Log() << "CFlexASIO::future()"; return ASE_InvalidParameter; }
			ASIOError outputReady() throw() final { Log() << "CFlexASIO::outputReady()"; return ASE_NotPresent; }

		private:
			PaError OpenStream(PaStream**, double sampleRate, unsigned long framesPerBuffer) throw();
			static int StaticStreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw() { return static_cast<CFlexASIO*>(userData)->StreamCallback(input, output, frameCount, timeInfo, statusFlags); }
			int StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags) throw();

			PortAudioLogger portAudioLogger;

			std::optional<Config> config;
			bool portaudio_initialized;
			std::string init_error;

			const PaHostApiInfo* pa_api_info;
			const PaDeviceInfo* input_device_info;
			const PaDeviceInfo* output_device_info;
			long input_channel_count;
			long output_channel_count;
			// WAVEFORMATEXTENSIBLE channel masks. Not always available.
			DWORD input_channel_mask;
			DWORD output_channel_mask;

			ASIOSampleRate sample_rate;

			// PortAudio buffer addresses are dynamic and are only valid for the duration of the stream callback.
			// In contrast, ASIO buffer addresses are static and are valid for as long as the stream is running.
			// Thus we need our own buffer on top of PortAudio's buffers. This doens't add any latency because buffers are copied immediately.
			std::unique_ptr<Buffers> buffers;
			std::vector<ASIOBufferInfo> buffers_info;
			ASIOCallbacks callbacks;

			PaStream* stream;
			bool host_supports_timeinfo;
			// The index of the "unlocked" buffer (or "half-buffer", i.e. 0 or 1) that contains data not currently being processed by the ASIO host.
			size_t our_buffer_index;
			ASIOSamples position;
			ASIOTimeStamp position_timestamp;
			bool started;
			std::optional<Win32HighResolutionTimer> win32HighResolutionTimer;
		};

		OBJECT_ENTRY_AUTO(__uuidof(::CFlexASIO), CFlexASIO);

		std::string GetModuleName() {
			std::string moduleName(MAX_PATH, 0);
			moduleName.resize(GetModuleFileNameA(NULL, moduleName.data(), DWORD(moduleName.size())));
			return moduleName;
		}

		CFlexASIO::CFlexASIO() throw() :
			portaudio_initialized(false), init_error(""), pa_api_info(nullptr),
			input_device_info(nullptr), output_device_info(nullptr),
			input_channel_count(0), output_channel_count(0),
			input_channel_mask(0), output_channel_mask(0),
			sample_rate(0), buffers(nullptr), stream(NULL), started(false)
		{
			Log() << "CFlexASIO::CFlexASIO()";
			Log() << "FlexASIO " << BUILD_CONFIGURATION << " " << BUILD_PLATFORM << " " << version << " built on " << buildTime;
			Log() << "Host process: " << GetModuleName();
			// Note: we're supposed to use Pa_GetVersionInfo(), but sadly, it looks like it's not exported from the PortAudio DLL.
			Log() << "PortAudio version: " << Pa_GetVersionText();
		}

		void LogPortAudioApiList() {
			const auto pa_api_count = Pa_GetHostApiCount();
			for (PaHostApiIndex pa_api_index = 0; pa_api_index < pa_api_count; ++pa_api_index) {
				const auto pa_api_info = Pa_GetHostApiInfo(pa_api_index);
				Log() << "PortAudio host API backend at index " << pa_api_index << ": " << ((pa_api_info != nullptr) ? pa_api_info->name : "(null)");
			}
		}

		const PaHostApiInfo* SelectDefaultPortAudioApi() {
			Log() << "Selecting default PortAudio host API";
			// The default API used by PortAudio is WinMME. It's also the worst one.
			// The following attempts to get a better API (in order of preference).
			auto pa_api_index = Pa_HostApiTypeIdToHostApiIndex(paWASAPI);
			if (pa_api_index == paHostApiNotFound)
				pa_api_index = Pa_HostApiTypeIdToHostApiIndex(paDirectSound);
			if (pa_api_index == paHostApiNotFound)
				pa_api_index = Pa_GetDefaultHostApi();
			if (pa_api_index < 0)
			{
				Log() << "Unable to select a default PortAudio host API backend: " << Pa_GetErrorText(pa_api_index);
				return nullptr;
			}
			Log() << "Selecting PortAudio host API index " << pa_api_index;
			return Pa_GetHostApiInfo(pa_api_index);
		}

		const PaHostApiInfo* SelectPortAudioApiByName(std::string_view name) {
			Log() << "Searching for a PortAudio host API named '" << name << "'";
			const auto pa_api_count = Pa_GetHostApiCount();
			const PaHostApiInfo* pa_api_info = nullptr;

			for (PaHostApiIndex pa_api_index = 0; pa_api_index < pa_api_count; ++pa_api_index) {
				pa_api_info = Pa_GetHostApiInfo(pa_api_index);
				if (pa_api_info == nullptr) {
					Log() << "Unable to get PortAudio API info for API index " << pa_api_index;
					continue;
				}
				// TODO: the comparison should be case insensitive.
				if (pa_api_info->name == name) {
					Log() << "Found host API at index " << pa_api_index;
					break;
				}
				pa_api_info = nullptr;
			}

			if (pa_api_info == nullptr)
				Log() << "Unable to find a PortAudio host API backend named '" << name << "'";

			return pa_api_info;
		}

		ASIOBool CFlexASIO::init(void* sysHandle) throw()
		{
			Log() << "CFlexASIO::init()";
			if (input_device_info || output_device_info)
			{
				Log() << "Already initialized";
				return ASE_NotPresent;
			}

			config = LoadConfig();
			if (!config.has_value()) {
				init_error = "Could not load FlexASIO configuration. See FlexASIO log for details.";
				Log() << "Refusing to initialize due to configuration errors";
				return ASIOFalse;
			}

			Log() << "Initializing PortAudio";
			PaError error = Pa_Initialize();
			if (error != paNoError)
			{
				init_error = std::string("Could not initialize PortAudio: ") + Pa_GetErrorText(error);
				Log() << init_error;
				return ASIOFalse;
			}
			portaudio_initialized = true;

			LogPortAudioApiList();
			pa_api_info = config->backend.has_value() ? SelectPortAudioApiByName(*config->backend) : SelectDefaultPortAudioApi();
			if (pa_api_info == nullptr) {
				init_error = "Unable to select PortAudio host API backend";
				Log() << init_error;
				return ASIOFalse;
			}

			Log() << "Selected PortAudio host API backend: " << pa_api_info->name;

			sample_rate = 0;

			Log() << "Getting input device info";
			if (pa_api_info->defaultInputDevice != paNoDevice)
			{
				input_device_info = Pa_GetDeviceInfo(pa_api_info->defaultInputDevice);
				if (!input_device_info)
				{
					init_error = std::string("Unable to get input device info");
					Log() << init_error;
					return ASIOFalse;
				}
				Log() << "Selected input device: " << input_device_info->name;
				input_channel_count = input_device_info->maxInputChannels;
				sample_rate = (std::max)(input_device_info->defaultSampleRate, sample_rate);
			}

			Log() << "Getting output device info";
			if (pa_api_info->defaultOutputDevice != paNoDevice)
			{
				output_device_info = Pa_GetDeviceInfo(pa_api_info->defaultOutputDevice);
				if (!output_device_info)
				{
					init_error = std::string("Unable to get output device info");
					Log() << init_error;
					return ASIOFalse;
				}
				Log() << "Selected output device: " << output_device_info->name;
				output_channel_count = output_device_info->maxOutputChannels;
				sample_rate = (std::max)(output_device_info->defaultSampleRate, sample_rate);
			}

			if (pa_api_info->type == paWASAPI)
			{
				// PortAudio has some WASAPI-specific goodies to make us smarter.
				WAVEFORMATEXTENSIBLE input_waveformat;
				PaError error = PaWasapi_GetDeviceDefaultFormat(&input_waveformat, sizeof(input_waveformat), pa_api_info->defaultInputDevice);
				if (error <= 0)
					Log() << "Unable to get WASAPI default format for input device";
				else
				{
					input_channel_count = input_waveformat.Format.nChannels;
					input_channel_mask = input_waveformat.dwChannelMask;
				}

				WAVEFORMATEXTENSIBLE output_waveformat;
				error = PaWasapi_GetDeviceDefaultFormat(&output_waveformat, sizeof(output_waveformat), pa_api_info->defaultOutputDevice);
				if (error <= 0)
					Log() << "Unable to get WASAPI default format for output device";
				else
				{
					output_channel_count = output_waveformat.Format.nChannels;
					output_channel_mask = output_waveformat.dwChannelMask;
				}
			}

			if (sample_rate == 0)
				sample_rate = 44100;

			Log() << "Initialized successfully";
			return ASIOTrue;
		}

		CFlexASIO::~CFlexASIO() throw()
		{
			Log() << "CFlexASIO::~CFlexASIO()";
			if (started)
				stop();
			if (buffers)
				disposeBuffers();
			if (portaudio_initialized)
			{
				Log() << "Closing PortAudio";
				PaError error = Pa_Terminate();
				if (error != paNoError)
					Log() << "Pa_Terminate() returned " << Pa_GetErrorText(error) << "!";
				else
					Log() << "PortAudio closed successfully";
			}
		}

		ASIOError CFlexASIO::getClockSources(ASIOClockSource* clocks, long* numSources) throw()
		{
			Log() << "CFlexASIO::getClockSources()";
			if (!clocks || !numSources || *numSources < 1)
			{
				Log() << "Invalid parameters";
				return ASE_NotPresent;
			}

			clocks->index = 0;
			clocks->associatedChannel = -1;
			clocks->associatedGroup = -1;
			clocks->isCurrentSource = ASIOTrue;
			strcpy_s(clocks->name, 32, "Internal");
			*numSources = 1;
			return ASE_OK;
		}

		ASIOError CFlexASIO::setClockSource(long reference) throw()
		{
			Log() << "CFlexASIO::setClockSource(" << reference << ")";
			if (reference != 0)
			{
				Log() << "Parameter out of bounds";
				return ASE_InvalidMode;
			}
			return ASE_OK;
		}

		ASIOError CFlexASIO::getChannels(long* numInputChannels, long* numOutputChannels) throw()
		{
			Log() << "CFlexASIO::getChannels()";
			if (!input_device_info && !output_device_info)
			{
				Log() << "getChannels() called in unitialized state";
				return ASE_NotPresent;
			}

			*numInputChannels = input_channel_count;
			*numOutputChannels = output_channel_count;

			Log() << "Returning " << *numInputChannels << " input channels and " << *numOutputChannels << " output channels";
			return ASE_OK;
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

		ASIOError CFlexASIO::getChannelInfo(ASIOChannelInfo* info) throw()
		{
			Log() << "CFlexASIO::getChannelInfo()";

			Log() << "Channel info requested for " << (info->isInput ? "input" : "output") << " channel " << info->channel;
			if (info->isInput)
			{
				if (info->channel < 0 || info->channel >= input_channel_count)
				{
					Log() << "No such input channel, returning error";
					return ASE_NotPresent;
				}
			}
			else
			{
				if (info->channel < 0 || info->channel >= output_channel_count)
				{
					Log() << "No such output channel, returning error";
					return ASE_NotPresent;
				}
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
			return ASE_OK;
		}

		ASIOError CFlexASIO::getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) throw()
		{
			// These values are purely arbitrary, since PortAudio doesn't provide them. Feel free to change them if you'd like.
			// TODO: let the user should these values
			Log() << "CFlexASIO::getBufferSize()";
			*minSize = 48; // 1 ms at 48kHz, there's basically no chance we'll get glitch-free streaming below this
			*maxSize = 48000; // 1 second at 48kHz, more would be silly
			*preferredSize = 1024; // typical - 21.3 ms at 48kHz
			*granularity = 1; // Don't care
			Log() << "Returning: min buffer size " << *minSize << ", max buffer size " << *maxSize << ", preferred buffer size " << *preferredSize << ", granularity " << *granularity;
			return ASE_OK;
		}

		PaError CFlexASIO::OpenStream(PaStream** stream, double sampleRate, unsigned long framesPerBuffer) throw()
		{
			Log() << "CFlexASIO::OpenStream(" << sampleRate << ", " << framesPerBuffer << ")";

			PaStreamParameters common_parameters = { 0 };
			common_parameters.sampleFormat = portaudio_sample_format | paNonInterleaved;
			common_parameters.hostApiSpecificStreamInfo = NULL;

			PaWasapiStreamInfo common_wasapi_stream_info = { 0 };
			if (pa_api_info->type == paWASAPI) {
				common_wasapi_stream_info.size = sizeof(common_wasapi_stream_info);
				common_wasapi_stream_info.hostApiType = paWASAPI;
				common_wasapi_stream_info.version = 1;
				common_wasapi_stream_info.flags = 0;
				Log() << "Opening WASAPI stream in " << (config->wasapiExclusiveMode ? "exclusive" : "shared") << " mode";
				if (config->wasapiExclusiveMode) {
					common_wasapi_stream_info.flags |= paWinWasapiExclusive;
				}
			}

			PaStreamParameters input_parameters = common_parameters;
			PaWasapiStreamInfo input_wasapi_stream_info = common_wasapi_stream_info;
			if (input_device_info)
			{
				input_parameters.device = pa_api_info->defaultInputDevice;
				input_parameters.channelCount = input_channel_count;
				input_parameters.suggestedLatency = input_device_info->defaultLowInputLatency;
				if (pa_api_info->type == paWASAPI)
				{
					if (input_channel_mask != 0)
					{
						input_wasapi_stream_info.flags |= paWinWasapiUseChannelMask;
						input_wasapi_stream_info.channelMask = input_channel_mask;
					}
					input_parameters.hostApiSpecificStreamInfo = &input_wasapi_stream_info;
				}
			}

			PaStreamParameters output_parameters = common_parameters;
			PaWasapiStreamInfo output_wasapi_stream_info = common_wasapi_stream_info;
			if (output_device_info)
			{
				output_parameters.device = pa_api_info->defaultOutputDevice;
				output_parameters.channelCount = output_channel_count;
				output_parameters.suggestedLatency = output_device_info->defaultLowOutputLatency;
				if (pa_api_info->type == paWASAPI)
				{
					if (output_channel_mask != 0)
					{
						output_wasapi_stream_info.flags |= paWinWasapiUseChannelMask;
						output_wasapi_stream_info.channelMask = output_channel_mask;
					}
					output_parameters.hostApiSpecificStreamInfo = &output_wasapi_stream_info;
				}
			}

			return Pa_OpenStream(
				stream,
				input_device_info ? &input_parameters : NULL,
				output_device_info ? &output_parameters : NULL,
				sampleRate, framesPerBuffer, paNoFlag, &CFlexASIO::StaticStreamCallback, this);
		}

		ASIOError CFlexASIO::canSampleRate(ASIOSampleRate sampleRate) throw()
		{
			Log() << "CFlexASIO::canSampleRate(" << sampleRate << ")";
			if (!input_device_info && !output_device_info)
			{
				Log() << "canSampleRate() called in unitialized state";
				return ASE_NotPresent;
			}

			PaStream* temp_stream;
			PaError error = OpenStream(&temp_stream, sampleRate, paFramesPerBufferUnspecified);
			if (error != paNoError)
			{
				init_error = std::string("Cannot do this sample rate: ") + Pa_GetErrorText(error);
				Log() << init_error;
				return ASE_NoClock;
			}

			Log() << "Sample rate is available";
			Pa_CloseStream(temp_stream);
			return ASE_OK;
		}

		ASIOError CFlexASIO::getSampleRate(ASIOSampleRate* sampleRate) throw()
		{
			Log() << "CFlexASIO::getSampleRate()";
			if (sample_rate == 0)
			{
				Log() << "getSampleRate() called in unitialized state";
				return ASE_NoClock;
			}
			*sampleRate = sample_rate;
			Log() << "Returning sample rate: " << *sampleRate;
			return ASE_OK;
		}

		ASIOError CFlexASIO::setSampleRate(ASIOSampleRate sampleRate) throw()
		{
			Log() << "CFlexASIO::setSampleRate(" << sampleRate << ")";
			if (buffers)
			{
				if (callbacks.asioMessage)
				{
					Log() << "Sending a reset request to the host as it's not possible to change sample rate when streaming";
					callbacks.asioMessage(kAsioResetRequest, 0, NULL, NULL);
					return ASE_OK;
				}
				else
				{
					Log() << "Changing the sample rate after createBuffers() is not supported";
					return ASE_NotPresent;
				}
			}
			sample_rate = sampleRate;
			return ASE_OK;
		}

		ASIOError CFlexASIO::createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) throw()
		{
			Log() << "CFlexASIO::createBuffers(" << numChannels << ", " << bufferSize << ")";
			if (numChannels < 1 || bufferSize < 1 || !callbacks || !callbacks->bufferSwitch)
			{
				Log() << "Invalid invocation";
				return ASE_InvalidMode;
			}
			if (!input_device_info && !output_device_info)
			{
				Log() << "createBuffers() called in unitialized state";
				return ASE_InvalidMode;
			}
			if (buffers)
			{
				Log() << "createBuffers() called twice";
				return ASE_InvalidMode;
			}

			buffers_info.reserve(numChannels);
			std::unique_ptr<Buffers> temp_buffers(new Buffers(2, numChannels, bufferSize));
			Log() << "Buffers instantiated, memory range : " << temp_buffers->buffers << "-" << temp_buffers->buffers + temp_buffers->getSize();
			for (long channel_index = 0; channel_index < numChannels; ++channel_index)
			{
				ASIOBufferInfo& buffer_info = bufferInfos[channel_index];
				if (buffer_info.isInput)
				{
					if (buffer_info.channelNum < 0 || buffer_info.channelNum >= input_channel_count)
					{
						Log() << "out of bounds input channel";
						return ASE_InvalidMode;
					}
				}
				else
				{
					if (buffer_info.channelNum < 0 || buffer_info.channelNum >= output_channel_count)
					{
						Log() << "out of bounds output channel";
						return ASE_InvalidMode;
					}
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
			{
				init_error = std::string("Unable to open PortAudio stream: ") + Pa_GetErrorText(error);
				Log() << init_error;
				return ASE_HWMalfunction;
			}

			buffers = std::move(temp_buffers);
			stream = temp_stream;
			this->callbacks = *callbacks;
			return ASE_OK;
		}

		ASIOError CFlexASIO::disposeBuffers() throw()
		{
			Log() << "CFlexASIO::disposeBuffers()";
			if (!buffers)
			{
				Log() << "disposeBuffers() called before createBuffers()";
				return ASE_InvalidMode;
			}
			if (started)
			{
				Log() << "disposeBuffers() called before stop()";
				return ASE_InvalidMode;
			}

			Log() << "Closing PortAudio stream";
			PaError error = Pa_CloseStream(stream);
			if (error != paNoError)
			{
				init_error = std::string("Unable to close PortAudio stream: ") + Pa_GetErrorText(error);
				Log() << init_error;
				return ASE_NotPresent;
			}
			stream = NULL;

			buffers.reset();
			buffers_info.clear();
			return ASE_OK;
		}

		ASIOError CFlexASIO::getLatencies(long* inputLatency, long* outputLatency) throw()
		{
			Log() << "CFlexASIO::getLatencies()";
			if (!stream)
			{
				Log() << "getLatencies() called before createBuffers()";
				return ASE_NotPresent;
			}

			const PaStreamInfo* stream_info = Pa_GetStreamInfo(stream);
			if (!stream_info)
			{
				Log() << "Unable to get stream info";
				return ASE_NotPresent;
			}

			// TODO: should we add the buffer size?
			*inputLatency = (long)(stream_info->inputLatency * sample_rate);
			*outputLatency = (long)(stream_info->outputLatency * sample_rate);
			Log() << "Returning input latency of " << *inputLatency << " samples and output latency of " << *outputLatency << " samples";
			return ASE_OK;
		}

		ASIOError CFlexASIO::start() throw()
		{
			Log() << "CFlexASIO::start()";
			if (!buffers)
			{
				Log() << "start() called before createBuffers()";
				return ASE_NotPresent;
			}
			if (started)
			{
				Log() << "start() called twice";
				return ASE_NotPresent;
			}

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
			if (error != paNoError)
			{
				started = false;
				init_error = std::string("Unable to start PortAudio stream: ") + Pa_GetErrorText(error);
				Log() << init_error;
				return ASE_HWMalfunction;
			}

			Log() << "Started successfully";
			return ASE_OK;
		}

		ASIOError CFlexASIO::stop() throw()
		{
			Log() << "CFlexASIO::stop()";
			if (!started)
			{
				Log() << "stop() called before start()";
				return ASE_NotPresent;
			}

			Log() << "Stopping stream";
			PaError error = Pa_StopStream(stream);
			if (error != paNoError)
			{
				init_error = std::string("Unable to stop PortAudio stream: ") + Pa_GetErrorText(error);
				Log() << init_error;
				return ASE_NotPresent;
			}

			started = false;
			win32HighResolutionTimer.reset();
			Log() << "Stopped successfully";
			return ASE_OK;
		}

		int CFlexASIO::StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags) throw()
		{
			Log() << "CFlexASIO::StreamCallback(" << frameCount << ")";
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

			Log() << "Returning from stream callback";
			return paContinue;
		}

		ASIOError CFlexASIO::getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) throw()
		{
			Log() << "CFlexASIO::getSamplePosition()";
			if (!started)
			{
				Log() << "getSamplePosition() called before start()";
				return ASE_SPNotAdvancing;
			}

			*sPos = position;
			*tStamp = position_timestamp;
			Log() << "Returning: sample position " << ASIOToInt64(position) << ", timestamp " << ASIOToInt64(position_timestamp);
			return ASE_OK;
		}

	}
}

IASIO* CreateFlexASIO() {
	::CFlexASIO* flexASIO = nullptr;
	assert(::flexasio::CFlexASIO::CreateInstance(&flexASIO) == S_OK);
	assert(flexASIO != nullptr);
	return flexASIO;
}

void ReleaseFlexASIO(IASIO* const iASIO) {
	assert(iASIO != nullptr);
	iASIO->Release();
}
