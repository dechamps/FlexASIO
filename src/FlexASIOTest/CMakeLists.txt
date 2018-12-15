add_executable(FlexASIOTest test.cpp)
target_link_libraries(FlexASIOTest
	PRIVATE ASIOHost_asio
	PRIVATE FlexASIO
	PRIVATE FlexASIOUtil_asio
)
