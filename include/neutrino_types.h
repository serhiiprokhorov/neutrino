#pragma once

#include <stdint.h>

typedef struct 
{
    uint64_t v;
} neutrino_nanoepoch_t;

typedef struct 
{
    uint64_t v;
} neutrino_stream_id_t;

typedef struct 
{
    uint64_t v;
} neutrino_event_id_t;


/*
namespace neutrino
{
    namespace payload
    {
        struct header_t
        {
            typedef uint8_t type_t;
        };
        struct nanoepoch_t
        {
            typedef uint64_t type_t;
        };
        struct stream_id_t
        {
            typedef uint64_t type_t;
        };
        struct event_id_t
        {
            typedef uint64_t type_t;
        };
        struct event_type_t
        {
            typedef uint8_t type_t;
            enum class event_types
            {
                NO_CONTEXT = 0
                , CONTEXT_ENTER = 1
                , CONTEXT_LEAVE = 2
                , CONTEXT_PANIC = 3
                , _LAST
            };
        };
        struct error_t
        {
            typedef uint8_t type_t;
        };
    } // payload
    namespace frame
    {
        namespace v00
        {
            namespace frame_error
            {
                const uint8_t header = uint8_t(1) & 0b00111111;
            }
            namespace checkpoint
            {
                const uint8_t header = uint8_t(2) & 0b00111111;
            }
            namespace context
            {
                const uint8_t header_context = uint8_t(3) & 0b00111111;
            }
        }
    } // frame
} // neutrino
*/