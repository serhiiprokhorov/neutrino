add_library(producer_v00_lib STATIC)
target_sources(producer_v00_lib
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/producer_lib.cpp
	PRIVATE 
	${PROJECT_SOURCE_DIR}/src/v00/transport_lib.cpp
	${PROJECT_SOURCE_DIR}/src/v00/neutrino_frames_serialized_network_bo.cpp
	${PROJECT_SOURCE_DIR}/src/shared_lib.cpp
)

if(TARGET_WIN32)
	target_sources(producer_v00_lib
		PRIVATE 
		${PROJECT_SOURCE_DIR}/src/v00/neutrino_frames_serialized_network_bo_win32.cpp
	)
endif()

if(USE_MT)
	target_sources(producer_v00_lib
		PUBLIC 
		${PROJECT_SOURCE_DIR}/src/transport/consumer_stub_buffered_mt.cpp
	)
	target_sources(producer_v00_lib
		PRIVATE 
		${PROJECT_SOURCE_DIR}/src/transport/consumer_transport_endpoint_async.cpp
	)
	if(TARGET_WIN32)
		target_sources(producer_v00_lib
			PRIVATE 
			${PROJECT_SOURCE_DIR}/src/transport/consumer_transport_endpoint_async_win32_handle.cpp
			${PROJECT_SOURCE_DIR}/src/transport/consumer_transport_endpoint_async_win32_handle_pipe.cpp
			${PROJECT_SOURCE_DIR}/src/transport/consumer_transport_endpoint_async_win32_handle_winsock2.cpp
		)
	endif()
else()
target_sources(producer_v00_lib
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/transport/consumer_stub_buffered_st.cpp
)
endif()

target_include_directories(producer_v00_lib PRIVATE ${PROJECT_SOURCE_DIR}/include)
