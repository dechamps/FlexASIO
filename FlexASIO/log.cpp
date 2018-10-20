#include "log.h"

#include <filesystem>
#include <optional>
#include <string>

#include <shlobj.h>

namespace flexasio {

	namespace {

		std::optional<std::wstring> GetUserDirectory() {
			PWSTR userDirectory;
			if (::SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &userDirectory) != S_OK) return std::nullopt;
			const std::wstring userDirectoryString(userDirectory);
			::CoTaskMemFree(userDirectory);
			return userDirectoryString;
		}

		std::optional<std::ofstream> OpenLogFile() {
			const auto userDirectory = GetUserDirectory();
			if (!userDirectory.has_value()) return std::nullopt;

			std::filesystem::path path(*userDirectory);
			path.append("FlexASIO.log");

			if (!std::filesystem::exists(path)) return std::nullopt;

			return std::ofstream(path, std::ios::app | std::ios::out);
		}
	}

	Log::Log() : file(OpenLogFile()) {
		if (!file.has_value()) return;
		*file << "FlexASIO: [" << timeGetTime() << "] ";
	}

	Log::~Log() {
		if (!file.has_value()) return;
		*file << std::endl << std::flush;
	}

}