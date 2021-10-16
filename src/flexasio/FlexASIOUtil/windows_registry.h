#pragma once

#include <windows.h>

#include <memory>

namespace flexasio {

	struct HKEYDeleter final {
		void operator()(HKEY hkey) const;
	};
	using UniqueHKEY = std::unique_ptr<std::remove_pointer_t<HKEY>, HKEYDeleter>;

}
