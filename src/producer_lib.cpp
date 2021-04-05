#include <chrono>

#include <neutrino_producer.hpp>
#include <neutrino_frames_local.hpp>

using namespace neutrino::impl;

extern "C"
{

uint64_t neutrino_nanoepoch(void)
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void neutrino_checkpoint(const uint64_t nanoepoch, const uint64_t stream_id, const uint64_t event_id)
{
    producer::get_consumer()->consume_checkpoint(nanoepoch, stream_id, event_id);
}

void neutrino_context_enter(const uint64_t nanoepoch, const uint64_t stream_id, const uint64_t event_id)
{
    producer::get_consumer()->consume_context(nanoepoch, stream_id, event_id, local::payload::event_type_t::event_types::CONTEXT_ENTER);
}

void neutrino_context_leave(const uint64_t nanoepoch, const uint64_t stream_id, const uint64_t event_id)
{
    producer::get_consumer()->consume_context(nanoepoch, stream_id, event_id, local::payload::event_type_t::event_types::CONTEXT_LEAVE);
}

void neutrino_context_panic(const uint64_t nanoepoch, const uint64_t stream_id, const uint64_t event_id)
{
    producer::get_consumer()->consume_context(nanoepoch, stream_id, event_id, local::payload::event_type_t::event_types::CONTEXT_PANIC);
}

} // extern "C"
