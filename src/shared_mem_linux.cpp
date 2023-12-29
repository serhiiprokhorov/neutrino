#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <neutrino_shared_mem_linux.hpp>

namespace neutrino::transport::shared_memory
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
    throw errno_error(std::source_location::current(), errno, "memfd_create");
  }


  if (ftruncate(m_fd, buffer_bytes) == -1) {
    throw errno_error(std::source_location::current(), errno, "ftruncate");
  }

  if (fcntl(m_fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) == -1) {
    throw errno_error(std::source_location::current(), errno, "fcntl seals");
  }

  /* Map shared memory object */
  void* rptr = mmap(NULL, buffer_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
  if (rptr == MAP_FAILED) {
    throw errno_error(std::source_location::current(), errno, "mmap");
  }

  m_rptr = reinterpret_cast<uint8_t*>(rptr);

  close(m_fd);
}

initializer_memfd_t::initializer_memfd_t(unsigned int fd)
  : m_fd(fd)
{
  // fd exists but the size is unknown
  errno = 0;
  auto off = lseek(fd, 0, SEEK_END);
  if(errno != 0) {
    throw errno_error(std::source_location::current(), errno, "lseek");
  }

  if(off < 1) {
    throw errno_error(std::source_location::current(), errno, "lseek negative offset");
  }

  m_bytes = static_cast<std::size_t>(off);

  /* Map shared memory object */
  void * rptr = mmap(NULL, m_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
  if (m_rptr == MAP_FAILED) {
    throw errno_error(std::source_location::current(), errno, "ftruncate");
  }

  m_rptr = reinterpret_cast<uint8_t*>(rptr);

  close(m_fd);

}
}