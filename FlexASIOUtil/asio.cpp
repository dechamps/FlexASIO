#include "asio.h"

#include <cstring>

#include "..\ASIOSDK2.3.1\common\asio.h"

namespace flexasio {
	template <typename ASIOInt64> int64_t ASIOToInt64(ASIOInt64 asioInt64) {
		int64_t result;
		static_assert(sizeof asioInt64 == sizeof result);
		memcpy(&result, &asioInt64, sizeof result);
		return result;
	}
	template int64_t ASIOToInt64<ASIOTimeStamp>(ASIOTimeStamp);
	template int64_t ASIOToInt64<ASIOSamples>(ASIOSamples);

	template <typename ASIOInt64> ASIOInt64 Int64ToASIO(int64_t int64) {
		ASIOInt64 result;
		static_assert(sizeof int64 == sizeof result);
		memcpy(&result, &int64, sizeof result);
		return result;
	}
	template ASIOTimeStamp Int64ToASIO<ASIOTimeStamp>(int64_t);
	template ASIOSamples Int64ToASIO<ASIOSamples>(int64_t);
}