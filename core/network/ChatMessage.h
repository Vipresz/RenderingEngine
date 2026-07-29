#pragma once

#include "core/network/NetBuffer.h"
#include "core/network/NetTypes.h"

#include <string>

namespace Engine {
namespace Net {

constexpr size_t kMaxChatMessageLength = 125;

struct ChatCommand {
    std::string text;
};

struct ChatMessage {
    UserId userId = kInvalidUserId;
    std::string displayName;
    std::string text;
    uint64_t tick = 0;
};

std::string SanitizeChatText(std::string text);
bool IsValidChatText(const std::string& text);

NetBuffer SerializeChatCommand(const ChatCommand& command);
bool DeserializeChatCommand(const NetBuffer& buffer, ChatCommand& out);

NetBuffer SerializeChatMessage(const ChatMessage& message);
bool DeserializeChatMessage(const NetBuffer& buffer, ChatMessage& out);

} // namespace Net
} // namespace Engine
