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

#include "asiobridge.h"

CASIOBridge::CASIOBridge() :
	portaudio_initialized(false), init_error(""),
	input_device_index(0), input_device_info(nullptr),
	output_device_index(0), output_device_info(nullptr),
	sample_rate(0), buffers(nullptr), stream(NULL), started(false)
{
	Log() << "CASIOBridge::CASIOBridge()";
}

ASIOBool CASIOBridge::init(void* sysHandle)
{
	Log() << "CASIOBridge::init()";
	if (input_device_info || output_device_info)
	{
		Log() << "Already initialized";
		return ASE_NotPresent;
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

	Log() << "Getting input/output device indexes";
	input_device_index = Pa_GetDefaultInputDevice();
	output_device_index = Pa_GetDefaultOutputDevice();
	if (input_device_index == paNoDevice && output_device_index == paNoDevice)
	{
		init_error = "No input nor output device found";
		Log() << init_error;
		return ASIOFalse;
	}

	sample_rate = 0;

	Log() << "Getting input device info";
	if (input_device_index != paNoDevice)
	{
		input_device_info = Pa_GetDeviceInfo(input_device_index);
		if (!input_device_info)
		{
			init_error = std::string("Unable to get input device info");
			Log() << init_error;
			return ASIOFalse;
		}
		sample_rate = (std::max)(input_device_info->defaultSampleRate, sample_rate);
	}

	Log() << "Getting output device info";
	if (output_device_index != paNoDevice)
	{
		output_device_info = Pa_GetDeviceInfo(output_device_index);
		if (!output_device_info)
		{
			init_error = std::string("Unable to get output device info");
			Log() << init_error;
			return ASIOFalse;
		}
		sample_rate = (std::max)(output_device_info->defaultSampleRate, sample_rate);
	}

	if (sample_rate == 0)
		sample_rate = 44100;

	Log() << "Initialized successfully";
	return ASIOTrue;
}

CASIOBridge::~CASIOBridge()
{
	Log() << "CASIOBridge::~CASIOBridge()";
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

ASIOError CASIOBridge::getChannels(long* numInputChannels, long* numOutputChannels)
{
	Log() << "CASIOBridge::getChannels()";
	if (!input_device_info && !output_device_info)
	{
		Log() << "getChannels() called in unitialized state";
		return ASE_NotPresent;
	}

	if (!input_device_info)
		*numInputChannels = 0;
	else
		*numInputChannels = input_device_info->maxInputChannels;

	if (!output_device_info)
		*numOutputChannels = 0;
	else
		*numOutputChannels = output_device_info->maxOutputChannels;

	Log() << "Returning " << *numInputChannels << " input channels and " << *numOutputChannels << " output channels";
	return ASE_OK;
}

ASIOError CASIOBridge::getChannelInfo(ASIOChannelInfo* info)
{
	Log() << "CASIOBridge::getChannelInfo()";

	Log() << "Channel info requested for " << (info->isInput ? "input" : "output") << " channel " << info->channel;
	if (info->isInput)
	{
		if (!input_device_info || info->channel > input_device_info->maxInputChannels)
		{
			Log() << "No such input channel, returning error";
			return ASE_NotPresent;
		}
	}
	else
	{
		if (!output_device_info || info->channel > output_device_info->maxOutputChannels)
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
	channel_string << (info->isInput ? "Input" : "Output") << " " << info->channel;
	strcpy_s(info->name, 32, channel_string.str().c_str());
	Log() << "Returning: " << info->name << ", " << (info->isActive ? "active" : "inactive") << ", group " << info->channelGroup << ", type " << info->type;
	return ASE_OK;
}

ASIOError CASIOBridge::getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity)
{
	// These values are purely arbitrary, since PortAudio doesn't provide them. Feel free to change them if you'd like.
	// TODO: let the user should these values
	Log() << "CASIOBridge::getBufferSize()";
	*minSize = 48; // 1 ms at 48kHz, there's basically no chance we'll get glitch-free streaming below this
	*maxSize = 48000; // 1 second at 48kHz, more would be silly
	*preferredSize = 1024; // typical - 21.3 ms at 48kHz
	*granularity = 1; // Don't care
	Log() << "Returning: min buffer size " << *minSize << ", max buffer size " << *maxSize << ", preferred buffer size " << *preferredSize << ", granularity " << *granularity;
	return ASE_OK;
}

PaError CASIOBridge::OpenStream(PaStream** stream, double sampleRate, unsigned long framesPerBuffer) throw()
{
	Log() << "CASIOBridge::OpenStream(" << sampleRate << ", " << framesPerBuffer << ")";

	PaStreamParameters input_parameters;
	if (input_device_info)
	{
		input_parameters.device = input_device_index;
		input_parameters.channelCount = input_device_info->maxInputChannels;
		input_parameters.sampleFormat = portaudio_sample_format | paNonInterleaved;
		input_parameters.suggestedLatency = input_device_info->defaultLowInputLatency;
		input_parameters.hostApiSpecificStreamInfo = NULL;
	}

	PaStreamParameters output_parameters;
	if (output_device_info)
	{
		output_parameters.device = output_device_index;
		output_parameters.channelCount = output_device_info->maxOutputChannels;
		output_parameters.sampleFormat = portaudio_sample_format | paNonInterleaved;
		output_parameters.suggestedLatency = output_device_info->defaultLowOutputLatency;
		output_parameters.hostApiSpecificStreamInfo = NULL;
	}

	return Pa_OpenStream(
		stream,
		input_device_info ? &input_parameters : NULL,
		output_device_info ? &output_parameters : NULL,
		sampleRate, framesPerBuffer, paNoFlag, &CASIOBridge::StaticStreamCallback, this);
}

ASIOError CASIOBridge::canSampleRate(ASIOSampleRate sampleRate) throw()
{
	Log() << "CASIOBridge::canSampleRate(" << sampleRate << ")";
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

ASIOError CASIOBridge::getSampleRate(ASIOSampleRate* sampleRate) throw()
{
	Log() << "CASIOBridge::getSampleRate()";
	if (sample_rate == 0)
	{
		Log() << "getSampleRate() called in unitialized state";
		return ASE_NoClock;
	}
	*sampleRate = sample_rate;
	Log() << "Returning sample rate: " << *sampleRate;
	return ASE_OK;
}

ASIOError CASIOBridge::setSampleRate(ASIOSampleRate sampleRate) throw()
{
	Log() << "CASIOBridge::setSampleRate(" << sampleRate << ")";
	if (buffers)
	{
		// TODO: reset the stream instead of ignoring the call
		Log() << "Changing the sample rate after createBuffers() is not supported";
		return ASE_NotPresent;
	}
	sample_rate = sampleRate;
	return ASE_OK;
}

ASIOError CASIOBridge::createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) throw()
{
	Log() << "CASIOBridge::createBuffers(" << numChannels << ", " << bufferSize << ")";
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
			if (!input_device_info || buffer_info.channelNum >= input_device_info->maxInputChannels)
			{
				Log() << "out of bounds input channel";
				return ASE_InvalidMode;
			}
		}
		else
		{
			if (!output_device_info || buffer_info.channelNum >= output_device_info->maxOutputChannels)
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
	PaError error = OpenStream(&temp_stream, sample_rate, temp_buffers->buffer_size);
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

ASIOError CASIOBridge::disposeBuffers() throw()
{
	Log() << "CASIOBridge::disposeBuffers()";
	if (!buffers)
	{
		Log() << "disposeBuffers() called before createBuffers()";
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

	buffers.reset();
	buffers_info.clear();
	return ASE_OK;
}

ASIOError CASIOBridge::getLatencies(long* inputLatency, long* outputLatency)
{
	Log() << "CASIOBridge::getLatencies()";
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

ASIOError CASIOBridge::start() throw()
{
	Log() << "CASIOBridge::start()";
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

	Log() << "Starting stream";
	our_buffer_index = 0;
	position.samples = 0;
	started = true;
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

ASIOError CASIOBridge::stop()
{
	Log() << "CASIOBridge::stop()";
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
	stream = NULL;
	Log() << "Stopped successfully";
	return ASE_OK;
}

int CASIOBridge::StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags)
{
	Log() << "CASIOBridge::StreamCallback("<< frameCount << ")";
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

	Log() << "Handing off the buffer to the ASIO host";
	callbacks.bufferSwitch(our_buffer_index, ASIOFalse);
	std::swap(locked_buffer_index, our_buffer_index);
	position.samples += frameCount;
	
	Log() << "Returning from stream callback";
	return paContinue;
}

ASIOError CASIOBridge::getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp)
{
	Log() << "CASIOBridge::getSamplePosition()";
	if (!started)
	{
		Log() << "getSamplePosition() called before start()";
		return ASE_SPNotAdvancing;
	}

	*sPos = position.asio_samples;
	tStamp->lo = tStamp->hi = 0; // TODO
	Log() << "Returning: sample position " << position.samples << ", timestamp 0";
	return ASE_OK;
}
