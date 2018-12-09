#include "version.h"

#ifndef FLEXASIO_VERSION
#include "..\build\version.h"
#endif

namespace flexasio {
	const char gitstr[] = FLEXASIO_GITSTR;
	const char version[] = FLEXASIO_VERSION;
	const char buildTime[] = FLEXASIO_BUILD_TIMESTR;
}
