#include "shell.h"

#include <shlobj.h>

namespace flexasio {

	std::optional<std::wstring> GetUserDirectory() {
		PWSTR userDirectory = nullptr;
		if (::SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &userDirectory) != S_OK) return std::nullopt;
		const std::wstring userDirectoryString(userDirectory);
		::CoTaskMemFree(userDirectory);
		return userDirectoryString;
	}

}
