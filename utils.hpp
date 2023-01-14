//
// Created by kobi on 1/14/23.
//

#ifndef JAN_2023_COROUTINES_UTILS_HPP
#define JAN_2023_COROUTINES_UTILS_HPP

#include <thread>

namespace example {
    auto gettid() {
        return std::this_thread::get_id();
    }
}

#endif //JAN_2023_COROUTINES_UTILS_HPP
