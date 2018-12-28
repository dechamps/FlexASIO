add_executable(FlexASIOTest test.cpp ../versioninfo.rc)
target_compile_definitions(FlexASIOTest PRIVATE PROJECT_DESCRIPTION="FlexASIO Self-test program")
target_link_libraries(FlexASIOTest
	PRIVATE asiosdk_asio
	PRIVATE asiosdk_headers
	PRIVATE FlexASIO
	PRIVATE FlexASIO_version_stamp
	PRIVATE FlexASIOUtil_asio
	PRIVATE FlexASIOUtil_log
	PRIVATE cxxopts::cxxopts
)
install(TARGETS FlexASIOTest RUNTIME DESTINATION bin)
