#include "log.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

#include <shlobj.h>
#include <windows.h>

#include "shell.h"
#include "../version/version.h"

namespace flexasio {

	namespace {

		template <auto functionPointer> struct FunctionPointerFunctor {
			template <typename... Args> auto operator()(Args&&... args) {
				return functionPointer(std::forward<Args>(args)...);
			}
		};

		using Library = std::unique_ptr<std::decay_t<decltype(*HMODULE())>, FunctionPointerFunctor<FreeLibrary>>;

		// On systems that support GetSystemTimePreciseAsFileTime() (i.e. Windows 8 and greater), use that.
		// Otherwise, fall back to GetSystemTimeAsFileTime().
		// See https://github.com/dechamps/FlexASIO/issues/15
		decltype(GetSystemTimeAsFileTime)* GetSystemTimeAsFileTimeFunction() {
			static const auto function = [] {
				static const Library library(LoadLibraryA("kernel32.dll"));
				if (library != nullptr) {
					const auto function = GetProcAddress(library.get(), "GetSystemTimePreciseAsFileTime");
					if (function != nullptr) return reinterpret_cast<decltype(GetSystemTimePreciseAsFileTime)*>(function);
				}
				return GetSystemTimeAsFileTime;
			}();
			return function;
		}

		int64_t FileTimeToTenthsOfUs(const FILETIME filetime) {
			ULARGE_INTEGER integer;
			integer.LowPart = filetime.dwLowDateTime;
			integer.HighPart = filetime.dwHighDateTime;
			return integer.QuadPart;
		}

		std::string FormatSystemTimeISO8601(const SYSTEMTIME& systemtime) {
			std::stringstream stream;
			stream.fill('0');
			stream << std::setw(2) << systemtime.wYear << "-" << std::setw(2) << systemtime.wMonth << "-" << std::setw(2) << systemtime.wDay;
			stream << "T";
			stream << std::setw(2) << systemtime.wHour << ":" << std::setw(2) << systemtime.wMinute << ":" << std::setw(2) << systemtime.wSecond;
			return stream.str();
		}

		std::pair<TIME_ZONE_INFORMATION, LONG> GetTimezoneAndBias() {
			TIME_ZONE_INFORMATION localTimezone;
			// Note: contrary to what MSDN seems to indicate, TIME_ZONE_INFORMATION::Bias does *not* include the StandardBias/DaylightBias.
			switch (GetTimeZoneInformation(&localTimezone)) {
			case TIME_ZONE_ID_UNKNOWN:
				return { localTimezone, localTimezone.Bias };
			case TIME_ZONE_ID_STANDARD:
				return { localTimezone, localTimezone.Bias + localTimezone.StandardBias };
			case TIME_ZONE_ID_DAYLIGHT:
				return { localTimezone, localTimezone.Bias + localTimezone.DaylightBias };
			default:
				abort();
			}
		}

		std::string FormatFiletimeISO8601(const FILETIME filetime) {
			SYSTEMTIME systemtimeUTC;
			if (!FileTimeToSystemTime(&filetime, &systemtimeUTC)) abort();
			systemtimeUTC.wMilliseconds = 0;

			const auto localTimezoneAndBias = GetTimezoneAndBias();

			SYSTEMTIME systemtimeLocal;
			if (!SystemTimeToTzSpecificLocalTime(&localTimezoneAndBias.first, &systemtimeUTC, &systemtimeLocal)) abort();

			std::stringstream stream;
			stream.fill('0');

			stream << FormatSystemTimeISO8601(systemtimeLocal);

			FILETIME truncatedFiletime;
			if (!SystemTimeToFileTime(&systemtimeUTC, &truncatedFiletime)) abort();
			stream << "." << std::setw(7) << FileTimeToTenthsOfUs(filetime) - FileTimeToTenthsOfUs(truncatedFiletime);

			stream << ((localTimezoneAndBias.second >= 0) ? "-" : "+");
			const auto absoluteBias = std::abs(localTimezoneAndBias.second);
			stream << std::setw(2) << absoluteBias / 60 << ":" << std::setw(2) << absoluteBias % 60;

			return stream.str();
		}


		std::string GetModuleName() {
			std::string moduleName(MAX_PATH, 0);
			moduleName.resize(GetModuleFileNameA(NULL, moduleName.data(), DWORD(moduleName.size())));
			return moduleName;
		}

		class LogOutput {
		public:
			static std::unique_ptr<LogOutput> Open() {
				const auto userDirectory = GetUserDirectory();
				if (!userDirectory.has_value()) return nullptr;

				std::filesystem::path path(*userDirectory);
				path.append("FlexASIO.log");

				if (!std::filesystem::exists(path)) return nullptr;

				return std::make_unique<LogOutput>(path);
			}

			static LogOutput* Get() {
				static const auto output = Open();
				return output.get();
			}

			LogOutput(const std::filesystem::path& path) : stream(path, std::ios::app | std::ios::out) {
				Log() << "Logfile opened";
				Log() << "Log time source: " << ((GetSystemTimeAsFileTimeFunction() == GetSystemTimeAsFileTime) ? "GetSystemTimeAsFileTime" : "GetSystemTimePreciseAsFileTime");

				Log() << "FlexASIO " << BUILD_CONFIGURATION << " " << BUILD_PLATFORM << " " << version << " built on " << buildTime;
				Log() << "Host process: " << GetModuleName();
			}

			~LogOutput() {
				Log() << "Closing logfile";
			}

			void Write(const std::string& str) {
				std::scoped_lock stream_lock(stream_mutex);
				stream << str << std::endl;
			}

		private:
			Logger Log() {
				return Logger([this](const std::string& str) { Write(str); });
			}

			std::mutex stream_mutex;
			std::ofstream stream;
		};

	}

	Logger::Logger(Write write) {
		if (!write) return;

		enabledState.emplace();
		enabledState->write = std::move(write);

		FILETIME now = { 0 };
		GetSystemTimeAsFileTimeFunction()(&now);
		enabledState->stream << FormatFiletimeISO8601(now) << " " << GetCurrentProcessId() << " " << GetCurrentThreadId() << " ";
	}

	Logger::~Logger() {
		if (!enabledState.has_value()) return;
		enabledState->write(enabledState->stream.str());
	}

	Logger Log() {
		auto* const logOutput = LogOutput::Get();
		if (logOutput == nullptr) return Logger({});
		return Logger([](const std::string& str){
			LogOutput::Get()->Write(str);
		});
	}

}