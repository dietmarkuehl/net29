// include/beman/net29/detail/poll_context.hpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_NET29_DETAIL_POLL_CONTEXT
#define INCLUDED_BEMAN_NET29_DETAIL_POLL_CONTEXT

// ----------------------------------------------------------------------------

#include <beman/net29/detail/netfwd.hpp>
#include <beman/net29/detail/container.hpp>
#include <beman/net29/detail/context_base.hpp>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

// ----------------------------------------------------------------------------

namespace beman::net29::detail
{
    struct poll_record;
    struct poll_context;
}

// ----------------------------------------------------------------------------

struct beman::net29::detail::poll_record final
{
    poll_record(::beman::net29::detail::native_handle_type h): handle(h) {}
    ::beman::net29::detail::native_handle_type                   handle;
    bool                                                   blocking{true};
};

// ----------------------------------------------------------------------------

struct beman::net29::detail::poll_context final
    : ::beman::net29::detail::context_base
{
    ::beman::net29::detail::container<::beman::net29::detail::poll_record> d_sockets;
    ::std::vector<::pollfd>     d_poll;
    ::std::vector<::beman::net29::detail::io_base*> d_outstanding;

    auto make_socket(int fd) -> ::beman::net29::detail::socket_id override final
    {
        return this->d_sockets.insert(fd);
    }
    auto make_socket(int d, int t, int p, ::std::error_code& error)
        -> ::beman::net29::detail::socket_id override final
    {
        int fd(::socket(d, t, p));
        if (fd < 0)
        {
            error = ::std::error_code(errno, ::std::system_category());
            return ::beman::net29::detail::socket_id::invalid;
        }
        return this->make_socket(fd);
    }
    auto release(::beman::net29::detail::socket_id id, ::std::error_code& error) -> void override final
    {
        ::beman::net29::detail::native_handle_type handle(this->d_sockets[id].handle);
        this->d_sockets.erase(id);
        if (::close(handle) < 0)
        {
            error = ::std::error_code(errno, ::std::system_category());
        }
    }
    auto native_handle(::beman::net29::detail::socket_id id) -> ::beman::net29::detail::native_handle_type override final
    {
        return this->d_sockets[id].handle;
    }
    auto set_option(::beman::net29::detail::socket_id id,
                     int level,
                     int name,
                     void const* data,
                     ::socklen_t size,
                     ::std::error_code& error) -> void override final
    {
        if (::setsockopt(this->native_handle(id), level, name, data, size) < 0)
        {
            error = ::std::error_code(errno, ::std::system_category());
        }
    }
    auto bind(::beman::net29::detail::socket_id id,
               ::beman::net29::detail::endpoint const& endpoint,
               ::std::error_code& error) -> void override final
    {
        if (::bind(this->native_handle(id), endpoint.data(), endpoint.size()) < 0)
        {
            error = ::std::error_code(errno, ::std::system_category());
        }
    }
    auto listen(::beman::net29::detail::socket_id id, int no, ::std::error_code& error) -> void override final
    {
        if (::listen(this->native_handle(id), no) < 0)
        {
            error = ::std::error_code(errno, ::std::system_category());
        }
    }

    auto run_one() -> ::std::size_t override final
    {
        if (this->d_poll.empty())
        {
            return ::std::size_t{};
        }
        while (true)
        {
            int rc(::poll(this->d_poll.data(), this->d_poll.size(), -1));
            if (rc < 0)
            {
                switch (errno)
                {
                default:
                    return ::std::size_t();
                case EINTR:
                case EAGAIN:
                    break;
                }
            }
            else
            {
                for (::std::size_t i(this->d_poll.size()); 0 < i--; )
                {
                    if (this->d_poll[i].revents & (this->d_poll[i].events | POLLERR))
                    {
                        ::beman::net29::detail::io_base* completion = this->d_outstanding[i];
                        if (i + 1u != this->d_poll.size())
                        {
                            this->d_poll[i] = this->d_poll.back();
                            this->d_outstanding[i] = this->d_outstanding.back();
                        }
                        this->d_poll.pop_back();
                        this->d_outstanding.pop_back();
                        completion->work(*this, completion);
                        return ::std::size_t(1);
                    }
                }
            }
        }
        return ::std::size_t{};
    }
    auto wakeup() -> void
    {
        //-dk:TODO wake-up polling thread
    }

    auto add_Outstanding(::beman::net29::detail::io_base* completion) -> bool
    {
        auto id{completion->id};
        if (this->d_sockets[id].blocking || !completion->work(*this, completion))
        {
            this->d_poll.emplace_back(::pollfd{this->native_handle(id), short(completion->event), short()});
            this->d_outstanding.emplace_back(completion);
            this->wakeup();
            return false;
        }
        return true;
    }

    auto cancel(::beman::net29::detail::io_base*, ::beman::net29::detail::io_base*) -> void override final
    {
        //-dk:TODO
    }
    auto accept(::beman::net29::detail::context_base::accept_operation* completion)
        -> bool override final
    {
        completion->work =
            [](::beman::net29::detail::context_base& ctxt, ::beman::net29::detail::io_base* comp)
            {
                auto id{comp->id};
                auto& completion(*static_cast<accept_operation*>(comp));

                while (true)
                {
                    int rc = ::accept(ctxt.native_handle(id), ::std::get<0>(completion).data(), &::std::get<1>(completion));
                    if (0 <= rc)
                    {
                        ::std::get<2>(completion) =  ctxt.make_socket(rc);
                        completion.complete();
                        return true;
                    }
                    else
                    {
                        switch (errno)
                        {
                        default:
                            completion.error(::std::error_code(errno, ::std::system_category()));
                            return true;
                        case EINTR:
                            break;
                        case EWOULDBLOCK:
                            return false;
                        }
                    }
                }
            };
        return this->add_Outstanding(completion);
    }
    auto connect(::beman::net29::detail::context_base::connect_operation*) -> bool override { return {}; /*-dk:TODO*/ } 
    auto receive(::beman::net29::detail::context_base::receive_operation*) -> bool override { return {}; /*-dk:TODO*/ }
    auto send(::beman::net29::detail::context_base::send_operation*) -> bool override { return {}; /*-dk:TODO*/ }
    auto resume_after(::beman::net29::detail::context_base::resume_after_operation*) -> bool override
    {
        //-dk:TODO
        return {};
    }
    auto resume_at(::beman::net29::detail::context_base::resume_at_operation*) -> bool override
    {
        //-dk:TODO
        return {};
    }
};

// ----------------------------------------------------------------------------

#endif