#include "windows_com.h"

#include "windows_error.h"

#include <stdexcept>

namespace flexasio {

	COMInitializer::COMInitializer(DWORD coInit) {
		const auto coInitializeResult = CoInitializeEx(NULL, coInit);
		if (FAILED(coInitializeResult))
			throw std::runtime_error("Failed to initialize COM: " + GetWindowsErrorString(coInitializeResult));
	}

	COMInitializer::~COMInitializer() {
		CoUninitialize();
	}

}
