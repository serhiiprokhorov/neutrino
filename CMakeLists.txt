cmake_minimum_required(VERSION 3.22.0)

project(neutrino 
	VERSION 0.1
	#DESCRIPTION "DESCRIPTION"
	#HOMEPAGE_URL "HOMEPAGE_URL"
	LANGUAGES CXX C
)

# config strategy:
# - default compiler or user-defined toolchain file (referenced by CMakePresets.json)
# - build specific settings in CMakePresets.json
# - platform-specific extension to lookup sources

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "compile ${CMAKE_BUILD_TYPE}")
message(STATUS "platform ${NEUTRINO_PLATFORM}")
message(STATUS "UT ${NEUTRINO_UT}")
message(STATUS "transport ${NEUTRINO_TRANSPORT}")
message(STATUS "transport version ${NEUTRINO_TRANSPORT_VERSION}")


#add_library(consumer_lib STATIC)
add_library(producer_st_lib STATIC)
add_library(producer_mt_exclusive_lib STATIC)
add_library(producer_mt_lockfree_lib STATIC)
#add_library(ut_lib STATIC)

target_sources(producer_st_lib
	PRIVATE 
	${PROJECT_SOURCE_DIR}/src/neutrino_producer.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_initializer.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_${NEUTRINO_TRANSPORT_VERSION}_header.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_${NEUTRINO_TRANSPORT_VERSION}_buffer.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_${NEUTRINO_TRANSPORT_VERSION}_port_synchronized.cpp
)
target_sources(producer_mt_exclusive_lib
	PRIVATE 
	${PROJECT_SOURCE_DIR}/src/neutrino_producer.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_initializer.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_${NEUTRINO_TRANSPORT_VERSION}_header.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_${NEUTRINO_TRANSPORT_VERSION}_buffer.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_${NEUTRINO_TRANSPORT_VERSION}_port_exclusive.cpp
)
target_sources(producer_mt_lockfree_lib
	PRIVATE 
	${PROJECT_SOURCE_DIR}/src/neutrino_producer.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_initializer.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_${NEUTRINO_TRANSPORT_VERSION}_header.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_${NEUTRINO_TRANSPORT_VERSION}_buffer.cpp
	${PROJECT_SOURCE_DIR}/src/${NEUTRINO_PLATFORM}/${NEUTRINO_TRANSPORT}_${NEUTRINO_TRANSPORT_VERSION}_port_lockfree.cpp
)

#target_include_directories(consumer_lib PRIVATE ${PROJECT_SOURCE_DIR}/include )
target_include_directories(producer_st_lib PRIVATE ${PROJECT_SOURCE_DIR}/include )
target_include_directories(producer_mt_exclusive_lib PRIVATE ${PROJECT_SOURCE_DIR}/include )
target_include_directories(producer_mt_lockfree_lib PRIVATE ${PROJECT_SOURCE_DIR}/include )

##############################################################################################
#
# UT
#
##############################################################################################

find_package(PkgConfig)
pkg_search_module(GTEST REQUIRED gtest_main)
enable_testing()
include(GoogleTest)

add_executable(ut_st)
add_executable(ut_mt_exclusive)
add_executable(ut_mt_lockfree)

target_include_directories(ut_st PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/ut )
target_include_directories(ut_mt_exclusive PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/ut )
target_include_directories(ut_mt_lockfree PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/ut )

target_sources(ut_st PRIVATE ut/gtest_main.cpp)
target_sources(ut_st PRIVATE ut/shared_mem_v00_synchronized_ut.cpp)
target_link_libraries(ut_st PRIVATE ${GTEST_LDFLAGS} producer_st_lib)
target_compile_options(ut_st PRIVATE ${GTEST_CFLAGS})

target_sources(ut_mt_exclusive PRIVATE ut/gtest_main.cpp)
target_sources(ut_mt_exclusive PRIVATE ut/shared_mem_v00_exclusive_ut.cpp)
target_link_libraries(ut_mt_exclusive PRIVATE ${GTEST_LDFLAGS})
target_compile_options(ut_mt_exclusive PRIVATE ${GTEST_CFLAGS})

target_sources(ut_mt_lockfree PRIVATE ut/gtest_main.cpp)
target_sources(ut_mt_lockfree PRIVATE ut/shared_mem_v00_lockfree_ut.cpp)
target_link_libraries(ut_mt_lockfree PRIVATE ${GTEST_LDFLAGS})
target_compile_options(ut_mt_lockfree PRIVATE ${GTEST_CFLAGS})

gtest_discover_tests(ut_st)
gtest_discover_tests(ut_mt_exclusive)
gtest_discover_tests(ut_mt_lockfree)

#add_test(ut_st_test ut_st)
#add_test(ut_mt_exclusive_test ut_mt_exclusive)
#add_test(ut_mt_lockfree_test ut_mt_lockfree)