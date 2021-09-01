include (neutrino_transport_v00_shared)

if(TARGET_WIN32)
	list(APPEND transport_v00_sources
		${PROJECT_SOURCE_DIR}/src/transport/neutrino_consumer_transport_shared_mem_win.cpp
	)
endif()
