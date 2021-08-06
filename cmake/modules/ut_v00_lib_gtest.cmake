add_executable(ut_v00_lib_gtest)

if(TARGET_WIN32)
target_link_options(ut_v00_lib_gtest 
	PRIVATE
		"/PROFILE"
)
endif()

get_target_property(producer_v00_lib_SOURCES producer_v00_lib INTERFACE_SOURCES)
get_target_property(consumer_v00_lib_SOURCES consumer_v00_lib INTERFACE_SOURCES)

target_sources(ut_v00_lib_gtest
	PRIVATE
		${producer_v00_lib_SOURCES}
		${consumer_v00_lib_SOURCES}
		${PROJECT_SOURCE_DIR}/src/neutrino_shared_lib.cpp
		${PROJECT_SOURCE_DIR}/src/v00/neutrino_transport_lib.cpp
		${PROJECT_SOURCE_DIR}/src/ut/mock_lib.cpp
		${PROJECT_SOURCE_DIR}/src/ut/gtest_main.cpp
		${PROJECT_SOURCE_DIR}/src/v00/ut_lib_gtest.cpp
		${PROJECT_SOURCE_DIR}/src/transport/ut_lib_gtest.cpp
		${PROJECT_SOURCE_DIR}/src/ut_lib_gtest.cpp
)
if(TARGET_WIN32)
	target_sources(ut_v00_lib_gtest
		PUBLIC 
		${PROJECT_SOURCE_DIR}/src/transport/neutrino_consumer_transport_shared_mem_win.cpp
	)
	if(USE_MT)
	target_sources(ut_v00_lib_gtest
		PUBLIC 
		${PROJECT_SOURCE_DIR}/src/transport/neutrino_producer_transport_buffered_mt.cpp
	)
	else()
	target_sources(ut_v00_lib_gtest
		PUBLIC 
		${PROJECT_SOURCE_DIR}/src/transport/neutrino_producer_transport_buffered_mt.cpp
	)
	endif()
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
