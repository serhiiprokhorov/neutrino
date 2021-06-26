#include <cstring>
#include <async_group.hpp>

namespace neutrino
{
    namespace impl
    {
        static void async_worker_fn(std::atomic_bool* need_stop, async_group_t::worker_t* pcm)
        {
            while(!(need_stop->load()))
            {
                if(pcm->m_need_data(pcm->m_params->m_connection_retry_timeout))
                {
                    auto status = pcm->m_read_data(pcm->m_params->m_connection_timeout);
                    if(status == worker_t::STATUS::BAD)
                        break;
                    if(status == worker_t::STATUS::HAS_DATA)
                        pcm->m_status();
                }
            }
        }

        void async_group_t::start_async_group()
        {
            std::scoped_lock<std::mutex> l(m_workers_mutex);
            for (auto& c : m_connections)
            {
                c.m_worker.swap(std::thread(async_worker_fn, &m_need_stop, this));
            }
        }

        void async_group_t::stop_async_group()
        {
            std::scoped_lock<std::mutex> l(m_workers_mutex);
            m_need_stop = true;
            for (auto& c : m_connections)
                if(c.m_worker.joinable) 
                    c.m_worker.join();
        }
    }
}
