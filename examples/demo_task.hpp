// examples/demo_task.hpp                                             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_EXAMPLES_DEMO_TASK
#define INCLUDED_EXAMPLES_DEMO_TASK

#include <beman/net29/net.hpp>
#include <exception>
#include <coroutine>
#include <tuple>
#include <optional>
#include <type_traits>

// ----------------------------------------------------------------------------

namespace demo
{
    namespace ex = ::beman::net29::detail::ex;

    template <typename Result = void>
    struct task
    {
        template <typename T>
        struct state_base
        {
            ::std::optional<T> result;
            virtual auto complete_value() -> void = 0;
            virtual auto complete_error(::std::exception_ptr) -> void = 0;
            virtual auto complete_stopped() -> void = 0;

            template <typename Receiver>
            auto set_value(Receiver& receiver)
            {
                ::beman::net29::detail::ex::set_value(
                    ::std::move(receiver),
                    ::std::move(*this->result)
                 );
            }
        };
        template <>
        struct state_base<void>
        {
            virtual auto complete_value() -> void = 0;
            virtual auto complete_error(::std::exception_ptr) -> void = 0;
            virtual auto complete_stopped() -> void = 0;

            template <typename Receiver>
            auto set_value(Receiver& receiver)
            {
                ::beman::net29::detail::ex::set_value(::std::move(receiver));
            }
        };

        template <typename Promise, ex::sender Sender>
        struct sender_awaiter
        {
            struct receiver
            {
                using receiver_concept = ex::receiver_t;

                Promise const*     env{};
                sender_awaiter* self{};

                template <typename... Args>
                auto set_value(Args&&... args) noexcept -> void
                {
                    this->self->result.emplace(::std::forward<Args>(args)...);
                    this->self->handle.resume();
                }
                template <typename Error>
                auto set_error(Error&& error) noexcept -> void
                {
                    if constexpr (::std::same_as<::std::decay_t<Error>, ::std::exception_ptr>)
                        this->self->error = error;
                    else
                        this->self->error = ::std::make_exception_ptr(::std::forward<Error>(error));
                    this->self->handle.resume();
                }
                auto set_stopped() noexcept -> void
                {
                    ::std::cout << "set_stopped()\n";
                    this->promise->state->complete_stopped();
                }
            };
            template <typename... T>
            struct single_or_tuple { using type = ::std::tuple<::std::decay_t<T>...>; };
            template <typename T>
            struct single_or_tuple<T> { using type = ::std::decay_t<T>; };
            template <typename... T>
            using single_or_tuple_t = typename single_or_tuple<T...>::type;
            using value_type
                = ex::value_types_of_t<
                    Sender, Promise, single_or_tuple_t, ::std::type_identity_t>;
            using state_type = decltype(ex::connect(::std::declval<Sender>(), std::declval<receiver>()));

            ::std::coroutine_handle<Promise> handle;
            ::std::exception_ptr             error;
            ::std::optional<value_type>      result;
            state_type                       state;

            sender_awaiter(Promise* promise, Sender sender)
                : state(ex::connect(::std::move(sender), receiver{promise, this}))
            {
            }

            auto await_ready() const noexcept -> bool { return false; }
            auto await_suspend(::std::coroutine_handle<Promise> handle) -> void
            {
                this->handle = handle;
                ex::start(this->state);
            }
            auto await_resume()
            {
                if (this->error)
                    std::rethrow_exception(this->error);
                return *this->result;
            }
        };
        template <typename E, typename S>
        sender_awaiter(E const*, S&&) -> sender_awaiter<E, ::std::remove_cvref_t<S>>;

        template <typename R>
        struct promise_result
        {
            state_base<R>* state{};
            template <typename T>
            auto return_value(T&& r) -> void
            {
                this->state->result.emplace(std::forward<T>(r));
                this->state->complete_value();
            }
        };
        template <>
        struct promise_result<void>
        {
            state_base<void>* state{};
            auto return_value() -> void
            {
                this->state->complete_value();
            }
        };
        struct promise_type
            : promise_result<Result>
        {
            auto initial_suspend() -> ::std::suspend_always { return {}; }
            auto final_suspend() noexcept -> ::std::suspend_always { return {}; }
            auto get_return_object()
            {
                return task{::std::coroutine_handle<promise_type>::from_promise(*this)};
            }
            auto unhandled_exception() -> void { std::terminate(); }
            auto await_transform(ex::sender auto && sender)
            {
                return sender_awaiter(this, sender);
            }
        };

        template <typename Receiver>
        struct state
            : state_base<::std::decay_t<Result>>
        {
            using operation_state_concept = ::beman::net29::detail::ex::operation_state_t;

            ::std::coroutine_handle<promise_type> handle;
            ::std::decay_t<Receiver>              receiver;

            template <typename R>
            state(::std::coroutine_handle<promise_type> handle, R&& receiver)
                : handle(::std::move(handle))
                , receiver(::std::forward<R>(receiver))
            {
            }

            auto start() & noexcept -> void
            {
                this->handle.promise().state = this;
                this->handle.resume();
            }

            auto complete_value() -> void override
            {
                this->set_value(this->receiver);
            }
            auto complete_error(::std::exception_ptr error) -> void override
            {
                ::beman::net29::detail::ex::set_error(
                    ::std::move(this->receiver),
                    ::std::move(error)
                );
            }
            auto complete_stopped() -> void override
            {
                ::beman::net29::detail::ex::set_stopped(::std::move(this->receiver));
            }
        };


        ::std::coroutine_handle<task<Result>::promise_type> handle;
        template <typename T>
        struct completion { using type = ::beman::net29::detail::ex::set_value_t(T); };
        template <>
        struct completion<void> { using type = ::beman::net29::detail::ex::set_value_t(); };

        using sender_concept = ::beman::net29::detail::ex::sender_t;
        using completion_signatures = ::beman::net29::detail::ex::completion_signatures<
            ::beman::net29::detail::ex::set_error_t(::std::exception_ptr),
            ::beman::net29::detail::ex::set_stopped_t(),
            typename completion<::std::decay_t<Result>>::type
        >;

        template <typename Receiver>
        auto connect(Receiver&& receiver)
        {
            return state<Receiver>(
                ::std::move(this->handle),
                ::std::forward<Receiver>(receiver)
            );
        }
    };
}

// ----------------------------------------------------------------------------

#endif