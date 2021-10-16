#pragma once

#include <windows.h>

namespace flexasio {

	class COMInitializer final {
	public:
		COMInitializer(DWORD coInit);
		~COMInitializer();

		COMInitializer(const COMInitializer&) = delete;
		COMInitializer& operator=(const COMInitializer&) = delete;
	};

}
