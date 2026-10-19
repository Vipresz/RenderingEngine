#pragma once

#include "core/network/PasswordHasher.h"

#include <string>
#include <unordered_map>

namespace Engine {
namespace Net {

class CredentialStore {
public:
    void SetRequireCredentials(bool required) { m_requireCredentials = required; }
    bool RequiresCredentials() const { return m_requireCredentials || !m_users.empty(); }

    bool RegisterUser(const std::string& username, const std::string& password);
    bool Verify(const std::string& username, const std::string& password) const;
    bool HasUser(const std::string& username) const;

    bool LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path) const;

private:
    void OnUsersLoaded();
    struct StoredCredential {
        PasswordHashRecord record;
    };

    bool m_requireCredentials = false;
    std::unordered_map<std::string, StoredCredential> m_users;
};

} // namespace Net
} // namespace Engine
