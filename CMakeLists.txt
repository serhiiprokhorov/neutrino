cmake_minimum_required(VERSION 3.22.0)

project(neutrino 
	VERSION 0.1
	#DESCRIPTION "DESCRIPTION"
	#HOMEPAGE_URL "HOMEPAGE_URL"
	LANGUAGES CXX C
)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "Neutrino producer lib ${NEUTRINO_PRODUCER_V00_LIB}")
message(STATUS "Neutrino consumer lib ${NEUTRINO_CONSUMER_V00_LIB}")
message(STATUS "Neutrino multithread ${NEUTRINO_MT}")
message(STATUS "Neutrino UT ${NEUTRINO_UT}")
message(STATUS "Neutrino target CMAKE_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

if(NEUTRINO_UT)
	include(FetchContent)
	FetchContent_Declare(
	  googletest
	  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
	)
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)	

	enable_testing()

	#include(CTest)
	#include(GoogleTest)
    #find_package(GTest CONFIG REQUIRED)   
	add_compile_definitions(UT)
endif()

if(NEUTRINO_PRODUCER_V00_LIB)
	include(neutrino_producer_v00_lib)
endif()

if(NEUTRINO_CONSUMER_V00_LIB)
	include(neutrino_consumer_v00_lib)
endif()

if(NEUTRINO_UT)
	#TODO is it needed?
	include(ut_v00_lib_gtest)
endif()
