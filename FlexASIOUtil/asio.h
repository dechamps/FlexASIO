#pragma once

#include "..\ASIOSDK2.3.1\common\asiosys.h"
#include "..\ASIOSDK2.3.1\common\asio.h"

#include <cstdint>
#include <string>

namespace flexasio {
	template <typename ASIOInt64> int64_t ASIOToInt64(ASIOInt64);
	template <typename ASIOInt64> ASIOInt64 Int64ToASIO(int64_t);

	std::string GetASIOErrorString(ASIOError error);

}