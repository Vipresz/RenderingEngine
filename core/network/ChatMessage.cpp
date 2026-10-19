#include "core/network/ChatMessage.h"

#include <algorithm>
#include <cctype>

namespace Engine {
namespace Net {

namespace {

void TrimInPlace(std::string& text) {
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
    text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(), text.end());
}

} // namespace

std::string SanitizeChatText(std::string text) {
    text.erase(std::remove_if(text.begin(),
                              text.end(),
                              [](unsigned char ch) { return ch < 0x20 && ch != '\t'; }),
               text.end());
    if (text.size() > kMaxChatMessageLength) {
        text.resize(kMaxChatMessageLength);
    }
    TrimInPlace(text);
    return text;
}

bool IsValidChatText(const std::string& text) {
    return !text.empty() && text.size() <= kMaxChatMessageLength;
}

NetBuffer SerializeChatCommand(const ChatCommand& command) {
    NetBuffer buffer;
    buffer.WriteString(command.text);
    return buffer;
}

bool DeserializeChatCommand(const NetBuffer& buffer, ChatCommand& out) {
    NetBuffer copy = buffer;
    copy.ResetRead();
    if (!copy.ReadString(out.text)) {
        return false;
    }
    out.text = SanitizeChatText(std::move(out.text));
    return true;
}

NetBuffer SerializeChatMessage(const ChatMessage& message) {
    NetBuffer buffer;
    buffer.WriteU32(message.userId);
    buffer.WriteString(message.displayName);
    buffer.WriteString(message.text);
    buffer.WriteU64(message.tick);
    return buffer;
}

bool DeserializeChatMessage(const NetBuffer& buffer, ChatMessage& out) {
    NetBuffer copy = buffer;
    copy.ResetRead();
    if (!copy.ReadU32(out.userId) || !copy.ReadString(out.displayName) ||
        !copy.ReadString(out.text) || !copy.ReadU64(out.tick)) {
        return false;
    }
    return true;
}

} // namespace Net
} // namespace Engine
