add_executable(FlexASIOTest test.cpp ../versioninfo.rc)
target_compile_definitions(FlexASIOTest PRIVATE PROJECT_DESCRIPTION="FlexASIO Self-test program")
target_link_libraries(FlexASIOTest
	PRIVATE ASIOHost_asio
	PRIVATE FlexASIO
	PRIVATE FlexASIOUtil_asio
)
