#include "windows_error.h"

#include <algorithm>
#include <string>

static void rtrim(std::string& s)
{
	size_t end = s.find_last_of(" \n\r\t\f\v");
	if (end != std::string::npos && end > 0)
		s.resize(end - 1U);
}

namespace flexasio {

	std::string GetWindowsErrorString(DWORD error) {
		LPWSTR buffer = nullptr;
		std::string message;
		const auto dwSize = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, (LPWSTR) &buffer, 0, nullptr);
		if (dwSize == 0) {
			message = "failed to format error message - result, error " + std::to_string(GetLastError());
		}
		else {
			// FormatMessage results in a max of 128K buffer
			const auto iSize = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buffer, int(dwSize), nullptr, 0, nullptr, nullptr);
			if (iSize <= 0)
				message = "failed to convert error message to determine size - result, error " + std::to_string(GetLastError());
			else {
				message.resize(size_t(iSize + 1), '\0'); // add space for the trailing 0
				if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buffer, iSize, message.data(), int(message.size()), nullptr, nullptr) != iSize)
					message = "failed to convert error message - result, error " + std::to_string(GetLastError());
				else
					::rtrim(message);
			}
			::LocalFree(buffer);
		}

		return "Windows error code " + std::to_string(error) + " \"" + message + "\"";
	}
}