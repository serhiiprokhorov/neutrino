#include <cstring>
#include <thread>
#include <cassert>

#include <neutrino_transport_buffered_mt.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            std::ptrdiff_t buffered_exclusive_endpoint_t::consume(const std::uint8_t* p, const std::uint8_t* e)
            {
                std::lock_guard<std::mutex> l(m_buffer_mtx);
                return buffered_singlethread_endpoint_t::consume(p, e);
            }

            std::ptrdiff_t buffered_optimistic_endpoint_t::consume(const std::uint8_t* p, const std::uint8_t* e)
            {
                const auto beyond_the_end = (unsigned long long)1 + m_message_buf.size();

                // step one: copy data
                std::ptrdiff_t b = e - p;

                if(!b) // 0 bytes is a way how caller asks to flush the buffer
                {
                    flush();
                    return 0;
                }

                auto optimistic_lock_retries_on_frame_add = m_params.m_optimistic_lock_retries;
                while (optimistic_lock_retries_on_frame_add--)
                {
                    auto start = m_frame_start.load();

                    if(start > m_message_buf.size())
                    {
                        // flush in progress
                        std::this_thread::yield();
                        continue;
                    }

                    auto end = start + b;

                    if (end >= m_message_buf.size()) // TODO: overflow
                    {
                        // proposed amount of bytes + current buffer in-use bytes may overflow the buffer, flush first
                        if(!flush())
                        {
                            // TODO error.fetch_or(neutrino::impl::frame_v00::header::bits::MASK_PREV_FRAME_ERROR);
                            return 0;
                        }
                        optimistic_lock_retries_on_frame_add++; // add retry since flush() is not a failure
                        continue;
                    }

                    if (!m_frame_start.compare_exchange_strong(start, beyond_the_end))
                    {
                        // conflict: other thread updated m_frame_start, retry
                        std::this_thread::yield();
                        continue;
                    }

                    // a region [pcfg->m_message_buf + start ... pcfg->m_message_buf + start + b) is now in exclusive use of current thread
                    std::copy_n(p, b, m_message_buf.begin() + start);

                    auto dummy = beyond_the_end;
                    if (!m_frame_start.compare_exchange_strong(dummy, end))
                    {
                        // conflict: other thread updated m_frame_start, retry
                        std::this_thread::yield();
                        continue;
                    }

                    // step two: send if data above watermark
                    if(m_frame_start.load() > m_buffered_endpoint_params.m_message_buf_watermark)
                        flush();
                    return b;
                }
                return 0;
            }

            bool buffered_optimistic_endpoint_t::flush()
            {
                auto optimistic_lock_retries_on_consume = m_params.m_optimistic_lock_retries;

                while (optimistic_lock_retries_on_consume--)
                {
                    auto occupied = m_frame_start.load();
                    if (!occupied)
                        return true;
                    // prevent any other additions to a buffer while it is being flushed
                    // also block (spin lock) other threads flush
                    auto beyond_the_end = decltype(occupied)(2) + m_message_buf.size();
                    if (occupied < m_message_buf.size() && m_frame_start.compare_exchange_strong(occupied, beyond_the_end))
                    {
                        const auto* p = &(m_message_buf[0]);
                        if (!m_endpoint->consume(p, p + occupied))
                        {
                            // TODO: retry on fatal consumer error
                            // TODO: retval & retry || retval & fatal
                            // TODO error.fetch_or(neutrino::impl::frame_v00::header::bits::MASK_PREV_FRAME_ERROR);
                            return false;
                        }
                        if(m_frame_start.compare_exchange_strong(beyond_the_end, 0)) // TODO: allows sporadic re-consume
                            break;
                    }

                    // more data has been added to a buffer during consume operation, consume that new data
                    std::this_thread::yield();
                }

                return true;
            }
        }
    }
}
