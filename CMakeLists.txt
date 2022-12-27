cmake_minimum_required(VERSION 3.24)
project(jan_2023_coroutines)

set(CMAKE_CXX_STANDARD 23)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(jan_2023_coroutines main.cpp
        udp-socket.hpp
        result.hpp
        constants.hpp
        compute-crc.hpp)
target_link_libraries(jan_2023_coroutines
        CONAN_PKG::liburing
        CONAN_PKG::bshoshany-thread-pool
        CONAN_PKG::crc_cpp
        CONAN_PKG::fmt)
