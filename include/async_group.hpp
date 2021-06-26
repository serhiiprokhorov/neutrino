#pragma once

#include <chrono>
#include <functional>
#include <atomic>
#include <thread>
#include <list>

namespace neutrino
{
    namespace impl
    {
        struct async_group_t
        {
            struct params_t
            {
                std::chrono::nanoseconds m_timeout;
                std::chrono::nanoseconds m_retry_timeout;
            };

            struct worker_t
            {
                std::shared_ptr<params_t> m_params;

                std::thread m_worker;

                enum class STATUS
                {
                    BAD
                    , ACTIVE
                    , WAIT
                };

                std::function<bool(const std::chrono::nanoseconds&)> m_need_data = [](const std::chrono::nanoseconds&) { return false; };
                std::function<STATUS(const std::chrono::nanoseconds&)> m_read_data = [](const std::chrono::nanoseconds&) { return STATUS::WAIT; };
                std::function<bool()> m_status = []() { return false; };

                worker_t(std::shared_ptr<params_t> asp)
                    : m_params(asp)
                {
                }
            };

            std::shared_ptr<params_t> m_params;

            std::atomic_bool m_need_stop = false;
            std::list<worker_t> m_workers;
            std::mutex m_workers_mutex;

            async_group_t(std::shared_ptr<params_t> asp)
                : m_params(asp)
            {
            }

            ~async_group_t()
            {
                stop_async_group();
            }

            void start_async_group();

            void stop_async_group();

            void add_worker(std::function<void(worker_t&)> factory)
            {
                worker_t* pworker = nullptr;
                {
                    std::scoped_lock<std::mutex> l(m_workers_mutex);
                    m_workers.emplace_back();
                    pworker = &(m_workers.back());
                }
                factory(*pworker);
            }
        };
    }
}
