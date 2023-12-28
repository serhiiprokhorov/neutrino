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
            namespace platform {
                /// @brief a ring of shared buffers;
                /// @tparam SHARED_HEADER provides low level functions/data types to operate shared memory
                template <typename SHARED_HEADER>
                struct buffers_ring_t {
                    /// @brief ring helper struct, also exposes func requirements to SHARED_HEADER template
                    struct buffer_t final
                    {
                        SHARED_HEADER::header_control_t m_handle; // platform specific shared memory info associated with this buffer instance
                        buffer_t* m_next = nullptr; // ptr to the next available buffer

                        operator bool() const {
                            return m_handle.m_header != nullptr;
                        }

                        buffer_t(SHARED_HEADER::header_control_t handle) :
                            m_handle(handle)
                        {}

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
                    buffers_ring_t(uint8_t* shared_memory, std::size_t size_bytes, bool create_new, std::size_t cc_buffers) {
                        // cut the given range of shared memory into the requested number of buffers
                        // make sure it is enough space to format single buffer as a given SHARED_HEADER
                        const std::size_t bytes_per_buffer = memory.size() / cc_buffers;
                        if(bytes_per_buffer < SHARED_HEADER::smallest_buffer_size_bytes)
                            return ;

                        m_buffers.reserve(cc_buffers);

                        uint8_t * start = memory.data();
                        uint8_t * end = start + memory.size();

                        while(start < end)
                        {
                            if(!m_buffers.emplace_back(SHARED_HEADER::format_at(start, create_new, bytes_per_buffer))) {
                                // unable to format a header in a given area start...start+bytes_per_buffer
                                // remove unformatted buffer and break the loop
                                m_buffers.pop_back();
                                break;
                            }
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

            namespace producer {

                namespace port {

                    /// @brief places an event into the very first available buffer, handles mark-dirty and lookup-next-free
                    template <typename SHARED_HEADER>
                    struct synchronized_t {

                        platform::buffers_ring_t::buffer_t* m_last_used;

                        // synchronized_t operates in a threadsafe environment
                        // with no other threads around
                        uint8_t* m_free;
                        uint64_t m_dirty_buffer_counter = 0; // incremented every time a buffer is signaled

                        synchronized_t(platform::buffers_ring_t& current) {
                            m_last_used = current.get_first();
                            m_free = m_last_used->m_handle.m_first_available;
                        }

                        // scans available buffers for the first continuous span of memory to place the bytes requested
                        // marks the buffer as dirty (it notifies the consumer about this buffer) when its capacity reached the high_water_mark
                        // implements spin when no more buffers are available (consumer is chocking)
                        // buffer transitions:
                        //  - from clean to dirty by producer
                        //  - from dirty to clean by consumer
                        template <typename SERIALIZED>
                        bool put(Args... args) noexcept {
                            if(m_free > m_last_used->m_handle.m_hi_water_mark) {
                                auto* next = m_last_used -> next_available();
                                // next_available() returning the same buffer means no free buffers at the moment
                                if(m_last_used == next)
                                    return false; // drop the event
                                // reset the next buffer as last used and remember where it begins
                                m_last_used = next;
                                m_free = m_last_used->m_handle.m_first_available;
                            }

                            // create serialized presentation of the event using in-place new
                            // to avoid unnecessary copy
                            new (m_free) SERIALIZED(...args);

                            // buffer's hi water mark is set so to make sure the biggest message fits between hi water mark and the end of a buffer
                            // i.e. (m_hi_watermark + serialized.bytes) < buffer.end()
                            // no need to check for buffer overflow before adding the message
                            // and no other threads are using this buffer since this code is synchronized
                            if((m_free += SERIALIZED::bytes) >= m_last_used->m_handle.m_hi_water_mark) {
                                // the buffer has reached its high water mark, time to mark-dirty
                                // mark as dirty notifies consumer it is ready to consume
                                // the counter helps detect missed buffers
                                m_last_used->m_header.dirty(
                                    (m_free - m_last_used.m_header.m_first_available),
                                    ++m_dirty_buffer_counter
                                );
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
                        bool put(SERIALIZED serialized) noexcept {
                            std::lock_guard l(m_mutex);
                            // when lock is acquired, call synch version
                            return m_synchronized.put(serialized);
                        }
                    };
                    template <typename SHARED_HEADER>
                    struct lock_free_t {

                        const uint64_t m_retries_serialize = 1;

                        std::atomic<platform::buffers_ring_t::buffer_t*> m_last_used;
                        std::atomic<uint8_t*> m_free;
                        std::atomic_uint64_t m_dirty_buffer_counter = 0; // incremented every time a buffer is signaled

                        std::atomic_int_fast64_t m_in_use = 0; // increments each time a thread is about to write to the buffer, decrements when write is done

                        lock_free_t(platform::buffers_ring_t& ring, const uint64_t retries_serialize)
                        : m_retries_serialize(retries_serialize) {
                            m_last_used = ring.get_first();
                            m_free = m_last_used.load()->m_header.m_first_available;
                            m_first_available_byte.store(0);
                        }

                        template <typename SERIALIZED>
                        bool put(SERIALIZED serialized) {
                            // Multiple threads runs this function,
                            // simultaneously writing a serialized event
                            // into some area which is in exclusive access by one single thread.
                            // Areas are isolated, not crossing boundaries of each other,
                            // multiple threads do not affect each other and the data is not corrupted.
                            // Area is reserved atomically using compare_exchange on m_free.
                            //
                            // Until the end of the current buffer is reached (hi water mark), operation is completely threadsafe:
                            // - every thread gets its own area to write an event [m_free, m_free + event_size)
                            // - those areas are not interleaving
                            //
                            // At some point one or more threads reach 
                            // the hi water mark and the buffer has to be signaled as "ready-to-be-consumed" 
                            // and switched with the free one.
                            // Signalling and switching is not thread safe because its a multi step operation.
                            //
                            // Here is a description of a lock free algorithm to perform buffer switch.
                            // The buffer is a container of events, event occupy some area, no gaps between those areas, 
                            // each area contains one serialized event, the event is generated by exactly one thread.
                            // Concurrent threads placing an event into the buffer can be labeled according to 
                            // a distance to the hi water wark:
                            // - "early" thread's area [m_free, m_free + event_size) is completely below hi water mark,
                            //   features:
                            //     - responsibility is limited to event serialization only;
                            //     - never waits
                            //     - events delayed until the buffer is full (depends on other threads)  
                            // - "late" thread's area [m_free, m_free + event_size) is completely above hi water mark,
                            //   features:
                            //     - responsibility is to wait until the buffer is switched
                            //     - waits for "edge" thread to switch the buffer
                            //     - events are delayed until buffer is switched + until the next buffer is full
                            //     - once the buffer is switched, the thread is re-labeled as "early" or "edge" with corresponding responsibilities
                            // - "edge" thread's area [m_free, m_free + event_size) starts below and ends above hi water mark,
                            //   features:
                            //     - responsibility is to signal the buffer and switch to the next available buffer
                            //     - events are not delayed
                            //
                            // NOTE: regarding possible event drop.
                            // The "edge" thread may drop events when no buffers are available. 
                            //
                            // NOTE: regarding "late" threads delays.
                            // A "late" thread waits until "edge" thread switches the buffer. In case of no buffers available,
                            // the "edge" thread drops the event and exits the function. This may leave all "late" threads
                            // in the endless wait. To break this endless wait, "edge" thread updates and "late" threads monitors
                            // some variable which indicates "break endless wait" and allows "late" threads to abandon the wait.

                            auto retries = m_retries_serialize;

                            do {
                                // optimistically reserve the range [my_reserved_begins,my_reserved_ends)
                                // compare_exchange_strong will confirm or reject that
                                auto my_reserved_begins = m_free.load();
                                auto my_reserved_ends = my_reserved_begins + SERIALIZED::bytes;

                                if(my_reserved_begins >= m_last_used->m_header.m_hi_watermark) {
                                    // hi water mark means the current buffer is full 
                                    // and the current thread is "late" thread.
                                    // It's time to look up for another buffer

                                    auto* my_buffer = m_last_used.load(memory_order::memory_order_consume);
                                    auto* next = my_buffer->next_available();
                                    if(next == my_buffer) {
                                        // same buffer returned means no buffers available at the moment, drop the event
                                        return false;
                                    }

                                    // m_first_available is a pointer and is unique inside buffer ring,
                                    // one or more threads getting the same value of next will install the same value of m_first_available
                                    // with no collide
                                    if(m_free.compare_exchange_strong(my_reserved_begins, next->m_first_available)) {
                                        m_last_used.store(next);
                                    }
                                    std::this_thread::yield();
                                    continue;
                                }

                                // "early" and "edge" thread may reach this point
                                if(m_free.compare_exchange_strong(my_reserved_begins, my_reserved_ends)) {

                                    auto* my_buffer = m_last_used.load(memory_order::memory_order_consume);

                                    continue here with additional check using atomic event counter
                                    to handle the possibility two threads update m_free with exact same values

                                    // compare_exchange_strong guarantee no concurrent updates to m_free
                                    // and the area between [my_reserved_begins, new_free) is in exclusive 
                                    // use of the current thread (due to linearity of pointers). 
                                    m_in_use++;
                                    // Note: multiple threads simultaneously write into a different areas of the current buffer. 
                                    // m_in_use lets "edge" thread to wait for concurrent threads to finish the writing
                                    serialized.into(my_reserved_begins);
                                    m_in_use--;

                                    // this condition filters away "early" threads, they can exit now
                                    if(my_reserved_ends < m_last_used->m_header.m_hi_watermark)
                                        return true; 

                                    // NOTE: there could be only one single thread ("edge" thread) at this point 
                                    // due to linearity and continuity of address space:
                                    // - there is exactly one range [my_reserved_begins, my_reserved_ends) with hi water mark inside
                                    //   and exactly one thread associated with this range
                                    // wait for other "early" threads to finish writing and mark dirty
                                    while(m_in_use.load(memory_order::memory_order_consume) != 0) {
                                        my_dirty_buffer_counter = m_dirty_buffer_counter.load();
                                        continue;
                                    }
                                    // notify consumer about this buffer
                                    my_buffer->dirty(
                                        my_reserved_ends-m_last_used->m_header.m_first_available, 
                                        ++m_dirty_buffer_counter
                                    );

                                    return true; // "edge" thread exit
                            } while(--retries)

                            return false;
                        }
                    };
                };
            }
        }

    }
}
}
