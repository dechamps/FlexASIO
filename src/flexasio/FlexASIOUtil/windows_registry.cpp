#include "windows_registry.h"

#include <stdexcept>
#include <system_error>

namespace flexasio {

	void HKEYDeleter::operator()(HKEY hkey) const {
		const auto regCloseKeyError = ::RegCloseKey(hkey);
		if (regCloseKeyError != ERROR_SUCCESS)
			throw std::system_error(regCloseKeyError, std::system_category(), "Unable to close registry key");
	}

}
