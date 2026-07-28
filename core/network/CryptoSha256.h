#pragma once

#include <cstddef>
#include <cstdint>

namespace Engine {
namespace Net {

void Sha256(const uint8_t* data, size_t length, uint8_t out[32]);
void HmacSha256(const uint8_t* key, size_t keyLength, const uint8_t* message, size_t messageLength, uint8_t out[32]);

} // namespace Net
} // namespace Engine
