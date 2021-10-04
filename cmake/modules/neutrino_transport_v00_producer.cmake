include (neutrino_transport_v00_shared)

if(TARGET_WIN32)
	if(USE_MT)
	list(APPEND transport_v00_sources
		PUBLIC 
		${PROJECT_SOURCE_DIR}/src/transport/neutrino_producer_transport_buffered_mt.cpp
	)
	endif()
	list(APPEND transport_v00_sources
		PUBLIC 
		${PROJECT_SOURCE_DIR}/src/transport/neutrino_producer_transport_buffered_st.cpp
	)
endif()
