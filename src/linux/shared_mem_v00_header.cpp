#include <memory>
#include <stdexcept>
#include <cstddef> 
#include <cstdint> 
#include <atomic>

#include <semaphore.h>

#include <neutrino_errors.hpp>
#include "shared_mem_v00_header.hpp"


namespace{
  struct v00_shared_header_impl_t
  {
    /// states:
    /// - "blocked/signalled" (value ==0) consumer waits, producer adds events    
    /// - "unblocked/not signaled" (value >0) consumer reads the events, producer tries if the buffer is free
    sem_t m_ready; 
    /// filled by dirty(), tells consumer how many bytes are occupied by events data;
    /// needed because the buffer may include some different amount of bytes, 
    /// the buffer is signalled when there is not enough space to put a biggest event (hi watermark is reached)
    /// states:
    /// - ==0 the buffer is clean, producer may add new events (a producer maintains its own counter of bytes available), consumer is blocked
    /// - >0 the buffer is dirty/signaled, producer may not touch the buffer, consumer process the events 
    std::atomic_uint64_t m_inuse_bytes = 0; 
    /// indicates an order in which buffers are marked "ready to be consumed", 
    /// needed to help resolve ambiguity consumer side if signals were delayed or processed in out of order
    std::atomic_uint64_t m_sequence = 0;
  };
}

namespace neutrino::transport::shared_memory
{

std::size_t v00_shared_header_t::reserve_bytes = sizeof(v00_shared_header_impl_t); 


void v00_shared_header_t::init(uint8_t* at)
{
  auto* impl = reinterpret_cast<v00_shared_header_impl_t*>(at);
  if( sem_init(&impl->m_ready, 1 /* this sem is shared between processes */, 1) != 0 ) {
    throw os::errno_error(__FUNCTION__);
  }
}

void v00_shared_header_t::destroy(uint8_t* at) noexcept
{
  auto* impl = reinterpret_cast<v00_shared_header_impl_t*>(at);
  sem_destroy(&(impl->m_ready));
}

bool v00_shared_header_t::is_clean(uint8_t* at) noexcept
{
  auto* impl = reinterpret_cast<v00_shared_header_impl_t*>(at);
  return impl->m_inuse_bytes.load() != 0;
}

void v00_shared_header_t::clear(uint8_t* at) noexcept
{
  auto* impl = reinterpret_cast<v00_shared_header_impl_t*>(at);
  impl->m_inuse_bytes.store(0);
  impl->m_sequence.store(0);
}

void v00_shared_header_t::dirty(uint8_t* at, const uint64_t bytes, const uint64_t sequence) noexcept
{
  auto* impl = reinterpret_cast<v00_shared_header_impl_t*>(at);
  impl->m_inuse_bytes.store(bytes);
  impl->m_sequence.store(sequence);
}

}

