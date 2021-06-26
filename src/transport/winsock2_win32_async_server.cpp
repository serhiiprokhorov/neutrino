#include <cstring>
#include <win32_async_server.hpp>
#include <neutrino_transport.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            static async_group_t::worker_t::STATUS read_on_socket_completition(
                win32_completition_port_data_t::COMPLETITION_PORT_ACTION action
                , SOCKET socket
                , win32_completition_port_data_t::port_t& compl_port
                , win32_completition_port_data_t::buffer_t::buffer_consumer_t& consumer
                , DWORD completitionKey
                , DWORD NumberOfBytesTransferred
            )
            {
                async_group_t::worker_t::STATUS ret = async_group_t::worker_t::STATUS::WAIT;

                // WSARecv may return data immediately, loop while it is still a case
                do
                {
                    if (NumberOfBytesTransferred)
                    {
                        ret = worker_t::CONNECTION_STATUS::HAS_DATA;
                        if (!compl_port.m_buffer.consume_with_carryover(consumer, NumberOfBytesTransferred))
                            break; // connection is sending junk, disconnect
                    }

                    DWORD Flags = 0;
                    WSABUF wsaBuf = compl_port.get_WSABUF(pcompl_port->.m_buffer);
                    auto retWSARecv = WSARecv(
                        socket,
                        wsaBuf,
                        1,
                        &NumberOfBytesTransferred,
                        &Flags,
                        &compl_port.m_overlapped,
                        nullptr
                    );
                    if (retWSARecv == SOCKET_ERROR)
                    {
                        auto err = WSAGetLastError();
                        // WSA_IO_PENDING is not an error, indicates no data is awailable, wait for next compl port event
                        if (err == WSA_IO_PENDING)
                            return worker_t::STATUS::WAIT;
                        break; // disconnect on any other socket error
                    }
                } while (NumberOfBytesTransferred);

                compl_port.release();
                return worker_t::STATUS::BAD;
            }

            struct exception_fatal {};
            struct exception_retry {};

            winsock2_win32_async_server_t::winsock2_win32_async_server_t(
                std::shared_ptr<params_t> apo
                , win32_completition_port_data_t& compl_port_data
                , endpoint_pool_t& endpoint_pool
                , std::function<bool(SOCKET&)> listener_configure
            ) : m_listener_factory(listener_factory), win32_async_server_t(compl_port_data, endpoint_pool)
            {
                std::shared_ptr<SOCKET> listener = INVALID_SOCKET;
                m_compl_port_data.m_async_group.add_worker(
                    [this](worker_t& acm)
                    {
                        acm.m_need_data = [listener, listener_configure](const std::chrono::nanoseconds&)
                        {
                            SOCKET s = *listener_configure;
                            bool configured = s != INVALID_SOCKET;
                            if (!configured)
                            {
                                s = WSASocket(af, type, protocol, NULL, SG_UNCONSTRAINED_GROUP, WSA_FLAG_OVERLAPPED);
                                if((configured = listener_configure(s)))
                                {
                                    std::swap(listener, std::shared_ptr<SOCKET>(s, [](auto& x){::closesocket(x);});
                                }
                            }
                            return configured;
                        };
                        acm.m_read_data = [this, listener](const std::chrono::nanoseconds& to)
                        {
                            auto ret { async_group_t::worker_t::STATUS::WAIT };

                            SOCKET listener_socket = *listener;
                            if(listener_socket == INVALID_SOCKET)
                                return async_group_t::worker_t::STATUS::NO_DATA;

                            sockaddr addr;
                            INT addrlen = 0;
                            auto socket = WSAAccept(listener_socket, &addr, &addrlen);

                            if (socket == INVALID_SOCKET)
                            {
                                return async_group_t::worker_t::STATUS::NO_DATA;
                            }

                            try
                            {
                                DWORD completitionKey = endpoint_id.m_id;
                                if (CreateIoCompletionPort(socket, m_compl_port, completitionKey, 0) == INVALID_HANDLE_VALUE)
                                {
                                    throw exception_fatal;
                                }

                                // post notification to help process a data available immediately
                                if (!PostQueuedCompletionStatus(m_compl_port, 0, completitionKey, &compl_port))
                                {
                                    throw exception_fatal;
                                }

                                auto endpoint_guard = m_endpoint_pool.allocate_endpoint();
                                if (!endpoint_guard)
                                {
                                    throw exception_fatal;
                                }

                                auto consumer = [endpoint_guard](const char8_t* b, const char8_t* e)
                                {
                                    return endpoint_guard.m_endpoint->consume(b, e);
                                };

                                auto f = [socket, consumer]->async_group_t::worker_t::STATUS(
                                    win32_completition_port_data_t::COMPLETITION_PORT_ACTION
                                    , win32_completition_port_data_t::port_t & pcompl_port
                                    , ULONG CompletionKey
                                    , DWORD NumberOfBytesTransferred
                                )
                                {
                                    return read_on_socket_completition(
                                        socket
                                        , *pcompl_port
                                        , consumer
                                        , completitionKey
                                        , NumberOfBytesTransferred
                                    );
                                };

                                if (!m_compl_port_data.bind_free_port(f))
                                {
                                    throw exception_fatal;
                                }
                                return async_group_t::worker_t::STATUS::WAIT;
                            }
                            catch(const exception_fatal&)
                            {
                                ret = async_group_t::worker_t::STATUS::BAD;
                            }

                            ::closesocket(socket);
                            //auto err = WSAGetLastError();
                            return ret;
                        };
                        acm.m_status = [listener](const std::chrono::nanoseconds& to)
                        {
                            auto s = *listener;
                            return s != INVALID_SOCKET;
                        };
                    }
                );
            }

            winsock2_win32_async_server_t::~winsock2_win32_async_server_t()
            {
                if(m_listener != INVALID_SOCKET)
                {
                    ::closesocket(m_listener);
                }
            }


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
