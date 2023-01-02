//
// Created by kobi on 12/29/22.
//

#ifndef JAN_2023_COROUTINES_IOURING_HPP
#define JAN_2023_COROUTINES_IOURING_HPP

#include <stdexcept>

#include "liburing.h"

namespace example {
    class IOUring {
    public:
        explicit IOUring(const std::size_t queue_size) {
            if (auto s = io_uring_queue_init(queue_size, &ring_, 0); s < 0) {
                throw std::runtime_error("error initializing io_uring: " + std::to_string(s));
            }
        }

        IOUring(const IOUring &) = delete;
        IOUring &operator=(const IOUring &) = delete;
        IOUring(IOUring &&) = delete;
        IOUring &operator=(IOUring &&) = delete;

        ~IOUring() { io_uring_queue_exit(&ring_); }

        [[nodiscard]]
        io_uring *get() {
            return &ring_;
        }

    private:
        io_uring ring_;
    };
}

#endif //JAN_2023_COROUTINES_IOURING_HPP
