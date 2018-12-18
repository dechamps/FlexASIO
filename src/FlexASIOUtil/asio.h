#pragma once

#include <common\asiosys.h>
#include <common\asio.h>

#include <cstdint>
#include <string>

namespace flexasio {
	template <typename ASIOInt64> int64_t ASIOToInt64(ASIOInt64);
	template <typename ASIOInt64> ASIOInt64 Int64ToASIO(int64_t);

	std::string GetASIOErrorString(ASIOError error);

	std::string GetASIOSampleTypeString(ASIOSampleType sampleType);

	std::string GetASIOFutureSelectorString(long selector);
	std::string GetASIOMessageSelectorString(long selector);

	std::string GetAsioTimeInfoFlagsString(unsigned long timeInfoFlags);
	std::string GetASIOTimeCodeFlagsString(unsigned long timeCodeFlags);

	std::string DescribeASIOTimeInfo(const AsioTimeInfo& asioTimeInfo);
	std::string DescribeASIOTimeCode(const ASIOTimeCode& asioTimeCode);
	std::string DescribeASIOTime(const ASIOTime& asioTime);

}