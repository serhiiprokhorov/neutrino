#include <cstdio>
#include <cassert>
#include <iostream>
#include <array>
#include <algorithm>
#include <list>
#include <map>
#include <vector>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <transport/shared_mem/neutrino_transport_shared_mem_linux.hpp>

namespace neutrino::transport::shared_memory::platform {

std::unique_ptr<buffer_t> v00_formatter_t::create_ring(initializer_memfd_t& memory, std::size_t cc_buffers) 
{
  const std::size_t header_sz = sizeof(v00_header_t);
  const std::size_t buffer_size = memory.m_buffer_bytes / cc_buffers;

  if(buffer_size <= header_sz) {
    register_error(0, "v00_formatter.create_ring unable to markup the requested number of buffers");
    return nullptr;
  }

  void* start = memory.m_rptr;

  std::vector<buffer_t> ret;
  ret.reserve(cc_buffers);

  buffer_t * prev_buffer_ptr = nullptr;
  while(true) {
    ret.emplace_back();
    auto& buffer = ret.back();
    if(prev_buffer_ptr) {
      prev_buffer_ptr->m_next = &buffer;
    }
    prev_buffer_ptr = &buffer;

    buffer.m_handle = reinterpret_cast<v00_header_t*>(start);
    v00_header->m_header_size = sizeof(v00_header_t);
    v00_header->m_maximumSize = buffer_size; // total bytes , including header + events

    v00_header->m_inuse_bytes = 0;
    v00_header->m_sequence = 0;

    start += buffer_size;
  }

  v00_header_t* v00_header = reinterpret_cast<v00_header_t*>(memory.m_rptr);
}

initializer_memfd_t::~initializer_memfd_t()
{
  if(m_fd != -1) {
    close(m_fd);
  }
}

initializer_memfd_t::initializer_memfd_t(std::size_t buffer_bytes, )
{
  std::unique_ptr<initializer_memfd_t> ret(new initializer_memfd_t()); 

  auto total_length = cc_buffers * (buffer_bytes + sizeof(control_t));
  if(total_length <= sizeof(control_t)) {
    register_error(0, "create_handle.total_length invalid");
    return ret;
  }

  // create a tmp file which resides in RAM and is not connected to fs (anonymous file)
  // represented by fd
  // use fd to mmap this file as a shared memory
  // share fd with a child process to get access to the same memory

  // MFD_ALLOW_SEALING will be used later to seal the size
  ret->m_fd = memfd_create("initializer_memfd_t::buffer_t", MFD_ALLOW_SEALING);
  if (ret->m_fd == -1) {
    register_error(errno, "create_handle.memfd_create");
    return ret;
  }


  if (ftruncate(ret->m_fd, total_length) == -1) {
    register_error(errno, "create_handle.ftruncate");
    return ret;
  }

  if (fcntl(ret->m_fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) == -1) {
    register_error(errno, "create_handle.fcntl seals");
    return ret;
  }


  /* Map shared memory object */
  ret->m_rptr = mmap(NULL, param.m_sz, PROT_READ | PROT_WRITE, MAP_SHARED, ret->m_fd, 0);
  if (ret->m_rptr == MAP_FAILED) {
    register_error(errno, "create_handle.ftruncate");
    return ret;
  }
  
  return ret;
}

std::unique_ptr<initializer_memfd_t> initializer_memfd_t::open(unsigned int fd)
{

}

enum class OPEN_MODE
{
    CREATE
    , OPEN
};

buffer_t::name_t::name_t(
  const char* buffer_prefix, 
  const uint64_t consumer_app_id, 
  const char* buffer_suffix, 
  const VERSIONS ver, 
  std::function<void (int, const char*)> register_error
  ) {

  m_ver = VERSIONS::unknown;
  const auto fn_sprintf = [&] (char* buf, std::size_t len) { 
    return snprintf(buf, len, "%s_%lli_%lli", buffer_prefix, consumer_app_id, buffer_suffix); 
  };

  const auto reserve = fn_sprintf(nullptr, 0);
  if( reserve < 0 ) {
    register_error(reserve, "buffer_t::create_name reserve error");
  } else {
    // term zero is always written
    m_nm.resize(reserve + 1);

    if( const auto format_res = fn_sprintf(m_nm.data(), m_nm.size() - 1) < 0) {
      register_error(format_res, "buffer_t::create_name format error");
      m_nm.clear();
    } else {
      m_ver = ver;
    };
  };
}

buffer_t::handle_t* create_handle(const buffer_t::name_t& bn, const buffer_t::parameters_t& param, std::function<void (int, const char*)> register_error) {
  
  struct {
    std::function<void(void*)> fn_init;
    buffer_t::handle_t handle;
  } handle_init;

  switch(bn.m_ver) {
    case VERSIONS::v00:
      handle_init.fn_init = [sz = param.m_sz](void* rptr) {
        v00_header_t* v00_header = reinterpret_cast<v00_header_t*>(rptr);
        v00_header->m_header_size = sizeof(v00_header_t);
        v00_header->m_maximumSize = sz; // total bytes , including header + events

          alignas(alignof(uint64_t)) uint64_t m_inuse_bytes;
          alignas(alignof(uint64_t)) uint64_t m_sequence;
      };
    default:
      register_error(errno, "create_handle.handle.version");
      return nullptr;
  }; 


  struct rollback_handler_t {
    std::map<std::string, std::function<void(const std::string&)>> destroy_handlers;
    std::list<std::string> destroy_sequence;

    void set(std::string nm, std::function<void(const std::string&)> to_do) {
      destroy_handlers[nm] = to_do; 
      destroy_sequence.emplace_back(nm);
    }

    ~rollback_handler_t() {
      for(const auto& nm : destroy_sequence) {
        destroy_handlers[nm](nm);
      }
    }

  } rollback_handler;


  // create a tmp file which resides in RAM and is not connected to fs (anonymous file)
  // represented by fd
  // use fd to mmap this file as a shared memory
  // share fd with a child process to get access to the same memory  

  // MFD_ALLOW_SEALING will be used later to seal the size
  auto fd = memfd_create(bn.m_nm.c_str(), MFD_ALLOW_SEALING);
  if (fd == -1) {
    register_error(errno, "create_handle.memfd_create");
    return nullptr;
  }

  rollback_handler.set("close memfd_create fd", [fd, register_error] (const std::string& nm) {
    if( close(fd) != 0) {
      register_error(errno, std::string("create_handle.{}").append(nm).append("}.close").c_str());
    }
  });

  if (ftruncate(fd, param.m_sz) == -1) {
    register_error(errno, "create_handle.ftruncate");
    return nullptr;
  }

  if (fcntl(fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) == -1) {
    register_error(errno, "create_handle.fcntl seals");
    return nullptr;
  }


  /* Map shared memory object */
  void* rptr = mmap(NULL, param.m_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (rptr == MAP_FAILED) {
    register_error(errno, "create_handle.ftruncate");
    return nullptr;
  }

  rollback_handler.set("munmap memfd_create fd", [rptr, sz = param.m_sz, register_error](const std::string& nm) { 
    if (munmap(rptr, sz) != 0) {
      register_error(errno, std::string("create_handle.{}").append(nm).append("}.munmap").c_str());
    } 
  });

  handle_init.fn_init(rptr);

  switch(bn.m_ver) {
    case VERSIONS::v00:
    {
      handle_init.handle.m_destroy = [rptr, fd, sz = param.m_sz]() {
        flcose(fd);
        munmap(rptr, sz);
      };

      v00_header_t* v00_header = reinterpret_cast<v00_header_t*>(rptr);
      handle_init.handle.m_is_clean = [v00_header]() {
        return v00_header->m_inuse_bytes == 0;
      };

      handle_init.handle.m_clear = [v00_header]() {
        v00_header->m_inuse_bytes = 0;
      };

      handle_init.handle.m_dirty = [v00_header](const uint64_t dirty_buffer_counter) {
        v00_header->m_sequence = dirty_buffer_counter;
      };

      buffer_t::span_t span {
        .first_available = reinterpret_cast<uint8_t*>(rptr) + sizeof(v00_header_t), /// first available byte in the region
        .hi_water_mark = reinterpret_cast<uint8_t*>(rptr) + param.m_sz - param.m_biggest_event
      };

      handle_init.handle.m_available = [span]() {
        return span;
      };
    }
    break;
    default:
      register_error(errno, "create_handle.handle.version");
      return nullptr;
  }; 

}

void destroy_handle(const buffer_t::handle_t* handle) {
  // 
}

buffer_t::handle_t* open_handle(const buffer_t::parameters_t& param) {
  // open existing shared memory by name
//     }
//     else
//     {
//         if (m_header_size != sizeof(*this))
//         {
//             throw std::runtime_error(std::string(__FUNCTION__).append(" header_size mismatch"));
//         }
//         if (m_dwLayoutVersion != v00_header_dwLayoutVersion)
//         {
//             throw std::runtime_error(std::string(__FUNCTION__).append(" dwLayoutVersion mismatch"));
//         }
//         if (m_dwMaximumSize != dwMaximumSize)
//         {
//             throw std::runtime_error(std::string(__FUNCTION__).append(" dwMaximumSize mismatch"));
//         }
//     }
}

void close_handle(const buffer_t::handle_t* handle);

const bool is_clean(const buffer_t::handle_t* handle) noexcept;
void dirty(const uint64_t dirty_buffer_counter, const buffer_t::handle_t* handle) noexcept;
void clear(const buffer_t::handle_t* handle) noexcept;

const buffer_t::span_t available(const buffer_t::handle_t* handle) noexcept;
}



// const DWORD expected_layout_version = 0;

// namespace
// {
//   uint64_t cc_buf_reported_clean{ 0 };
//   uint64_t cc_buf_reported_dirty{ 0 };
//   uint64_t cc_buf_reported_inuse{ 0 };
//   uint64_t cc_buf_reported_setfree{ 0 };
//   uint64_t cc_buf_is_clean_true{ 0 };
//   uint64_t cc_buf_is_clean_false{ 0 };
//   uint64_t cc_consumer_wait_ret{ 0 };
//   uint64_t cc_consumer_wait_timeout{ 0 };
//   uint64_t cc_consumer_buf_outdated_clear{ 0 };
//   uint64_t cc_consumer_buf_ready{ 0 };
//   uint64_t cc_consumer_buf_consumed{ 0 };
//   uint64_t cc_consumer_buf_clear{ 0 };
//   uint64_t cc_consumer_continue{ 0 };
//   uint64_t cc_consumer_buf_missed{ 0 };

//   uint64_t cc_consumer_buf_event_new{ 0 };
//   uint64_t cc_consumer_buf_event_repeated{ 0 };
//   uint64_t cc_consumer_buf_event_outdated{ 0 };

//   std::array<uint64_t, 10000> cc_ready_data;
// }

// namespace neutrino
// {
// namespace impl
// {
// namespace memory
// {
// namespace linux_shared_mem
// {

// void print_stats(std::ostream& out) {
//   out << "neutrino::impl::memory::win_shared_mem stats" << std::endl
//     << "\tcc_buf_reported_clean=" << cc_buf_reported_clean << std::endl
//     << "\tcc_buf_reported_dirty=" << cc_buf_reported_dirty << std::endl
//     << "\tcc_buf_reported_inuse=" << cc_buf_reported_inuse << std::endl
//     << "\tcc_buf_reported_setfree=" << cc_buf_reported_setfree << std::endl
//     << "\tcc_buf_is_clean_true=" << cc_buf_is_clean_true << std::endl
//     << "\tcc_buf_is_clean_false=" << cc_buf_is_clean_false << std::endl
//     << "\tcc_consumer_wait_ret=" << cc_consumer_wait_ret << std::endl
//     << "\tcc_consumer_wait_timeout=" << cc_consumer_wait_timeout << std::endl
//     << "\tcc_consumer_buf_outdated_clear=" << cc_consumer_buf_outdated_clear << std::endl
//     << "\tcc_consumer_buf_ready=" << cc_consumer_buf_ready << std::endl
//     << "\tcc_consumer_buf_consumed=" << cc_consumer_buf_consumed << std::endl
//     << "\tcc_consumer_buf_clear=" << cc_consumer_buf_clear << std::endl
//     << "\tcc_consumer_continue=" << cc_consumer_continue << std::endl
//     << "\tcc_consumer_buf_missed=" << cc_consumer_buf_missed << std::endl
//     << "\tcc_consumer_buf_event_new=" << cc_consumer_buf_event_new << std::endl
//     << "\tcc_consumer_buf_event_repeated=" << cc_consumer_buf_event_repeated << std::endl
//     << "\tcc_consumer_buf_event_outdated=" << cc_consumer_buf_event_outdated << std::endl
//     ;

//   std::size_t max_idx = cc_ready_data.size() - 1;
//   for (; max_idx >= 0; max_idx--) {
//     if (cc_ready_data[max_idx])
//       break;
//   }

//   out << "\tcc_ready_data" << std::endl;
//   for (std::size_t idx = 0; idx < max_idx; idx++) {
//     out << "\t" << idx << ":" << cc_ready_data[idx] << std::endl;
//   }
// }

// v00_names_t::v00_names_t(std::string shmm_name, std::string event_name, std::string sem_name)
//   : m_shmm_name(shmm_name), m_event_name(event_name), m_sem_name(sem_name)
// {
// }

// v00_names_t::v00_names_t(unsigned long pid, const std::string& domain, const std::string& suffix)
//   : v00_names_t(
//       std::string(std::string::size_type(20 + domain.size() + suffix.size() + 7), '\x0').assign(std::to_string(pid)).append("_").append(domain).append("_shmm_").append(suffix)
//       , std::string(std::string::size_type(20 + domain.size() + suffix.size() + 7), '\x0').assign(std::to_string(pid)).append("_").append(domain).append("_event_").append(suffix)
//       , std::string(std::string::size_type(20 + domain.size() + suffix.size() + 6), '\x0').assign(std::to_string(pid)).append("_").append(domain).append("_sem_").append(suffix)
//   )
// {
// }

// const v00_names_t v00_names_t::with_suffix(const std::string& sf) const
// {
//   std::string shmm_name; shmm_name.reserve(shmm_name.size() + sf.size() + 10);
//   std::string event_name; event_name.reserve(event_name.size() + sf.size() + 10);
//   std::string sem_name; sem_name.reserve(sem_name.size() + sf.size() + 10);

//   return v00_names_t(
//     std::move(shmm_name.assign(m_shmm_name)).append("_").append(sf)
//     , std::move(event_name.assign(m_event_name)).append("_").append(sf)
//     , std::move(sem_name.assign(m_sem_name)).append("_").append(sf)
//   );
// }


// const DWORD v00_header_dwLayoutVersion = 0;

// v00_header_t::v00_header_t(OPEN_MODE op, DWORD dwMaximumSize)
// {
//     if(op == OPEN_MODE::CREATE)
//     {
//         m_header_size = sizeof(*this);
//         m_dwLayoutVersion = v00_header_dwLayoutVersion;
//         m_hostPID = GetCurrentProcessId();
//         m_dwMaximumSize = dwMaximumSize;
//         m_inuse_bytes = 0;
//         m_sequence = 1; // sequence 0 is reserved, consumer uses it as "span-is-free" indicator
//     }
//     else
//     {
//         if (m_header_size != sizeof(*this))
//         {
//             throw std::runtime_error(std::string(__FUNCTION__).append(" header_size mismatch"));
//         }
//         if (m_dwLayoutVersion != v00_header_dwLayoutVersion)
//         {
//             throw std::runtime_error(std::string(__FUNCTION__).append(" dwLayoutVersion mismatch"));
//         }
//         if (m_dwMaximumSize != dwMaximumSize)
//         {
//             throw std::runtime_error(std::string(__FUNCTION__).append(" dwMaximumSize mismatch"));
//         }
//     }
// }

// void v00_header_t::set_inuse(const LONG64 bytes_inuse, const LONG64 diff_started) noexcept
// {
//   InterlockedExchange64(&m_inuse_bytes, (LONG64)bytes_inuse);
//   InterlockedExchange64(&m_sequence, (LONG64)diff_started);
//   cc_buf_reported_inuse++;
// }

// void v00_header_t::set_free() noexcept
// {
//   InterlockedExchange64(&m_inuse_bytes, (LONG64)0);
//   InterlockedExchange64(&m_sequence, (LONG64)0);
//   cc_buf_reported_setfree++;
// }

// v00_sync_t::v00_sync_t(OPEN_MODE op, const v00_names_t& nm)
// {
//   if(op == OPEN_MODE::CREATE)
//     {
//         m_hevent = CreateEventA(
//             NULL
//             //SYNCHRONIZE | EVENT_MODIFY_STATE
//             , true/*manual reset*/, false /*nonsignaled*/, nm.m_event_name.c_str());
//         if (m_hevent == NULL)
//         {
//             throw std::runtime_error(std::string(__FUNCTION__).append(" CreateEventA(").append(nm.m_event_name).append( ") GetLastError ").append(std::to_string(GetLastError())));
//         }

//         m_hsem = CreateSemaphoreA(
//             NULL
//             // SYNCHRONIZE | SEMAPHORE_MODIFY_STATE
//             , 1/*initial count*/, 1/*max count*/, nm.m_sem_name.c_str());
//         if (m_hsem == NULL)
//         {
//             throw std::runtime_error(std::string(__FUNCTION__).append(" CreateSemaphore(").append(nm.m_sem_name).append(") GetLastError ").append(std::to_string(GetLastError())));
//         }
//     }
//     else
//     {
//         m_hevent = OpenEventA(
//             SYNCHRONIZE | EVENT_MODIFY_STATE
//             , FALSE /* can not inherit the handle */
//             , nm.m_event_name.c_str()
//         );
//         if (m_hevent == NULL)
//         {
//             throw std::runtime_error(std::string(__FUNCTION__).append(" OpenEventA(").append(nm.m_event_name).append(") GetLastError ").append(std::to_string(GetLastError())));
//         }

//         m_hsem = OpenSemaphoreA(
//             SYNCHRONIZE | SEMAPHORE_MODIFY_STATE
//             , FALSE /* can not inherit the handle */
//             , nm.m_sem_name.c_str()
//         );
//         if (m_hsem == NULL)
//         {
//             throw std::runtime_error(std::string(__FUNCTION__).append(" OpenSemaphoreA(").append(nm.m_sem_name).append(") GetLastError ").append(std::to_string(GetLastError())));
//         }
//     }
// }

// v00_sync_t::~v00_sync_t()
// {
//     if (m_hevent && FALSE == CloseHandle(m_hevent))
//     {
//         std::cerr << __FUNCTION__ << " CloseHandle(m_hevent) GetLastError " << GetLastError();
//     }
//     if (m_hsem && FALSE == CloseHandle(m_hsem))
//     {
//         std::cerr << __FUNCTION__ << " CloseHandle(m_hsem) GetLastError " << GetLastError();
//     }
// }

// void v00_sync_t::dirty() noexcept
// {
//     assert(m_hevent != INVALID_HANDLE_VALUE);
//     if (FALSE == SetEvent(m_hevent))
//     {
//         std::cerr << __FUNCTION__ << " SetEvent(m_event) GetLastError " << GetLastError();
//     }
//     cc_buf_reported_dirty++;
// }

// void v00_sync_t::clear() noexcept
// {
//     assert(m_hevent != INVALID_HANDLE_VALUE);
//     if (FALSE == ResetEvent(m_hevent))
//     {
//         std::cerr << __FUNCTION__ << " ResetEvent(m_event) GetLastError " << GetLastError();
//     }
//     cc_buf_reported_clean++;
// }

// const bool v00_sync_t::is_clean() const noexcept
// {
//   switch(WaitForSingleObject(m_hevent, 0))
//   {
//   case WAIT_ABANDONED:
//     std::cerr << __FUNCTION__ << " WaitForSingleObject WAIT_ABANDONED GetLastError " << GetLastError();
//     break;
//   case WAIT_TIMEOUT:
//     cc_buf_is_clean_true++;
//     return true; // not signaled, the buffer is not ready to consume/data can be added into it
//   }
//   cc_buf_is_clean_false++;
//   return false; // signaled, the buffer had been marked as dirty/ready to consume
// }

// v00_base_buffer_t::v00_base_buffer_t(OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size, v00_sync_t& sync, buffer_t* b, std::chrono::steady_clock::time_point started)
//   : m_sync(sync)
//   , m_data_size(buf_size)
//   , m_data(nullptr)
//   , buffer_t(b)
//   , m_started(started)
// {
//   const DWORD shm_size = buf_size + sizeof(mapped_memory_layout_t);
//   if (op == OPEN_MODE::CREATE)
//   {
//     m_hshmm = CreateFileMappingA(
//       INVALID_HANDLE_VALUE,
//       NULL,
//       PAGE_READWRITE | SEC_COMMIT,
//       0,
//       shm_size,
//       nm.m_shmm_name.c_str()
//     );

//     if (m_hshmm == NULL)
//     {
//       throw std::runtime_error(std::string(__FUNCTION__).append(" CreateFileMappingA(").append(nm.m_shmm_name).append(") GetLastError ").append(std::to_string(GetLastError())));
//     }
//   }
//   else
//   {
//     m_hshmm = OpenFileMappingA(
//       FILE_MAP_ALL_ACCESS
//       , FALSE /* can not inherit the handle */
//       , nm.m_shmm_name.c_str()
//     );

//     if (m_hshmm == NULL)
//     {
//       std::cerr << __FUNCTION__ << " OpenFileMappingA(" << nm.m_shmm_name << ") GetLastError ERROR_ALREADY_EXISTS" << std::endl;
//     }
//   }

//   if (GetLastError() == ERROR_ALREADY_EXISTS)
//   {
//     std::cerr << __FUNCTION__ << " CreateFileMappingA GetLastError ERROR_ALREADY_EXISTS" << std::endl;
//   }

//   if (m_hshmm)
//   {
//     m_mapped_memory = MapViewOfFile(
//       m_hshmm
//       , FILE_MAP_ALL_ACCESS
//       , 0
//       , 0
//       , shm_size
//     );
//   }
//   if (m_mapped_memory == NULL)
//   {
//     throw std::runtime_error(std::string(__FUNCTION__).append(" MapViewOfFile(").append(nm.m_shmm_name).append(") GetLastError ").append(std::to_string(GetLastError())));
//   }

//   // inplace create
//   m_data = new (m_mapped_memory) mapped_memory_layout_t(op, buf_size);
// }

// v00_base_buffer_t::~v00_base_buffer_t()
// {
//   if (FALSE == UnmapViewOfFile(m_mapped_memory))
//   {
//     std::cerr << __FUNCTION__ << " UnmapViewOfFile GetLastError " << GetLastError() << std::endl;
//   }
//   if (FALSE == CloseHandle(m_hshmm))
//   {
//     std::cerr << __FUNCTION__ << " CloseHandle(m_hshmm) GetLastError " << GetLastError() << std::endl;
//   }
//   m_data -> ~mapped_memory_layout_t();
// }

// v00_buffer_t::span_t v00_buffer_t::get_span(const uint64_t length) noexcept
// {
//   auto occupied = m_occupied.load();
//   const auto next_occupied = occupied + length;

//   if (next_occupied > m_data_size || !m_occupied.compare_exchange_weak(occupied, next_occupied))
//     return { nullptr, 0 };


//   return { &(m_data->m_first_byte) + occupied, m_data_size - next_occupied, 0 };
// }

// v00_buffer_t::span_t v00_singlethread_buffer_t::get_span(const uint64_t length) noexcept
// {
//   const auto occupied = m_occupied;
//   const auto next_occupied = m_occupied + length;

//   if (next_occupied > m_data_size)
//     return { nullptr, 0 };

//   m_occupied = next_occupied;

//   return { &(m_data->m_first_byte) + occupied, m_data_size - next_occupied, 0 };
// }

// v00_base_pool_t::v00_base_pool_t(std::size_t num_buffers, OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size)
//   : m_started(std::chrono::steady_clock::now())
// {
//   if (num_buffers > MAXIMUM_WAIT_OBJECTS)
//     throw std::runtime_error(std::string().append(" num_buffers ").append(std::to_string(num_buffers)).append(" exceeds MAXIMUM_WAIT_OBJECTS ").append(std::to_string(MAXIMUM_WAIT_OBJECTS)));

//   m_syncs.reserve(num_buffers);
//   m_buffers.reserve(num_buffers);
// }

// v00_base_pool_t::~v00_base_pool_t()
// {
// }

// v00_pool_t::v00_pool_t(std::size_t num_buffers, OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size)
//   : v00_base_pool_t(num_buffers, op, nm, buf_size)
// {
//   m_buffer = nullptr;

//   for(std::size_t i = 0; i < num_buffers; i++)
//   {
//     const auto indexed_nm = nm.with_suffix( std::to_string(i) );
//     m_syncs.emplace_back(new v00_sync_t(op, indexed_nm));
//     m_buffers.emplace_back(new v00_buffer_t(op, indexed_nm, buf_size, *(m_syncs.back()), m_buffer.load(), m_started));
//     m_buffer = (m_buffers.back()).get();
//   }
//   m_buffers.front()->m_next = m_buffer;
// }

// v00_singlethread_pool_t::v00_singlethread_pool_t(std::size_t num_buffers, OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size)
//   : v00_base_pool_t(num_buffers, op, nm, buf_size)
// {
//   m_buffer = nullptr;

//   for (std::size_t i = 0; i < num_buffers; i++)
//   {
//     const auto indexed_nm = nm.with_suffix(std::to_string(i));
//     m_syncs.emplace_back(new v00_sync_t(op, indexed_nm));
//     m_buffers.emplace_back(new v00_singlethread_buffer_t(op, indexed_nm, buf_size, *(m_syncs.back()), m_buffer, m_started));
//     m_buffer = (m_buffers.back()).get();
//   }
//   m_buffers.front()->m_next = m_buffer;
// }

// v00_async_listener_t::v00_async_listener_t(std::shared_ptr<v00_pool_t> pool)
//   : m_pool(pool)
// {
//   // defaults
//   m_params = std::make_shared<parameters_t>();
//   m_params->m_ready_data_size = 10000;

//   //m_processed.reserve(180000000);
//   m_stop_event = CreateEventA(NULL, false/*auto reset*/, false /*nonsignaled*/, NULL);
//   if (m_stop_event == NULL)
//   {
//     throw std::runtime_error(std::string(__FUNCTION__).append(" CreateEventA m_stop_event GetLastError ").append(std::to_string(GetLastError())));
//   }

//   m_sync_handles.reserve(m_pool->m_syncs.size() + 1);

//   m_sync_handles.push_back(m_stop_event);

//   for (const auto& s : m_pool->m_syncs)
//   {
//     m_sync_handles.push_back(s->m_hevent);
//   }

// }

// v00_async_listener_t::~v00_async_listener_t()
// {
//   CloseHandle(m_stop_event);
// }

// template <typename X>
// inline void print_mask(const char label, X next_sequence, X x) {
//     char buf[200];
//     char* p = buf + snprintf(buf, sizeof(buf) / sizeof(buf[0]), "m%c %lld ", label, next_sequence);
//     while (x) {
//       *(p++) = x & 1 ? '1' : '0';
//       x >>= 1;
//     }
//     *(p++) = '\n';
//     *p = '\x0';
//     std::cerr << buf;
// }

// std::function<void()> v00_async_listener_t::start(std::function <void(const uint64_t sequence, const uint8_t* p, const uint8_t* e)> consume_one)
// {
//   std::fill(cc_ready_data.begin(), cc_ready_data.end(), 0);
//   const auto runner_f = [this, consume_one]()
//   {
//     uint64_t write_ahead_queue_len = 0;
//     uint64_t read_data_offset = 0;
//     uint64_t next_sequence = 1;

//     const auto buffers_size = m_pool->m_buffers.size();

//     std::vector<std::pair<std::size_t, shared_memory::buffer_t::span_t>> write_ahead_buffer;
//     write_ahead_buffer.resize(m_pool->m_syncs.size() * m_params->m_ready_data_size);
//     const auto write_ahead_buffer_size = write_ahead_buffer.size();

//     bool stop_requested = false;
//     //DWORD wait_timeout = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(30)).count());
//     DWORD wait_timeout = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1)).count());
//     //DWORD wait_timeout = static_cast<DWORD>(std::chrono::milliseconds(1).count());

//     uint64_t sorting_buffer_mask = 0; // occupied buffers
//     if(m_pool->m_buffers.size() > (sizeof(sorting_buffer_mask) * 8))
//       throw std::runtime_error(std::string(__FUNCTION__).append(" too many buffers"));

//     // this array holds "ready to consume" buffers in the order of their's sequences
//     // due to async nature of events, "buffer is ready" events occures in random order and with X > Y, sequence X buffer may be signaled later than sequence Y buffer
//     // to restore natural order of buffers, sorting_buffer holds "ready to consume" buffers at positions which are related to the very last consumed sequence:
//     // when last consumed sequence is S and there are pending "buffer is ready" events, each referring sequences X, Y, Z 
//     // then sorting_buffer[X-S] will hold X-buffer data
//     // then sorting_buffer[Y-S] will hold Y-buffer data
//     // then sorting_buffer[Z-S] will hold Z-buffer data
//     // Since sequences are monitonic, then sorting_buffer is only appended.
//     // sorting_buffer_mask marks cells which are occupied.
//     // Every time there are "ready to consume" buffers, the logic below calcs the offset of a cell idx=X-S, where X is a buffer's sequence and S is a last consumed sequence.
//     // Then the data is copied into a corresponding cell and the sorting_buffer_mask is updated.
//     // 0-indexed cell always contains a data which can be consumed because its sequence is exact next sequence.
//     // At the end of a main loop, if sorting_buffer_mask LSB is marked then 0-indexed data is processed, the whole array is rotated and sorting_buffer_mask is shifted.
//     // 
//     // For performance reasons, it only could be 64 items in 
//     std::vector<std::vector<uint8_t>> sorting_buffer;
//      uint8_t dummy = 0;

//     {
//       // get the max size of each indiv buffer
//       // they should be equal-sized but just in case
//       std::size_t max_size = 0;
//       for (const auto& bb : m_pool->m_buffers)
//         if (max_size < bb->m_data_size)
//           max_size = bb->m_data_size;

//       const auto sorting_buffer_sz = m_pool->m_buffers.size() * 2;
//       sorting_buffer.reserve(sorting_buffer_sz);
//       for (std::size_t xx = 0; xx < sorting_buffer_sz; xx++)
//       {
//         sorting_buffer.emplace_back();
//         sorting_buffer.back().reserve(max_size);
//       }
//     }

//     std::string problem; problem.reserve(1000);
//     while (true)
//     {
//       // validate no unexpected states
//       {
//         const uint64_t diff_clean_dirty_mismatch = 1000;
//         const uint64_t diff_ahead = 1000;
//         if(cc_buf_reported_dirty > cc_buf_reported_clean && (cc_buf_reported_dirty - cc_buf_reported_clean) > diff_clean_dirty_mismatch)
//         {
//           problem.append("deadlock detected by cc_buf_reported_dirty-cc_buf_reported_clean;");
//         }
//         if (cc_buf_reported_inuse > cc_buf_reported_setfree && (cc_buf_reported_inuse - cc_buf_reported_setfree) > diff_clean_dirty_mismatch)
//         {
//           problem.append("deadlock detected by cc_buf_reported_inuse-cc_buf_reported_setfree;");
//         }
//         if(!problem.empty())
//         {
//           //throw std::runtime_error(problem);
//           std::cerr << problem << std::endl;
//         }
//       }

//       DWORD ret = ::WaitForMultipleObjectsEx(
//         DWORD(m_sync_handles.size()),
//         &(m_sync_handles[0]),
//         false /*bWaitAll*/,
//         wait_timeout,
//         false /*bAlertable*/
//       );

//       cc_consumer_wait_ret++;

//       if (ret == WAIT_FAILED)
//         throw std::runtime_error(std::string(__FUNCTION__).append(" WaitForMultipleObjectsEx WAIT_FAILED GetLastError ").append(std::to_string(GetLastError())));

//       if (ret == WAIT_TIMEOUT)
//       {
//         if(stop_requested) // stop was requested and no more events are pending
//           break;

//         cc_consumer_wait_timeout++;
//         consume_one(next_sequence, nullptr, nullptr);
//         continue; // TODO: prevent quick loop
//       }

//       /*WAIT_OBJECT_0 .. WAIT_OBJECT_0 + m_sync_handles.size() - 1  */
//       const DWORD WAIT_OBJECT_RANGE = WAIT_OBJECT_0 + m_sync_handles.size();
//       if (ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_RANGE)
//       {
//         if (ret == WAIT_OBJECT_0)
//         {
//           stop_requested = true;
//           wait_timeout = 0; // wait no more, process any events pending and exit
//         }

//         std::size_t idx = 0;
//         for (DWORD first_idx = ret - WAIT_OBJECT_0 - 1/*first event is "full stop" signaling event*/;
//           first_idx < m_pool->m_buffers.size();
//           first_idx++)
//         {
//           auto& b = m_pool->m_buffers[first_idx];
//           if (b->is_clean())
//             continue;

//           const auto s = b->get_data();
//           if(next_sequence > s.sequence)
//           {
//             // this buffer is expired because next expected sequence is greater than buffer's sequence
//             cc_consumer_buf_outdated_clear++;
//           }
//           else
//           {
//             idx = s.sequence - next_sequence;

//             //char buf[200];
//             //sprintf(buf, "idx %lld\n", idx);
//             //std::cerr << buf;

//             if(sizeof(cc_ready_data) / sizeof(cc_ready_data[0]) > idx)
//               cc_ready_data[idx]++;

//             if (idx >= sorting_buffer.size()) {
//               // this buffer is too far away and can not be consumed
//               // TODO: handle dropped
//               cc_consumer_buf_outdated_clear++;
//             }
//             else {
//               cc_consumer_buf_ready++;

//               sorting_buffer[idx].resize(s.free_bytes);
//               if(s.free_bytes)
//                 memmove(&(sorting_buffer[idx][0]), s.m_span, s.free_bytes);
//               sorting_buffer_mask |= (uint64_t(1) << idx);
//             }
//           }
//           b->clear();
//           cc_consumer_buf_clear++;
//         }

//         //print_mask('0', next_sequence, sorting_buffer_mask);

//         while (sorting_buffer_mask & 1) {
//           cc_consumer_buf_consumed++;

//           sorting_buffer_mask >>= 1;

//           const auto sz = sorting_buffer.front().size();
//           const auto* p = sz ? &(sorting_buffer.front().front()) : &dummy;
//           consume_one(next_sequence, p, p + sz);

//           ++next_sequence;
//           // rotate the array
//           for (std::size_t ii = 1; ii < sorting_buffer.size(); ii++) {
//             sorting_buffer[ii - 1].swap(sorting_buffer[ii]);
//           }
//         }

//         //print_mask('1', next_sequence, sorting_buffer_mask);

//         cc_consumer_continue++;
//         continue; 
//       } // if (ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_RANGE)

//       /*WAIT_ABANDONED_0 .. (WAIT_ABANDONED_0 + nCount� 1)*/
//       // exit if any of events have been abandoned
//       break;
//     }
//     return false;
//   };

//   std::shared_ptr<std::thread> x{ new std::thread(runner_f) };

//   return [this, x]()
//   {
//     if (FALSE == SetEvent(m_stop_event))
//     {
//       std::cerr << __FUNCTION__ << " SetEvent(m_stop_event) GetLastError " << GetLastError() << std::endl;
//     }
//     x->join();
//   };
// }

// } // namespace win_shared_mem
// } // transport
// } // impl
// } // neutrino
