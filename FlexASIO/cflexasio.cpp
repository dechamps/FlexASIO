#include "cflexasio.h"

#include "flexasio.h"
#include "flexasio.rc.h"
#include "flexasio_h.h"

#include "../FlexASIOUtil/log.h"

#include "..\ASIOSDK2.3.1\common\iasiodrv.h"

#include <atlbase.h>
#include <atlcom.h>

#include <cassert>
#include <string_view>

// Provide a definition for the ::CFlexASIO class declaration that the MIDL compiler generated.
// The actual implementation is in a derived class in an anonymous namespace, as it should be.
//
// Note: ASIO doesn't use COM properly, and doesn't define a proper interface.
// Instead, it uses the CLSID to create an instance and then blindfully casts it to IASIO, giving the finger to QueryInterface() and to sensible COM design in general.
// Of course, since this is a blind cast, the order of inheritance below becomes critical: if IASIO is not first, the cast is likely to produce a wrong vtable offset, crashing the whole thing. What a nice design.
class CFlexASIO : public IASIO, public IFlexASIO {};

namespace flexasio {
	namespace {

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
			CFlexASIO() throw() { Enter("CFlexASIO()", [] {}); }
			~CFlexASIO() throw() { Enter("~CFlexASIO()", [] {}); }

			// IASIO implementation

			ASIOBool init(void* sysHandle) throw() final {
				return (Enter("init()", [&] {
					if (flexASIO.has_value()) throw ASIOException(ASE_InvalidMode, "init() called more than once");
					flexASIO.emplace(sysHandle);
				}) == ASE_OK) ? ASIOTrue : ASIOFalse;
			}
			void getDriverName(char* name) throw() final {
				Enter("getDriverName()", [&] {
					strcpy_s(name, 32, "FlexASIO");
				});
			}
			long getDriverVersion() throw() final {
				Enter("getDriverVersion()", [] {});
				return 0;
			}
			void getErrorMessage(char* string) throw() final {
				Enter("getErrorMessage()", [&] {
					strcpy_s(string, 124, lastError.c_str());
				});
			}
			ASIOError getClockSources(ASIOClockSource* clocks, long* numSources) throw() final;
			ASIOError setClockSource(long reference) throw() final;
			ASIOError getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) throw() final;

			ASIOError getChannels(long* numInputChannels, long* numOutputChannels) throw() final {
				return EnterWithMethod("getChannels()", &FlexASIO::GetChannels, numInputChannels, numOutputChannels);
			}
			ASIOError getChannelInfo(ASIOChannelInfo* info) throw() final {
				return EnterWithMethod("getChannelInfo()", &FlexASIO::GetChannelInfo, info);
			}
			ASIOError canSampleRate(ASIOSampleRate sampleRate) throw() final {
				bool result;
				const auto error = EnterInitialized("canSampleRate()", [&] {
					result = flexASIO->CanSampleRate(sampleRate);
				});
				if (error != ASE_OK) return error;
				return result ? ASE_OK : ASE_NoClock;
			}
			ASIOError setSampleRate(ASIOSampleRate sampleRate) throw() final {
				return EnterWithMethod("setSampleRate()", &FlexASIO::SetSampleRate, sampleRate);
			}
			ASIOError getSampleRate(ASIOSampleRate* sampleRate) throw() final {
				return EnterWithMethod("getSampleRate()", &FlexASIO::GetSampleRate, sampleRate);
			}

			ASIOError createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) throw() final {
				return EnterWithMethod("createBuffers()", &FlexASIO::CreateBuffers, bufferInfos, numChannels, bufferSize, callbacks);
			}
			ASIOError disposeBuffers() throw() final {
				return EnterWithMethod("disposeBuffers()", &FlexASIO::DisposeBuffers);
			}
			ASIOError getLatencies(long* inputLatency, long* outputLatency) throw() final {
				return EnterWithMethod("getLatencies()", &FlexASIO::GetLatencies, inputLatency, outputLatency);
			}

			ASIOError start() throw() final {
				return EnterWithMethod("start()", &FlexASIO::Start);
			}
			ASIOError stop() throw() final {
				return EnterWithMethod("stop()", &FlexASIO::Stop);
			}
			ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) throw() final {
				return EnterWithMethod("getSamplePosition()", &FlexASIO::GetSamplePosition, sPos, tStamp);
			}

			ASIOError controlPanel() throw() final {
				return EnterWithMethod("controlPanel()", &FlexASIO::ControlPanel);
			}
			ASIOError future(long selector, void *opt) throw() final {
				return Enter("future()", [] {
					throw ASIOException(ASE_InvalidParameter, "future() is not supported");
				});
			}

			ASIOError outputReady() throw() final {
				return Enter("outputReady()", [] {
					throw ASIOException(ASE_InvalidParameter, "outputReady() is not supported");
				});
			}

		private:
			std::string lastError;
			std::optional<FlexASIO> flexASIO;

			template <typename Functor> ASIOError Enter(std::string_view context, Functor functor);
			template <typename... Args> ASIOError EnterInitialized(std::string_view context, Args&&... args);
			template <typename Method, typename... Args> ASIOError EnterWithMethod(std::string_view context, Method method, Args&&... args);
		};

		OBJECT_ENTRY_AUTO(__uuidof(::CFlexASIO), CFlexASIO);

		template <typename Functor> ASIOError CFlexASIO::Enter(std::string_view context, Functor functor) {
			Log() << "--- ENTERING CONTEXT: " << context;
			ASIOError result;
			try {
				functor();
				result = ASE_OK;
			}
			catch (const ASIOException& exception) {
				lastError = exception.what();
				result = exception.GetASIOError();
			}
			catch (const std::exception& exception) {
				lastError = exception.what();
				result = ASE_HWMalfunction;
			}
			catch (...) {
				lastError = "unknown exception";
				result = ASE_HWMalfunction;
			}
			if (result == ASE_OK) {
				Log() << "--- EXITING CONTEXT: " << context << " [OK]";
			}
			else {
				Log() << "--- EXITING CONTEXT: " << context << " [" << result << " " << lastError << "]";
			}
			return result;
		}

		template <typename... Args> ASIOError CFlexASIO::EnterInitialized(std::string_view context, Args&&... args) {
			if (!flexASIO.has_value()) {
				throw ASIOException(ASE_InvalidMode, std::string("entered ") + std::string(context) + " but uninitialized state");
			}
			return Enter(context, std::forward<Args>(args)...);
		}

		template <typename Method, typename... Args> ASIOError CFlexASIO::EnterWithMethod(std::string_view context, Method method, Args&&... args) {
			return EnterInitialized(context, [&] { return ((*flexASIO).*method)(std::forward<Args>(args)...); });
		}

		ASIOError CFlexASIO::getClockSources(ASIOClockSource* clocks, long* numSources) throw()
		{
			return Enter("getClockSources()", [&] {
				if (!clocks || !numSources || *numSources < 1)
					throw ASIOException(ASE_InvalidParameter, "invalid parameters to getClockSources()");

				clocks->index = 0;
				clocks->associatedChannel = -1;
				clocks->associatedGroup = -1;
				clocks->isCurrentSource = ASIOTrue;
				strcpy_s(clocks->name, 32, "Internal");
				*numSources = 1;
			});
		}

		ASIOError CFlexASIO::setClockSource(long reference) throw()
		{
			return Enter("setClockSource()", [&] {
				Log() << "reference = " << reference;
				if (reference != 0) throw ASIOException(ASE_InvalidParameter, "setClockSource() parameter out of bounds");
			});
		}

		ASIOError CFlexASIO::getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) throw()
		{
			return Enter("getBufferSize()", [&] {
				// These values are purely arbitrary, since PortAudio doesn't provide them. Feel free to change them if you'd like.
				// TODO: let the user should these values
				*minSize = 48; // 1 ms at 48kHz, there's basically no chance we'll get glitch-free streaming below this
				*maxSize = 48000; // 1 second at 48kHz, more would be silly
				*preferredSize = 1024; // typical - 21.3 ms at 48kHz
				*granularity = 1; // Don't care
				Log() << "Returning: min buffer size " << *minSize << ", max buffer size " << *maxSize << ", preferred buffer size " << *preferredSize << ", granularity " << *granularity;
			});
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
