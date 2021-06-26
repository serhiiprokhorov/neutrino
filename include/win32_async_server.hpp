#pragma once

#include "endpoint_pool.hpp"
#include "win32_completition_port_data.hpp"

#include <WinSock2.h>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct win32_async_server_t
            {
                win32_completition_port_data_t& m_compl_port_data;
                endpoint_pool_t& m_endpoint_pool;
            };

            struct namedpipe_win32_async_server_t : public win32_async_server_t
            {
                namedpipe_win32_async_server_t(
                    std::shared_ptr<params_t> apo
                    , win32_completition_port_data_t& compl_port_data
                    , endpoint_pool_t& endpoint_pool
                    , std::shared_ptr<const std::string> pipeName
                    , std::size_t num_instances
                    , DWORD nOutBufferSize
                    , DWORD nInBufferSize
                );
            };

            struct winsock2_win32_async_server_t : public win32_async_server_t
            {
                winsock2_win32_async_server_t(
                    std::shared_ptr<params_t> apo
                    , win32_completition_port_data_t& compl_port_data
                    , endpoint_pool_t& endpoint_pool
                    , std::function<bool(SOCKET&)> listener_configure
                );
                ~winsock2_win32_async_server_t();
            };
        }
    }
}
