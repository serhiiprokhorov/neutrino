#pragma once

#include <vector>
#include <memory>
#include <list>
#include <algorithm>
#include <mutex>
#include <thread>
#include "neutrino_transport.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct span_t
            {
                virtual ~span_t() = default;

                virtual uint8_t* data() noexcept = 0;
                virtual const std::size_t size() const noexcept = 0;
            };

            struct sync_t
            {
                virtual ~sync_t() = default;

                virtual const bool is_clean() noexcept const = 0;
                virtual void dirty() noexcept = 0;
                virtual void clear() noexcept = 0;
            };

            struct buffer_t
            {
                typedef std::list<
                    std::shared_ptr<span_t>
                    , std::shared_ptr<sync_t>
                > container_t;

                std::mutex m_sync; 
                container_t m_buffers;

                template<typename T>
                buffer_t(T&& t)
                    : m_buffers(std::forward<T>(t))
                {
                }

                container_t::iterator first_available() noexcept
                {
                    return this_or_next_available(m_buffers.begin());
                }

                container_t::iterator this_or_next_available(container_t::iterator current) noexcept
                {
                    assert(!m_buffer.empty());
                    assert(current != m_buffer.end());

                    if(current -> is_dirty())
                    {
                        std::lock_guard<std::mutex> l(m_sync);
                        auto candidate = current;
                        do
                        {
                            if (++candidate == m_buffers.end())
                                candidate = m_buffers.begin();
                        } while (candidate->is_dirty() && candidate != current);
                        current = candidate;
                    }
                    return current;
                }
            };

            struct buffered_endpoint_proxy_t : public endpoint_proxy_t
            {
                struct buffered_endpoint_params_t
                {
                    std::size_t m_message_buf_watermark{ 0 };
                } const m_buffered_endpoint_params;

                std::shared_ptr<buffer_t> m_message_buf;
                buffer_t::container_t::iterator m_current_buf;

                std::shared_ptr<endpoint_proxy_t> m_endpoint_sp;

                buffered_endpoint_proxy_t(
                    std::shared_ptr<endpoint_t> endpoint
                    , std::shared_ptr<buffer_t> message_buf
                    , const buffered_endpoint_params_t po
                )
                    : 
                      m_buffered_endpoint_params(po)
                    , m_message_buf(message_buf)
                    , m_current_buf(message_buf->first_available())
                    , m_endpoint_sp(endpoint)
                {
                }
            };
        }
    }
}
