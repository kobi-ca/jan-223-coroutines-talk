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
#include "iouring.hpp"
#include "utils.hpp"

namespace {

    struct request_data {
        std::coroutine_handle<> handle;
        example::result result;
    };

    void readSync(example::udp_connection& udp, request_data& data) {
        fmt::print("{} readsync\n", gettid());
        example::result &result = data.result;
        const auto sz = udp.read(result.buffer_); // blocks
        const std::vector<std::byte> buffer(result.buffer_.cbegin(),
                                            result.buffer_.cbegin() + sz);
        result.crc_ = example::compute_crc16ccit(buffer);
        result.sz_ = sz;
        data.handle.resume();
    }

    class read_udp_socket {
    public:
        explicit read_udp_socket(example::udp_connection& udp) : udp_ {&udp} {}
        auto operator co_await() {
            struct Awaiter {
                ~Awaiter() { fmt::print("{} read awaiter ~\n", gettid()); }
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

    struct write_ctx final {
        std::coroutine_handle<> handle;
        int status_code{-1};
    };

    class write_file_awaitable {
    public:
        write_file_awaitable(example::IOUring& uring,
                             example::result& res,
                             const int fd) {
            sqe_ = io_uring_get_sqe(uring.get());
            constexpr auto offset = 0ULL;
            fmt::print("{} submitting\n", gettid());
            io_uring_prep_write(sqe_, fd, res.buffer_.data(), res.sz_, offset);
        }

        auto operator co_await () {
            struct Awaiter {
                ~Awaiter() { fmt::print("{} write awaiter ~\n", gettid()); }
                io_uring_sqe *entry_{};
                write_ctx write_ctx_;
                explicit Awaiter(io_uring_sqe * const sqe) : entry_{sqe} {}
                bool await_ready() { return false; }
                void await_suspend(std::coroutine_handle<> handle) noexcept {
                    write_ctx_.handle = handle;
                    fmt::print("{} setting data\n", gettid());
                    io_uring_sqe_set_data(entry_, &write_ctx_);
                }
                int await_resume() { return write_ctx_.status_code; }
            };
            return Awaiter{sqe_};
        }
    private:
        io_uring_sqe *sqe_;
    };

    example::Task readUdpWriteFileAsync(example::udp_connection& udp,
                                        example::IOUring &uring) {
        fmt::print("{} starting readUdp\n", gettid());
        auto res = co_await read_udp_socket(udp);
        auto fd = ::open(fmt::format("udp-crc-{}.txt", udp.port()).c_str(),
                        O_TRUNC | O_CREAT | O_WRONLY,
                         S_IRUSR | S_IWUSR);
        if (fd < 0) {
            fmt::print("error opening file {}\n", errno);
            ::fflush(nullptr);
            co_return {};
        }
        fmt::print("{} before co_await write file\n", gettid());
        const auto status = co_await write_file_awaitable(uring, res, fd);
        if(!status) {
            fmt::print("error write_file_awaitable\n");
            co_return {};
        }
        ::close(fd);
        fmt::print("{} exiting readWriteFileAsync\n", gettid());
        co_return res;
    }

    [[nodiscard]]
    bool all_done(const std::vector<example::Task> &tasks) {
        bool done1{}, done2{};
        if (tasks[0].done()) {
            fmt::print("task 0 is done\n");
            done1 = true;
        }
        if (tasks[1].done()) {
            fmt::print("task 1 is done\n");
            done2 = true;
        }
        return done1 && done2;
//        return std::all_of(tasks.cbegin(), tasks.cend(),
//                           [](const auto &t) { return t.done(); });
    }

    [[nodiscard]]
    std::size_t consumeCQEntries(example::IOUring &uring) {
        fmt::print("consumecqentry\n");
        int processed{0};
        io_uring_cqe *cqe{};
        unsigned int head; // head of the ring buffer, unused
        io_uring_for_each_cqe(uring.get(), head, cqe) {
            auto *ctx = static_cast<write_ctx *>(io_uring_cqe_get_data(cqe));
            assert(ctx);
            // make sure to set the status code before resuming the coroutine
            ctx->status_code = cqe->res;
            ctx->handle.resume(); // await_resume is called here
            ++processed;
        }
        io_uring_cq_advance(uring.get(), processed);
        return processed;
    }

    [[nodiscard]]
    auto consumeCQEntriesBlocking(example::IOUring &uring) {
        fmt::print("{} start sleepinng\n", gettid());
        io_uring_submit_and_wait(uring.get(), 1); // blocks if queue empty
        return consumeCQEntries(uring);
    }

    [[nodiscard]]
    auto consumeCQEntriesNonBlocking(example::IOUring &uring) {
        io_uring_cqe *temp;
        //fmt::print("start peeking\n");
        auto res = io_uring_peek_cqe(uring.get(), &temp);
        fmt::print("res = {}\n", res);
        if (res == 0) {
            return consumeCQEntries(uring);
        }
        return std::size_t{};
    }
}


int main() {
    fmt::print("{} main\n", gettid());
    constexpr auto total_incoming_sockets = 2uz;
    std::vector<std::unique_ptr<example::udp_connection>> connections;
    const std::tuple<uint16_t, uint16_t>
            start_end_ports_open_range{9090,
                                       9090 + total_incoming_sockets};
    for (auto [port, end] = start_end_ports_open_range; port < end; ++port) {
        connections.push_back(std::make_unique<example::udp_connection>(port));
    }
    std::vector<example::Task> tasks;
    example::IOUring uring{total_incoming_sockets};
    tasks.reserve(total_incoming_sockets);
    for(const auto& conn : connections) {
        auto& udp_conn = *conn;
        fmt::print("{} creating task\n", gettid());
        auto&& t = readUdpWriteFileAsync(udp_conn, uring);
        fmt::print("{} pushing task\n", gettid());
        tasks.push_back(std::move(t));
    }

    using namespace std::chrono_literals;
    //std::this_thread::sleep_for(5s);
    io_uring_submit(uring.get());
    while(!all_done(tasks)) {
        std::this_thread::sleep_for(1s);
        //fmt::print("consume {}\n", consumeCQEntriesBlocking(uring));
        std::ignore = consumeCQEntriesNonBlocking(uring);
    }

    for(const auto& res : tasks) {
        fmt::print("result crc {:#5x}\n", res.get_result().crc_ );
    }

    return 0;
}
