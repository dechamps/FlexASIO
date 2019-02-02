#pragma once

#include <dechamps_cpplog/log.h>

namespace flexasio {

	// In performance-critical code paths, use IsLoggingEnabled() to avoid wasting time formatting a log message that will go nowhere.
	bool IsLoggingEnabled();
	::dechamps_cpplog::Logger Log();

}
