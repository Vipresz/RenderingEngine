#pragma once

#include <chrono>
#include <cstdint>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace Engine {
namespace Tests {

// Returns an even TCP port in [20000, 40000) for loopback tests. Mix PID, time, and salt
// so parallel ctest workers rarely bind the same port.
inline uint16_t AllocateEphemeralTestPort(unsigned int salt = 0) {
#if defined(_WIN32)
    const uint32_t pid = static_cast<uint32_t>(::GetCurrentProcessId());
#else
    const uint32_t pid = static_cast<uint32_t>(::getpid());
#endif
    const uint32_t tick = static_cast<uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFu);
    const uint32_t mix = pid ^ (salt * 0x9E3779B9u) ^ (tick * 0x85EBCA6Bu);
    return static_cast<uint16_t>(20000u + ((mix % 10000u) * 2u));
}

} // namespace Tests
} // namespace Engine
