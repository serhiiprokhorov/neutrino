#pragma once

#include <vector>
#include <memory>
#include <string>
#include <list>
#include <algorithm>
#include <mutex>
#include <thread>
#include <cassert>

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
            /// @brief a ring of shared buffers;
            /// @tparam SHARED_HEADER provides low level functions/data types to operate shared memory
            template <typename _SHARED_HEADER, typename EVENT_SET>
            struct buffers_ring_t {
                typedef _SHARED_HEADER SHARED_HEADER;
                typedef EVENT_SET _EVENT_SET;

                /// @brief ring helper struct, also exposes func requirements to SHARED_HEADER template
                struct buffer_t final
                {
                    std::unique_ptr<SHARED_HEADER> const m_header = nullptr;
                    uint8_t* const m_first_available = nullptr;
                    uint8_t* const m_end = nullptr;
                    uint8_t* const m_hi_water_mark = nullptr;
                    buffer_t* m_next = nullptr; // ptr to the next available buffer

                    buffer_t(std::unique_ptr<SHARED_HEADER>&& header, const std::size_t bytes_available)
                        : m_header(std::move(header))
                        , m_first_available(reinterpret_cast<uint8_t*>(m_header) + sizeof(SHARED_HEADER))
                        , m_end(reinterpret_cast<uint8_t*>(m_header) + bytes_available)
                        , m_hi_water_mark(m_end - EVENT_SET::biggest_event_size_bytes)
                    {
                    }

                    static constexpr std::size_t smallest_bytes = sizeof(SHARED_HEADER) + EVENT_SET::biggest_event_size_bytes;

                    ~buffer_t() = default;

                    /// @brief look up for the next available buffer
                    /// @return pointer to the buffer; 
                    /// @warning caller is responsible to validate if the buffer is actually free
                    buffer_t* next_available() {
                        // lookup next avail buffer in the ring and update m_available to use it with next message
                        // when no buffers are available ->  and its time to ignore message
                        auto* next = m_next;
                        while(!next->is_clean() && next != this) {
                            next = next -> m_next;
                        }
                        // NOTE: in case no buffers are available (consumer is chocked) 
                        // next will point to the same buffer the search started with (i.e. this)
                        return next;
                    }

                };

                /// @brief creates buffers ring on top of a given shared memory block using SHARED_HEADER metadata
                template <typename INITIALIZER>
                buffers_ring_t(INITIALIZER& memory, std::size_t cc_buffers) {

                    if(cc_buffers < 1) {
                        throw std::runtime_error("buffers_ring_t: cc_buffers < 1");
                    }

                    // split the given piece of shared memory into the requested number of buffers
                    // make sure it is enough space to format single buffer as a given SHARED_HEADER
                    const auto bytes_per_buffer = memory.size() / cc_buffers;

                    if(bytes_per_buffer < buffer_t::smallest_bytes) {
                        char buf[2000];
                        snprintf(buf, sizeof(buf) - 1, 
                            "buffers_ring_t: bytes_per_buffer=%lld < buffer_t::smallest_bytes(%lld)", 
                                bytes_per_buffer, buffer_t::smallest_bytes);
                        throw std::runtime_error( buf );
                    }

                    m_buffers.reserve(cc_buffers);

                    uint8_t * start = memory.data();
                    uint8_t * end = start + memory.size();

                    // the deleter below helps call SHARED_HEADER::destroy for consumer app only
                    // it seems logical to simply keep is_consumer inside SHARED_HEADER
                    // but it can't since this struct is shared between consumer and producer apps
                    auto deleter = memory.m_is_consumer 
                        ? [](SHARED_HEADER* ptr) { m_header->destroy(); } // destroy if consumer
                        : [](SHARED_HEADER* ptr) {}; // do nothing if producer

                    while(start < end)
                    {
                        m_buffers.emplace_back( 
                            std::unique_ptr<SHARED_HEADER>(new (start) SHARED_HEADER(memory.m_is_consumer), deleter)
                            , bytes_per_buffer 
                            );
                        m_buffers.back()->m_header->init();
                        start += bytes_per_buffer;
                    };

                    // NOTE: this func uses raw pointers, no further reallocations are allowed 
                    make_ring();
                }

                buffer_t* get_first() { return &m_buffers.front(); }

            private:
                std::vector<buffer_t> m_buffers;

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
                    
                    buffer_t* prev_buffer = &(m_buffers.front());
                    for( auto it = m_buffers.begin() + 1; it != m_buffers.end(); ++it ) {
                        prev_buffer->m_next = &(*it);
                        prev_buffer = prev_buffer->m_next;
                    }
                }
            };
        }
    }
}
