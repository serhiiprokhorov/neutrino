#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void neutrino_producer_startup(const char* cfg, const uint32_t cfg_bytes);
    void neutrino_producer_shutdown(void);

    /*TODO: errors as retvals?*/

    uint64_t neutrino_nanoepoch(void);
    void neutrino_checkpoint(const uint64_t m_nanoepoch, const uint64_t stream_id, const uint64_t event_id);
    void neutrino_context_enter(const uint64_t m_nanoepoch, const uint64_t stream_id, const uint64_t event_id);
    void neutrino_context_leave(const uint64_t m_nanoepoch, const uint64_t stream_id, const uint64_t event_id);
    void neutrino_context_panic(const uint64_t m_nanoepoch, const uint64_t stream_id, const uint64_t event_id);

#ifdef __cplusplus
}
#endif
