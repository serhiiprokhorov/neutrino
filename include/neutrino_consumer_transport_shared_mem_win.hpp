#pragma once

#include <memory>
#include <string>
#include <stdexcept>

#include "neutrino_transport_buffered.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            namespace win_shared_mem
            {
                struct v00_names_t
                {
                    v00_names_t(const std::string& domain, std::size_t index);

                    const std::string m_shmm_name;
                    const std::string m_event_name;
                    const std::string m_sem_name;
                };

                enum class OPEN_MODE
                {
                    CREATE
                    , OPEN
                };

                struct v00_header_t
                {
                    DWORD m_header_size;
                    DWORD m_dwLayoutVersion;
                    DWORD m_hostPID;
                    DWORD m_dwMaximumSizeHigh;
                    DWORD m_dwMaximumSizeLow;

                    v00_header_t(OPEN_MODE op, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow);
                    std::size_t size() const { return sizeof(*this); }
                };

                struct v00_sync_t : public sync_t
                {
                    HANDLE m_hevent = INVALID_HANDLE_VALUE;
                    HANDLE m_hsem = INVALID_HANDLE_VALUE;
                    bool m_is_clean = true;

                    v00_sync_t(OPEN_MODE op, const v00_names_t&)
                    ~v00_sync_t();

                    const bool is_clean() const override { return m_is_clean; }
                    void dirty() override noexcept;
                    void clear() override noexcept;
                };

                struct v00_span_t : public span_t
                {
                    HANDLE m_hshmm = INVALID_HANDLE_VALUE;
                    LPVOID m_mapped_memory = nullptr;
                    DWORD m_dwMaximumSizeHigh = 0;
                    DWORD m_dwMaximumSizeLow = 0;

                    std::unique_ptr<v00_header_t> m_header;

                    uint8_t* data() override;
                    std::size_t size() override;

                    v00_span_t(OPEN_MODE op, const v00_names_t&, const std::size_t buf_size);
                    ~v00_span_t();
                };
            };
        }
    }
}
