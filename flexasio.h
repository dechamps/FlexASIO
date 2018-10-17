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

#pragma once

#include "flexasio_h.h"

#include <memory>
#include <string>
#include <vector>

#include <atlbase.h>
#include <atlcom.h>

#include "flexasio.rc.h"
#include "ASIOSDK2.3.1\common\iasiodrv.h"
#include "util.h"
#include "portaudio.h"

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

union ASIOSamplesUnion
{
	ASIOSamples asio_samples;
	long long int samples;
};

union ASIOTimeStampUnion
{
	ASIOTimeStamp asio_timestamp;
	long long int timestamp;
};

// ASIO doesn't use COM properly, and doesn't define a proper interface.
// Instead, it uses the CLSID to create an instance and then blindfully casts it to IASIO, giving the finger to QueryInterface() and to sensible COM design in general.
// Of course, since this is a blind cast, the order of inheritance below becomes critical: if IASIO is not first, the cast is likely to produce a wrong vtable offset, crashing the whole thing. What a nice design.
class CFlexASIO :
	public IASIO,
	public IFlexASIO,
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CFlexASIO, &__uuidof(CFlexASIO)>
{
	BEGIN_COM_MAP(CFlexASIO)
		COM_INTERFACE_ENTRY(IFlexASIO)

		 // To add insult to injury, ASIO mistakes the CLSID for an IID when calling CoCreateInstance(). Yuck.
		COM_INTERFACE_ENTRY(CFlexASIO)

		// IASIO doesn't have an IID (see above), which is why it doesn't appear here.
	END_COM_MAP()

	DECLARE_REGISTRY_RESOURCEID(IDR_FLEXASIO)

	public:
		CFlexASIO() throw();
		virtual ~CFlexASIO() throw();

		// IASIO implementation

		virtual ASIOBool init(void* sysHandle);
		virtual void getDriverName(char* name) throw()  { Log() << "CFlexASIO::getDriverName()"; strcpy_s(name, 32, "FlexASIO"); }
		virtual long getDriverVersion() throw()  { Log() << "CFlexASIO::getDriverVersion()"; return 0; }
		virtual void getErrorMessage(char* string) throw()  { Log() << "CFlexASIO::getErrorMessage()"; strcpy_s(string, 124, init_error.c_str()); }

		virtual ASIOError getClockSources(ASIOClockSource* clocks, long* numSources) throw();
		virtual ASIOError setClockSource(long reference) throw();
		virtual ASIOError getChannels(long* numInputChannels, long* numOutputChannels) throw();
		virtual ASIOError getChannelInfo(ASIOChannelInfo* info) throw();
		virtual ASIOError getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) throw();
		virtual ASIOError canSampleRate(ASIOSampleRate sampleRate) throw();
		virtual ASIOError setSampleRate(ASIOSampleRate sampleRate) throw();
		virtual ASIOError getSampleRate(ASIOSampleRate* sampleRate) throw();

		virtual ASIOError createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) throw();
		virtual ASIOError disposeBuffers() throw();
		virtual ASIOError getLatencies(long* inputLatency, long* outputLatency) throw();

		virtual ASIOError start() throw();
		virtual ASIOError stop() throw();
		virtual ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) throw();

		// Not implemented
		virtual ASIOError controlPanel() throw()  { Log() << "CFlexASIO::controlPanel()"; return ASE_NotPresent; }
		virtual ASIOError future(long selector, void *opt) throw()  { Log() << "CFlexASIO::future()"; return ASE_InvalidParameter; }
		virtual ASIOError outputReady() throw()  { Log() << "CFlexASIO::outputReady()"; return ASE_NotPresent; }

	private:
		PaError OpenStream(PaStream**, double sampleRate, unsigned long framesPerBuffer) throw();
		static int StaticStreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw() { return static_cast<CFlexASIO*>(userData)->StreamCallback(input, output, frameCount, timeInfo, statusFlags); }
		int StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags) throw();

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
		ASIOSamplesUnion position;
		ASIOTimeStampUnion position_timestamp;
		bool started;
};

OBJECT_ENTRY_AUTO(__uuidof(CFlexASIO), CFlexASIO)
