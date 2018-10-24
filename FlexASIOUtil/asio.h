#pragma once

#include <cstdint>

namespace flexasio {
	template <typename ASIOInt64> int64_t ASIOToInt64(ASIOInt64);
	template <typename ASIOInt64> ASIOInt64 Int64ToASIO(int64_t);
}