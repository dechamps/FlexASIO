#include "version.h"

#include "..\build\version.h"

namespace flexasio {
	const char version[] = FLEXASIO_VERSION;
	const char buildTime[] = FLEXASIO_BUILD_TIMESTR;
}
