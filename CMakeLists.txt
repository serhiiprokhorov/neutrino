cmake_minimum_required(VERSION 3.12.0)

project(neutrino 
	VERSION 0.1
	#DESCRIPTION "DESCRIPTION"
	#HOMEPAGE_URL "HOMEPAGE_URL"
	LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_PRODUCER_V00_LIB "Use PRODUCER_V00_LIB" ON)
option(USE_CONSUMER_V00_LIB "Use CONSUMER_V00_LIB" ON)
option(USE_MT "Use multithreaded model" ON)
option(BUILD_TESTING "Use UT" ON)

set(TARGET_PLATFORM OFF)

#no target platform is provided, try derive one from the environment
if(TARGET_PLATFORM STREQUAL "" OR TARGET_PLATFORM STREQUAL "OFF" OR NOT TARGET_PLATFORM)
	if(WIN32)
		set(TARGET_PLATFORM "WIN32")
	else()
	endif()
	message(STATUS "auto TARGET_PLATFORM ${TARGET_PLATFORM}")
endif()

if(TARGET_PLATFORM STREQUAL "WIN32")
	option(TARGET_WIN32 "Target WIN32" ON)
else()
	message(FATAL_ERROR "platform ${USE_PLATFORM} is not supported")
endif()
message(STATUS "TARGET_WIN32 ${TARGET_WIN32}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

if(BUILD_TESTING)
	include(CTest)
	include(GoogleTest)
    find_package(GTest CONFIG REQUIRED)   
	add_compile_definitions(UT)
endif()

if(USE_PRODUCER_V00_LIB)
	include(producer_v00_lib)
endif()

if(USE_CONSUMER_V00_LIB)
	include(consumer_v00_lib)
endif()

if(BUILD_TESTING)
	#TODO is it needed?
	include(ut_v00_lib_gtest)
endif()
