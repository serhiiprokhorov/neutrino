#include <cstring>
#include <neutrino_transport_buffered_st.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            std::ptrdiff_t buffered_singlethread_endpoint_t::consume(const std::uint8_t* p, const std::uint8_t* e)
            {
                // step one: copy data
                std::ptrdiff_t b = e - p;

                if(!b) // 0 bytes is a way how caller asks to flush the buffer
                {
                    flush();
                    return 0;
                }

                auto end = m_frame_start + b;

                if (end >= m_sz) 
                {
                    // proposed amount of bytes + current buffer in-use bytes may overflow the buffer, flush first
                    if(!flush())
                    {
                        // TODO: retry on fatal consumer error
                        // TODO: retval & retry || retval & fatal
                        // TODO error.fetch_or(neutrino::impl::frame_v00::header::bits::MASK_PREV_FRAME_ERROR);
                        return 0;
                    }
                    end = m_frame_start + b;
                    if(end > m_sz)
                    {
                        // TODO: handle overflow
                        return 0;
                    }
                }

                // a region [pcfg->m_message_buf + start ... pcfg->m_message_buf + start + b) is now in exclusive use of current thread
                std::copy(p, p + b, m_data + m_frame_start);
                m_frame_start = end;

                if(m_frame_start > m_buffered_endpoint_params.m_message_buf_watermark)
                    flush();
                return b;
            }
            bool buffered_singlethread_endpoint_t::flush()
            {
                if(!m_frame_start)
                    return true;

                auto* p = m_data;
                if (!m_endpoint->consume(p, p + m_frame_start))
                {
                    // TODO: retry on fatal consumer error
                    // TODO: retval & retry || retval & fatal
                    // TODO error.fetch_or(neutrino::impl::frame_v00::header::bits::MASK_PREV_FRAME_ERROR);
                    return false;
                }
                m_frame_start = 0;

                return true;
            }
        }
    }
}
