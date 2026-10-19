#pragma once

#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Engine {
namespace Tests {

inline std::filesystem::path GetExecutableDirectory() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(std::string(buffer, length)).parent_path();
#else
    return {};
#endif
}

inline bool SetWorkingDirectoryToExecutable() {
#ifdef _WIN32
    const std::filesystem::path exeDir = GetExecutableDirectory();
    if (exeDir.empty()) {
        return false;
    }
    std::error_code ec;
    std::filesystem::current_path(exeDir, ec);
    return !ec;
#else
    return true;
#endif
}

} // namespace Tests
} // namespace Engine
