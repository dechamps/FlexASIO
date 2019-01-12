cmake_minimum_required(VERSION 3.11)
project(ASIOTest DESCRIPTION "Generic ASIO Test Suite")

find_package(cxxopts CONFIG REQUIRED)
find_package(dechamps_cpputil CONFIG REQUIRED)
find_package(dechamps_cpplog CONFIG REQUIRED)
find_package(dechamps_ASIOUtil CONFIG REQUIRED)
find_package(sndfile CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)
add_compile_options(
	/WX /W4 /permissive- /analyze

	# Suppress warnings about shadowing declarations.
	#
	# In most cases, this happens when a lambda is used to initialize some
	# variable, and the lambda declares a local variable with the same name as the
	# variable it's tasked with initializing. In such cases the shadowing is
	# actually desirable, because it prevents one from accidentally using the (not
	# yet initialized) outer variable instead of the (valid) local variable within
	# the lambda.
	/wd4458 /wd4456
)

add_library(ASIOTest SHARED test.cpp dll.def)
target_link_libraries(ASIOTest
	PRIVATE dechamps_ASIOUtil::asiosdk_ginclude
	PRIVATE dechamps_ASIOUtil::asiosdk_asio
	PRIVATE dechamps_ASIOUtil::asio
	PRIVATE dechamps_cpplog::log
	PRIVATE dechamps_cpputil::find
	PRIVATE dechamps_cpputil::string
	PRIVATE cxxopts::cxxopts
	PRIVATE SndFile::sndfile
)

target_include_directories(ASIOTest INTERFACE "$<INSTALL_INTERFACE:include>")

install(FILES test.h DESTINATION include/ASIOTest)
install(TARGETS ASIOTest EXPORT ASIOTest-config RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)
install(EXPORT ASIOTest-config NAMESPACE ASIOTest:: DESTINATION lib/cmake/ASIOTest)
