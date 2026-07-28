#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Engine {
namespace Net {

bool SecureCompareBytes(const uint8_t* a, const uint8_t* b, size_t length);
bool SecureCompare(const std::string& a, const std::string& b);

} // namespace Net
} // namespace Engine
