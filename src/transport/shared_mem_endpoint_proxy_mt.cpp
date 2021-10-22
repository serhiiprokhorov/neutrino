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
              // each thread calls get_span() and gets isolated blocks of memory, those blocks are not intereaving due to get_span(length) atomically reserves blocks
              //
              // if get_span() was succesfull, the span it returns is exclusive to current thread and it may copy data in there
              //    until buffer is full, concurrent threads write data in m_buffer simultaneously
              // 
              // get_span() fails if the buffer is full (can not fit range bytes e - p)
              // in this case two things are needed to move forward: 
              // 1) move next available buffer into m_pool->m_buffer (by using m_pool->next_available())
              //    multiple threads may call m_pool->next_available() but only one can put it into m_pool->m_buffer ecause of atomic exchange.
              //    if atomic exchange failed, current buffer restarts the operation, not moving forward
              // 2) mark the current buffer as dirty (a way to inform the host app of the buffer is ready)
              //    this is exclusive, is done by only that thread which succeeded on atomic exchange of m_pool->m_buffer.
              //    No other thread can succeed with atomic exchange
              //    
                auto bytes_to_copy = e - p;

                if (!bytes_to_copy)
                  return true;

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
                        if (span.free_bytes > m_shared_memory_endpoint_proxy_params.m_message_buf_watermark)
                          return true;
                        bytes_to_copy = 0; // about to repeat whole loop for buffer cleanup, prevent write the data once agan
                      }
                    }
                    {
                      shared_memory::buffer_t* next_buf = m_pool->next_available(active_buf);

                      if(next_buf == active_buf)
                        return false; // TODO: handle overflow

                      if(!m_pool->m_buffer.compare_exchange_weak(active_buf, next_buf))
                      {
                        // the code may appear here from two different states:
                        // - bytes_to_copy != 0, meaning the buffer was full and we are trying to pick up another buffer to put the data in
                        // - bytes_to_copy == 0, meaning we have copied the data and the buffer has exceeded the watermark
                        //
                        // in former case we have have to repeat, the data yet not stored
                        // in latter case we have nothing to do, concurrent thread done everithing
                        if(!bytes_to_copy)
                          return true; // other thread has changed m_pool->m_buffer and made it dirty already, nothing to do 
                        // other thread has already switched the buffer
                        std::this_thread::yield();
                        continue;
                      }
                      // ---------------------------------------------------
                      //
                      // exclusive part, protected by compare_exchange_weak
                      //
                      // ---------------------------------------------------

                      // it is now safe to make active_buf dirty and sync with host app
                      // because no other thread access active_buf with prof: we just updated m_current_buf with compare_exchange_weak
                      active_buf->dirty(m_dirty_count++);

                      std::this_thread::yield();
                      continue;
                    }

                    return true;
                }
                return false; // expired retry counter, indicates overflow, TODO: handle overflow
            }
        }
    }
}
