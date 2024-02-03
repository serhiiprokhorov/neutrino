
#include <gtest/gtest.h>

#include <string_view>

#include <neutrino_shared_mem_v00_port_synchronized.hpp>

using namespace neutrino::transport::shared_memory;

struct SynchronizedTest : public testing::Test
{
    // shared memory mapped at consumer side
    mem_buf_v00_linux_t m_consumer_membuf;
    // producer port
    std::unique_ptr<synchronized_port_v00_linux_t> m_port;

    void SetUp() override {
        
        m_consumer_membuf = init_v00_buffers_ring("process=consumer");

        // export mem fd as env variable
        char buf[200];
        snprintf(buf, sizeof(buf)-1,"%i",m_consumer_membuf.first->m_fd);
        ::setenv("NEUTRINO_CONSUMER_FD", )
        
        

        m_port = std::make_unique<synchronized_port_v00_linux_t>(
            transport::shared_memory::init_v00_buffers_ring("")
        );
    }

    void TearDown() override {
        m_port
    }
};