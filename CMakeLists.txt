cmake_minimum_required(VERSION 3.24)
project(jan_2023_coroutines)

set(CMAKE_CXX_STANDARD 23)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(jan_2023_coroutines sync.cpp
        udp-socket.hpp
        result.hpp
        constants.hpp
        compute-crc.hpp)
target_link_libraries(jan_2023_coroutines
        PRIVATE
        CONAN_PKG::crc_cpp
        CONAN_PKG::fmt)

add_executable(coro coro.cpp
        udp-socket.hpp
        result.hpp
        constants.hpp
        compute-crc.hpp
        task.hpp
        iouring.hpp)
target_link_libraries(coro
        PRIVATE
        uring
        CONAN_PKG::crc_cpp
        CONAN_PKG::fmt)

add_executable(gen generator.cpp)
target_link_libraries(gen
        PRIVATE
        CONAN_PKG::fmt)

add_executable(immediate-ret-generator immediate-ret-generator.cpp)
target_link_libraries(immediate-ret-generator PRIVATE CONAN_PKG::fmt)
