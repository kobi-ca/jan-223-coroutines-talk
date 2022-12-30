//
// Created by kobi on 12/29/22.
//

#ifndef JAN_2023_COROUTINES_TASK_HPP
#define JAN_2023_COROUTINES_TASK_HPP

#include <coroutine>
#include <utility> // exchange
#include <cassert>

#include "result.hpp"

namespace example {
    class Task {
    public:
        struct promise_type {
            example::result result;

            Task get_return_object() { return Task(this); }
            void unhandled_exception() noexcept {}
            void return_value(example::result res) noexcept { result = std::move(res); }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
        };

        explicit Task(promise_type *promise)
                : handle_{HandleT::from_promise(*promise)} {}
        Task(Task &&other) noexcept : handle_{std::exchange(other.handle_, nullptr)} {}

        ~Task() {
            if (handle_) {
                handle_.destroy();
            }
        }

        [[nodiscard]]
        example::result get_result() const & {
            assert(handle_.done());
            return handle_.promise().result;
        }

        example::result&& get_result() && {
            assert(handle_.done());
            return std::move(handle_.promise().result);
        }

        [[nodiscard]]
        bool done() const { return handle_.done(); }

        using HandleT = std::coroutine_handle<promise_type>;
        HandleT handle_;
    };
}

#endif //JAN_2023_COROUTINES_TASK_HPP
