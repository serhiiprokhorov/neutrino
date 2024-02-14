#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <neutrino_errors.hpp>
#include <neutrino_shared_mem_initializer_linux.hpp>

namespace neutrino::transport::shared_memory
{

initializer_memfd_t::~initializer_memfd_t()
{
  munmap(m_rptr, m_bytes);
  if(m_fd != -1) {
    close(m_fd);
  }
}

initializer_memfd_t::initializer_memfd_t(std::size_t buffer_bytes, const char*, std::function<void(unsigned int)> with_fd)
  : m_is_consumer(true), m_bytes(buffer_bytes)
{
  m_fd = memfd_create("initializer_memfd_t::buffer_t", MFD_ALLOW_SEALING);
  if (m_fd == -1) {
    throw neutrino::os::errno_error(std::source_location::current(), "memfd_create");
  }


  if (ftruncate(m_fd, buffer_bytes) == -1) {
    throw neutrino::os::errno_error(std::source_location::current(), "ftruncate");
  }

  if (fcntl(m_fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) == -1) {
    throw neutrino::os::errno_error(std::source_location::current(), "fcntl seals");
  }

  // Map shared memory object
  // NOTE: mmap adds file reference and some docs recommends to close m_fd after that, 
  //       but in this app it can not be closed as it going to be shared with producer process 
  // NOTE: memfd_create mutually adds MAP_ANONYMOUS
  void* rptr = mmap(NULL, buffer_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
  if (rptr == MAP_FAILED) {
    throw neutrino::os::errno_error(std::source_location::current(), "mmap");
  }

  m_rptr = reinterpret_cast<uint8_t*>(rptr);

  with_fd(m_fd);
}

initializer_memfd_t::initializer_memfd_t(unsigned int fd)
  : m_is_consumer(false), m_fd(fd)
{
  // fd exists but the size is unknown
  errno = 0;
  auto off = lseek(m_fd, 0, SEEK_END);
  if(errno != 0) {
    throw neutrino::os::errno_error(std::source_location::current(), "lseek");
  }

  if(off < 1) {
    throw neutrino::os::errno_error(std::source_location::current(), "lseek negative offset");
  }

  m_bytes = static_cast<std::size_t>(off);

  // Map shared memory object
  // m_fd comes from consumer process which originally created the shared file
  void * rptr = mmap(NULL, m_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
  if (m_rptr == MAP_FAILED) {
    throw neutrino::os::errno_error(std::source_location::current(), "ftruncate");
  }

  m_rptr = reinterpret_cast<uint8_t*>(rptr);

}

}