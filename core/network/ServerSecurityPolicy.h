#pragma once

#include "core/network/CredentialStore.h"

#include <string>

namespace Engine {
namespace Net {

struct ServerSecurityPolicy {
    // Reject login when neither an auth token nor credential store is configured.
    bool enforceAuthentication = false;
    // Encrypt UDP payloads (movement/transforms) once a session token exists.
    bool encryptUdpPayloads = false;
    // Allow login without auth token when required token is unset (dev-only).
    bool allowOpenLogin = true;
};

inline bool IsAuthenticationConfigured(const std::string& requiredAuthToken,
                                     const CredentialStore& credentialStore) {
    return !requiredAuthToken.empty() || credentialStore.RequiresCredentials();
}

inline bool CanAcceptLogin(const ServerSecurityPolicy& policy,
                           const std::string& requiredAuthToken,
                           const CredentialStore& credentialStore) {
    if (!policy.enforceAuthentication) {
        return true;
    }
    return IsAuthenticationConfigured(requiredAuthToken, credentialStore);
}

} // namespace Net
} // namespace Engine
