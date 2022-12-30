#include <vector>
#include <tuple>
#include <coroutine>
#include <thread>
#include <algorithm>

#include "fmt/format.h"

#include "result.hpp"
#include "task.hpp"
#include "udp-socket.hpp"
#include "compute-crc.hpp"

namespace {

    struct request_data {
        std::coroutine_handle<> handle;
        example::result result;
    };

    void readSync(example::udp_connection& udp, request_data& data) {
        example::result &result = data.result;
        const auto sz = udp.read(result.buffer_); // blocks
        const std::vector<std::byte> buffer(result.buffer_.cbegin(),
                                            result.buffer_.cbegin() + sz);
        result.crc_ = example::compute_crc16ccit(buffer);
        data.handle.resume();
    }

    class read_udp_socket {
    public:
        explicit read_udp_socket(example::udp_connection& udp) : udp_ {&udp} {}
        auto operator co_await() {
            struct Awaiter {
                example::udp_connection* udp_{};
                request_data data_;
                std::thread t_;
                explicit Awaiter(example::udp_connection* const udp) : udp_{udp} {}
                bool await_ready() { return false; }
                void await_suspend(std::coroutine_handle<> handle) noexcept {
                    data_.handle = handle;
                    t_ = std::thread(readSync, std::ref(*udp_), std::ref(data_));
                    t_.detach();
                }
                example::result await_resume() { return data_.result; }
            };
            return Awaiter{udp_};
        }
    private:
        example::udp_connection* udp_{};
    };

    example::Task readAsync(example::udp_connection& udp) {
        const auto res = co_await read_udp_socket(udp);
        co_return res;
    }

    [[nodiscard]]
    bool all_done(const std::vector<example::Task> &tasks) {
        return std::all_of(tasks.cbegin(), tasks.cend(),
                           [](const auto &t) { return t.done(); });
    }
}


int main() {
    constexpr auto total_incoming_sockets = 2uz;
    std::vector<std::unique_ptr<example::udp_connection>> connections;
    const std::tuple<uint16_t, uint16_t>
            start_end_ports_open_range{9090,
                                       9090 + total_incoming_sockets};
    for (auto [port, end] = start_end_ports_open_range; port < end; ++port) {
        connections.push_back(std::make_unique<example::udp_connection>(port));
    }
    std::vector<example::Task> tasks;
    tasks.reserve(total_incoming_sockets);
    for(const auto& conn : connections) {
        auto& udp_conn = *conn;
        tasks.push_back(readAsync(udp_conn));
    }

    while(!all_done(tasks)) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);
    }

    for(const auto& res : tasks) {
        fmt::print("result crc {:#5x}\n", res.get_result().crc_ );
    }

    return 0;
}
