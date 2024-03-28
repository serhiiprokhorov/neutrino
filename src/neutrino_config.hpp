#pragma once

#include <stdexcept>
#include <source_location>
#include <string>
#include <cstring>

namespace neutrino
{
    namespace configure
    {
        const char* env_NEUTRINO_PRODUCER_TRANSPORT = "NEUTRINO_PRODUCER_TRANSPORT";
        const char* env_NEUTRINO_PRODUCER_SYNC_MODE = "NEUTRINO_PRODUCER_SYNC_MODE";
        const char* env_NEUTRINO_PRODUCER_SHM_SIZE = "NEUTRINO_PRODUCER_SHM_SIZE";
        const char* env_NEUTRINO_PRODUCER_SYNC_BYTES = "NEUTRINO_PRODUCER_SYNC_BYTES";

        const char* opt_transport_shared_mem = "shared_mem";
        const char* opt_transport_shared_mem_sync_mode_exclusive = "exclusive";
        const char* opt_transport_shared_mem_sync_mode_lockfree = "lockfree";
        const char* opt_transport_shared_mem_sync_mode_synchronized = "synchronized";
    }
}