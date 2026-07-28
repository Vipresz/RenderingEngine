#include "core/network/CredentialStore.h"

#include <fstream>
#include <sstream>

namespace Engine {
namespace Net {

namespace {

std::string EncodeHex(const std::vector<uint8_t>& bytes) {
    static const char* kHex = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(bytes.size() * 2);
    for (uint8_t byte : bytes) {
        encoded.push_back(kHex[(byte >> 4) & 0x0F]);
        encoded.push_back(kHex[byte & 0x0F]);
    }
    return encoded;
}

bool DecodeHex(const std::string& encoded, std::vector<uint8_t>& out) {
    if (encoded.size() % 2 != 0) {
        return false;
    }

    out.clear();
    out.reserve(encoded.size() / 2);
    auto hexValue = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };

    for (size_t i = 0; i < encoded.size(); i += 2) {
        const int high = hexValue(encoded[i]);
        const int low = hexValue(encoded[i + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        out.push_back(static_cast<uint8_t>((high << 4) | low));
    }
    return true;
}

} // namespace

bool CredentialStore::RegisterUser(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) {
        return false;
    }

    StoredCredential credential;
    credential.record = PasswordHasher::HashPassword(password);
    m_users[username] = std::move(credential);
    m_requireCredentials = true;
    return true;
}

bool CredentialStore::Verify(const std::string& username, const std::string& password) const {
    const auto it = m_users.find(username);
    if (it == m_users.end()) {
        return false;
    }
    return PasswordHasher::VerifyPassword(password, it->second.record);
}

bool CredentialStore::HasUser(const std::string& username) const {
    return m_users.find(username) != m_users.end();
}

bool CredentialStore::LoadFromFile(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    m_users.clear();
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const size_t firstColon = line.find(':');
        const size_t secondColon = line.find(':', firstColon == std::string::npos ? 0 : firstColon + 1);
        if (firstColon == std::string::npos || secondColon == std::string::npos) {
            continue;
        }

        const std::string username = line.substr(0, firstColon);
        const std::string saltHex = line.substr(firstColon + 1, secondColon - firstColon - 1);
        const std::string hashHex = line.substr(secondColon + 1);

        StoredCredential credential;
        if (!DecodeHex(saltHex, credential.record.salt) || !DecodeHex(hashHex, credential.record.hash)) {
            continue;
        }
        m_users[username] = std::move(credential);
    }

    OnUsersLoaded();
    return !m_users.empty();
}

void CredentialStore::OnUsersLoaded() {
    if (!m_users.empty()) {
        m_requireCredentials = true;
    }
}

bool CredentialStore::SaveToFile(const std::string& path) const {
    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "# username:salt_hex:hash_hex\n";
    for (const auto& entry : m_users) {
        output << entry.first << ':' << EncodeHex(entry.second.record.salt) << ':'
               << EncodeHex(entry.second.record.hash) << '\n';
    }
    return true;
}

} // namespace Net
} // namespace Engine
