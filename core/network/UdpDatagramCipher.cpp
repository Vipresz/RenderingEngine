#include "core/network/UdpDatagramCipher.h"

#include "core/network/CryptoSha256.h"
#include "core/network/SecureCompare.h"

#include <cstring>

namespace Engine {
namespace Net {

namespace {

void WriteU64BE(uint8_t* out, uint64_t value) {
    for (int i = 7; i >= 0; --i) {
        out[7 - i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
}

void XorKeystream(const UdpCipherKey& key,
                  const uint8_t nonce[kUdpCipherNonceBytes],
                  uint64_t sequence,
                  uint8_t* data,
                  size_t dataSize) {
    uint8_t counterPrefix[8];
    WriteU64BE(counterPrefix, sequence);

    size_t offset = 0;
    uint32_t blockIndex = 0;
    while (offset < dataSize) {
        uint8_t blockInput[kUdpCipherNonceBytes + 8 + 4];
        std::memcpy(blockInput, nonce, kUdpCipherNonceBytes);
        std::memcpy(blockInput + kUdpCipherNonceBytes, counterPrefix, sizeof(counterPrefix));
        blockInput[kUdpCipherNonceBytes + 8] = static_cast<uint8_t>((blockIndex >> 24) & 0xFF);
        blockInput[kUdpCipherNonceBytes + 9] = static_cast<uint8_t>((blockIndex >> 16) & 0xFF);
        blockInput[kUdpCipherNonceBytes + 10] = static_cast<uint8_t>((blockIndex >> 8) & 0xFF);
        blockInput[kUdpCipherNonceBytes + 11] = static_cast<uint8_t>(blockIndex & 0xFF);

        uint8_t keystream[32];
        HmacSha256(key.bytes, sizeof(key.bytes), blockInput, sizeof(blockInput), keystream);

        const size_t chunk = (std::min)(dataSize - offset, sizeof(keystream));
        for (size_t i = 0; i < chunk; ++i) {
            data[offset + i] ^= keystream[i];
        }
        offset += chunk;
        ++blockIndex;
    }
}

void ComputeTag(const UdpCipherKey& key,
                const uint8_t nonce[kUdpCipherNonceBytes],
                uint64_t sequence,
                const uint8_t* ciphertext,
                size_t ciphertextSize,
                uint8_t outTag[kUdpCipherTagBytes]) {
    uint8_t sequenceBytes[8];
    WriteU64BE(sequenceBytes, sequence);

    std::vector<uint8_t> message;
    message.reserve(kUdpCipherNonceBytes + sizeof(sequenceBytes) + ciphertextSize);
    message.insert(message.end(), nonce, nonce + kUdpCipherNonceBytes);
    message.insert(message.end(), sequenceBytes, sequenceBytes + sizeof(sequenceBytes));
    message.insert(message.end(), ciphertext, ciphertext + ciphertextSize);

    uint8_t fullTag[32];
    HmacSha256(key.bytes, sizeof(key.bytes), message.data(), message.size(), fullTag);
    std::memcpy(outTag, fullTag, kUdpCipherTagBytes);
}

} // namespace

UdpCipherKey DeriveUdpCipherKey(uint64_t sessionToken, const std::string& authToken) {
    UdpCipherKey key{};
    uint8_t sessionBytes[8];
    WriteU64BE(sessionBytes, sessionToken);

    std::vector<uint8_t> material;
    material.insert(material.end(), sessionBytes, sessionBytes + sizeof(sessionBytes));
    material.insert(material.end(),
                     reinterpret_cast<const uint8_t*>(authToken.data()),
                     reinterpret_cast<const uint8_t*>(authToken.data()) + authToken.size());

    const char label[] = "renderingengine-udp-v1";
    material.insert(material.end(), label, label + sizeof(label) - 1);
    HmacSha256(sessionBytes,
               sizeof(sessionBytes),
               material.data(),
               material.size(),
               key.bytes);
    return key;
}

bool EncryptUdpPayload(const UdpCipherKey& key,
                       uint64_t sequence,
                       const uint8_t* plaintext,
                       size_t plaintextSize,
                       std::vector<uint8_t>& outEncrypted) {
    if (!plaintext && plaintextSize > 0) {
        return false;
    }

    uint8_t nonce[kUdpCipherNonceBytes];
    uint8_t nonceSeed[32];
    uint8_t sequenceBytes[8];
    WriteU64BE(sequenceBytes, sequence);
    HmacSha256(key.bytes, sizeof(key.bytes), sequenceBytes, sizeof(sequenceBytes), nonceSeed);
    std::memcpy(nonce, nonceSeed, kUdpCipherNonceBytes);

    outEncrypted.clear();
    outEncrypted.resize(kUdpCipherNonceBytes + plaintextSize + kUdpCipherTagBytes);
    std::memcpy(outEncrypted.data(), nonce, kUdpCipherNonceBytes);
    if (plaintextSize > 0) {
        std::memcpy(outEncrypted.data() + kUdpCipherNonceBytes, plaintext, plaintextSize);
    }
    XorKeystream(key, nonce, sequence, outEncrypted.data() + kUdpCipherNonceBytes, plaintextSize);
    ComputeTag(key,
               nonce,
               sequence,
               outEncrypted.data() + kUdpCipherNonceBytes,
               plaintextSize,
               outEncrypted.data() + kUdpCipherNonceBytes + plaintextSize);
    return true;
}

bool DecryptUdpPayload(const UdpCipherKey& key,
                       uint64_t sequence,
                       const uint8_t* encrypted,
                       size_t encryptedSize,
                       std::vector<uint8_t>& outPlaintext) {
    if (!encrypted || encryptedSize < kUdpCipherNonceBytes + kUdpCipherTagBytes) {
        return false;
    }

    const size_t ciphertextSize = encryptedSize - kUdpCipherNonceBytes - kUdpCipherTagBytes;
    const uint8_t* nonce = encrypted;
    const uint8_t* ciphertext = encrypted + kUdpCipherNonceBytes;
    const uint8_t* tag = encrypted + kUdpCipherNonceBytes + ciphertextSize;

    uint8_t expectedTag[kUdpCipherTagBytes];
    ComputeTag(key, nonce, sequence, ciphertext, ciphertextSize, expectedTag);
  if (!SecureCompareBytes(tag, expectedTag, kUdpCipherTagBytes)) {
        return false;
    }

    outPlaintext.assign(ciphertext, ciphertext + ciphertextSize);
    XorKeystream(key, nonce, sequence, outPlaintext.data(), outPlaintext.size());
    return true;
}

} // namespace Net
} // namespace Engine
