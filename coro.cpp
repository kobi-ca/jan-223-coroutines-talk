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
        result.sz_ = sz;
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
            ring_ = uring.get();
            constexpr auto offset = 0ULL;
            io_uring_prep_write(sqe_, fd, res.buffer_.data(), res.sz_, offset);
        }

        auto operator co_await () {
            struct Awaiter {
                io_uring_sqe *entry_{};
                io_uring* ring_{};
                write_ctx write_ctx_;
                explicit Awaiter(io_uring_sqe * const sqe, io_uring* const r) : entry_{sqe}, ring_(r) {}
                bool await_ready() { return false; }
                void await_suspend(std::coroutine_handle<> handle) noexcept {
                    write_ctx_.handle = handle;
                    io_uring_sqe_set_data(entry_, &write_ctx_);
                    io_uring_submit(ring_);
                }
                int await_resume() { return write_ctx_.status_code; }
            };
            return Awaiter{sqe_, ring_};
        }
    private:
        io_uring_sqe *sqe_{};
        io_uring *ring_{};
    };

    example::Task readUdpWriteFileAsync(example::udp_connection& udp,
                                        example::IOUring &uring) {
        auto res = co_await read_udp_socket(udp);
        auto fd = ::open(fmt::format("udp-crc-{}.txt", udp.port()).c_str(),
                        O_TRUNC | O_CREAT | O_WRONLY,
                         S_IRUSR | S_IWUSR);
        if (fd < 0) {
            fmt::print("error opening file {}\n", errno);
            ::fflush(nullptr);
            co_return {};
        }
        const auto status = co_await write_file_awaitable(uring, res, fd);
        if(!status) {
            fmt::print("error write_file_awaitable\n");
            co_return {};
        }
        ::close(fd);
        co_return res;
    }

    [[nodiscard]]
    bool all_done(const std::vector<example::Task> &tasks) {
        return std::all_of(tasks.cbegin(), tasks.cend(),
                           [](const auto &t) { return t.done(); });
    }

    [[nodiscard]]
    std::size_t consumeCQEntries(example::IOUring &uring) {
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
        io_uring_submit_and_wait(uring.get(), 1); // blocks if queue empty
        return consumeCQEntries(uring);
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
    example::IOUring uring{total_incoming_sockets};
    tasks.reserve(total_incoming_sockets);
    for(const auto& conn : connections) {
        auto& udp_conn = *conn;
        tasks.push_back(readUdpWriteFileAsync(udp_conn, uring));
    }

    while(!all_done(tasks)) {
        fmt::print("consume {}\n", consumeCQEntriesBlocking(uring));
    }

    for(const auto& res : tasks) {
        fmt::print("result crc {:#5x}\n", res.get_result().crc_ );
    }

    return 0;
}
