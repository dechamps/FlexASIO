cmake_minimum_required(VERSION 3.11)
project(dechamps_cpplog LANGUAGES CXX)

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

add_library(log STATIC log.cpp)
target_include_directories(log INTERFACE "$<INSTALL_INTERFACE:include>")

install(FILES log.h DESTINATION include/dechamps_cpplog)
install(TARGETS log EXPORT dechamps_cpplog-config ARCHIVE DESTINATION lib/dechamps_cpplog)
install(EXPORT dechamps_cpplog-config NAMESPACE dechamps_cpplog:: DESTINATION lib/cmake/dechamps_cpplog)
