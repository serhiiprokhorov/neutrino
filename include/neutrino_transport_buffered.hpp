#pragma once

#include <vector>
#include <memory>
#include "neutrino_transport.hpp"

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            struct buffered_endpoint_t : public endpoint_t
            {
                struct buffered_endpoint_params_t
                {
                    std::size_t m_message_buf_size{ 0 };
                    std::size_t m_message_buf_watermark{ 0 };
                } const m_buffered_endpoint_params;

                std::vector<uint8_t> m_message_buf;
                uint8_t* m_data = nullptr;
                std::size_t m_sz = 0;

                std::shared_ptr<endpoint_t> m_endpoint_sp;
                endpoint_t* m_endpoint = nullptr;

                buffered_endpoint_t(std::shared_ptr<endpoint_t> endpoint, const buffered_endpoint_params_t po)
                    : m_endpoint_sp(endpoint), m_buffered_endpoint_params(po)
                {
                    m_message_buf.resize(m_buffered_endpoint_params.m_message_buf_size);
                    m_data = m_message_buf.data();
                    m_sz = m_message_buf.size();

                    m_endpoint = m_endpoint_sp.get();
                }

            };
        }
    }
}
