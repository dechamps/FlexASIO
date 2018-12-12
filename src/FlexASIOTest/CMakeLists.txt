add_executable(FlexASIOTest test.cpp)
target_link_libraries(FlexASIOTest
	PRIVATE ASIOHost
	PRIVATE FlexASIO
	PRIVATE FlexASIOUtil_asio
	PRIVATE FlexASIOUtil_endian
)
