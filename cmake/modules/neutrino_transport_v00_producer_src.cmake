include (neutrino_transport_v00_shared_src)

if(USE_MT)
	list(APPEND transport_v00_sources
		PUBLIC 
		${PROJECT_SOURCE_DIR}/src/transport/shared_mem_endpoint_proxy_mt.cpp
	)
endif()

list(APPEND transport_v00_sources
		PUBLIC 
		${PROJECT_SOURCE_DIR}/src/transport/shared_mem_endpoint_proxy_st.cpp
	)
