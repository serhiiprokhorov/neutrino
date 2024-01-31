#pragma once

#include <vector>
#include <memory>
#include <string>
#include <list>
#include <algorithm>
#include <mutex>
#include <thread>
#include <cassert>

#include "neutrino_errors.hpp"

namespace neutrino
{
    /// @brief a producer generates events and a consumer reads those events;
    /// Assumption "producer never waits": any error to send out event causes producer to drop the event.
    /// Assumption "hungry consumer": consumer's capacity to process events should exceed producer's,
    /// with rare cases of intermittent consumer's slow downs.
    namespace transport
    {
        /// @brief assumes consumer and producer shares a memory which holds the events, producer signals consumer when its ready.
        ///
        /// The shared memory is split into one or more buffers, each buffer consists of:
        /// - header (signals, capacity, runtime counters etc) and events area
        /// - events area
        /// .
        /// Each buffer at any moment is in exclusive use by producer or consumer:
        /// - producer fills it up with events until full, the producer signals "this buffer is full" to consumer
        /// - the buffer stays full until consumer grabbed all data events and marked it as "clean"  
        /// .
        /// With multiple buffers available, the producer can continue placing events while the consumer still working on a current buffer.
        /// This helps to reduce the number of dropped events in case of intermittent consumer's slow down.
        namespace shared_memory
        {
            /// @brief describes a range of bytes with layout: {SHARED_HEADER} {first available} ... {high water mark} ... {end}
            template <typename _SHARED_HEADER, typename _EVENT_SET>
            struct buffer_t final
            {
                typedef _SHARED_HEADER SHARED_HEADER;
                typedef _EVENT_SET EVENT_SET;

                static constexpr auto min_size() { 
                    return 
                        EVENT_SET::biggest_event_size_bytes() + 
                        SHARED_HEADER::reserve_bytes;
                };

                uint8_t* const m_start;
                uint8_t* const m_first_available;
                uint8_t* const m_end;
                uint8_t* const m_hi_water_mark;
                buffer_t* m_next = nullptr; // ptr to the next available buffer

                buffer_t(
                    uint8_t* start,
                    const std::size_t size
                ): 
                    m_start(start),
                    m_first_available(start + SHARED_HEADER::reserve_bytes),
                    m_end(start + size),
                    m_hi_water_mark(start + size - EVENT_SET::biggest_event_size_bytes())
                {
                }

                /// @brief look up for the next available buffer
                /// @return pointer to the buffer; 
                /// @warning caller is responsible to validate if the buffer is actually free
                buffer_t* next_available() noexcept {
                    // lookup next avail buffer in the ring and update m_available to use it with next message
                    // when no buffers are available ->  and its time to ignore message
                    auto* next = m_next;
                    while(!SHARED_HEADER::is_clean(next->m_start) && next != this) {
                        next = next -> m_next;
                    }
                    // NOTE: in case no buffers are available (consumer is chocked) 
                    // next will point to the same buffer the search started with (i.e. this)
                    return next;
                }

            };
            /// @brief a ring of shared buffers;
            template <typename _BUFFER>
            struct buffers_ring_t {
                typedef _BUFFER BUFFER;

                /// @brief ring of buffers on top of a given shared memory block using SHARED_HEADER metadata
                buffers_ring_t(
                    uint8_t* shared_memory_data, 
                    const std::size_t shared_memory_size_bytes, 
                    const std::size_t cc_buffers
                    )
                {

                    if(cc_buffers < 1) {
                        throw configure::impossible_option_value("buffers_ring_t::cc_buffers must be >= 1");
                    }

                    // split the given piece of shared memory into the requested number of buffers
                    // make sure it is enough space to format single buffer as a given SHARED_HEADER
                    const auto requested_buffer_size_bytes = shared_memory_size_bytes / cc_buffers;

                    if(requested_buffer_size_bytes < BUFFER::min_size()) {
                        char buf[2000];
                        snprintf(buf, sizeof(buf) - 1, 
                            "buffers_ring_t too many buffers: "
                            "shared memory size=%lld by cc_buffers=%lld is bytes=%lld: too small, less than buffer min_size_bytes(%lld)", 
                                shared_memory_size_bytes, cc_buffers, requested_buffer_size_bytes, BUFFER::min_size());
                        throw configure::impossible_option_value(buf);
                    }

                    // early reservation + inplace ctor guarantee no further reallocations
                    m_buffers.reserve(cc_buffers);

                    // each buffer must be aligned according to platform;
                    // make sure buffer's size is enough to include integer number of align intervals
                    // make each buffer bigger by exact number of bytes
                    const auto alignment = alignof(max_align_t);
                    auto buffer_size_bytes_aligned = requested_buffer_size_bytes;
                    if( const auto rem = buffer_size_bytes_aligned % alignment ) {
                        // rem shows how many bytes the buffer includes in extra over alignment interval
                        // increase buffer's size by exact number of bytes to fix one more alignment interval
                        // this may cause less number of buffers to be allocated
                        buffer_size_bytes_aligned += (alignment - rem);
                    }

                    uint8_t * end = shared_memory_data + shared_memory_size_bytes;

                    while(shared_memory_data < end)
                    {
                        m_buffers.emplace_back( 
                            shared_memory_data, 
                            buffer_size_bytes_aligned);

                        shared_memory_data += buffer_size_bytes_aligned;
                    };

                    if(!m_buffers.empty() && m_buffers.back().m_end > end) {
                        // this last buffer sticks out past the end of a given interval
                        // remove it
                        m_buffers.pop_back(); 
                    }

                    if(m_buffers.empty()) {
                        char buf[2000];
                        snprintf(buf, sizeof(buf) - 1, 
                            "buffers_ring_t can't allocate buffers: "
                            "shared memory size=%lld, cc_buffers=%lld, buffer bytes=%lld, alignment %lld, after alignment is %lld", 
                                shared_memory_size_bytes, cc_buffers, requested_buffer_size_bytes, alignment, buffer_size_bytes_aligned);
                        throw configure::impossible_option_value(buf);
                    }

                    // NOTE: this func uses raw pointers, no further reallocations are allowed 
                    make_ring();
                }

                BUFFER* get_first() { return &m_buffers.front(); }

                void init_all() {
                    for(auto& b : m_buffers)
                        BUFFER::SHARED_HEADER::init(b.m_start);
                }

                void destroy_all() {
                    for(auto& b : m_buffers)
                        BUFFER::SHARED_HEADER::destroy(b.m_start);
                }

            private:
                std::vector<BUFFER> m_buffers;

                void make_ring() 
                {
                    const auto sz = m_buffers.size();

                    if(sz == 0)
                        return;

                    m_buffers.back().m_next = &(m_buffers.front());

                    if(sz == 1)
                    {
                        return;
                    }
                    
                    BUFFER* prev_buffer = &(m_buffers.front());
                    for( auto it = m_buffers.begin() + 1; it != m_buffers.end(); ++it ) {
                        prev_buffer->m_next = &(*it);
                        prev_buffer = prev_buffer->m_next;
                    }
                }
            };
        }
    }
}
