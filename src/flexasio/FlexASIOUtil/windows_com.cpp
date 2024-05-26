#include "windows_com.h"

#include <stdexcept>
#include <system_error>

namespace flexasio {

	COMInitializer::COMInitializer(DWORD coInit) {
		const auto coInitializeResult = CoInitializeEx(NULL, coInit);
		if (FAILED(coInitializeResult))
			throw std::system_error(coInitializeResult, std::system_category(), "Failed to initialize COM");
	}

	COMInitializer::~COMInitializer() {
		CoUninitialize();
	}

}
