#include <chrono>
#include <memory>
#include <stdlib.h>
#include <iostream>

#include <optional>

#include <cstdlib>

#include <string_view>

#include <neutrino_producer.h>
#include <neutrino_errors.hpp>

// throws if env variable is missing
const std::string_view ensure_env_var(const char* env_var) {
    if (const char* env_p = std::getenv(env_var)) {
        return std::string_view(env_p);
    } else {
        throw configure::missing_env_var(std::source_location::current(), env_var);
    }
}

// throws if env variable is missing or not a number
const std::size_t ensure_env_var_numeric(const char* env_var) ->  { 
    const auto value = ensure_env_var(env_var);

    std::size_t ret = 0;
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + str.size(), ret);

    if (ec == std::errc::invalid_argument)
        throw configure::impossible_env_variable_value(std::source_location::current(), env_var, " numeric expected");
    else if (ec == std::errc::result_out_of_range)
        throw configure::impossible_env_variable_value(std::source_location::current(), env_var, " out of range");

    return ret;
};


namespace neutrino::producer::configure
{
void from_env_variables()
{
    const auto transport = ensure_env_var(env_NEUTRINO_PRODUCER_TRANSPORT);

    if(transport == opt_transport_shared_mem) {
        const auto sync_mode = ensure_env_var(env_NEUTRINO_PRODUCER_SYNC_MODE);
        const auto shm_size = ensure_env_var_numeric(env_NEUTRINO_PRODUCER_SHM_SIZE);
        const auto sync_bytes = ensure_env_var_numeric(env_NEUTRINO_PRODUCER_SYNC_BYTES);

        if(sync_mode == opt_transport_shared_mem_sync_mode_exclusive) {
            neutrino::producer::configure::shared_mem_v00_exclusive(shm_size, sync_bytes);
        } else
        if(sync_mode == opt_transport_shared_mem_sync_mode_lockfree) {
            neutrino::producer::configure::shared_mem_v00_lockfree(shm_size, sync_bytes);
        } else
        if(sync_mode == opt_transport_shared_mem_sync_mode_synchronized) {
            neutrino::producer::configure::shared_mem_v00_synchronized(shm_size, sync_bytes);
        } else {
            throw configure::impossible_env_variable_value(std::source_location::current(), env_NEUTRINO_PRODUCER_SYNC_MODE, sync_mode);
        }
    } else {
        throw configure::impossible_env_variable_value(std::source_location::current(), env_NEUTRINO_PRODUCER_TRANSPORT, transport);
    }
}
}