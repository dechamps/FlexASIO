#include "version.h"

#include <dechamps_CMakeUtils/version_stamp.h>

namespace flexasio {
	const char gitstr[] = FLEXASIO_GITSTR;
	const char version[] = FLEXASIO_VERSION;
	const char buildTime[] = FLEXASIO_BUILD_TIMESTR;
}
