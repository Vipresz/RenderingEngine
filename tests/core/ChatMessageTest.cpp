#include <gtest/gtest.h>

#include "core/network/ChatMessage.h"
#include "core/network/MovementCommand.h"

TEST(ChatMessageTest, SanitizesControlCharactersAndTrimsWhitespace) {
    std::string dirty = "  hello\x01world  ";
    const std::string clean = Engine::Net::SanitizeChatText(std::move(dirty));
    EXPECT_EQ(clean, "helloworld");
    EXPECT_TRUE(Engine::Net::IsValidChatText(clean));
}

TEST(ChatMessageTest, RejectsEmptyMessages) {
    EXPECT_FALSE(Engine::Net::IsValidChatText(""));
    EXPECT_FALSE(Engine::Net::IsValidChatText(Engine::Net::SanitizeChatText("   ")));
}

TEST(ChatMessageTest, RoundTripsChatCommandAndMessage) {
    Engine::Net::ChatCommand command;
    command.text = "Need backup at spawn";

    Engine::Net::ChatCommand decodedCommand;
    ASSERT_TRUE(Engine::Net::DeserializeChatCommand(Engine::Net::SerializeChatCommand(command),
                                                    decodedCommand));
    EXPECT_EQ(decodedCommand.text, command.text);

    Engine::Net::ChatMessage message;
    message.userId = 42;
    message.displayName = "Astra";
    message.text = command.text;
    message.tick = 99;

    Engine::Net::ChatMessage decodedMessage;
    ASSERT_TRUE(Engine::Net::DeserializeChatMessage(Engine::Net::SerializeChatMessage(message),
                                                      decodedMessage));
    EXPECT_EQ(decodedMessage.userId, message.userId);
    EXPECT_EQ(decodedMessage.displayName, message.displayName);
    EXPECT_EQ(decodedMessage.text, message.text);
    EXPECT_EQ(decodedMessage.tick, message.tick);
}

TEST(ChatMessageTest, UsesSharedChatChannelName) {
    EXPECT_STREQ(Engine::Net::kChatChannel, "chat");
}

TEST(ChatMessageTest, TruncatesMessagesToMaxLength) {
    std::string longText(200, 'a');
    const std::string sanitized = Engine::Net::SanitizeChatText(std::move(longText));
    EXPECT_EQ(sanitized.size(), Engine::Net::kMaxChatMessageLength);
    EXPECT_TRUE(Engine::Net::IsValidChatText(sanitized));
}
