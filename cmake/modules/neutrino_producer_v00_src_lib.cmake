include (neutrino_transport_v00_producer_src)

target_sources(producer_v00_lib
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/neutrino_producer.cpp
	${PROJECT_SOURCE_DIR}/src/neutrino_consumer_proxy.cpp
	${PROJECT_SOURCE_DIR}/src/config/neutrino_producer_config.cpp
	${PROJECT_SOURCE_DIR}/src/transport/neutrino_producer_consumer_proxy_singleton.cpp
	PRIVATE 
	${transport_v00_sources}
)

target_include_directories(producer_v00_lib PRIVATE ${PROJECT_SOURCE_DIR}/include)
