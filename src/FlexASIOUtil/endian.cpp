#include "endian.h"

#include <cstdint>
#include <memory>

namespace flexasio {
	Endianness GetEndianness() {
		uint16_t uint16 = 1;
		char bytes[sizeof uint16];
		memcpy(bytes, &uint16, sizeof uint16);
		if (bytes[0] == 1 && bytes[1] == 0) {
			return Endianness::LITTLE;
		}
		if (bytes[0] == 0 && bytes[1] == 1) {
			return Endianness::BIG;
		}
		abort();
	}
}