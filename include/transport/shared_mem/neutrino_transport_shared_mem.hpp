#pragma once

#include <vector>
#include <memory>
#include <string>
#include <list>
#include <algorithm>
#include <mutex>
#include <thread>
#include <cassert>

#include "../neutrino_transport.hpp"

namespace neutrino
{
    namespace transport
    {
        namespace shared_memory
        {
            namespace platform {
                /// @brief shared between producer and consumer; 
                /// has states:
                /// - "clean", in this state the buffer has some empty space at the end and may accept more data
                /// - "dirty", in this state the buffer can not accept more data,
                //             still may have empty space at the end but not enough to fit the data proposed by a producer
                // Multiple buffers form a chain, each buffer includes a pointer to the next buffer
                template <typename SHARED_HEADER>
                struct buffer_t final
                {
                    /// @brief points to the region in memory available to store events
                    /// NOTE: hi_water_mark+sizeof(largest event) is always inside the region (safe for writing) 
                    struct span_t {
                        uint8_t* start; /// first byte which belongs to the span
                        uint8_t* first_available; /// first available byte (assuming there is a reserved space between start and first_available)
                        const uint8_t* hi_water_mark;
                        const std::size_t sz; /// total bytes the span occupies
                    };

                    SHARED_HEADER::header_t* m_handle = nullptr; // associated platform specific shared header
                    buffer_t* m_next = nullptr; // ptr to the next available buffer

                    buffer_t(SHARED_HEADER::header_t* handle) :
                        m_handle(handle)
                    {}

                    ~buffer_t() = default;

                    static const bool is_clean(const handle_t* handle) noexcept;
                    static void dirty(const uint64_t dirty_buffer_counter, const handle_t* handle) noexcept;
                    static void clear(const handle_t* handle) noexcept;

                    static const span_t available(const handle_t* handle) noexcept;
                };


                template <typename SHARED_HEADER>
                struct buffers_ring_t {
                    std::vector<buffer_t> m_buffers;

                    buffer_t* get_first() { return &m_buffers.front(); }

                    /// @brief creates buffers ring on top of a given shared memory block using SHARED_HEADER metadata
                    /// @tparam SHARED_MEMORY  
                    template <typename SHARED_MEMORY>
                    buffers_ring_t(SHARED_MEMORY& memory, std::size_t cc_buffers) {
                        const std::size_t bytes_per_buffer = memory.size() / cc_buffers;

                        // make sure it is enough space to format single buffer as a given SHARED_HEADER
                        if(bytes_per_buffer < SHARED_HEADER::min_bytes_needed)
                            return ;

                        m_buffers.reserve(cc_buffers);

                        uint8_t * start = memory.data();
                        uint8_t * end = start + memory.size();

                        while(start < end)
                        {
                            auto header_ctr = SHARED_HEADER::format_at(start, bytes_per_buffer);
                            if(!header_ctr.m_header)
                                break;
                            m_buffers.emplace_back(handle);
                            start += bytes_per_buffer;
                        };

                        make_ring(tmp_buffers);
                    }
                private:
                    void make_ring(std::vector<buffer_t>&& buffers) 
                    {
                        m_buffers = buffers;

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

            namespace producer {
                template <typename SHARED_HEADER>
                struct span_at_buffer_t {
                    // buffer stores multiple events, m_current remains stable until the buffer reached high water mark
                    platform::buffer_t* m_buffer = nullptr;
                    // currently available range of bytes
                    platform::buffer_t::span_t m_span;

                    void set(platform::buffer_t* buffer) {
                        m_span = platform::buffer_t::available((m_buffer = buffer)->m_handle);
                    }
                };

                namespace port {

                    bool lookup_available() {
                        // lookup next avail buffer in the ring and update m_available to use it with next message
                        // when no buffers are available ->  and its time to ignore message
                        auto* next = m_available.m_buffer->m_next;
                        while(!platform::buffer_t::is_clean(next->m_handle)) {
                            if(next == m_available.m_buffer) {
                                // consumer is chocked, no available buffers in the ring, return error
                                // note m_available remains the same and this loop will be repeated 
                                return false; 
                            }
                            next = next -> m_next;
                        }
                        m_available.set(next);
                    }

                    /// @brief places an event into the very first available buffer, handles mark-dirty and lookup-next-free
                    template <typename SHARED_HEADER>
                    struct synchronized_t {

                        span_at_buffer_t m_available;
                        uint64_t m_dirty_buffer_counter = 0; // increments with each mark-dirty

                        synchronized_t(platform::buffers_ring_t& current) {
                            m_available.m_buffer = current.get_first();
                            m_available.m_span = m_available.m_buffer->available();
                        }

                        // scans available buffers for the first continuous span of memory to place the bytes requested
                        // marks the buffer as dirty (it notifies the consumer about this buffer) when its capacity reached the high_water_mark
                        // implements spin when no more buffers are available (consumer is chocking)
                        // buffer transitions:
                        //  - from clean to dirty by producer
                        //  - from dirty to clean by consumer
                        template <typename SERIALIZED>
                        bool put(SERIALIZED serialized) {
                            if(m_available.m_span.first_available > m_available.m_span.hi_water_mark && !lookup_available())
                                return false;
                                
                            // buffer's hi water mark is always lower than the biggest message
                            // i.e. for each buffer and message combo this stands (m_hi_watermark + serialized.bytes) < buffer.end()
                            // no need to check for buffer overflow before adding the message
                            // also no need to worry about preallocating the buffer since its synchronized
                            serialized.into(m_available.m_span.first_available);
                            if((m_available.m_span.first_available += serialized.bytes) >= m_available.m_span.hi_water_mark) {
                                // the buffer has reached its high water mark, time to mark-dirty
                                // mark as dirty notifies consumer it is ready to consume
                                // the counter helps detect missed buffers
                                SHARED_HEADER::dirty(m_available.m_buffer->m_header, ++m_dirty_buffer_counter);
                                lookup_available();
                            }
                            return true;
                        }
                    };
                    // exclusive access is the same as synchronized + mutex
                    template <typename SHARED_HEADER>
                    struct exclusive_t final {
                        std::mutex m_mutex;
                        synchronized_t m_synchronized;

                        exclusive_t(platform::buffers_ring_t& current)
                        : m_synchronized(current) {}

                        template <typename SERIALIZED>
                        bool place(SERIALIZED serialized) {
                            std::lock_guard l(m_mutex);
                            // when lock is acquired, call synch version
                            return m_synchronized(serialized);
                        }
                    };
                    template <typename SHARED_HEADER>
                    struct lock_free_t {

                        const uint64_t m_retries_serialize = 1;

                        // lock free needs an atomic guaranteed by the platform, some platforms can guarantee atomic struct
                        // m_first_available_byte is used together with m_first_available to implement atomic address
                        std::atomic_uint_fast64_t m_first_available_byte;
                        span_at_buffer_t m_available;

                        std::atomic_int64_t m_dirty_buffer_counter = 0; // increments with each mark-dirty
                        std::atomic_int_fast64_t m_in_use = 0; // increments each time a thread is about to write to the buffer, decrements when write is done

                        lock_free_t(platform::buffers_ring_t& current, const uint64_t retries_serialize)
                        : m_retries_serialize(retries_serialize) {
                            m_available.set(current.get_first());
                            m_first_available_byte.store(0);
                        }

                        template <typename SERIALIZED>
                        bool place(SERIALIZED serialized) {
                            // multiple threads access this function simultaneously
                            // each thread attempts to reserve a place in a buffer to fit the event
                            // compare_exchange guarantee no two threads update m_begin, i.e. updates to m_begin are serialized with no lock
                            // once m_begin is updated, area m_begin..(m_begin+bytes) is in exclusive use by this thread

                            auto retries = m_retries_serialize;
                            bool at_high_water = false;
                            do {
                                auto current_first_available_byte = m_first_available_byte.load();
                                auto desired_first_available_byte = my_first_available_byte + serialized.bytes;

                                // calculate the area begin and end optimistically in assumption no concurrent threads
                                auto* my_begin = m_available.m_span.m_first_available + current_first_available_byte;
                                auto* my_end = m_available.m_span.m_first_available + desired_first_available_byte;

                                // try gain exclusive access to my_begin ... my_begin + serialized.bytes area
                                // no other thread could access this area
                                if(m_first_available_byte.compare_exchange_strong(my_first_available_byte, reserved)) {
                                    // even m_first_available_byte update is synchronized, the code below is not
                                    // multiple threads may write into the current buffer simultaneously.
                                    //
                                    // What is important, each thread writes into its specific area, its thread safe.
                                    // At some point one or more threads reaches the end of the buffer and this is not thread safe.
                                    //
                                    // Below described lock free algorithm which performs buffer switch in lock free mode.
                                    //
                                    // The buffer consists of areas, no gaps between, each area keeps one serialized event,
                                    // areas may be labeled in relation to the hi water wark 
                                    // (which is close to the end of the buffer but still enough space to keep the largest event):
                                    // - "below watermark" (my_first_available_byte + serialized.bytes) < hi_water_mark
                                    // - "above watermark" (my_first_available_byte) >= hi_water_mark
                                    // - "edge" area starts below and ends above watermark
                                    //    NOTE: due to continuous nature of integers there can be only one "edge" area, proved by math geeks
                                    // Any area can be assigned to only one event,
                                    // and the event is always associated with one thread,
                                    // and due to compare_exchange there can be only one thread which gained control over the area.
                                    // Concurrent threads can be labeled in relation to hi water wark:
                                    // - "early" thread gets one of "below watermark" areas
                                    //   responsibility if this thread is to serialize the event and exit the function
                                    // - "late" thread can't get any area because there is no more bytes below "watermark"
                                    //   responsibility if this thread is wait until the is "below watermark" area is available and 
                                    //   this thread eventually becomes "early"
                                    // - "edge" thread gets the area which starts below and ends above "watermark"
                                    //   responsibility of this thread is to mark the buffer as dirty and to pick up next available clean buffer
                                    if(my_begin >= m_available.m_span.m_hi_watermark) {
                                        std::this_thread::yield();
                                        continue; // "late" threads waits to become "early"
                                    }
                                        
                                    // "early" and "edge" threads only beyond this point
                                    m_in_use++;
                                    serialized.into(my_begin);
                                    m_in_use--;

                                    if(my_end < m_available.m_span.m_hi_watermark)
                                        return true; // "early" thread exit

                                    // "edge" thread only
                                    // 
                                    // wait for other "early" threads to finish writing and mark dirty
                                    while(m_in_use.load() != 0) {
                                        my_dirty_buffer_counter = m_dirty_buffer_counter.load();
                                        continue;
                                    }
                                    m_current->dirty(++m_dirty_buffer_counter);

                                    // lookup next avail buffer in the ring and update m_available to use it with next message
                                    // when no buffers are available ->  and its time to ignore message
                                    auto* next = m_available.m_buffer->m_next;
                                    while(!platform::buffer_t::is_clean(next->m_handle)) {
                                        if(next == m_available.m_buffer) {
                                            // consumer is chocked, no available buffers in the ring, return error
                                            // note m_available remains the same and this loop will be repeated 
                                            return false; // "edge" thread exit when no buffers are available
                                        }
                                        next = next -> m_next;
                                    }

                                    // at this moment m_available still points to the buffer m_first_available_byte exceeds hi water mark
                                    // and m_first_available_byte gets continuously incremented by "late" threads even more.
                                    // While it is possible to simply update m_available with next buffer,
                                    // next->hi_water_mark could be big enough to occasionally let one of those threads through hi water.
                                    // It must be avoided by resetting m_first_available_byte to the lowest value 
                                    // between current and next buffers watermarks.
                                    // And only then update m_available because 
                                    // the lowest high water mark will keep "late" threads waiting in the loop.
                                    const auto next_span = next->available();
                                    if(next_span.hi_water_mark < m_first_available_byte.load()) {
                                        while(true) {
                                            auto current = m_first_available_byte.load();
                                            if(m_first_available_byte.compare_exchange_strong(current, next_span.hi_water_mark))
                                                break;
                                        }
                                    }

                                    m_available.set(next);

                                    while(true) {
                                        auto current = m_first_available_byte.load();
                                        if(m_first_available_byte.compare_exchange_strong(current, 0))
                                            break;
                                    }

                                    return true; // "edge" thread exit
                            } while(--retries)

                            return false;
                        }
                    };
                }
            }

            /// @brief maintains a ring of buffers, keeps a pointer to a buffer in clean state, 
            // updates this pointer to the next available buffer in the ring when the current buffer becomes "dirty"
            template <typename SHARED_BUFFER_IMPL_T>
            struct shared_buffers_ring_t
            {
                struct params_t
                {
                    std::size_t m_retries_on_overflow{10};
                    std::chrono::microseconds m_sleep_on_overflow{ std::chrono::milliseconds{10} };
                } const m_params;

                virtual shared_buffer_t* get_buffer() = 0;
                virtual bool set_buffer(shared_buffer_t*) = 0;

                // implements spinner when all buffers in the ring are dirty
                SHARED_BUFFER_IMPL_T* next_available(SHARED_BUFFER_IMPL_T* c, std::size_t bytes) const noexcept
                {
                    assert(c);
                    auto* buf = c->m_next;

                    assert(buf);

                    auto retries = m_params.m_retries_on_overflow;
                    while (retries > 0 && buf != c && !buf->is_clean())
                    {
                      buf = buf->m_next;
                      assert(buf);
                      retries--;
                    }

                    return buf;
                }
            };

            template <typename SHARED_BUFFER_RING_IMPL_T>
            struct v00_ring_consumer_proxy_t : public consumer_proxy_t
            {
                enum class EVENTS : uint8_t {
                    CHECKPOINT = 1,
                    CONTEXT_ENTER = 2,
                    CONTEXT_LEAVE = 3,
                    CONTEXT_EXCEPTION = 4
                };

                struct params_t
                {
                    std::size_t m_message_buf_watermark{ 0 };
                    SHARED_BUFFER_IMPL_T::params_t m_ring_params;
                } const m_params;

                std::unique_ptr<SHARED_BUFFER_RING_IMPL_T> m_buffers_ring;

                v00_ring_consumer_proxy_t(
                    const params_t& po
                    , std::unique_ptr<SHARED_BUFFER_RING_IMPL_T>&& buffers_ring
                )
                    : 
                    m_params(po)
                    , m_buffers_ring(std::move(buffers_ring))
                {
                }


                void consume_checkpoint(
                    const neutrino_nanoepoch_t& ne
                    , const neutrino_stream_id_t& stream
                    , const neutrino_event_id_t& event
                ) override 
                {
                    constexpr const auto bytes = sizeof(EVENTS::CHECKPOINT) 
                        + sizeof(neutrino_nanoepoch_t) 
                        + sizeof(neutrino_stream_id_t) 
                        + sizeof(neutrino_event_id_t);

                    auto span = m_buffers_ring->next_available(bytes);

                    std::memmove(span.m_span, &(EVENTS::CHECKPOINT), sizeof(EVENTS::CHECKPOINT));
                    std::memmove(span.m_span, &ne, sizeof(ne));
                    std::memmove(span.m_span, &(stream), sizeof(stream));
                    std::memmove(span.m_span, &(event), sizeof(event));

                };

                void consume_context_enter(
                    const neutrino_nanoepoch_t& ne
                    , const neutrino_stream_id_t& stream
                    , const neutrino_event_id_t& event
                ) override 
                {
                    constexpr const auto bytes = sizeof(EVENTS::CONTEXT_ENTER) 
                        + sizeof(neutrino_nanoepoch_t) 
                        + sizeof(neutrino_stream_id_t) 
                        + sizeof(neutrino_event_id_t);

                    auto span = m_buffers_ring->next_available(bytes);

                    std::memmove(span.m_span, &(EVENTS::CONTEXT_ENTER), sizeof(EVENTS::CONTEXT_ENTER));
                    std::memmove(span.m_span, &ne, sizeof(ne));
                    std::memmove(span.m_span, &(stream), sizeof(stream));
                    std::memmove(span.m_span, &(event), sizeof(event));

                };

                void consume_context_leave(
                    const neutrino_nanoepoch_t&
                    , const neutrino_stream_id_t&
                    , const neutrino_event_id_t&
                ) override 
                {
                    constexpr const auto bytes = sizeof(EVENTS::CONTEXT_LEAVE) 
                        + sizeof(neutrino_nanoepoch_t) 
                        + sizeof(neutrino_stream_id_t) 
                        + sizeof(neutrino_event_id_t);

                    auto span = m_buffers_ring->next_available(bytes);

                    std::memmove(span.m_span, &(EVENTS::CONTEXT_LEAVE), sizeof(EVENTS::CONTEXT_LEAVE));
                    std::memmove(span.m_span, &ne, sizeof(ne));
                    std::memmove(span.m_span, &(stream), sizeof(stream));
                    std::memmove(span.m_span, &(event), sizeof(event));

                };

                void consume_context_exception(
                    const neutrino_nanoepoch_t&
                    , const neutrino_stream_id_t&
                    , const neutrino_event_id_t&
                ) override 
                {
                    constexpr const auto bytes = sizeof(EVENTS::CONTEXT_EXCEPTION) 
                        + sizeof(neutrino_nanoepoch_t) 
                        + sizeof(neutrino_stream_id_t) 
                        + sizeof(neutrino_event_id_t);

                    auto span = m_buffers_ring->next_available(bytes);

                    std::memmove(span.m_span, &(EVENTS::CONTEXT_EXCEPTION), sizeof(EVENTS::CONTEXT_EXCEPTION));
                    std::memmove(span.m_span, &ne, sizeof(ne));
                    std::memmove(span.m_span, &(stream), sizeof(stream));
                    std::memmove(span.m_span, &(event), sizeof(event));

                };
            };
        }
    }
}
