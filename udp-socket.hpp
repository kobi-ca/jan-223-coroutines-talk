//
// Created by kobi on 12/26/22.
//

#ifndef JAN_2023_COROUTINES_UDP_SOCKET_HPP
#define JAN_2023_COROUTINES_UDP_SOCKET_HPP

#include <unistd.h>
#include <netinet/in.h>

#include <stdexcept>

#include "constants.hpp"

namespace example {

    class udp_connection final {
    public:
        explicit udp_connection(const uint16_t port) {
            sock_ = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock_ < 0) {
                sock_ = INVALID_SOCK;
                throw std::runtime_error("failed to open socket");
            }
            server_addr_.sin_family = AF_INET;
            server_addr_.sin_port = ::htons(port);
            server_addr_.sin_addr.s_addr = INADDR_ANY;
        }
        [[nodiscard]] auto fd() const noexcept { return sock_; }
        ~udp_connection() {
            if(sock_ != INVALID_SOCK) {
                ::close(sock_);
                sock_ = INVALID_SOCK;
            }
        }
        udp_connection(udp_connection&& udp) = delete;
        udp_connection& operator=(udp_connection&& udp) = delete;

        [[nodiscard]]
        std::size_t read(std::array<std::byte, BUFSIZE> &out) {
            socklen_t src_addr_out{};
            const int flags = 0;
            const auto res = recvfrom(sock_, out.data(), out.size(), flags,
                                      reinterpret_cast<sockaddr *>(&server_addr_),
                                      &src_addr_out);
            if (res <= 0) {
                throw std::runtime_error("failed to recvfrom");
            }
            return static_cast<std::size_t>(res);
        }
    private:
        static constexpr auto INVALID_SOCK = -1;
        sockaddr_in server_addr_{};
        int sock_{INVALID_SOCK};
    };

}

#endif //JAN_2023_COROUTINES_UDP_SOCKET_HPP
