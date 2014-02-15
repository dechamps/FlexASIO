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

#include "asiobridge_h.h"

#include <memory>
#include <string>

#include <atlbase.h>
#include <atlcom.h>

#include "asiobridge.rc.h"
#include "iasiodrv.h"
#include "util.h"
#include "portaudio.h"

// ASIO doesn't use COM properly, and doesn't define a proper interface.
// Instead, it uses the CLSID to create an instance and then blindfully casts it to IASIO, giving the finger to QueryInterface() and to sensible COM design in general.
// Of course, since this is a blind cast, the order of inheritance below becomes critical: if IASIO is not first, the cast is likely to produce a wrong vtable offset, crashing the whole thing. What a nice design.
class CASIOBridge :
	public IASIO,
	public IASIOBridge,
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CASIOBridge, &__uuidof(CASIOBridge)>
{
	BEGIN_COM_MAP(CASIOBridge)
		COM_INTERFACE_ENTRY(IASIOBridge)

		 // To add insult to injury, ASIO mistakes the CLSID for an IID when calling CoCreateInstance(). Yuck.
		COM_INTERFACE_ENTRY(CASIOBridge)

		// IASIO doesn't have an IID (see above), which is why it doesn't appear here.
	END_COM_MAP()

	DECLARE_REGISTRY_RESOURCEID(IDR_ASIOBRIDGE)

	public:
		CASIOBridge() throw();
		virtual ~CASIOBridge() throw();

		// IASIO implementation

		virtual ASIOBool init(void* sysHandle);
		virtual void getDriverName(char* name) throw()  { Log() << "CASIOBridge::getDriverName()"; strcpy_s(name, 32, "ASIOBridge"); }
		virtual long getDriverVersion() throw()  { Log() << "CASIOBridge::getDriverVersion()"; return 0; }
		virtual void getErrorMessage(char* string) throw()  { Log() << "CASIOBridge::getErrorMessage()"; strcpy_s(string, 124, init_error.c_str()); }

		virtual ASIOError getChannels(long* numInputChannels, long* numOutputChannels) throw();
		virtual ASIOError getChannelInfo(ASIOChannelInfo* info) throw();
		virtual ASIOError getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) throw();
		virtual ASIOError canSampleRate(ASIOSampleRate sampleRate) throw();
		virtual ASIOError setSampleRate(ASIOSampleRate sampleRate) throw();
		virtual ASIOError getSampleRate(ASIOSampleRate* sampleRate) throw();

		virtual ASIOError start() throw()  { Log() << "start()"; return ASE_OK; }
		virtual ASIOError stop() throw()  { Log() << "stop()"; return ASE_OK; }
		virtual ASIOError getLatencies(long* inputLatency, long* outputLatency) throw()  { Log() << "getLatencies()"; return ASE_OK; }
		virtual ASIOError getClockSources(ASIOClockSource* clocks, long* numSources) throw()  { Log() << "getClockSources()"; return ASE_OK; }
		virtual ASIOError setClockSource(long reference) throw()  { Log() << "setClockSources()"; return ASE_OK; }
		virtual ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) throw()  { Log() << "getSamplePosition()"; return ASE_OK; }
		virtual ASIOError createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) throw()  { Log() << "createBuffers()"; return ASE_OK; }
		virtual ASIOError disposeBuffers() throw()  { Log() << "disposeBuffers()"; return ASE_OK; }
		virtual ASIOError controlPanel() throw()  { Log() << "controlPanel()"; return ASE_OK; }
		virtual ASIOError future(long selector, void *opt) throw()  { Log() << "future()"; return ASE_OK; }
		virtual ASIOError outputReady() throw()  { Log() << "outputReady()"; return ASE_OK; }

	private:
		PaError OpenStream(PaStream**, double sampleRate, unsigned long framesPerBuffer) throw();
		static int StaticStreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) throw() { return static_cast<CASIOBridge*>(userData)->StreamCallback(input, output, frameCount, timeInfo, statusFlags); }
		int StreamCallback(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags) throw() { return 0; }

		bool portaudio_initialized;
		std::string init_error;

		PaDeviceIndex input_device_index;
		const PaDeviceInfo* input_device_info;
		PaDeviceIndex output_device_index;
		const PaDeviceInfo* output_device_info;

		ASIOSampleRate sample_rate;
};

OBJECT_ENTRY_AUTO(__uuidof(CASIOBridge), CASIOBridge)
