#pragma once

#cmakedefine01 FLEXASIO_BIG_ENDIAN

#include <type_traits>

namespace flexasio {
	enum class Endianness { LITTLE, BIG };

#if !defined(FLEXASIO_BIG_ENDIAN)
#error "unknown endianness"
#elif FLEXASIO_BIG_ENDIAN == 0
	constexpr Endianness endianness = Endianness::LITTLE;
#elif FLEXASIO_BIG_ENDIAN == 1
	constexpr Endianness endianness = Endianness::BIG;
#endif
}
