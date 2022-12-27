//
// Created by kobi on 12/26/22.
//

#ifndef JAN_2023_COROUTINES_COMPUTE_CRC_HPP
#define JAN_2023_COROUTINES_COMPUTE_CRC_HPP

#include <vector>

#include <crc_cpp.h>

#include "constants.hpp"

namespace example {
    [[nodiscard]] auto compute_crc16ccit(std::vector<std::byte> const &buffer) {
        static_assert(std::is_same_v<std::uint8_t, std::underlying_type<std::byte>::type>,
                "uint8_t need to be same as underlying of byte");
        crc_cpp::crc16_ccit crc;

        for (auto b: buffer) {
            const auto c = *(reinterpret_cast<std::uint8_t*>(&b));
            crc.update(c);
        }
        return crc.final();
    }
}

#endif //JAN_2023_COROUTINES_COMPUTE_CRC_HPP
