#pragma once

#include <deque>
#include <list>
#include <tuple>
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include "neutrino_transport.hpp"
#include "neutrino_producer.hpp"
#include "neutrino_frames_local.hpp"

namespace std
{
    std::string to_string(neutrino::impl::local::payload::event_type_t::event_types e);
}

namespace neutrino
{
    namespace mock
    {
        struct frames_collector_t : public impl::transport::endpoint_t
        {
            using impl::transport::endpoint_t::endpoint_t;

            struct raw_frame_t
            {
                std::vector<uint8_t> m_buffer;
                uint64_t m_consumed = 0;

                raw_frame_t() = default;
                raw_frame_t(const uint8_t* p, const uint8_t* e)
                {
                    m_buffer.reserve(e - p);
                    m_buffer.assign(p, e);
                }
            };

            std::mutex m_m;
            std::list<raw_frame_t> m_sumbissions; // may contain multiple frames due to buffering

            bool consume(const uint8_t* p, const uint8_t* e) override
            {
                std::lock_guard<std::mutex> lk(m_m);
                m_sumbissions.emplace_back(p, e);
                return true;
            }
        };

        template <typename endpoint_impl_t>
        struct worker_t : public frames_collector_t
        {
            endpoint_impl_t& m_endpoint_impl;

            bool consume(const uint8_t* p, const uint8_t* e) final
            {
                return frames_collector_t::consume(p, e) && m_endpoint_impl.consume(p, e);
            }

            worker_t(endpoint_impl_t& endpoint_impl)
                : m_endpoint_impl(endpoint_impl)
            {
            }
        };

        struct consumer_t : public impl::transport::consumer_t
        {
            std::map<neutrino::impl::local::payload::stream_id_t::type_t, std::size_t> m_expected_frame_cc;
            std::size_t m_actual_frame_cc = 0;

            std::list<
                std::tuple<
                    std::size_t
                    , neutrino::impl::local::payload::nanoepoch_t::type_t
                    , neutrino::impl::local::payload::stream_id_t::type_t
                    , neutrino::impl::local::payload::event_id_t::type_t
                >
            > m_expected_checkpoints;
            std::list<
                std::tuple<
                    std::size_t
                    , neutrino::impl::local::payload::nanoepoch_t::type_t
                    , neutrino::impl::local::payload::stream_id_t::type_t
                    , neutrino::impl::local::payload::event_id_t::type_t
                    , neutrino::impl::local::payload::event_type_t::event_types
                >
            > m_expected_contexts;

            decltype(m_expected_checkpoints) m_actual_checkpoints;
            decltype(m_expected_contexts) m_actual_contexts;

            auto& expect_checkpoint(neutrino::impl::local::payload::nanoepoch_t::type_t nanoepoch, neutrino::impl::local::payload::stream_id_t::type_t stream_id, neutrino::impl::local::payload::event_id_t::type_t event_id)
            {
                auto& cc = m_expected_frame_cc[stream_id];
                m_expected_checkpoints.emplace_back(cc, nanoepoch, stream_id, event_id);
                cc++;
                return *this;
            }

            auto& expect_context_enter(neutrino::impl::local::payload::nanoepoch_t::type_t nanoepoch, neutrino::impl::local::payload::stream_id_t::type_t stream_id, neutrino::impl::local::payload::event_id_t::type_t event_id)
            {
                auto& cc = m_expected_frame_cc[stream_id];
                m_expected_contexts.emplace_back(cc, nanoepoch, stream_id, event_id, neutrino::impl::local::payload::event_type_t::event_types::CONTEXT_ENTER);
                cc++;
                return *this;
            }

            auto& expect_context_leave(neutrino::impl::local::payload::nanoepoch_t::type_t nanoepoch, neutrino::impl::local::payload::stream_id_t::type_t stream_id, neutrino::impl::local::payload::event_id_t::type_t event_id)
            {
                auto& cc = m_expected_frame_cc[stream_id];
                m_expected_contexts.emplace_back(cc, nanoepoch, stream_id, event_id, neutrino::impl::local::payload::event_type_t::event_types::CONTEXT_LEAVE);
                cc++;
                return *this;
            }

            auto& expect_context_panic(neutrino::impl::local::payload::nanoepoch_t::type_t nanoepoch, neutrino::impl::local::payload::stream_id_t::type_t stream_id, neutrino::impl::local::payload::event_id_t::type_t event_id)
            {
                auto& cc = m_expected_frame_cc[stream_id];
                m_expected_contexts.emplace_back(cc, nanoepoch, stream_id, event_id, neutrino::impl::local::payload::event_type_t::event_types::CONTEXT_PANIC);
                cc++;
                return *this;
            }

            template <typename tt>
            bool throw_if_mismatch(const char* nm, tt actual, tt expected)
            {
                if (actual != expected)
                    throw std::runtime_error(
                    std::string(nm)
                    .append(" actual=").append(std::to_string(actual))
                    .append(" expected=").append(std::to_string(expected))
                );
                return true;
            }

            void consume_checkpoint(
                const neutrino::impl::local::payload::nanoepoch_t::type_t& nanoepoch
                , const neutrino::impl::local::payload::stream_id_t::type_t& stream_id
                , const neutrino::impl::local::payload::event_id_t::type_t& event_id
            ) override
            {
                // lookup first expected element by stream id
                for(auto expected = m_expected_checkpoints.begin(); expected != m_expected_checkpoints.end(); expected++)
                {
                    if(std::get<2>(*expected) != stream_id)
                        continue;

                    //TODO: implement m_actual_frame_cc by stream
                    //throw_if_mismatch("frame counter", m_actual_frame_cc, std::get<0>(*expected));
                    std::get<1>(*expected) && throw_if_mismatch("checkpoint nanoepoch", nanoepoch, std::get<1>(*expected));
                    throw_if_mismatch("checkpoint stream_id", stream_id, std::get<2>(*expected));
                    throw_if_mismatch("checkpoint event_id", event_id, std::get<3>(*expected)); // event id

                    m_actual_checkpoints.emplace_back(m_actual_frame_cc, nanoepoch, stream_id, event_id);

                    m_actual_frame_cc++;
                    m_expected_checkpoints.erase(expected);

                    return;
                }
                throw std::runtime_error(
                    std::string(__FUNCTION__)
                    .append(" not expected stream_id=").append(std::to_string(stream_id))
                    .append(" event_id=").append(std::to_string(event_id))
                );
            }

            virtual void consume_context(
                const neutrino::impl::local::payload::nanoepoch_t::type_t& nanoepoch
                , const neutrino::impl::local::payload::stream_id_t::type_t& stream_id
                , const neutrino::impl::local::payload::event_id_t::type_t& event_id
                , const neutrino::impl::local::payload::event_type_t::event_types& event_type
            ) override
            {
                // lookup first expected element by stream id
                for (auto expected = m_expected_contexts.begin(); expected != m_expected_contexts.end(); expected++)
                {
                    if (std::get<2>(*expected) != stream_id)
                        continue;

                    //TODO: implement m_actual_frame_cc by stream
                    //throw_if_mismatch("frame counter", m_actual_frame_cc, std::get<0>(*expected));
                    std::get<1>(*expected) && throw_if_mismatch("context nanoepoch", nanoepoch, std::get<1>(*expected));
                    throw_if_mismatch("context stream_id", stream_id, std::get<2>(*expected));
                    throw_if_mismatch("context event_id", event_id, std::get<3>(*expected));
                    throw_if_mismatch("context event_type", event_type, std::get<4>(*expected));

                    m_actual_contexts.emplace_back(m_actual_frame_cc, nanoepoch, stream_id, event_id, event_type);

                    m_actual_frame_cc++;
                    m_expected_contexts.erase(expected);

                    return;
                }
                throw std::runtime_error(
                    std::string(__FUNCTION__)
                    .append(" not expected stream_id=").append(std::to_string(stream_id))
                    .append(" event_id=").append(std::to_string(event_id))
                );
            }
        };
        struct scoped_guard
        {
            std::shared_ptr<neutrino::impl::transport::consumer_stub_t> m_prev_consumer;
            scoped_guard(std::shared_ptr<neutrino::impl::transport::consumer_stub_t> e)
                : m_prev_consumer(neutrino::impl::producer::set_consumer(e))
            {
            }

            ~scoped_guard()
            {
                neutrino::impl::producer::set_consumer(m_prev_consumer);
            }
        };
    }
}