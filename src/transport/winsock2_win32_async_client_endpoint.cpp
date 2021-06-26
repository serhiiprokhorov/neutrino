#include <cstring>
#include <win32_async_server.hpp>
#include <neutrino_transport.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            winsock2_win32_HANDLE_async_client_endpoint_t::winsock2_win32_HANDLE_async_client_endpoint_t(
                std::shared_ptr<params_t> apo
            )
                : win32_client_connection_data_t<SOCKET>()
                , async_group_t(apo)
                , endpoint_t()
            {
                throw std::runtime_error("not implemented");
            }

            std::ptrdiff_t winsock2_win32_HANDLE_async_client_endpoint_t::consume(const uint8_t* b, const uint8_t* e)
            {
                return win32_client_connection_data_t<SOCKET>::write(b, e);
            }

            bool winsock2_win32_HANDLE_async_client_endpoint_t::flush()
            {
                return win32_client_connection_data_t<SOCKET>::flush();
            }
        }
    }
}
