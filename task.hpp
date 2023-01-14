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
            ~promise_type() { fmt::print("promise ~\n"); }
            example::result result;

            Task get_return_object() { return Task(this); }
            void unhandled_exception() noexcept {}
            void return_value(example::result res) noexcept { result = res; }
            std::suspend_never initial_suspend() noexcept {
                fmt::print("in initial_suspend\n");
                return {};
            }
            std::suspend_always final_suspend() noexcept {
                fmt::print("in final_suspend\n");
                return {};
            }
        };

        explicit Task(promise_type *promise)
                : handle_{HandleT::from_promise(*promise)} {
            fmt::print("task ctor\n");
        }
        Task(Task &&other) noexcept : handle_{std::exchange(other.handle_, nullptr)} {}

        ~Task() {
            fmt::print("task~\n");
            if (handle_) {
                fmt::print("handle destroy in task~\n");
                handle_.destroy();
            }
        }

        [[nodiscard]]
        example::result get_result() const & {
            assert(handle_.done());
            return handle_.promise().result;
        }

        [[nodiscard]]
        bool done() const { return handle_.done(); }

    private:
        using HandleT = std::coroutine_handle<promise_type>;
        HandleT handle_;
    };
}

#endif //JAN_2023_COROUTINES_TASK_HPP
