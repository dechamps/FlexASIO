#include "log.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

#include <shlobj.h>

namespace flexasio {

	namespace {

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
		*stream << "FlexASIO: [" << timeGetTime() << "] ";
	}

	Log::~Log() {
		if (!stream.has_value()) return;
		*stream << std::endl;
		GetLogOutput()->Write(stream->str());
	}

}