#include "core/network/SecureCompare.h"

#include <cstring>

namespace Engine {
namespace Net {

bool SecureCompareBytes(const uint8_t* a, const uint8_t* b, size_t length) {
    if (!a || !b) {
        return length == 0;
    }

    uint8_t diff = 0;
    for (size_t i = 0; i < length; ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

bool SecureCompare(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    return SecureCompareBytes(reinterpret_cast<const uint8_t*>(a.data()),
                            reinterpret_cast<const uint8_t*>(b.data()),
                            a.size());
}

} // namespace Net
} // namespace Engine
