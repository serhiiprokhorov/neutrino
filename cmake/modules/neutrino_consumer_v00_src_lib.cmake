include (neutrino_transport_v00_consumer_src)

target_sources(consumer_v00_lib
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/neutrino_consumer.cpp
	${PROJECT_SOURCE_DIR}/src/config/neutrino_consumer_config.cpp
	PRIVATE 
	${transport_v00_sources}
)

target_include_directories(
	consumer_v00_lib 
	PRIVATE 
	${PROJECT_SOURCE_DIR}/include 
	${PROJECT_SOURCE_DIR}/include/transport 
	${PROJECT_SOURCE_DIR}/include/shared_mem
	)