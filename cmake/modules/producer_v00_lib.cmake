add_library(producer_v00_lib STATIC)
target_sources(producer_v00_lib
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/producer_lib.cpp
	PRIVATE 
	${PROJECT_SOURCE_DIR}/src/v00/transport_lib.cpp
	${PROJECT_SOURCE_DIR}/src/shared_lib.cpp
)
if(USE_MT)
target_sources(producer_v00_lib
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/transport/consumer_stub_buffered_mt.cpp
)
else()
target_sources(producer_v00_lib
	PUBLIC 
	${PROJECT_SOURCE_DIR}/src/transport/consumer_stub_buffered_st.cpp
)
endif()

target_include_directories(producer_v00_lib PRIVATE ${PROJECT_SOURCE_DIR}/include)
