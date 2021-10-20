#pragma once

#include <vector>
#include <memory>
#include <list>
#include <algorithm>
#include <mutex>
#include <thread>
#include <cassert>

#include "neutrino_transport.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace shared_memory
        {
            /*
            struct sync_t
            {
              virtual ~sync_t() = default;

              virtual const bool is_clean() const noexcept = 0;
              virtual void dirty() noexcept = 0;
              virtual void clear() noexcept = 0;
            };
            */

            struct buffer_t
            {
                buffer_t* m_next;

                buffer_t(buffer_t* _next)
                  : m_next(_next)
                {
                }

                virtual ~buffer_t() = default;

                virtual const bool is_clean() const noexcept = 0;
                virtual void dirty() noexcept = 0;
                virtual void clear() noexcept = 0;

                //virtual sync_t* get_sync() const noexcept = 0;

                /// @return either a ptr to mem block of size length or nullptr if not enough space in this buffer
                /// atomically updates buffer data so no other thread may use this area
                struct span_t
                {
                  uint8_t* m_span = nullptr;
                  uint64_t free_bytes = 0; // how many bytes left in a buffer
                  uint64_t sequence = 0; // non decreasing counter, indicates relative time when the buffer was marked dirty
                  operator bool() const noexcept { return m_span; }
                };

                virtual span_t get_span(const uint64_t length) noexcept = 0;

                virtual span_t get_data() noexcept = 0;
            };

            struct pool_t
            {
              std::atomic<buffer_t*> m_buffer{nullptr};

                buffer_t* next_available(buffer_t* c) const noexcept
                {
                    assert(c);
                    buffer_t* buf = c->m_next;

                    assert(buf);

                    while (buf != c && !buf->is_clean())
                    {
                      buf = buf->m_next;
                      assert(buf);
                    }

                    return buf;
                }
            };
        }

        namespace transport
        {
            struct shared_memory_endpoint_proxy_t : public endpoint_proxy_t
            {
                struct shared_memory_endpoint_proxy_params_t
                {
                    std::size_t m_message_buf_watermark{ 0 };
                    std::size_t m_retries_on_overflow{10};
                    std::chrono::microseconds m_sleep_on_overflow{ std::chrono::milliseconds{10} };
                } const m_shared_memory_endpoint_proxy_params;

                std::shared_ptr<shared_memory::pool_t> m_pool;

                shared_memory_endpoint_proxy_t(
                    const shared_memory_endpoint_proxy_params_t& po
                    , std::shared_ptr<shared_memory::pool_t> pool
                )
                    : 
                    m_shared_memory_endpoint_proxy_params(po)
                    , m_pool(pool)
                {
                }
            };
        }
    }
}
