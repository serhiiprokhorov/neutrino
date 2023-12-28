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

#include <neutrino_transport_shared_mem_v00_linux.hpp>

namespace neutrino::transport::shared_memory::linux
{
initializer_memfd_t::~initializer_memfd_t()
{
  munmap(m_rptr, m_bytes);
}

initializer_memfd_t::initializer_memfd_t(std::size_t buffer_bytes)
  : m_bytes(buffer_bytes)
{
  m_fd = memfd_create("initializer_memfd_t::buffer_t", MFD_ALLOW_SEALING);
  if (m_fd == -1) {
    register_error(errno, "initializer_memfd_t(size).memfd_create");
    return;
  }


  if (ftruncate(m_fd, total_length) == -1) {
    register_error(errno, "initializer_memfd_t(size).ftruncate");
    return;
  }

  if (fcntl(m_fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) == -1) {
    register_error(errno, "initializer_memfd_t(size).fcntl seals");
    return;
  }

  /* Map shared memory object */
  m_rptr = mmap(NULL, buffer_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
  if (m_rptr == MAP_FAILED) {
    register_error(errno, "initializer_memfd_t(size).ftruncate");
    return;
  }

  close(m_fd);
}

initializer_memfd_t::initializer_memfd_t(unsigned int fd)
  : m_fd(fd)
{
  // fd exists but the size is unknown
  errno = 0;
  auto off = lseek(fd, 0, SEEK_END);
  if(errno != 0) {
    register_error(errno, "initializer_memfd_t(fd).lseek");
    return;
  }

  if(off < 1) {
    register_error(errno, "initializer_memfd_t(fd).lseek negative offset");
    return;
  }

  m_bytes = static_cast<std::size_t>(off);

  /* Map shared memory object */
  m_rptr = mmap(NULL, buffer_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
  if (m_rptr == MAP_FAILED) {
    register_error(errno, "initializer_memfd_t(size).ftruncate");
    return;
  }

  close(m_fd);

}

void v00_shared_header_control_t::header_t::format(bool is_new) {
  if(is_new) {
    if( sem_init(&(m_ready), 1 /* this sem is shared between processes */, 1) != 0 ) {
      error(errno, "format_at.sem_init");
    }
  }
}

void v00_shared_header_control_t::header_t::destroy() {
  sem_destroy(&m_ready);
}

bool v00_shared_header_control_t::header_t::is_clean() {
  return m_inuse_bytes.load() == 0;
}

void v00_shared_header_control_t::header_t::clear() {
  m_inuse_bytes.store(0);
  m_sequence.store(0);
}

v00_shared_header_control_t::header_t::dirty(const uint64_t bytes, const uint64_t sequence) {
  m_inuse_bytes.store(bytes);
  m_sequence.store(sequence);
  sem_post(&m_ready);
}

}