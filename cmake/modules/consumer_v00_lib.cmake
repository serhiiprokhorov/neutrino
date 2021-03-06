add_library(consumer_v00_lib STATIC)
target_sources(consumer_v00_lib
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/consumer_lib.cpp
	PRIVATE 
	${PROJECT_SOURCE_DIR}/src/v00/transport_lib.cpp
	${PROJECT_SOURCE_DIR}/src/v00/neutrino_frames_serialized_network_bo.cpp
	${PROJECT_SOURCE_DIR}/src/shared_lib.cpp
)
if(TARGET_WIN32)
	target_sources(consumer_v00_lib
		PRIVATE 
		${PROJECT_SOURCE_DIR}/src/v00/neutrino_frames_serialized_network_bo_win32.cpp
	)
endif()
target_include_directories(consumer_v00_lib PRIVATE ${PROJECT_SOURCE_DIR}/include)
