include(CTest)
include(GoogleTest)
find_package(GTest CONFIG REQUIRED)   

add_executable(ut_v00_lib_gtest)

if(TARGET_WIN32)
target_link_options(ut_v00_lib_gtest 
	PRIVATE
		"/PROFILE"
)
endif()

if(USE_PRODUCER_V00_LIB)
	get_target_property(producer_v00_lib_SOURCES producer_v00_lib INTERFACE_SOURCES)
endif()

if(USE_CONSUMER_V00_LIB)
	get_target_property(consumer_v00_lib_SOURCES consumer_v00_lib INTERFACE_SOURCES)
endif()

target_sources(ut_v00_lib_gtest
	PRIVATE
		${producer_v00_lib_SOURCES}
		${consumer_v00_lib_SOURCES}
		${transport_v00_sources}
		${PROJECT_SOURCE_DIR}/src/ut/mock_lib.cpp
		${PROJECT_SOURCE_DIR}/src/ut/gtest_main.cpp
		${PROJECT_SOURCE_DIR}/src/v00/ut_lib_gtest.cpp
		${PROJECT_SOURCE_DIR}/src/transport/ut_lib_gtest.cpp
		${PROJECT_SOURCE_DIR}/src/transport/ut_shared_mem_win_gtest.cpp
		${PROJECT_SOURCE_DIR}/src/ut_lib_gtest.cpp
)

target_include_directories(ut_v00_lib_gtest PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ut_v00_lib_gtest 
	PRIVATE 
		GTest::gtest 
		##GTest::gtest_main 
		#GTest::gmock 
		#GTest::gmock_main
)
gtest_add_tests(TARGET ut_v00_lib_gtest)
#gtest_discover_tests(ut_v00_lib_gtest
#	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/gtest
#)
add_test(NAME monolithic COMMAND ut_v00_lib_gtest)
