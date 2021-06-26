#include <cstring>
#include <win32_completition_port_data.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            /*
            namespace win32_details
            {
                template <>
                void close_handle<HANDLE>(HANDLE& h, CONTROL<HANDLE>&)
                {
                    ::CloseHandle(h);
                    h = INVALID_HANDLE_VALUE;
                }

                template <>
                void close_handle<SOCKET>(SOCKET& s, CONTROL<HANDLE>&)
                {
                    ::closesocket(s);
                    s = INVALID_SOCKET;
                }

                template <>
                std::ptrdiff_t write<HANDLE>(HANDLE& h, CONTROL<HANDLE>&, const uint8_t* b, const uint8_t* e)
                {
                    if (h == INVALID_HANDLE_VALUE)
                        return false;

                    const auto* base_b = b;
                    while (b < e)
                    {
                        DWORD NumberOfBytesWritten = 0;
                        if (!WriteFile(h, b, e - b, &NumberOfBytesWritten, NULL) || !NumberOfBytesWritten)
                        {
                            close_handle(h);
                            break;
                        }
                        b += NumberOfBytesWritten;
                    }
                    return b - base_b;
                }

                template <>
                bool flush<HANDLE>(HANDLE h, CONTROL<HANDLE>&)
                {
                    auto h = m_h.load();
                    return h == INVALID_HANDLE_VALUE ? false : FlushFileBuffers(h);
                }

                template <>
                std::ptrdiff_t write<SOCKET>(SOCKET& s, CONTROL<HANDLE>& c, const uint8_t* b, const uint8_t* e)
                {
                    if (h == INVALID_SOCKET)
                        return false;

                    const auto* base_b = b;
                    while (b < e)
                    {
                        int sent_bytes = 0;
                        if(c.m_to)
                        {
                            sent_bytes = sendto(
                                s,
                                const char* buf,
                                int            len,
                                c.m_flags,
                                c.m_to,
                                c.m_tolen
                            );
                        }
                        else
                        {
                            sent_bytes = send(
                                s,
                                const char* buf,
                                int        len,
                                c.m_flags
                            );
                        }
                        if(sent_bytes == SOCKET_ERROR)
                        {
                            close_handle(s);
                            break;
                        }
                        b += sent_bytes;
                    }
                    return b - base_b;
                }

                template <>
                bool flush<SOCKET>(SOCKET h, CONTROL<HANDLE>&)
                {
                    auto h = m_h.load();
                    return h == INVALID_HANDLE_VALUE ? false : FlushFileBuffers(h);
                }
            }
            */

            win32_completition_port_data_t::win32_completition_port_data_t(
                std::size_t num_io
                , std::size_t io_buffer_size
                , std::size_t num_workers
                , std::size_t num_active_workers
                , std::shared_ptr<params_t> params
            )
                : async_group_t(params)
            {
                m_compl_port = CreateIoCompletionPort(
                    INVALID_HANDLE_VALUE,
                    NULL,
                    0,
                    num_active_workers
                );

                if(m_compl_port == INVALID_HANDLE_VALUE)
                {
                    throw std::runtime_error(std::string("CreateIoCompletionPort failed, GetLastError").append(std::to_string(GetLastError())));
                }

                m_ports.reserve(num_io);
                for(std::size_t i = 0; i < num_io; i++)
                {
                    m_ports.emplace_back(PORT_INUSE::FREE, io_buffer_size);
                }

                auto compl_port_worker_factory = [this](worker_t& acm)
                {
                    acm.m_need_data = [this]() { return m_compl_port != INVALID_HANDLE_VALUE; };
                    const DWORD dwMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(event_to).count();
                    acm.m_read_data = [this, dwMilliseconds]()
                    {
                        DWORD NumberOfBytesTransferred = 0;
                        ULONG CompletionKey = 0;
                        LPOVERLAPPED lpOverlapped = NULL;
                        auto retGetQueuedCompletionStatus = GetQueuedCompletionStatus(
                            m_compl_port,
                            &NumberOfBytesTransferred,
                            &CompletionKey,
                            &lpOverlapped,
                            dwMilliseconds);

                        if (!retGetQueuedCompletionStatus && lpOverlapped == NULL)
                        {
                            // fatal error on m_compl_port or spurious wakeup
                            // CompletitionKey is undefined
                            auto err = GetLastError();
                            if (err == ERROR_ABANDONED_WAIT_0) // compl port is no longer valid
                                return worker_t::STATUS::BAD;
                            return worker_t::STATUS::NO_DATA; /*IO has failed, recover is possible at next call*/
                        }

                        port_t* pcompl_port = reinterpret_cast<port_t*>(lpOverlapped);

                        //const server_endpoint_t::endpoint_id_t compl_id = CompletionKey;
                        return pcompl_port->m_on_completition(
                            // TODO: revisit GetQueuedCompletionStatus for status
                            COMPLETITION_PORT_ACTION::PROCESS
                            , *pcompl_port
                            , CompletionKey
                            , NumberOfBytesTransferred);
                    };
                    acm.m_status = []() { return true; };
                };

                for(std::size_t i = 0; i < num_workers; i++)
                {
                    async_group_t::add_worker(compl_port_worker_factory);
                }
            };

            win32_completition_port_data_t::~win32_completition_port_data_t()
            {
                // notify pending connections of a shutdown
                for (auto& cp : m_ports)
                {
                    if(cp.second.m_on_completition)
                    {
                        cp.second.m_on_completition(nullptr, 0 /*CompletionKey*/, 0 /*NumberOfBytesTransferred*/);
                    }
                }
                async_group_t::stop_async_group();
                win32_details::close_handle(m_compl_port);
            }
        }
    }
}
