#include "windows_error.h"

#include <string_view>

namespace flexasio {

	std::string GetWindowsErrorString(DWORD error) {
		std::string message(4096, 0);
		auto messageSize = ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0, message.data(), DWORD(message.size()), NULL);
		if (messageSize <= 0 || messageSize >= message.size()) {
			message = "failed to format error message - result " + std::to_string(messageSize) + ", error " + std::to_string(GetLastError()) + ")";
		}
		else {
			for (; messageSize > 0 && isspace(static_cast<unsigned char>(message[messageSize - 1])); --messageSize);
			message.resize(messageSize);
		}

		return "Windows error code " + std::to_string(error) + " \"" + message + "\"";
	}

}