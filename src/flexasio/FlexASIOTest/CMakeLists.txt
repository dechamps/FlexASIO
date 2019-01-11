add_executable(FlexASIOTest test.cpp ../versioninfo.rc)
target_compile_definitions(FlexASIOTest PRIVATE PROJECT_DESCRIPTION="FlexASIO Self-test program")
target_link_libraries(FlexASIOTest
	PRIVATE dechamps_ASIOUtil::asiosdk_ginclude
	PRIVATE dechamps_ASIOUtil::asiosdk_asio
	PRIVATE dechamps_ASIOUtil::asio
	PRIVATE FlexASIO
	PRIVATE FlexASIO_version_stamp
	PRIVATE dechamps_cpplog::log
	PRIVATE dechamps_cpputil::find
	PRIVATE dechamps_cpputil::string
	PRIVATE cxxopts::cxxopts
	PRIVATE SndFile::sndfile
)
install(TARGETS FlexASIOTest RUNTIME DESTINATION bin)
