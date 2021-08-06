add_library(consumer_v00_lib STATIC)
target_sources(consumer_v00_lib
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/neutrino_consumer_lib.cpp
	${PROJECT_SOURCE_DIR}/src/config/neutrino_consumer_config.cpp
	PRIVATE 
	${PROJECT_SOURCE_DIR}/src/neutrino_shared_lib.cpp
	${PROJECT_SOURCE_DIR}/src/v00/neutrino_transport_lib.cpp
)
if(TARGET_WIN32)
	target_sources(consumer_v00_lib
		PRIVATE 
		${PROJECT_SOURCE_DIR}/src/transport/neutrino_consumer_transport_shared_mem_win.cpp
	)
endif()
target_include_directories(consumer_v00_lib PRIVATE ${PROJECT_SOURCE_DIR}/include)
