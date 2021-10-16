#include "windows_registry.h"

#include "windows_error.h"

#include <stdexcept>

namespace flexasio {

	void HKEYDeleter::operator()(HKEY hkey) const {
		const auto regCloseKeyError = ::RegCloseKey(hkey);
		if (regCloseKeyError != ERROR_SUCCESS)
			throw std::runtime_error("Unable to close registry key: " + GetWindowsErrorString(regCloseKeyError));
	}

}
