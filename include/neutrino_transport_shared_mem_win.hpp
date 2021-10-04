#pragma once

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

#include <memory>
#include <string>
#include <stdexcept>

#include <neutrino_transport_buffered_endpoint_proxy.hpp>

namespace neutrino
{
    namespace impl
    {
        namespace memory
        {
            namespace win_shared_mem
            {
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
                    alignas(alignof(uint64_t)) LONGLONG m_dwInUseSize;

                    v00_header_t(OPEN_MODE op, DWORD dwMaximumSize);
                    std::size_t size() const { return sizeof(*this); }

                    void set_inuse(const uint64_t inuse) noexcept;
                    void set_free(const uint64_t bytes_inuse) noexcept;
                };

                struct v00_sync_t //: public shared_memory::sync_t
                {
                    HANDLE m_hevent = INVALID_HANDLE_VALUE;
                    HANDLE m_hsem = INVALID_HANDLE_VALUE;
                    bool m_is_clean = true;

                    v00_sync_t(OPEN_MODE op, const v00_names_t&);
                    ~v00_sync_t();

                    const bool is_clean() const noexcept;
                    void dirty() noexcept;
                    void clear() noexcept;
                };

                struct v00_buffer_t : public shared_memory::buffer_t
                {
                    HANDLE m_hshmm = INVALID_HANDLE_VALUE;
                    LPVOID m_mapped_memory = nullptr;

                    struct mapped_memory_layout_t
                    {
                      v00_header_t m_header;
                      uint8_t m_data[1];
                      mapped_memory_layout_t(OPEN_MODE op, DWORD dwMaximumSize)
                        : m_header(op, dwMaximumSize) {}
                    };

                    v00_sync_t& m_sync;
                    mapped_memory_layout_t* m_data; // inplace ctor, not a dynamic memory resource
                    const uint64_t m_data_size; 
                    std::atomic<uint64_t> m_occupied;

                    const bool is_clean() const noexcept final { return m_sync.is_clean(); }
                    void dirty() noexcept final { 
                      m_data->m_header.set_inuse(m_occupied.load());
                      m_sync.dirty();
                    }
                    void clear() noexcept final { 
                      m_sync.clear(); 
                      m_data->m_header.set_free(m_occupied.load());
                    }

                    virtual shared_memory::buffer_t* get_next() const noexcept
                    {
                      return m_next;
                    }

                    span_t get_span(const uint64_t length) noexcept final;

                    span_t get_data() noexcept final
                    {
                      m_occupied = m_data->m_header.m_dwInUseSize;
                      return { m_data->m_data, m_occupied.load() };
                    }

                   v00_buffer_t(OPEN_MODE op, const v00_names_t&, const DWORD buf_size, v00_sync_t& sync, buffer_t* b);
                    ~v00_buffer_t();
                };

                struct v00_pool_t : public shared_memory::pool_t
                {
                  std::vector<std::shared_ptr<v00_sync_t>> m_syncs;
                  std::vector<HANDLE> m_sync_handles;
                  std::vector<std::shared_ptr<v00_buffer_t>> m_buffers;


                  v00_pool_t(std::size_t num_buffers, OPEN_MODE op, const v00_names_t& nm, const DWORD buf_size);
                };
            };
        }
    }
}
