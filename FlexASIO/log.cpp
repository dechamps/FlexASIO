#include "log.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

#include <shlobj.h>
#include <windows.h>

namespace flexasio {

	namespace {

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
			assert(FileTimeToSystemTime(&filetime, &systemtimeUTC));
			systemtimeUTC.wMilliseconds = 0;

			const auto localTimezoneAndBias = GetTimezoneAndBias();

			SYSTEMTIME systemtimeLocal;
			assert(SystemTimeToTzSpecificLocalTime(&localTimezoneAndBias.first, &systemtimeUTC, &systemtimeLocal));

			std::stringstream stream;
			stream.fill('0');

			stream << FormatSystemTimeISO8601(systemtimeLocal);

			FILETIME truncatedFiletime;
			assert(SystemTimeToFileTime(&systemtimeUTC, &truncatedFiletime));
			stream << "." << std::setw(7) << FileTimeToTenthsOfUs(filetime) - FileTimeToTenthsOfUs(truncatedFiletime);

			stream << ((localTimezoneAndBias.second >= 0) ? "-" : "+");
			const auto absoluteBias = std::abs(localTimezoneAndBias.second);
			stream << std::setw(2) << absoluteBias / 60 << ":" << std::setw(2) << absoluteBias % 60;

			return stream.str();
		}

		std::optional<std::wstring> GetUserDirectory() {
			PWSTR userDirectory = nullptr;
			if (::SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &userDirectory) != S_OK) return std::nullopt;
			const std::wstring userDirectoryString(userDirectory);
			::CoTaskMemFree(userDirectory);
			return userDirectoryString;
		}

		class LogOutput {
		public:
			LogOutput(const std::filesystem::path& path) : stream(path, std::ios::app | std::ios::out) {}

			void Write(const std::string& str) {
				std::scoped_lock stream_lock(stream_mutex);
				stream << str << std::flush;
			}

		private:
			std::mutex stream_mutex;
			std::ofstream stream;
		};

		std::unique_ptr<LogOutput> OpenLogOutput() {
			const auto userDirectory = GetUserDirectory();
			if (!userDirectory.has_value()) return nullptr;

			std::filesystem::path path(*userDirectory);
			path.append("FlexASIO.log");

			if (!std::filesystem::exists(path)) return nullptr;

			return std::make_unique<LogOutput>(path);
		}

		LogOutput* GetLogOutput() {
			static const auto output = OpenLogOutput();
			return output.get();
		}

	}

	Log::Log() {
		if (GetLogOutput() == nullptr) return;
		stream.emplace();

		FILETIME now;
		GetSystemTimePreciseAsFileTime(&now);
		*stream << FormatFiletimeISO8601(now) << " " << GetCurrentProcessId() << " " << GetCurrentThreadId() << " ";
	}

	Log::~Log() {
		if (!stream.has_value()) return;
		*stream << std::endl;
		GetLogOutput()->Write(stream->str());
	}

}