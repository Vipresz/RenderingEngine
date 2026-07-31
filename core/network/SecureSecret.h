#pragma once

#include <cstddef>
#include <string>

namespace Engine {
namespace Net {

inline void SecureZeroMemory(void* data, size_t size) {
    if (!data || size == 0) {
        return;
    }
    volatile unsigned char* bytes = static_cast<volatile unsigned char*>(data);
    while (size-- > 0) {
        *bytes++ = 0;
    }
}

inline void SecureZeroString(std::string& value) {
    if (!value.empty()) {
        SecureZeroMemory(value.data(), value.size());
    }
    value.clear();
    value.shrink_to_fit();
}

} // namespace Net
} // namespace Engine
