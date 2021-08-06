add_library(producer_v00_lib STATIC)
target_sources(producer_v00_lib
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/neutrino_producer_lib.cpp
	${PROJECT_SOURCE_DIR}/src/config/neutrino_producer_config.cpp
	PRIVATE 
	${PROJECT_SOURCE_DIR}/src/neutrino_shared_lib.cpp
	${PROJECT_SOURCE_DIR}/src/v00/neutrino_transport_lib.cpp
)
if(TARGET_WIN32)
	if(USE_MT)
	target_sources(producer_v00_lib
		PUBLIC 
		${PROJECT_SOURCE_DIR}/src/transport/neutrino_producer_transport_buffered_mt.cpp
	)
	else()
	target_sources(producer_v00_lib
		PUBLIC 
		${PROJECT_SOURCE_DIR}/src/transport/neutrino_producer_transport_buffered_st.cpp
	)
	endif()
endif()

target_include_directories(producer_v00_lib PRIVATE ${PROJECT_SOURCE_DIR}/include)
