#include "version.h"

#include <dechamps_CMakeUtils/version_stamp.h>

namespace flexasio {
	const char git_description[] = FLEXASIO_GIT_DESCRIPTION;
	const char git_description_dirty[] = FLEXASIO_GIT_DESCRIPTION_DIRTY;
	const char buildTime[] = FLEXASIO_BUILD_TIMESTR;
}
