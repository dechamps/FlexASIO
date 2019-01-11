#include "version.h"

#include <dechamps_CMakeUtils/version_stamp.h>

namespace flexasio {
	const char git_description[] = FLEXASIO_GIT_DESCRIPTION;
	const char version[] = FLEXASIO_VERSION;
	const char buildTime[] = FLEXASIO_BUILD_TIMESTR;
}
