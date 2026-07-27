#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Engine {
namespace Net {

// Little-endian binary buffer for network payloads.
class NetBuffer {
public:
    NetBuffer() = default;
    explicit NetBuffer(std::vector<uint8_t> data) : m_data(std::move(data)) {}

    void Clear() {
        m_data.clear();
        m_readPos = 0;
    }

    const std::vector<uint8_t>& Data() const { return m_data; }
    std::vector<uint8_t>& Data() { return m_data; }

    size_t Size() const { return m_data.size(); }
    size_t ReadPos() const { return m_readPos; }
    size_t Remaining() const { return m_data.size() >= m_readPos ? m_data.size() - m_readPos : 0; }

    void ResetRead() { m_readPos = 0; }
    void SeekRead(size_t pos) { m_readPos = std::min(pos, m_data.size()); }

    void WriteBytes(const void* bytes, size_t count) {
        const auto* src = static_cast<const uint8_t*>(bytes);
        m_data.insert(m_data.end(), src, src + count);
    }

    bool ReadBytes(void* out, size_t count) {
        if (Remaining() < count) {
            return false;
        }
        std::memcpy(out, m_data.data() + m_readPos, count);
        m_readPos += count;
        return true;
    }

    void WriteU8(uint8_t value) { WriteBytes(&value, sizeof(value)); }
    void WriteU16(uint16_t value) { WriteLE(value); }
    void WriteU32(uint32_t value) { WriteLE(value); }
    void WriteU64(uint64_t value) { WriteLE(value); }
    void WriteI32(int32_t value) { WriteLE(static_cast<uint32_t>(value)); }
    void WriteFloat(float value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        WriteU32(bits);
    }

    bool ReadU8(uint8_t& value) { return ReadBytes(&value, sizeof(value)); }
    bool ReadU16(uint16_t& value) { return ReadLE(value); }
    bool ReadU32(uint32_t& value) { return ReadLE(value); }
    bool ReadU64(uint64_t& value) { return ReadLE(value); }
    bool ReadI32(int32_t& value) {
        uint32_t raw = 0;
        if (!ReadLE(raw)) {
            return false;
        }
        value = static_cast<int32_t>(raw);
        return true;
    }
    bool ReadFloat(float& value) {
        uint32_t bits = 0;
        if (!ReadU32(bits)) {
            return false;
        }
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    void WriteString(const std::string& value) {
        const uint32_t length = static_cast<uint32_t>(value.size());
        WriteU32(length);
        if (!value.empty()) {
            WriteBytes(value.data(), value.size());
        }
    }

    bool ReadString(std::string& value) {
        uint32_t length = 0;
        if (!ReadU32(length)) {
            return false;
        }
        if (length == 0) {
            value.clear();
            return true;
        }
        if (Remaining() < length) {
            return false;
        }
        value.assign(reinterpret_cast<const char*>(m_data.data() + m_readPos), length);
        m_readPos += length;
        return true;
    }

    void WriteBlob(const std::vector<uint8_t>& blob) {
        WriteU32(static_cast<uint32_t>(blob.size()));
        if (!blob.empty()) {
            WriteBytes(blob.data(), blob.size());
        }
    }

    bool ReadBlob(std::vector<uint8_t>& blob) {
        uint32_t length = 0;
        if (!ReadU32(length)) {
            return false;
        }
        if (length == 0) {
            blob.clear();
            return true;
        }
        if (Remaining() < length) {
            return false;
        }
        blob.assign(m_data.begin() + static_cast<std::ptrdiff_t>(m_readPos),
                    m_data.begin() + static_cast<std::ptrdiff_t>(m_readPos + length));
        m_readPos += length;
        return true;
    }

private:
    template <typename T>
    void WriteLE(T value) {
        uint8_t bytes[sizeof(T)];
        for (size_t i = 0; i < sizeof(T); ++i) {
            bytes[i] = static_cast<uint8_t>((static_cast<uint64_t>(value) >> (8 * i)) & 0xFFu);
        }
        WriteBytes(bytes, sizeof(T));
    }

    template <typename T>
    bool ReadLE(T& value) {
        uint8_t bytes[sizeof(T)];
        if (!ReadBytes(bytes, sizeof(T))) {
            return false;
        }
        uint64_t raw = 0;
        for (size_t i = 0; i < sizeof(T); ++i) {
            raw |= static_cast<uint64_t>(bytes[i]) << (8 * i);
        }
        value = static_cast<T>(raw);
        return true;
    }

    std::vector<uint8_t> m_data;
    size_t m_readPos = 0;
};

} // namespace Net
} // namespace Engine
