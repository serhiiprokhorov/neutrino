#pragma once

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

#include <memory>
#include <string>
#include <stdexcept>

#include "neutrino_transport_shared_mem.hpp"

namespace neutrino
{
  namespace transport
  {
    namespace shared_memory
    {
      namespace platform
      {
        struct initializer_memfd_t
        {
          initializer_memfd_t(std::size_t buffer_bytes);
          initializer_memfd_t(unsigned int fd);
          ~initializer_memfd_t();
            
          unsigned int m_fd = -1; /// fd of memfd
          void* m_rptr = nullptr; /// mmap shared mem ptr
          std::size_t m_bytes; /// size in bytes of a single buffer
        };

        struct formatter_t
        {
          static std::vector<buffer_t> create_ring(initializer_memfd_t& memory);
        };
      }
    }
  }
}

namespace neutrino
{
    namespace impl
    {
        namespace memory
        {
            namespace linux_shared_mem
            {
              struct buffer_initializer_t {
                std::unique_ptr<neutrino::>
              }


                void print_stats(std::ostream& out);

                struct v00_names_t
                {
                    v00_names_t(unsigned long pid, const std::string& domain, const std::string& suffix);
                    v00_names_t(std::string shmm_name, std::string event_name, std::string sem_name);

                    const v00_names_t with_suffix(const std::string& sf) const;

                    const std::string m_shmm_name;
                    const std::string m_event_name;
                    const std::string m_sem_name;
                };

                enum class OPEN_MODE
                {
                    CREATE
                    , OPEN
                };

                struct alignas(alignof(uint64_t)) v00_header_t
                {
                    DWORD m_header_size;
                    DWORD m_dwLayoutVersion;
                    DWORD m_hostPID; /// host app reading from a buffer
                    DWORD m_dwMaximumSize;
                    alignas(alignof(uint64_t)) LONG64 m_inuse_bytes;
                    alignas(alignof(uint64_t)) LONG64 m_sequence;

                    v00_header_t(OPEN_MODE op, DWORD dwMaximumSize);
                    std::size_t size() const { return sizeof(*this); }

                    // not thread safe
                    void set_inuse(const LONG64 inuse, const LONG64 diff_started) noexcept;
                    // not thread safe
                    void set_free() noexcept;
                };

                struct v00_sync_t //: public shared_memory::sync_t
                {
                    HANDLE m_hevent = INVALID_HANDLE_VALUE;
                    HANDLE m_hsem = INVALID_HANDLE_VALUE;

                    v00_sync_t(OPEN_MODE op, const v00_names_t&);
                    ~v00_sync_t();

                    const bool is_clean() const noexcept;
                    void dirty() noexcept;
                    void clear() noexcept;
                };

                struct v00_base_buffer_t : public shared_memory::buffer_t {

                  struct mapped_memory_layout_t
                  {
                    v00_header_t m_header;
                    uint8_t m_first_byte;
                    mapped_memory_layout_t(OPEN_MODE op, DWORD dwMaximumSize)
                      : m_header(op, dwMaximumSize) {}
                  };

                  HANDLE m_hshmm = INVALID_HANDLE_VALUE;
                  LPVOID m_mapped_memory = nullptr;

                  v00_sync_t& m_sync;
                  mapped_memory_layout_t* m_data; // inplace ctor, not a dynamic memory resource
                  const uint64_t m_data_size;
                  const std::chrono::steady_clock::time_point m_started; // TODO: for set_inuse

                  const bool is_clean() const noexcept final { return m_sync.is_clean(); }

                  void clear() noexcept final {
                    //char buf[200];
                    //snprintf(buf, sizeof(buf) / sizeof(buf[0]), "clear %lld\n", m_data->m_header.m_sequence);
                    //std::cerr << buf;
                    m_sync.clear();
                    m_data->m_header.set_free();
                  }

                  virtual shared_memory::buffer_t* get_next() const noexcept
                  {
                    return m_next;
                  }

                  v00_base_buffer_t(OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size, v00_sync_t& sync, buffer_t* b, std::chrono::steady_clock::time_point started);
                  ~v00_base_buffer_t();

                };


                struct v00_buffer_t : public v00_base_buffer_t
                {
                  using v00_base_buffer_t::v00_base_buffer_t;
                  std::atomic<uint64_t> m_occupied = 0;

                    void dirty(uint64_t dirty_buffer_counter) noexcept final {
                      //char buf[200];
                      //snprintf(buf, sizeof(buf) / sizeof(buf[0]), "dirty %lld\n", dirty_buffer_counter);
                      //std::cerr << buf;
                      m_data->m_header.set_inuse(
                        m_occupied.load()
                        //, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - m_started).count()
                        , dirty_buffer_counter
                      );
                      m_occupied = 0;
                      m_sync.dirty();
                    }

                    span_t get_span(const uint64_t length) noexcept final;

                    span_t get_data() noexcept final
                    {
                      m_occupied = m_data->m_header.m_inuse_bytes; // TODO: needs mem fence!!!
                      const uint64_t sequence = m_data->m_header.m_sequence;
                      //char buf[200];
                      //snprintf(buf, sizeof(buf) / sizeof(buf[0]), "get data %lld bytes %lld\n", sequence, m_occupied.load());
                      //std::cerr << buf;
                      return { &m_data->m_first_byte, m_occupied.load(), sequence };
                    }
                };

                struct v00_singlethread_buffer_t : public v00_base_buffer_t
                {
                  using v00_base_buffer_t::v00_base_buffer_t;
                  uint64_t m_occupied = 0;

                  void dirty(uint64_t dirty_buffer_counter) noexcept final {
                    //char buf[200];
                    //snprintf(buf, sizeof(buf) / sizeof(buf[0]), "dirty %lld\n", dirty_buffer_counter);
                    //std::cerr << buf;
                    m_data->m_header.set_inuse(
                      m_occupied
                      //, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - m_started).count()
                      , dirty_buffer_counter
                    );
                    m_occupied = 0;
                    m_sync.dirty();
                  }

                  span_t get_span(const uint64_t length) noexcept final;


                  span_t get_data() noexcept final
                  {
                    m_occupied = m_data->m_header.m_inuse_bytes; // TODO: needs mem fence!!!
                    const uint64_t sequence = m_data->m_header.m_sequence;
                    //char buf[200];
                    //snprintf(buf, sizeof(buf) / sizeof(buf[0]), "get data %lld bytes %lld\n", sequence, m_occupied.load());
                    //std::cerr << buf;
                    return { &m_data->m_first_byte, m_occupied, sequence };
                  }
                };

                struct v00_base_pool_t : public shared_memory::pool_t {
                  std::vector<std::shared_ptr<v00_sync_t>> m_syncs;
                  std::vector<std::shared_ptr<v00_base_buffer_t>> m_buffers;
                  std::chrono::steady_clock::time_point m_started;

                  v00_base_pool_t(std::size_t num_buffers, OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size);
                  ~v00_base_pool_t();
                };

                struct v00_pool_t : public v00_base_pool_t
                {
                  std::atomic<shared_memory::buffer_t*> m_buffer{ nullptr };

                  shared_memory::buffer_t* get_buffer() override {
                    return m_buffer.load();
                  };

                  bool set_buffer(shared_memory::buffer_t* x) override {
                    auto current = m_buffer.load();
                    return m_buffer.compare_exchange_weak(current, x);
                  }

                  v00_pool_t(std::size_t num_buffers, OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size);
                };

                struct v00_singlethread_pool_t : public v00_base_pool_t
                {
                  shared_memory::buffer_t* m_buffer{ nullptr };

                  shared_memory::buffer_t* get_buffer() override {
                    return m_buffer;
                  };

                  bool set_buffer(shared_memory::buffer_t* x) override {
                    m_buffer = x;
                    return true;
                  }

                  v00_singlethread_pool_t(std::size_t num_buffers, OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size);
                };

                // TODO: consumer-only header?
                struct v00_async_listener_t
                {
                  std::vector<HANDLE> m_sync_handles;
                  HANDLE m_stop_event;
                  std::shared_ptr<v00_pool_t> m_pool;

                  struct parameters_t
                  {
                    std::size_t m_ready_data_size; // size of m_ready_data, bigger size allows avoid copy of span_t
                  };
                  
                  std::shared_ptr<parameters_t> m_params;

                  v00_async_listener_t(std::shared_ptr<v00_pool_t> pool);
                  ~v00_async_listener_t();

                  // starts new thread, returns cancellation function
                  std::function<void()> start(std::function <void(const uint64_t sequence, const uint8_t* p, const uint8_t* e)> consume_one);
                };
            };
        }
    }
}