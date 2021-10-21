#include <cstring>
#include <neutrino_transport_shared_mem_endpoint_proxy_st.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            bool singlethread_shared_memory_endpoint_proxy_t::consume(const std::uint8_t* p, const std::uint8_t* e)
            {
                // step one: copy data
                // 0 requires to flush current buffer
                auto b = e - p;

                shared_memory::buffer_t* buffer = m_pool->m_buffer.load(); // TODO: remove threaded stuff

                do
                {
                  if(b)
                  {
                    if (auto span = buffer->get_span(b))
                    {
                      std::copy(p, e, span.m_span);
                      /* TODO: watermark has no use now
                      if(span.free_bytes > m_shared_memory_endpoint_proxy_params.m_message_buf_watermark)
                      */
                      break;
                    }
                  }

                  buffer->dirty(m_dirty_buffer_counter++);

                  shared_memory::buffer_t* next_buffer = m_pool->next_available(buffer);
                  if(next_buffer == buffer)
                  {
                    std::size_t m_retries_on_overflow{ 10 };
                    std::chrono::microseconds m_sleep_on_overflow{ std::chrono::milliseconds{10} };
                    if(!m_retries_on_overflow)
                    {
                      return false; // TODO: handle overflow
                    }
                    m_retries_on_overflow--;
                    std::this_thread::sleep_for(m_sleep_on_overflow);
                  }
                  else
                  {
                    m_pool->m_buffer = (buffer = next_buffer);
                  }

                } while(b);

                return true;
            }
        }
    }
}
