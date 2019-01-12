add_executable(FlexASIOTest main.cpp ../versioninfo.rc)
target_compile_definitions(FlexASIOTest PRIVATE PROJECT_DESCRIPTION="FlexASIO Self-test program")
target_link_libraries(FlexASIOTest
	PRIVATE ASIOTest::test
	PRIVATE FlexASIO
	PRIVATE dechamps_CMakeUtils_version_stamp
)

install(TARGETS FlexASIOTest RUNTIME DESTINATION bin)
