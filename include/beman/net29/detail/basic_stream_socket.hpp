// include/beman/net29/detail/basic_stream_socket.hpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_NET29_DETAIL_BASIC_STREAM_SOCKET
#define INCLUDED_BEMAN_NET29_DETAIL_BASIC_STREAM_SOCKET

// ----------------------------------------------------------------------------

#include <beman/net29/detail/netfwd.hpp>
#include <beman/net29/detail/io_context.hpp>
#include <beman/net29/detail/basic_socket.hpp>
#include <functional>
#include <system_error>

// ----------------------------------------------------------------------------

template <typename Protocol>
class beman::net29::basic_stream_socket
    : public basic_socket<Protocol>
{
public:
    using native_handle_type = ::beman::net29::detail::native_handle_type;
    using protocol_type = Protocol;
    using endpoint_type = typename protocol_type::endpoint;

private:
    endpoint_type d_endpoint;

public:
    basic_stream_socket(basic_stream_socket&&) = default;
    basic_stream_socket& operator= (basic_stream_socket&&) = default;
    basic_stream_socket(::beman::net29::detail::context_base* context, ::beman::net29::detail::socket_id id)
        : basic_socket<Protocol>(context, id)
    {
    }
    basic_stream_socket(::beman::net29::io_context& context, endpoint_type const& endpoint)
        : beman::net29::basic_socket<Protocol>(context.get_scheduler().get_context(),
            ::std::invoke([p = endpoint.protocol(), &context]{
                ::std::error_code error{};
                auto rc(context.make_socket(p.family(), p.type(), p.protocol(), error));
                if (error)
                {
                    throw ::std::system_error(error);
                }
                return rc;
            }))
        , d_endpoint(endpoint) 
    {
    }

    auto get_endpoint() const -> endpoint_type { return this->d_endpoint; }
};


// ----------------------------------------------------------------------------

#endif
