#pragma once

#include "neutrino_transport.hpp"

#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <cassert>
#include <functional>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            template <typename H>
            struct win32_client_connection_data_t
            {
                std::atomic<H> m_h;
                std::atomic<typename win32_details::CONTROL<H>> m_c;

                ~win32_client_connection_data_t()
                {
                    auto h = m_h.load();
                    auto c = m_c.load();
                    win32_details::close_handle(h, c);
                }

                std::ptrdiff_t write(const uint8_t* b, const uint8_t* e)
                {
                    auto c = m_c.load();
                    return win32_details::write(m_h.load(), c, b, e);
                }

                bool flush()
                {
                    auto c = m_c.load();
                    return win32_details::flush(m_h.load(), c);
                }
            };

            struct namedpipe_win32_async_client_endpoint_t : public win32_client_connection_data_t<HANDLE>, public async_group_t, public endpoint_t
            {
                namedpipe_win32_async_client_endpoint_t(
                    std::shared_ptr<params_t> apo
                    , std::shared_ptr<const std::string> pipeName
                );

                std::ptrdiff_t consume(const uint8_t* b, const uint8_t* e) override;
                bool flush() override;
            };

            struct winsock2_win32_async_client_endpoint_t : public win32_client_connection_data_t<SOCKET>, public async_group_t, public endpoint_t
            {
                winsock2_win32_async_client_endpoint_t(
                    std::shared_ptr<params_t> apo
                );

                std::ptrdiff_t consume(const uint8_t* b, const uint8_t* e) override;
                bool flush() override;
            };

        }
    }
}
