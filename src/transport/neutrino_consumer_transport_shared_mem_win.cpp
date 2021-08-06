#pragma once
#include <WinBase.h>
#include <stdio>
#include <cassert>
#include "neutrino_host_transport_shared_mem_win.hpp"

const DWORD expected_layout_version = 0;

namespace neutrino
{
namespace impl
{
namespace transport
{
namespace win_shared_mem
{

v00_names_t::v00_names_t(const std::string& domain, const std::string& suffix)
{
    m_shmm_name.reserve(domain.size() + 100);
    m_shmm_name.assign(domain).append("_shmm_").append(suffix);

    m_event_name.reserve(domain.size() + 100);
    m_event_name.assign(domain).append("_event_").append(suffix);

    m_sem_name.reserve(domain.size() + 100);
    m_sem_name.assign(domain).append("_sem_").append(suffix);
}

const v00_header_dwLayoutVersion = 0;

void v00_header_t::v00_header_t(OPEN_MODE op, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow)
{
    if(op == OPEN_MODE::CREATE)
    {
        m_header_size = sizeof(*this);
        m_dwLayoutVersion = v00_header_dwLayoutVersion;
        m_hostPID = GetCurrentProcessId();
        m_dwMaximumSizeHigh = dwMaximumSizeHigh;
        m_dwMaximumSizeLow = dwMaximumSizeLow;
    }
    else
    {
        if (pheader->m_header_size != sizeof(*pheader))
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" header_size mismatch"));
        }
        if (pheader->m_dwLayoutVersion != v00_header_dwLayoutVersion)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" dwLayoutVersion mismatch"));
        }
        if (pheader->m_dwMaximumSizeHigh != dwMaximumSizeHigh)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" dwMaximumSizeHigh mismatch"));
        }
        if (pheader->m_dwMaximumSizeLow != dwMaximumSizeLow)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" dwMaximumSizeLow mismatch"));
        }
    }
}

v00_sync_t::v00_sync_t(OPEN_MODE op, const v00_names_t& nm)
{
    if(op == OPEN_MODE::CREATE)
    {
        m_hevent = CreateEventA(
            SYNCHRONIZE | EVENT_MODIFY_STATE
            , true/*manual reset*/, false /*nonsignaled*/, nm.m_event_name.c_str());
        if (m_hevent == NULL)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" CreateEventA(").append(nm.m_event_name).append( ") GetLastError ").append(std::to_string(GetLastError())));
        }

        m_hsem = CreateSemaphoreA(
            SYNCHRONIZE | SEMAPHORE_MODIFY_STATE
            , 1/*initial count*/, 1/*max count*/, nm.m_sem_name.c_str());
        if (m_hsem == NULL)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" CreateSemaphore(").append(nm.m_sem_name).append(") GetLastError ").append(std::to_string(GetLastError())));
        }
    }
    else
    {
        ret->m_hevent = OpenEventA(
            SYNCHRONIZE | EVENT_MODIFY_STATE
            , FALSE /* can not inherit the handle */
            , nm.m_event_name.c_str()
        );
        if (ret->m_hevent == NULL)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" OpenEventA(").append(nm.m_event_name).append(") GetLastError ").append(std::to_string(GetLastError())));
        }

        ret->m_hsem = OpenSemaphoreA(
            SYNCHRONIZE | SEMAPHORE_MODIFY_STATE
            , FALSE /* can not inherit the handle */
            , nm.m_sem_name.c_str()
        );
        if (ret->m_hsem == NULL)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" OpenSemaphoreA(").append(nm.m_sem_name).append(") GetLastError ").append(std::to_string(GetLastError())));
        }
    }
}

v00_sync_t::~v00_sync_t()
{
    if (FALSE == CloseHandle(m_hevent))
    {
        std::cerr << __FUNCTION__ << " CloseHandle(m_hevent) GetLastError " << GetLastError();
    }
    if (FALSE == CloseHandle(m_hsem))
    {
        std::cerr << __FUNCTION__ << " CloseHandle(m_hsem) GetLastError " << GetLastError();
    }
}

void v00_sync_t::dirty() noexcept override
{
    assert(m_hevent != INVALID_HANDLE_VALUE);
    if (FALSE == SetEvent(m_hevent))
    {
        std::cerr << __FUNCTION__ << " SetEvent(m_event) GetLastError " << GetLastError();
    }
}

void v00_sync_t::clear() noexcept override
{
    assert(m_hevent != INVALID_HANDLE_VALUE);
    if (FALSE == ResetEvent(m_hevent))
    {
        std::cerr << __FUNCTION__ << " ResetEvent(m_event) GetLastError " << GetLastError();
    }
}

v00_span_t::v00_span_t(OPEN_MODE op, const v00_names_t& nm, const std::size_t buf_size)
{
    m_dwMaximumSizeHigh = 0;
    m_dwMaximumSizeLow = buf_size + sizeof(header_t);

    if(op == OPEN_MODE::CREATE)
    {
        m_hshmm = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE | SEC_COMMIT,
            ret->m_dwMaximumSizeHigh,
            ret->m_dwMaximumSizeLow,
            nm.m_shmm_name.c_str()
        );

        if (m_hshmm == NULL)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" CreateFileMappingA(").append(nm.m_shmm_name).append(") GetLastError ").append(std::to_string(GetLastError())));
        }
    }
    else
    {
        m_hshmm = OpenFileMappingA(
            FILE_MAP_ALL_ACCESS
            , FALSE /* can not inherit the handle */
            , nm.m_shmm_name.c_str()
        );

        if (m_hshmm == NULL)
        {
            std::cerr << __FUNCTION__ << " OpenFileMappingA(" << nm.m_shmm_name << ") GetLastError ERROR_ALREADY_EXISTS";
        }
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        std::cerr << __FUNCTION__ << " CreateFileMappingA GetLastError ERROR_ALREADY_EXISTS";
    }

    m_mapped_memory = MapViewOfFile(
        ret->m_hshmm
        , FILE_MAP_ALL_ACCESS
        , 0
        , 0
        buf_size
    );
    if (m_mapped_memory == NULL)
    {
        throw std::runtime_error(std::string(__FUNCTION__).append(" MapViewOfFile(").append(nm.m_shmm_name).append(") GetLastError ").append(std::to_string(GetLastError())));
    }

    // inplace create + format
    m_header = new (m_mapped_memory) v00_header_t(op, m_dwMaximumSizeHigh, m_dwMaximumSizeLow);
}

v00_span_t::~v00_span_t()
{
    if (FALSE == UnmapViewOfFile(m_mapped_memory))
    {
        std::cerr << __FUNCTION__ << " UnmapViewOfFile GetLastError " << GetLastError();
    }
    if (FALSE == CloseHandle(m_hshmm))
    {
        std::cerr << __FUNCTION__ << " CloseHandle(m_hshmm) GetLastError " << GetLastError();
    }
}

uint8_t* v00_span_t::data()
{
    assert(m_mapped_memory != NULL);
    return reinterpret_cast<uint8_t*>(m_mapped_memory) + m_header->size();
}

std::size_t v00_span_t::size()
{
    return m_dwMaximumSizeLow - m_header->size();
}

} // namespace win_shared_mem
} // transport
} // impl
} // neutrino
