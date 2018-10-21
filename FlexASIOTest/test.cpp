#include <iostream>
#include <optional>
#include <string_view>

#include "..\ASIOSDK2.3.1\common\asio.h"
#include "..\FlexASIO\flexasio.h"

// The global ASIO driver pointer that the ASIO host library internally uses.
extern IASIO* theAsioDriver;

namespace flexasio_test {
	namespace {

		std::string_view GetASIOErrorString(ASIOError error) {
			switch (error) {
			case ASE_OK: return "ASE_OK";
			case ASE_SUCCESS: return "ASE_SUCCESS";
			case ASE_NotPresent: return "ASE_NotPresent";
			case ASE_HWMalfunction: return "ASE_HWMalfunction";
			case ASE_InvalidParameter: return "ASE_InvalidParameter";
			case ASE_InvalidMode: return "ASE_InvalidMode";
			case ASE_SPNotAdvancing: return "ASE_SPNotAdvancing";
			case ASE_NoClock: return "ASE_NoClock";
			case ASE_NoMemory: return "ASE_NoMemory";
			default: return "(unknown ASE error code)";
			}
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
			std::cout << "ASIODriverInfo::asioVersion: " << asioDriverInfo.asioVersion << std::endl;
			std::cout << "ASIODriverInfo::driverVersion: " << asioDriverInfo.asioVersion << std::endl;
			std::cout << "ASIODriverInfo::name: " << asioDriverInfo.name << std::endl;
			std::cout << "ASIODriverInfo::errorMessage: " << asioDriverInfo.errorMessage << std::endl;
			std::cout << "ASIODriverInfo::sysRef: " << asioDriverInfo.sysRef << std::endl;
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

		bool Run() {
			if (!Init()) return false;

			std::cout << std::endl;

			const auto channelCounts = GetChannels();
			if (channelCounts.first == 0 && channelCounts.second == 0) return false;

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

			ReleaseFlexASIO(theAsioDriver);
			theAsioDriver = nullptr;

			return result;
		}

	}
}

int main(int, char**) {
	if (!::flexasio_test::InitAndRun()) return 1;
	return 0;
}