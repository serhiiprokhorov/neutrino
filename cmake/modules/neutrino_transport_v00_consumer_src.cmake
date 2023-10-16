include (neutrino_transport_v00_shared_src)

list(APPEND transport_v00_sources
	${PROJECT_SOURCE_DIR}/src/transport/shared_mem_${NEUTRINO_PLAT_IMPL_EXT}.cpp
)
