cmake_minimum_required(VERSION 3.11)
project(dechamps_cpputil LANGUAGES CXX)

include(TestBigEndian)
TEST_BIG_ENDIAN(DECHAMPS_CPPUTIL_BIG_ENDIAN)
configure_file("${CMAKE_CURRENT_LIST_DIR}/endian.in.h" endian.h)
add_library(endian INTERFACE)
target_include_directories(endian
	INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
	INTERFACE "$<INSTALL_INTERFACE:include>"
)

add_library(find INTERFACE)
target_include_directories(find INTERFACE "$<INSTALL_INTERFACE:include>")

add_library(string INTERFACE)
target_include_directories(string INTERFACE "$<INSTALL_INTERFACE:include>")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/endian.h" find.h string.h DESTINATION include/dechamps_cpputil)
install(TARGETS endian find string EXPORT dechamps_cpputil-config)
install(EXPORT dechamps_cpputil-config NAMESPACE dechamps_cpputil:: DESTINATION lib/cmake/dechamps_cpputil)
