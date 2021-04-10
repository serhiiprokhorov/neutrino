#include <chrono>
#include <array>

#include <neutrino_transport.hpp>
#include <neutrino_frames_serialized.hpp>
#include <neutrino_transport_endpoint_async_posix_handle.hpp>
#include <neutrino_transport_buffered_mt.hpp>
#include <neutrino_transport_buffered_st.hpp>

using namespace neutrino::impl;

namespace
{
    template <typename raw_encoding_t>
    struct frame_v00_raw_traits_t
    {
        typedef serialized::raw_t<local::payload::header_t, raw_encoding_t> header_raw_t;
        typedef serialized::raw_t<local::payload::nanoepoch_t, raw_encoding_t> nanoepoch_raw_t;
        typedef serialized::raw_t<local::payload::stream_id_t, raw_encoding_t> stream_id_raw_t;
        typedef serialized::raw_t<local::payload::event_id_t, raw_encoding_t> event_id_raw_t;
        typedef serialized::raw_t<local::payload::event_type_t, raw_encoding_t> event_type_raw_t;
        constexpr static const std::size_t max_buf_size = 2 * header_raw_t::span() + nanoepoch_raw_t::span() + stream_id_raw_t::span() + event_id_raw_t::span() + event_type_raw_t::span();
    };



    template <typename raw_encoding_t>
    struct frame_v00_deserializer_endpoint_impl_t : public transport::endpoint_impl_t, frame_v00_raw_traits_t<raw_encoding_t>
    {
        using transport::endpoint_impl_t::endpoint_impl_t;

        bool consume(const uint8_t* pBuf, const uint8_t* pBufEnd) final
        {
            const uint8_t* pFrameStart = pBuf;
            const std::size_t b = pBufEnd - pBuf;
            while(pFrameStart < pBufEnd)
            {
                local::payload::header_t::type_t header;
                const uint8_t* pFrameHeader = pFrameStart;
                const uint8_t* pFrameEnd = pFrameHeader;
                pFrameStart += header_raw_t::span();

                if (pFrameStart >= pBufEnd || !header_raw_t::convert(pFrameHeader, header))
                    break;

                if (header == local::frame::v00::checkpoint::header)
                {
                    const uint8_t* pFrameNanoepoch = pFrameStart;
                    const uint8_t* pFrameStreamId = pFrameNanoepoch + nanoepoch_raw_t::span();
                    const uint8_t* pFrameEventId = pFrameStreamId + stream_id_raw_t::span();
                    const uint8_t* pFrameFooter = pFrameEventId + event_id_raw_t::span();
                    pFrameEnd = pFrameFooter + header_raw_t::span();
                    if (pFrameEnd > pBufEnd)
                        break;
                    local::payload::header_t::type_t footer;
                    if (!header_raw_t::convert(pFrameFooter, footer))
                        break;
                    if (footer != header)
                        break;
                    local::payload::nanoepoch_t::type_t nanoepoch;
                    local::payload::stream_id_t::type_t stream_id;
                    local::payload::event_id_t::type_t event_id;
                    nanoepoch_raw_t::convert(pFrameNanoepoch, nanoepoch);
                    stream_id_raw_t::convert(pFrameStreamId, stream_id);
                    event_id_raw_t::convert(pFrameEventId, event_id);
                    m_consumer.consume_checkpoint(nanoepoch, stream_id, event_id);
                }
                else if (header == local::frame::v00::context::header_context)
                {
                    const uint8_t* pFrameNanoepoch = pFrameStart;
                    const uint8_t* pFrameStreamId = pFrameNanoepoch + nanoepoch_raw_t::span();
                    const uint8_t* pFrameEventId = pFrameStreamId + stream_id_raw_t::span();
                    const uint8_t* pFrameEventType = pFrameEventId + event_id_raw_t::span();
                    const uint8_t* pFrameFooter = pFrameEventType + event_type_raw_t::span();
                    pFrameEnd = pFrameFooter + header_raw_t::span();
                    if (pFrameEnd > pBufEnd)
                        break;
                    local::payload::header_t::type_t footer;
                    if (!header_raw_t::convert(pFrameFooter, footer))
                        break;
                    if (footer != header)
                        break;
                    local::payload::nanoepoch_t::type_t nanoepoch;
                    local::payload::stream_id_t::type_t stream_id;
                    local::payload::event_id_t::type_t event_id;
                    local::payload::event_type_t::type_t event_type;
                    nanoepoch_raw_t::convert(pFrameNanoepoch, nanoepoch);
                    stream_id_raw_t::convert(pFrameStreamId, stream_id);
                    event_id_raw_t::convert(pFrameEventId, event_id);
                    event_type_raw_t::convert(pFrameEventType, event_type);

                    if (event_type > static_cast<decltype(event_type)>(local::payload::event_type_t::event_types::NO_CONTEXT) && event_type < static_cast<decltype(event_type)>(local::payload::event_type_t::event_types::_LAST))
                    {
                        m_consumer.consume_context(nanoepoch, stream_id, event_id, static_cast<local::payload::event_type_t::event_types>(event_type));
                    }
                    else
                    {
                        break; // unknown event type
                    }
                }
                else
                    break;
                pFrameStart = pFrameEnd;
            }
            // TODO: notify not consumed bytes
            return (pFrameStart - pBuf) == b;
        };
    };

    template <typename raw_encoding_t>
    struct frame_v00_serializer_consumer_stub_impl_t : public transport::consumer_stub_t, frame_v00_raw_traits_t<raw_encoding_t>
    {
        using transport::consumer_stub_t::consumer_stub_t;

        void consume_checkpoint(
            const local::payload::nanoepoch_t::type_t& nanoepoch
            , const local::payload::stream_id_t::type_t& stream_id
            , const local::payload::event_id_t::type_t& event_id
        ) final
        {
            std::array<uint8_t, max_buf_size > buf;
            const auto header = local::frame::v00::checkpoint::header;
            m_endpoint.consume(
                buf.data()
                , header_raw_t::convert(header
                    , event_id_raw_t::convert(event_id
                        , stream_id_raw_t::convert(stream_id
                            , nanoepoch_raw_t::convert(nanoepoch
                                , header_raw_t::convert(header
                                    , buf.data())))))
            );
        }

        void consume_context(
            const local::payload::nanoepoch_t::type_t& nanoepoch
            , const local::payload::stream_id_t::type_t& stream_id
            , const local::payload::event_id_t::type_t& event_id
            , const local::payload::event_type_t::event_types& event_type
        ) final
        {
            std::array<uint8_t, max_buf_size> buf;
            const auto header = local::frame::v00::context::header_context;

            m_endpoint.consume(
                buf.data()
                , header_raw_t::convert( header
                    , event_type_raw_t::convert(event_type
                        , event_id_raw_t::convert(event_id
                            , stream_id_raw_t::convert(stream_id
                                , nanoepoch_raw_t::convert(nanoepoch
                                    , header_raw_t::convert(header
                                        , buf.data()))))))
            );
        }
    };
}

namespace neutrino
{
    namespace impl
    {
        namespace transport
        {
            namespace frame_v00
            {
                std::shared_ptr<consumer_stub_t> create_consumer_stub(known_encodings_t ke, endpoint_t& endpoint)
                {
                    switch(ke)
                    {
                    case known_encodings_t::BINARY_NETWORK:
                        // TODO: implement this
                        //return std::shared_ptr<consumer_stub_t>(new frame_v00_serializer_consumer_stub_impl_t<serialized::network_byte_order_target_t>(endpoint));
                    case known_encodings_t::BINARY_NATIVE:
                        return std::shared_ptr<consumer_stub_t>(new frame_v00_serializer_consumer_stub_impl_t<serialized::native_byte_order_target_t>(endpoint));
                    case known_encodings_t::JSON:
                    default:
                        break;
                    }
                    return std::shared_ptr<consumer_stub_t>(new consumer_stub_t(endpoint));
                }

                std::shared_ptr<endpoint_impl_t> create_endpoint_impl(known_encodings_t ke, consumer_t& consumer)
                {
                    switch (ke)
                    {
                    case known_encodings_t::BINARY_NETWORK:
                        // TODO: implement this
                        //return std::shared_ptr<endpoint_impl_t>(new frame_v00_deserializer_endpoint_impl_t<serialized::network_byte_order_target_t>(consumer));
                    case known_encodings_t::BINARY_NATIVE:
                        return std::shared_ptr<endpoint_impl_t>(new frame_v00_deserializer_endpoint_impl_t<serialized::native_byte_order_target_t>(consumer));
                    case known_encodings_t::JSON:
                    default:
                        break;
                    }
                    return std::shared_ptr<endpoint_impl_t>(new endpoint_impl_t(consumer));
                }
            }
        }
    }
}
