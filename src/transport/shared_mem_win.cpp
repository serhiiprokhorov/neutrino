#pragma once

#include <cstdio>
#include <cassert>
#include <iostream>
#include <neutrino_transport_shared_mem_win.hpp>

const DWORD expected_layout_version = 0;

namespace neutrino
{
namespace impl
{
namespace memory
{
namespace win_shared_mem
{

v00_names_t::v00_names_t(std::string shmm_name, std::string event_name, std::string sem_name)
  : m_shmm_name(shmm_name), m_event_name(event_name), m_sem_name(sem_name)
{
}

v00_names_t::v00_names_t(unsigned long pid, const std::string& domain, const std::string& suffix)
  : v00_names_t(
      std::string(std::string::size_type(20 + domain.size() + suffix.size() + 7), '\x0').assign(std::to_string(pid)).append("_").append(domain).append("_shmm_").append(suffix)
      , std::string(std::string::size_type(20 + domain.size() + suffix.size() + 7), '\x0').assign(std::to_string(pid)).append("_").append(domain).append("_event_").append(suffix)
      , std::string(std::string::size_type(20 + domain.size() + suffix.size() + 6), '\x0').assign(std::to_string(pid)).append("_").append(domain).append("_sem_").append(suffix)
  )
{
}

const v00_names_t v00_names_t::with_suffix(const std::string& sf) const
{
  std::string shmm_name; shmm_name.reserve(shmm_name.size() + sf.size() + 10);
  std::string event_name; event_name.reserve(event_name.size() + sf.size() + 10);
  std::string sem_name; sem_name.reserve(sem_name.size() + sf.size() + 10);

  return v00_names_t(
    std::move(shmm_name.assign(m_shmm_name)).append("_").append(sf)
    , std::move(event_name.assign(m_event_name)).append("_").append(sf)
    , std::move(sem_name.assign(m_sem_name)).append("_").append(sf)
  );
}


const DWORD v00_header_dwLayoutVersion = 0;

v00_header_t::v00_header_t(OPEN_MODE op, DWORD dwMaximumSize)
{
    if(op == OPEN_MODE::CREATE)
    {
        m_header_size = sizeof(*this);
        m_dwLayoutVersion = v00_header_dwLayoutVersion;
        m_hostPID = GetCurrentProcessId();
        m_dwMaximumSize = dwMaximumSize;
        m_inuse_bytes = 0;
        m_inuse_diff_ns = 0;
    }
    else
    {
        if (m_header_size != sizeof(*this))
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" header_size mismatch"));
        }
        if (m_dwLayoutVersion != v00_header_dwLayoutVersion)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" dwLayoutVersion mismatch"));
        }
        if (m_dwMaximumSize != dwMaximumSize)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" dwMaximumSize mismatch"));
        }
    }
}

void v00_header_t::set_inuse(const LONG64 bytes_inuse, const LONG64 diff_started) noexcept
{
  InterlockedExchange64(&m_inuse_bytes, (LONG64)bytes_inuse);
  InterlockedExchange64(&m_inuse_diff_ns, (LONG64)diff_started);
}

void v00_header_t::set_free() noexcept
{
  InterlockedExchange64(&m_inuse_bytes, (LONG64)0);
  InterlockedExchange64(&m_inuse_diff_ns, (LONG64)0);
}

v00_sync_t::v00_sync_t(OPEN_MODE op, const v00_names_t& nm)
{
  if(op == OPEN_MODE::CREATE)
    {
        m_hevent = CreateEventA(
            NULL
            //SYNCHRONIZE | EVENT_MODIFY_STATE
            , true/*manual reset*/, false /*nonsignaled*/, nm.m_event_name.c_str());
        if (m_hevent == NULL)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" CreateEventA(").append(nm.m_event_name).append( ") GetLastError ").append(std::to_string(GetLastError())));
        }

        m_hsem = CreateSemaphoreA(
            NULL
            // SYNCHRONIZE | SEMAPHORE_MODIFY_STATE
            , 1/*initial count*/, 1/*max count*/, nm.m_sem_name.c_str());
        if (m_hsem == NULL)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" CreateSemaphore(").append(nm.m_sem_name).append(") GetLastError ").append(std::to_string(GetLastError())));
        }
    }
    else
    {
        m_hevent = OpenEventA(
            SYNCHRONIZE | EVENT_MODIFY_STATE
            , FALSE /* can not inherit the handle */
            , nm.m_event_name.c_str()
        );
        if (m_hevent == NULL)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" OpenEventA(").append(nm.m_event_name).append(") GetLastError ").append(std::to_string(GetLastError())));
        }

        m_hsem = OpenSemaphoreA(
            SYNCHRONIZE | SEMAPHORE_MODIFY_STATE
            , FALSE /* can not inherit the handle */
            , nm.m_sem_name.c_str()
        );
        if (m_hsem == NULL)
        {
            throw std::runtime_error(std::string(__FUNCTION__).append(" OpenSemaphoreA(").append(nm.m_sem_name).append(") GetLastError ").append(std::to_string(GetLastError())));
        }
    }
}

v00_sync_t::~v00_sync_t()
{
    if (m_hevent && FALSE == CloseHandle(m_hevent))
    {
        std::cerr << __FUNCTION__ << " CloseHandle(m_hevent) GetLastError " << GetLastError();
    }
    if (m_hsem && FALSE == CloseHandle(m_hsem))
    {
        std::cerr << __FUNCTION__ << " CloseHandle(m_hsem) GetLastError " << GetLastError();
    }
}

void v00_sync_t::dirty() noexcept
{
    assert(m_hevent != INVALID_HANDLE_VALUE);
    if (FALSE == SetEvent(m_hevent))
    {
        std::cerr << __FUNCTION__ << " SetEvent(m_event) GetLastError " << GetLastError();
    }
}

void v00_sync_t::clear() noexcept
{
    assert(m_hevent != INVALID_HANDLE_VALUE);
    if (FALSE == ResetEvent(m_hevent))
    {
        std::cerr << __FUNCTION__ << " ResetEvent(m_event) GetLastError " << GetLastError();
    }
}

const bool v00_sync_t::is_clean() const noexcept
{
  switch(WaitForSingleObject(m_hevent, 0))
  {
  case WAIT_ABANDONED:
    std::cerr << __FUNCTION__ << " WaitForSingleObject WAIT_ABANDONED GetLastError " << GetLastError();
    break;
  case WAIT_TIMEOUT:
    return true; // not signaled, the buffer is not ready to consume/data can be added into it
  }
  return false; // signaled, the buffer had been marked as dirty/ready to consume
}

v00_buffer_t::v00_buffer_t(OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size, v00_sync_t& sync, buffer_t* b, std::chrono::steady_clock::time_point started)
  : m_sync(sync)
  , m_data_size(buf_size)
  , m_data(nullptr)
  , buffer_t(b)
  , m_started(started)
{
    const DWORD shm_size = buf_size + sizeof(mapped_memory_layout_t);
    if(op == OPEN_MODE::CREATE)
    {
        m_hshmm = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE | SEC_COMMIT,
            0,
            shm_size,
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

    if(m_hshmm)
    {
      m_mapped_memory = MapViewOfFile(
        m_hshmm
        , FILE_MAP_ALL_ACCESS
        , 0
        , 0
        , shm_size
      );
    }
    if (m_mapped_memory == NULL)
    {
        throw std::runtime_error(std::string(__FUNCTION__).append(" MapViewOfFile(").append(nm.m_shmm_name).append(") GetLastError ").append(std::to_string(GetLastError())));
    }

    // inplace create
    m_data = new (m_mapped_memory) mapped_memory_layout_t(op, buf_size);
}

v00_buffer_t::~v00_buffer_t()
{
    if (FALSE == UnmapViewOfFile(m_mapped_memory))
    {
        std::cerr << __FUNCTION__ << " UnmapViewOfFile GetLastError " << GetLastError();
    }
    if (FALSE == CloseHandle(m_hshmm))
    {
        std::cerr << __FUNCTION__ << " CloseHandle(m_hshmm) GetLastError " << GetLastError();
    }
    m_data -> ~mapped_memory_layout_t();
}

v00_buffer_t::span_t v00_buffer_t::get_span(const uint64_t length) noexcept
{
  auto occupied = m_occupied.load();
  auto next_occupied = occupied + length;

  if (next_occupied > m_data_size || !m_occupied.compare_exchange_weak(occupied, next_occupied))
    return { nullptr, 0 };

  return { &(m_data->m_first_byte) + occupied, m_data_size - next_occupied, 0 };
}

v00_pool_t::v00_pool_t(std::size_t num_buffers, OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size)
  : m_started(std::chrono::steady_clock::now())
{
  m_buffer = nullptr;

  if(num_buffers > MAXIMUM_WAIT_OBJECTS)
    throw std::runtime_error(std::string().append(" num_buffers ").append(std::to_string(num_buffers)).append(" exceeds MAXIMUM_WAIT_OBJECTS ").append(std::to_string(MAXIMUM_WAIT_OBJECTS)));

  m_syncs.reserve(num_buffers);
  m_buffers.reserve(num_buffers);

  for(std::size_t i = 0; i < num_buffers; i++)
  {
    const auto indexed_nm = nm.with_suffix( std::to_string(i) );
    m_syncs.emplace_back(new v00_sync_t(op, indexed_nm));
    m_buffers.emplace_back(new v00_buffer_t(op, indexed_nm, buf_size, *(m_syncs.back()), m_buffer.load(), m_started));
    m_buffer = (m_buffers.back()).get();
  }
  m_buffers.front()->m_next = m_buffer;
}

v00_pool_t::~v00_pool_t()
{
}

v00_async_listener_t::v00_async_listener_t(std::shared_ptr<v00_pool_t> pool)
  : m_pool(pool)
{
  m_stop_event = CreateEventA(NULL, false/*auto reset*/, false /*nonsignaled*/, NULL);
  if (m_stop_event == NULL)
  {
    throw std::runtime_error(std::string(__FUNCTION__).append(" CreateEventA m_stop_event GetLastError ").append(std::to_string(GetLastError())));
  }

  m_sync_handles.reserve(m_pool->m_syncs.size() + 1);

  m_sync_handles.push_back(m_stop_event);

  for (const auto& s : m_pool->m_syncs)
  {
    m_sync_handles.push_back(s->m_hevent);
  }

  m_ordered_consumption_sequence.reserve(m_pool->m_buffers.size());
}

v00_async_listener_t::~v00_async_listener_t()
{
  CloseHandle(m_stop_event);
}

std::function<void()> v00_async_listener_t::start(std::function <void(const uint8_t* p, const uint8_t* e)> consume_one)
{
  const auto runner_f = [this, consume_one]()
  {
    while (true)
    {
      DWORD ret = ::WaitForMultipleObjectsEx(
        m_sync_handles.size(),
        &(m_sync_handles[0]),
        false /*bWaitAll*/,
        INFINITE,
        false /*bAlertable*/
      );

      if (ret == WAIT_FAILED)
        throw std::runtime_error(std::string(__FUNCTION__).append(" WaitForMultipleObjectsEx WAIT_FAILED GetLastError ").append(std::to_string(GetLastError())));

      if (ret == WAIT_TIMEOUT)
        continue; // TODO: prevent quick loop

      /*WAIT_OBJECT_0 .. WAIT_OBJECT_0 + m_sync_handles.size() - 1  */
      const DWORD WAIT_OBJECT_RANGE = WAIT_OBJECT_0 + m_sync_handles.size();
      if (ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_RANGE)
      {
        if (ret == WAIT_OBJECT_0)
          return true;

        m_ordered_consumption_sequence.resize(0);

        for (DWORD first_idx = ret - WAIT_OBJECT_0 - 1/*first event is "full stop" signaling event*/; 
          first_idx < m_pool->m_buffers.size();
          first_idx++)
        {
          auto& b = m_pool->m_buffers[first_idx];
          if (b->is_clean())
            continue;

          m_ordered_consumption_sequence.emplace_back(std::size_t(first_idx), b->get_data());
        }

        std::stable_sort(m_ordered_consumption_sequence.begin(), m_ordered_consumption_sequence.end(),
          [](const auto & x1, const auto & x2)
          {
            return x1.second.sequence < x2.second.sequence;
          });

        for(const auto& x : m_ordered_consumption_sequence)
        {
          consume_one(x.second.m_span, x.second.m_span + x.second.free_bytes);
          m_pool->m_buffers[x.first]->clear();
        }

        continue;
      }

      /*WAIT_ABANDONED_0 .. (WAIT_ABANDONED_0 + nCount– 1)*/
      // exit if any of events have been abandoned
      break;
    }
    return false;
  };

  std::shared_ptr<std::thread> x{ new std::thread(runner_f) };

  return [this, x]()
  {
    if (FALSE == SetEvent(m_stop_event))
    {
      std::cerr << __FUNCTION__ << " SetEvent(m_stop_event) GetLastError " << GetLastError();
    }
    x->join();
  };
}

} // namespace win_shared_mem
} // transport
} // impl
} // neutrino
