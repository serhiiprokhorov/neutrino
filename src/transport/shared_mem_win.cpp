#pragma once

#include <cstdio>
#include <cassert>
#include <iostream>
#include <array>
#include <algorithm>
#include <neutrino_transport_shared_mem_win.hpp>

const DWORD expected_layout_version = 0;

namespace
{
  uint64_t cc_buf_reported_clean{ 0 };
  uint64_t cc_buf_reported_dirty{ 0 };
  uint64_t cc_buf_reported_inuse{ 0 };
  uint64_t cc_buf_reported_setfree{ 0 };
  uint64_t cc_buf_is_clean_true{ 0 };
  uint64_t cc_buf_is_clean_false{ 0 };
  uint64_t cc_consumer_wait_ret{ 0 };
  uint64_t cc_consumer_wait_timeout{ 0 };
  uint64_t cc_consumer_buf_outdated_clear{ 0 };
  uint64_t cc_consumer_buf_ready{ 0 };
  uint64_t cc_consumer_buf_consumed{ 0 };
  uint64_t cc_consumer_buf_clear{ 0 };
  uint64_t cc_consumer_continue{ 0 };
  uint64_t cc_consumer_buf_missed{ 0 };

  uint64_t cc_consumer_buf_event_new{ 0 };
  uint64_t cc_consumer_buf_event_repeated{ 0 };
  uint64_t cc_consumer_buf_event_outdated{ 0 };

  std::array<uint64_t, 10000> cc_ready_data;
}

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
        m_sequence = 1; // sequence 0 is reserved, consumer uses it as "span-is-free" indicator
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
  InterlockedExchange64(&m_sequence, (LONG64)diff_started);
  cc_buf_reported_inuse++;
}

void v00_header_t::set_free() noexcept
{
  InterlockedExchange64(&m_inuse_bytes, (LONG64)0);
  InterlockedExchange64(&m_sequence, (LONG64)0);
  cc_buf_reported_setfree++;
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
    cc_buf_reported_dirty++;
}

void v00_sync_t::clear() noexcept
{
    assert(m_hevent != INVALID_HANDLE_VALUE);
    if (FALSE == ResetEvent(m_hevent))
    {
        std::cerr << __FUNCTION__ << " ResetEvent(m_event) GetLastError " << GetLastError();
    }
    cc_buf_reported_clean++;
}

const bool v00_sync_t::is_clean() const noexcept
{
  switch(WaitForSingleObject(m_hevent, 0))
  {
  case WAIT_ABANDONED:
    std::cerr << __FUNCTION__ << " WaitForSingleObject WAIT_ABANDONED GetLastError " << GetLastError();
    break;
  case WAIT_TIMEOUT:
    cc_buf_is_clean_true++;
    return true; // not signaled, the buffer is not ready to consume/data can be added into it
  }
  cc_buf_is_clean_false++;
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
            std::cerr << __FUNCTION__ << " OpenFileMappingA(" << nm.m_shmm_name << ") GetLastError ERROR_ALREADY_EXISTS" << std::endl;
        }
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        std::cerr << __FUNCTION__ << " CreateFileMappingA GetLastError ERROR_ALREADY_EXISTS" << std::endl;
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
        std::cerr << __FUNCTION__ << " UnmapViewOfFile GetLastError " << GetLastError() << std::endl;
    }
    if (FALSE == CloseHandle(m_hshmm))
    {
        std::cerr << __FUNCTION__ << " CloseHandle(m_hshmm) GetLastError " << GetLastError() << std::endl;
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
  // defaults
  m_params = std::make_shared<parameters_t>();
  m_params->m_ready_data_size = 10000;

  //m_processed.reserve(180000000);
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

}

v00_async_listener_t::~v00_async_listener_t()
{
  CloseHandle(m_stop_event);
}

template <typename X>
inline void print_mask(const char label, X next_sequence, X x) {
    char buf[200];
    char* p = buf + snprintf(buf, sizeof(buf) / sizeof(buf[0]), "m%c %lld ", label, next_sequence);
    while (x) {
      *(p++) = x & 1 ? '1' : '0';
      x >>= 1;
    }
    *(p++) = '\n';
    *p = '\x0';
    std::cerr << buf;
}

std::function<void()> v00_async_listener_t::start(std::function <void(const uint64_t sequence, const uint8_t* p, const uint8_t* e)> consume_one)
{
  std::fill(cc_ready_data.begin(), cc_ready_data.end(), 0);
  const auto runner_f = [this, consume_one]()
  {
    uint64_t write_ahead_queue_len = 0;
    uint64_t read_data_offset = 0;
    uint64_t next_sequence = 1;

    const auto buffers_size = m_pool->m_buffers.size();

    std::vector<std::pair<std::size_t, shared_memory::buffer_t::span_t>> write_ahead_buffer;
    write_ahead_buffer.resize(m_pool->m_syncs.size() * m_params->m_ready_data_size);
    const auto write_ahead_buffer_size = write_ahead_buffer.size();

    bool stop_requested = false;
    //DWORD wait_timeout = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(30)).count());
    DWORD wait_timeout = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1)).count());
    //DWORD wait_timeout = static_cast<DWORD>(std::chrono::milliseconds(1).count());

    uint64_t sorting_buffer_mask = 0; // occupied buffers
    if(m_pool->m_buffers.size() > (sizeof(sorting_buffer_mask) * 8))
      throw std::runtime_error(std::string(__FUNCTION__).append(" too many buffers"));

    // this array holds "ready to consume" buffers in the order of their's sequences
    // due to async nature of events, "buffer is ready" events occures in random order and with X > Y, sequence X buffer may be signaled later than sequence Y buffer
    // to restore natural order of buffers, sorting_buffer holds "ready to consume" buffers at positions which are related to the very last consumed sequence:
    // when last consumed sequence is S and there are pending "buffer is ready" events, each referring sequences X, Y, Z 
    // then sorting_buffer[X-S] will hold X-buffer data
    // then sorting_buffer[Y-S] will hold Y-buffer data
    // then sorting_buffer[Z-S] will hold Z-buffer data
    // Since sequences are monitonic, then sorting_buffer is only appended.
    // sorting_buffer_mask marks cells which are occupied.
    // Every time there are "ready to consume" buffers, the logic below calcs the offset of a cell idx=X-S, where X is a buffer's sequence and S is a last consumed sequence.
    // Then the data is copied into a corresponding cell and the sorting_buffer_mask is updated.
    // 0-indexed cell always contains a data which can be consumed because its sequence is exact next sequence.
    // At the end of a main loop, if sorting_buffer_mask LSB is marked then 0-indexed data is processed, the whole array is rotated and sorting_buffer_mask is shifted.
    // 
    // For performance reasons, it only could be 64 items in 
    std::vector<std::vector<uint8_t>> sorting_buffer;
     uint8_t dummy = 0;

    {
      // get the max size of each indiv buffer
      // they should be equal-sized but just in case
      std::size_t max_size = 0;
      for (const auto& bb : m_pool->m_buffers)
        if (max_size < bb->m_data_size)
          max_size = bb->m_data_size;

      const auto sorting_buffer_sz = m_pool->m_buffers.size() * 2;
      sorting_buffer.reserve(sorting_buffer_sz);
      for (std::size_t xx = 0; xx < sorting_buffer_sz; xx++)
      {
        sorting_buffer.emplace_back();
        sorting_buffer.back().reserve(max_size);
      }
    }

    std::string problem; problem.reserve(1000);
    while (true)
    {
      // validate no unexpected states
      {
        const uint64_t diff_clean_dirty_mismatch = 1000;
        const uint64_t diff_ahead = 1000;
        if(cc_buf_reported_dirty > cc_buf_reported_clean && (cc_buf_reported_dirty - cc_buf_reported_clean) > diff_clean_dirty_mismatch)
        {
          problem.append("deadlock detected by cc_buf_reported_dirty-cc_buf_reported_clean;");
        }
        if (cc_buf_reported_inuse > cc_buf_reported_setfree && (cc_buf_reported_inuse - cc_buf_reported_setfree) > diff_clean_dirty_mismatch)
        {
          problem.append("deadlock detected by cc_buf_reported_inuse-cc_buf_reported_setfree;");
        }
        if(!problem.empty())
        {
          //throw std::runtime_error(problem);
          std::cerr << problem << std::endl;
        }
      }

      DWORD ret = ::WaitForMultipleObjectsEx(
        DWORD(m_sync_handles.size()),
        &(m_sync_handles[0]),
        false /*bWaitAll*/,
        wait_timeout,
        false /*bAlertable*/
      );

      cc_consumer_wait_ret++;

      if (ret == WAIT_FAILED)
        throw std::runtime_error(std::string(__FUNCTION__).append(" WaitForMultipleObjectsEx WAIT_FAILED GetLastError ").append(std::to_string(GetLastError())));

      if (ret == WAIT_TIMEOUT)
      {
        if(stop_requested) // stop was requested and no more events are pending
          break;

        cc_consumer_wait_timeout++;
        consume_one(next_sequence, nullptr, nullptr);
        continue; // TODO: prevent quick loop
      }

      /*WAIT_OBJECT_0 .. WAIT_OBJECT_0 + m_sync_handles.size() - 1  */
      const DWORD WAIT_OBJECT_RANGE = WAIT_OBJECT_0 + m_sync_handles.size();
      if (ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_RANGE)
      {
        if (ret == WAIT_OBJECT_0)
        {
          stop_requested = true;
          wait_timeout = 0; // wait no more, process any events pending and exit
        }

        std::size_t idx = 0;
        for (DWORD first_idx = ret - WAIT_OBJECT_0 - 1/*first event is "full stop" signaling event*/;
          first_idx < m_pool->m_buffers.size();
          first_idx++)
        {
          auto& b = m_pool->m_buffers[first_idx];
          if (b->is_clean())
            continue;

          const auto s = b->get_data();
          if(next_sequence > s.sequence)
          {
            // this buffer is expired because next expected sequence is greater than buffer's sequence
            cc_consumer_buf_outdated_clear++;
          }
          else
          {
            idx = s.sequence - next_sequence;

            char buf[200];
            sprintf(buf, "idx %lld\n", idx);
            std::cerr << buf;

            if(sizeof(cc_ready_data) / sizeof(cc_ready_data[0]) > idx)
              cc_ready_data[idx]++;

            if (idx >= sorting_buffer.size()) {
              // this buffer is too far away and can not be consumed
              // TODO: handle dropped
              cc_consumer_buf_outdated_clear++;
            }
            else {
              cc_consumer_buf_ready++;

              sorting_buffer[idx].resize(s.free_bytes);
              if(s.free_bytes)
                memmove(&(sorting_buffer[idx][0]), s.m_span, s.free_bytes);
              sorting_buffer_mask |= (uint64_t(1) << idx);
            }
          }
          b->clear();
          cc_consumer_buf_clear++;
        }

        print_mask('0', next_sequence, sorting_buffer_mask);

        while (sorting_buffer_mask & 1) {
          cc_consumer_buf_consumed++;

          sorting_buffer_mask >>= 1;

          const auto sz = sorting_buffer.front().size();
          const auto* p = sz ? &(sorting_buffer.front().front()) : &dummy;
          consume_one(next_sequence, p, p + sz);

          ++next_sequence;
          // rotate the array
          for (std::size_t ii = 1; ii < sorting_buffer.size(); ii++) {
            sorting_buffer[ii - 1].swap(sorting_buffer[ii]);
          }
        }

        print_mask('1', next_sequence, sorting_buffer_mask);

        cc_consumer_continue++;
        continue; 
      } // if (ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_RANGE)

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
      std::cerr << __FUNCTION__ << " SetEvent(m_stop_event) GetLastError " << GetLastError() << std::endl;
    }
    x->join();
  };
}

} // namespace win_shared_mem
} // transport
} // impl
} // neutrino
