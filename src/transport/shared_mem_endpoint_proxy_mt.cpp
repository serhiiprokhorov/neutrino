#include <cstring>
#include <thread>
#include <cassert>

#include <neutrino_transport_shared_mem_endpoint_proxy_mt.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            bool exclusive_mt_shared_memory_endpoint_proxy_t::consume(const std::uint8_t* p, const std::uint8_t* e)
            {
                std::lock_guard<std::mutex> l(m_buffer_mtx);
                return singlethread_shared_memory_endpoint_proxy_t::consume(p, e);
            }

            bool optimistic_mt_shared_memory_endpoint_proxy_t::consume(const std::uint8_t* p, const std::uint8_t* e)
            {
              // optimistic strategy:
              // m_pool->m_buffer is atomic
              // m_pool->m_buffer->get_span(length) atomically reserves block of memory of length bytes from the buffer or fails
              // m_pool->next_available() is reenterable and const, returns next available buffer
              // 
              // different threads enters with data in [p, e) 
              // each thread calls get_span(e-p) and gets isolated blocks of memory, those blocks are not intereaving due to get_span(length) atomically reserves blocks
              //
              // if get_span() was succesfull, the span it returns is exclusive to current thread and it may copy data in there
              //    until buffer is full, concurrent threads write data in m_buffer simultaneously
              // 
              // get_span() fails if the buffer is full (can not fit range bytes e - p)
              // in this case two things are needed to move forward: 
              // 1) move next available buffer into m_pool->m_buffer (by using m_pool->next_available())
              //    multiple threads may call m_pool->next_available() but only one can put it into m_pool->m_buffer because of atomic exchange.
              //    if atomic exchange failed, current buffer restarts the operation, not moving forward
              // 2) mark the current buffer as dirty (a way to inform the host app of the buffer is ready)
              //    this is exclusive, is done by that thread which succeeded on atomic exchange of m_pool->m_buffer.
              //    No other thread can succeed with atomic exchange
              //    
                auto bytes_to_copy = e - p;

                std::size_t retries_on_overflow{ m_shared_memory_endpoint_proxy_params.m_retries_on_overflow };
                const std::chrono::microseconds sleep_on_overflow{ m_shared_memory_endpoint_proxy_params.m_sleep_on_overflow };

                auto optimistic_lock_retries_on_frame_add = m_params.m_optimistic_lock_retries;
                while (optimistic_lock_retries_on_frame_add--)
                {
                    // ---------------------------------------------------
                    //
                    // shared part, concurrent threads executes it
                    //
                    // ---------------------------------------------------
                    shared_memory::buffer_t* active_buf = m_pool->m_buffer.load();
                    if(bytes_to_copy)
                    {
                      auto span = active_buf->get_span(bytes_to_copy);
                      if (span.m_span)
                      {
                        //safe to copy: a region [buf + bytes_unused ... bytes_to_copy) is now in exclusive use of the current thread
                        std::copy_n(p, bytes_to_copy, span.m_span);
                        if(bytes_to_copy < span.free_bytes)
                          return true;

                        // we appear here with buffer completely full and so can be dumped immediately  
                        // for this we drop bytes_to_copy to zero emulating flush command (consume of zero bytes is recognized as flush)
                        bytes_to_copy = 0; 
                      }
                    }
                    {
                      shared_memory::buffer_t* next_buf = m_pool->next_available(active_buf);

                      if(next_buf == active_buf)
                      {
                        // buffer chain is full, no buffer is available
                        // host app is expected to process them eventually
                        // lets wait for a configured timeout and retry cycles
                        if (!retries_on_overflow)
                        {
                          return false; // TODO: handle overflow
                        }
                        retries_on_overflow--;
                        std::this_thread::sleep_for(sleep_on_overflow);
                        continue;
                      }

                      if(m_pool->m_buffer.compare_exchange_weak(active_buf, next_buf))
                      {
                        // failure to perform compare_exchange_weak means the concurrent thread has it done already: active_buf has been decalred dirty and m_pool->m_buffer now contains next available buffer
                        // compare_exchange_weak failure may occur with the code in two different states:
                        // - bytes_to_copy != 0 means we still have some data to store and the active_buf is full and we are trying to pick up another buffer to put the data in
                        //    compare_exchange_weak failure indicates concurrent thread has alredy switched the buffer, we have to repeat the loop since we still have the data to store
                        // - bytes_to_copy == 0, means we have no data and just want to declare active_buf dirty (flush it) 
                        //    compare_exchange_weak failure indicates concurrent thread has it done alredy as part of the buffer switch, we have nothing to do
                        //
                        // success to perform compare_exchange_weak means we are in exclusive use of active_buf
                        // no other thread can access it because the buffer is full and the span returned by active_buf->get_span(bytes_to_copy) would always be empty
                        // ---------------------------------------------------
                        //
                        // exclusive part, protected by compare_exchange_weak
                        //
                        // ---------------------------------------------------

                        // it is now safe to make active_buf dirty and sync with host app
                        // because no other thread access active_buf with prof: we just updated m_current_buf with compare_exchange_weak
                        // 
                        // m_dirty_count represents a sequence in which buffers become "dirty"
                        // consumer will use this number to restore the order in which buffers became dirty
                        active_buf->dirty(++m_dirty_count);
                      }

                      // with no bytes to copy we have nothing to do on retry: either this thread or concurrect thread has declared active_buf->dirty(m_dirty_count++);
                      if (!bytes_to_copy)
                        return true;
                    }

                    std::this_thread::yield();
                }
                return false; // expired retry counter, indicates overflow, TODO: handle overflow
            }
        }
    }
}
