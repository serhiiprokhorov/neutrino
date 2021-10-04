#include <cstring>
#include <neutrino_transport_buffered_endpoint_proxy_st.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            bool singlethread_shared_memory_endpoint_proxy_t::consume(const std::uint8_t* p, const std::uint8_t* e)
            {
                // step one: copy data
                auto b = e - p;

                if(!b) // 0 bytes is a way how caller asks to flush the buffer
                    return true;

                shared_memory::buffer_t* buffer = m_pool->m_buffer.load(); // TODO: remove threaded stuff

                do
                {
                  if (auto span = buffer->get_span(b))
                  {
                    std::copy(p, e, span.m_span);
                    if(span.free_bytes > m_shared_memory_endpoint_proxy_params.m_message_buf_watermark)
                      return true;
                  }

                  shared_memory::buffer_t* next_buffer = m_pool->next_available(buffer);
                  if(next_buffer == buffer)
                    return false; // TODO: handle overflow

                  (buffer = next_buffer)->dirty();
                } while(true);

                return true;
            }
        }
    }
}
