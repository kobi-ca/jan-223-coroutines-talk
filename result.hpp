//
// Created by kobi on 12/26/22.
//

#ifndef JAN_2023_COROUTINES_RESULT_HPP
#define JAN_2023_COROUTINES_RESULT_HPP

#include <array>
#include <cstdint>

#include "constants.hpp"

namespace example {
    struct result final {
        std::array<std::byte, BUFSIZE> buffer_{};
        std::uint16_t crc_{};
        [[nodiscard]] static auto capacity() noexcept { return BUFSIZE; }
    };
}

#endif //JAN_2023_COROUTINES_RESULT_HPP
