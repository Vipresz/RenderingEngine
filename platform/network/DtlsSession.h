#pragma once

// UDP confidentiality for movement/transforms is provided by UdpDatagramCipher once a
// session token is established over TCP (optionally protected by TLS). Full DTLS
// handshake support can be layered on top of UdpChannel when ENGINE_ENABLE_TLS is set.

#include "network/TlsSession.h"

namespace Engine {
namespace Platform {
namespace Networking {

struct DtlsConfig {
    bool enabled = false;
    TlsConfig transportTls;
};

} // namespace Networking
} // namespace Platform
} // namespace Engine
