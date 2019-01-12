add_library(FlexASIOTest_test STATIC test.cpp)
target_link_libraries(FlexASIOTest_test
	PRIVATE dechamps_ASIOUtil::asiosdk_ginclude
	PRIVATE dechamps_ASIOUtil::asiosdk_asio
	PRIVATE dechamps_ASIOUtil::asio
	PRIVATE dechamps_cpplog::log
	PRIVATE dechamps_cpputil::find
	PRIVATE dechamps_cpputil::string
	PRIVATE cxxopts::cxxopts
	PRIVATE SndFile::sndfile
)

add_executable(FlexASIOTest main.cpp ../versioninfo.rc)
target_compile_definitions(FlexASIOTest PRIVATE PROJECT_DESCRIPTION="FlexASIO Self-test program")
target_link_libraries(FlexASIOTest
	PRIVATE FlexASIOTest_test
	PRIVATE FlexASIO
	PRIVATE dechamps_CMakeUtils_version_stamp
)

install(TARGETS FlexASIOTest RUNTIME DESTINATION bin)
