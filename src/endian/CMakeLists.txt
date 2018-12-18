include(TestBigEndian)
TEST_BIG_ENDIAN(FLEXASIO_BIG_ENDIAN)

configure_file("${CMAKE_CURRENT_LIST_DIR}/endian.in.h" endian.h)

add_library(FlexASIO_endian INTERFACE)
target_include_directories(FlexASIO_endian
	INTERFACE "${CMAKE_CURRENT_BINARY_DIR}"
)
