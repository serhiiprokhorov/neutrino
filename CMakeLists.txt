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

# TODO: case independent/stripped/normalized
set(NEUTRINO_PLAT_IMPL_EXT "unknown")
string(TOLOWER ${CMAKE_SYSTEM_NAME} NEUTRINO_PLAT_IMPL_EXT)

# variables coing from CMakePresets.json
message(STATUS "Neutrino producer lib ${NEUTRINO_PRODUCER_V00_LIB}")
if(NEUTRINO_PRODUCER_V00_LIB)
	message(STATUS "Neutrino producer mt ${NEUTRINO_PRODUCER_MT}")
endif()
message(STATUS "Neutrino consumer lib ${NEUTRINO_CONSUMER_V00_LIB}")
message(STATUS "Neutrino UT ${NEUTRINO_UT}")
message(STATUS "Neutrino target CMAKE_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_LIBRARY_ARCHITECTURE ${CMAKE_LIBRARY_ARCHITECTURE}")
message(STATUS "platform implementation extention ${NEUTRINO_PLAT_IMPL_EXT}")

# hardcoded, until external module repository is adopted
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")


# include gt specific stuff only with unit testing enabled
if(NEUTRINO_UT)
	include(neutrino_ut_tools)
endif()

if(NEUTRINO_PRODUCER_V00_LIB)
	include(neutrino_producer_v00_lib)
endif()

if(NEUTRINO_CONSUMER_V00_LIB)
	include(neutrino_consumer_v00_lib)
endif()

if(NEUTRINO_UT)
	#TODO is it needed?
	include(neutrino_ut_v00_lib_gtest)
endif()
