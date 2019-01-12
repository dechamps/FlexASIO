#include <ASIOTest/test.h>

#include "..\FlexASIO\cflexasio.h"

#include <cstdlib>

int main(int argc, char** argv) {
	auto* const asioDriver = CreateFlexASIO();
	if (asioDriver == nullptr) abort();

	const auto result = ::flexasio::RunTest(*asioDriver, argc, argv);

	ReleaseFlexASIO(asioDriver);
	return result;
}
