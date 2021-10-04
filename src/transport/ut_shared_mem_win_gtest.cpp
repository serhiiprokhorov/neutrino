#include <gtest/gtest.h>

#include <neutrino_transport_shared_mem_win.hpp>

TEST(neutrino_transport_shared_mem_win, v00_buffer_t)
{
  const std::size_t num_buffers = 5;
  const DWORD buf_size = 1000;

  std::vector<uint8_t> expected_data;
  expected_data.reserve(buf_size);

  for(std::size_t i = 0; i < buf_size; i++)
    expected_data.emplace_back((uint8_t)(i % 0xff));


  {
    neutrino::impl::memory::win_shared_mem::v00_names_t nm(123, "domain", "suffix");

    neutrino::impl::memory::win_shared_mem::v00_pool_t host_pool(
      num_buffers
      , neutrino::impl::memory::win_shared_mem::OPEN_MODE::CREATE
      , nm
      , buf_size);

    neutrino::impl::memory::win_shared_mem::v00_pool_t guest_pool(
      num_buffers
      , neutrino::impl::memory::win_shared_mem::OPEN_MODE::OPEN
      , nm
      , buf_size);

    {
      ASSERT_TRUE(guest_pool.m_buffer.load()->is_clean());
      ASSERT_TRUE(host_pool.m_buffer.load()->is_clean());

      neutrino::impl::shared_memory::buffer_t::span_t guest_span = guest_pool.m_buffer.load()->get_span(buf_size);
      ASSERT_EQ(0, guest_span.free_bytes);
      memmove(guest_span.m_span, &(expected_data[0]), buf_size);

      ASSERT_TRUE(guest_pool.m_buffer.load()->is_clean());
      ASSERT_TRUE(host_pool.m_buffer.load()->is_clean());

      guest_pool.m_buffer.load()->dirty();
      ASSERT_FALSE(guest_pool.m_buffer.load()->is_clean());
      ASSERT_FALSE(host_pool.m_buffer.load()->is_clean());

      neutrino::impl::shared_memory::buffer_t::span_t host_span = host_pool.m_buffer.load()->get_data();

      ASSERT_EQ(buf_size, host_span.free_bytes);
      ASSERT_EQ(0, memcmp(host_span.m_span, guest_span.m_span, buf_size));

      host_pool.m_buffer.load()->clear();
      ASSERT_TRUE(guest_pool.m_buffer.load()->is_clean());
      ASSERT_TRUE(host_pool.m_buffer.load()->is_clean());

    }
  }
}