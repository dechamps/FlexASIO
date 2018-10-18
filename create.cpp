#include "create.h"

#include <cassert>
#include "flexasio.h"

IASIO * CreateFlexASIO() {
	CFlexASIO * flexASIO = nullptr;
	assert(CFlexASIO::CreateInstance(&flexASIO) == S_OK);
	assert(flexASIO != nullptr);
	return flexASIO;
}

void ReleaseFlexASIO(IASIO * const iASIO) {
	assert(iASIO != nullptr);
	iASIO->Release();
}
