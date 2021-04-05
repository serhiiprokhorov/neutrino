add_executable(ut_v00_lib_gtest)

target_link_options(ut_v00_lib_gtest 
	PRIVATE
		"/PROFILE"
)

get_target_property(producer_v00_lib_SOURCES producer_v00_lib INTERFACE_SOURCES)
get_target_property(consumer_v00_lib_SOURCES consumer_v00_lib INTERFACE_SOURCES)
target_sources(ut_v00_lib_gtest
	PRIVATE
		${producer_v00_lib_SOURCES}
		${consumer_v00_lib_SOURCES}
		${PROJECT_SOURCE_DIR}/src/shared_lib.cpp
		${PROJECT_SOURCE_DIR}/src/v00/transport_lib.cpp
		${PROJECT_SOURCE_DIR}/src/ut/mock_lib.cpp
		${PROJECT_SOURCE_DIR}/src/ut/gtest_main.cpp
		${PROJECT_SOURCE_DIR}/src/v00/ut_lib_gtest.cpp
		${PROJECT_SOURCE_DIR}/src/transport/ut_lib_gtest.cpp
		${PROJECT_SOURCE_DIR}/src/ut_lib_gtest.cpp
)
if(USE_MT)
target_sources(ut_v00_lib_gtest
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/transport/consumer_stub_buffered_mt.cpp
	${PROJECT_SOURCE_DIR}/src/transport/consumer_stub_buffered_st.cpp
)
else()
target_sources(ut_v00_lib_gtest
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/transport/consumer_stub_buffered_st.cpp
)
endif()

target_include_directories(ut_v00_lib_gtest PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(ut_v00_lib_gtest 
	PRIVATE 
		GTest::gtest 
		##GTest::gtest_main 
		#GTest::gmock 
		#GTest::gmock_main
)
gtest_discover_tests(ut_v00_lib_gtest
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/gtest
)
