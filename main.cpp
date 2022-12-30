#include <vector>
#include <tuple>
#include <memory>

#include "fmt/format.h"

#include "result.hpp"
#include "udp-socket.hpp"
#include "compute-crc.hpp"

[[nodiscard]]
example::result readSync(example::udp_connection& udp) {
    example::result result;
    const auto sz = udp.read(result.buffer_); // blocks
    const std::vector<std::byte> buffer(result.buffer_.cbegin(),
                                        result.buffer_.cbegin() + sz);
    result.crc_ = example::compute_crc16ccit(buffer);
    return result;
}

int main() {
    std::vector<example::result> results;
    constexpr auto total_incoming_sockets = 2uz;
    std::vector<std::unique_ptr<example::udp_connection>> connections;
    const std::tuple<uint16_t, uint16_t>
            start_end_ports_open_range{9090,
                                       9090 + total_incoming_sockets};
    for (auto [port, end] = start_end_ports_open_range; port < end; ++port) {
        connections.push_back(std::make_unique<example::udp_connection>(port));
    }
    results.reserve(total_incoming_sockets);
    for(const auto& conn : connections) {
        auto& udp_conn = *conn;
        results.push_back(readSync(udp_conn));
    }
    for(const auto& res : results) {
        fmt::print("result crc {:#5x}\n", res.crc_ );
    }

    return 0;
}
