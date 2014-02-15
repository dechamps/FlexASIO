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
	sample_rate(0)
{
	Log() << "CASIOBridge::CASIOBridge()";
}

ASIOBool CASIOBridge::init(void* sysHandle)
{
	Log() << "CASIOBridge::init()";

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
	}

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

	info->isActive = false; // TODO
	info->channelGroup = 0;
	info->type = ASIOSTFloat32LSB;
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
		input_parameters.sampleFormat = paFloat32;
		input_parameters.suggestedLatency = input_device_info->defaultLowInputLatency;
		input_parameters.hostApiSpecificStreamInfo = NULL;
	}

	PaStreamParameters output_parameters;
	if (output_device_info)
	{
		output_parameters.device = output_device_index;
		output_parameters.channelCount = output_device_info->maxOutputChannels;
		output_parameters.sampleFormat = paFloat32;
		output_parameters.suggestedLatency = output_device_info->defaultLowOutputLatency;
		output_parameters.hostApiSpecificStreamInfo = NULL;
	}

	return Pa_OpenStream(
		stream,
		input_device_info ? &input_parameters : NULL,
		output_device_info ? &output_parameters : NULL,
		sampleRate, framesPerBuffer, paPrimeOutputBuffersUsingStreamCallback, &CASIOBridge::StaticStreamCallback, this);
}

ASIOError CASIOBridge::canSampleRate(ASIOSampleRate sampleRate) throw()
{
	Log() << "CASIOBridge::canSampleRate(" << sampleRate << ")";
	if (!input_device_info && !output_device_info)
	{
		Log() << "canSampleRate() called in unitialized state";
		return ASE_NotPresent;
	}

	PaStream* stream;
	PaError error = OpenStream(&stream, sampleRate, paFramesPerBufferUnspecified);
	if (error != paNoError)
	{
		init_error = std::string("Cannot do this sample rate: ") + Pa_GetErrorText(error);
		Log() << init_error;
		return ASE_NoClock;
	}

	Log() << "Sample rate is available";
	Pa_CloseStream(stream);
	return ASE_OK;
}

ASIOError CASIOBridge::getSampleRate(ASIOSampleRate* sampleRate) throw()
{
	Log() << "CASIOBridge::getSampleRate()";
	if (sample_rate == 0)
	{
		Log() << "getSampleRate() called before SetSampleRate()";
		return ASE_NoClock;
	}
	*sampleRate = sample_rate;
	Log() << "Returning sample rate: " << *sampleRate;
	return ASE_OK;
}

ASIOError CASIOBridge::setSampleRate(ASIOSampleRate sampleRate) throw()
{
	Log() << "CASIOBridge::setSampleRate(" << sampleRate << ")";
	sample_rate = sampleRate;
	return ASE_OK;
}