//
// Created by kobi on 1/2/23.
//

#include <cstdint>
#include <coroutine>
#include <utility>

#include "fmt/format.h"

template <typename T>
struct Generator final {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    handle_type coro_;

    explicit Generator(handle_type h) : coro_{h} {}
    ~Generator() { if (coro_) { coro_.destroy(); }}
    Generator(const Generator&) = delete;
    Generator& operator = (const Generator&) = delete;
    Generator(Generator&& c) noexcept :
        coro_{std::exchange(c.coro_, nullptr)} {}
    [[nodiscard]]
    T get() const {
        coro_.resume();
        return coro_.promise().count_;
    }

    struct promise_type final {
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        auto get_return_object() { return Generator{ handle_type::from_promise(*this) }; }
        std::suspend_always yield_value(int value) { count_ = value; return {}; }
        void return_void() {}
        void unhandled_exception() {
            std::exit(1);
        }
        T count_{};
    };
};

Generator<std::size_t> compute() {
    auto value = 17;
    for(;;) {
        co_yield value;
        value += 10;
    }
}

int main() {
    auto generator = compute();
    fmt::print("{}\n", generator.get());
    fmt::print("{}\n", generator.get());
    fmt::print("{}\n", generator.get());
}