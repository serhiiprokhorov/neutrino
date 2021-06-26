#include <cstring>
#include <neutrino_transport_endpoint_async_win32.hpp>

#include <winbase.h>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            namedpipe_win32_async_server_endpoint_t::namedpipe_win32_async_server_endpoint_t(
                std::shared_ptr<params_t> apo
                , std::shared_ptr<const std::string> pipeName
                , std::size_t num_instances
                , DWORD nOutBufferSize
                , DWORD nInBufferSize
                , endpoint_factory_t& endpoint_factory
            )
                : win32_server_connection_data_t()
                , async_group_t(apo)
                , server_endpoint_t(endpoint_factory)
            {
                m_vh.assign(decltype(m_vh)::size_type{ num_instances }, INVALID_HANDLE_VALUE);
                for(std::size_t c = 0; c < num_instances; c++)
                {
                    add_worker(
                        [=](worker_t& acm)
                        {
                            // TODO: pipe is limited in number of connections
                            //       add conections here with loop
                            // TODO: add an array of handles,events whatever and bind them to conn monitors via lambda
                            acm.m_need_data = [](const std::chrono::nanoseconds&) { return true; }; // greedy server
                            acm.m_read_data = [=](const std::chrono::nanoseconds& to)
                            {
                                auto& h = m_vh[c];
                                // TODO: no need to create a pipe each time
                                if (h == INVALID_HANDLE_VALUE)
                                {
                                    for (auto first_instance : { FILE_FLAG_FIRST_PIPE_INSTANCE , 0 })
                                    {
                                        h = CreateNamedPipeA(
                                            pipeName->c_str(),
                                            PIPE_ACCESS_INBOUND
                                            | FILE_FLAG_FIRST_PIPE_INSTANCE // TODO: reconsider
                                            , PIPE_TYPE_MESSAGE
                                            | PIPE_WAIT
                                            | PIPE_ACCEPT_REMOTE_CLIENTS
                                            , num_instances // nMaxInstances
                                            , nOutBufferSize
                                            , nInBufferSize
                                            , std::chrono::duration_cast<std::chrono::milliseconds>(apo->m_connection_timeout).count() // nDefaultTimeOut milliseconds
                                            , NULL // lpSecurityAttributes
                                        );
                                        if (h != INVALID_HANDLE_VALUE)
                                        {
                                            break;
                                        }
                                    }
                                }
                                // If the operation is synchronous, ConnectNamedPipe does not return until the operation has completed
                                if (h != INVALID_HANDLE_VALUE && !ConnectNamedPipe(h, NULL))
                                {
                                    auto err = GetLastError();
                                    switch (err)
                                    {
                                    case ERROR_IO_PENDING:
                                        return false; // not a fatal error
                                    case ERROR_PIPE_CONNECTED:
                                        return true;
                                    case ERROR_NO_DATA:
                                        DisconnectNamedPipe(h);
                                        return false;
                                    }
                                    CloseHandle(h);
                                    h = INVALID_HANDLE_VALUE;
                                }

                                return h != INVALID_HANDLE_VALUE;
                            };

                            acm.m_status = [this, c]() 
                            { 
                                return m_vh[c] != INVALID_HANDLE_VALUE;
                            };
                        }
                    );
                }
            }

            namedpipe_win32_HANDLE_async_client_endpoint_t::namedpipe_win32_HANDLE_async_client_endpoint_t(
                std::shared_ptr<async_connection_monitor_params_t> apo
                , std::shared_ptr<const std::string> pipeName
            )
                : win32_HANDLE_client_connection_data_t()
                , async_group_t(apo)
                , endpoint_t()
            {
                add_worker(
                    [this, pipeName](worker_t& acm)
                    {
                        acm.m_need_data = [this](const std::chrono::nanoseconds&) { return m_h == INVALID_HANDLE_VALUE; };

                        acm.m_read_data = [pipeName, this]()
                        {
                            m_h = CreateFileA(
                                pipeName->c_str(),
                                GENERIC_WRITE,
                                FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING, // expect to be opened by server
                                FILE_ATTRIBUTE_NORMAL,
                                NULL // lpSecurityAttributes
                            );

                            return m_h != INVALID_HANDLE_VALUE;
                        };

                        acm.m_status = [this]() { return m_h != INVALID_HANDLE_VALUE; }; // no need to notify
                    }
                );
            }

            std::ptrdiff_t namedpipe_win32_HANDLE_async_client_endpoint_t::consume(const uint8_t* b, const uint8_t* e) 
            {
                return win32_HANDLE_client_connection_data_t::write(b, e);
            }

            bool namedpipe_win32_HANDLE_async_client_endpoint_t::flush() 
            {
                return win32_HANDLE_client_connection_data_t::flush();
            }
        }
    }
}
