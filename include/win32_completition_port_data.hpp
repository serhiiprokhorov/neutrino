#pragma once

#include "async_group.hpp"

#include <atomic>
#include <vector>
#include <functional>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct win32_completition_port_data_t
            {
                HANDLE m_compl_port = INVALID_HANDLE_VALUE;
                async_group_t m_async_group;

                enum class COMPLETITION_PORT_ACTION
                {
                    PROCESS
                    , DISCONNECT
                };

                struct buffer_t
                {
                    std::vector<CHAR> m_data;

                    auto& resize(std::size_t buf_size)
                    {
                        m_data.resize(buf_size);
                        return *this;
                    }

                    const std::size_t m_carryover_bytes_threshold = 20; // TODO: parametrize a limit of how many bytes we collect until decide the connection is sending junk
                    ptrdiff_t m_carryover_bytes = 0;

                    typedef std::function<const std::ptrdiff_t(const char8_t*, const char8_t*)> buffer_consumer_t;

                    // false if carryover bytes exceed threshold, the connection is sending junk
                    // TODO: eliminate header dependenfy from endpoint_t
                    bool consume_with_carryover(buffer_consumer_t& consumer, DWORD NumberOfBytesTransferred)
                    {
                        auto bytes_to_consume = compl_port.m_carryover_bytes + NumberOfBytesTransferred;
                        m_carryover_bytes = consumer(m_data.begin(), m_data.begin() + bytes_to_consume);

                        if (m_carryover_bytes == bytes_to_consume // no progress
                            || m_carryover_bytes > m_carryover_bytes_threshold // too many bytes of junk
                            )
                            return false;

                        if (m_carryover_bytes > 0)
                            std::copy_n(m_data.begin() + carryover_bytes, carryover_bytes, m_data.begin());

                        return true;
                    }
                };

                struct port_t
                {
                    WSAOVERLAPPED m_overlapped = {0};
                    buffer_t m_buffer;

                    WSABUF get_WSABUF(win32_completition_buffer_t& b) const
                    {
                        WSABUF ret = {0};
                        ret.buf = &b.m_data[m_carryover_bytes];
                        ret.len = b.m_data.size();
                    }

                    typedef std::function<
                        async_group_t::worker_t::STATUS(
                            COMPLETITION_PORT_ACTION
                            , port_t& /*compl_port*/
                            , ULONG /*CompletionKey*/
                            , DWORD /*NumberOfBytesTransferred*/
                        )> handler_t;

                    handler_t m_on_completition = nullptr;

                    void release()
                    {
                        m_on_completition = nullptr;
                    }

                    port_t(std::size_t buf_size)
                    {
                        m_overlapped.hEvent = INVALID_HANDLE_VALUE;
                        resize(buf_size).get_WASBUF(m_wsaBuf);
                    }

                    ~port_t()
                    {
                        if(m_overlapped.hEvent != INVALID_HANDLE_VALUE)
                            ::CloseHandle(m_overlapped.hEvent);
                    }
                };

                std::vector<port_t> m_ports;

                bool bind_free_port(port_t::handler_t f)
                {
                    for(auto& c : m_ports)
                    {
                        if(!cm_on_completition)
                        {
                            c.m_on_completition = f;
                            return true;
                        }
                    }
                    return false;
                }

                win32_completition_port_data_t(
                    std::size_t num_channels
                    , std::size_t channel_buffer_size
                    , std::size_t num_workers
                    , std::size_t num_active_workers
                    , std::shared_ptr<params_t> params);

                ~win32_completition_port_data_t();
            };
        }
    }
}
