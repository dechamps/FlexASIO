#include "shell.h"

#include <system_error>
#include <shlobj.h>

namespace flexasio {

	std::wstring GetUserDirectory() {
		PWSTR userDirectory = nullptr;
		const auto getKnownFolderPathHResult = ::SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &userDirectory);
		if (getKnownFolderPathHResult != S_OK)
			throw std::system_error(getKnownFolderPathHResult, std::system_category(), "SHGetKnownFolderPath() failed");
		const std::wstring userDirectoryString(userDirectory);
		::CoTaskMemFree(userDirectory);
		return userDirectoryString;
	}

}
