#include <atomic>
#include <chrono>
#include <functional>
#include <gtest/gtest.h>
#include <thread>

#include "TestPort.h"
#include "core/network/ChatMessage.h"
#include "core/network/InventoryAction.h"
#include "core/network/MovementCommand.h"
#include "network/AuthoritativeGameSimulator.h"
#include "network/NetClient.h"
#include "network/NetServer.h"

using Engine::Platform::Networking::AuthoritativeGameSimulator;
using Engine::Platform::Networking::ClientCallbacks;
using Engine::Platform::Networking::NetClient;
using Engine::Platform::Networking::NetServer;

namespace {

constexpr const char* kHost = "127.0.0.1";

bool WaitUntil(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

Engine::Net::GenericDataPayload MakeInventoryActionPayload(Engine::Net::InventoryActionType action,
                                                         uint16_t slotIndex) {
    Engine::Net::InventoryActionCommand command;
    command.action = action;
    command.slotIndex = slotIndex;

    Engine::Net::GenericDataPayload payload;
    payload.channel = Engine::Net::kInventoryActionChannel;
    payload.data = Engine::Net::SerializeInventoryAction(command).Data();
    return payload;
}

} // namespace

TEST(AuthoritativeGameSimulatorChannelsTest, UseReducesStackByOne) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);
    simulator.SetInventoryActionValidator(
        [](Engine::Net::InventoryActionType, const std::string& itemId) { return itemId == "maple"; });

    ASSERT_TRUE(server.Start(kHost, 0, 0));
    simulator.OnClientLoggedIn(1, "player");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "maple", 10));

    Engine::Net::GenericDataPayload payload = MakeInventoryActionPayload(
        Engine::Net::InventoryActionType::Use, 0);
    ASSERT_TRUE(simulator.HandleGenericData(1, payload));
    simulator.Update(0.016f);

    EXPECT_EQ(simulator.CountOwnedItem(1, "maple"), 9u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorChannelsTest, DropRemovesEntireStack) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);

    ASSERT_TRUE(server.Start(kHost, 0, 0));
    simulator.OnClientLoggedIn(1, "player");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 10));

    Engine::Net::GenericDataPayload payload = MakeInventoryActionPayload(
        Engine::Net::InventoryActionType::Drop, 0);
    ASSERT_TRUE(simulator.HandleGenericData(1, payload));
    simulator.Update(0.016f);

    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 0u);
    EXPECT_FALSE(simulator.HasOwnedItem(1, "gold_coin", 1));

    server.Stop();
}

TEST(AuthoritativeGameSimulatorChannelsTest, RejectsUseWhenValidatorFails) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);
    simulator.SetInventoryActionValidator(
        [](Engine::Net::InventoryActionType, const std::string& itemId) { return itemId == "maple"; });

    ASSERT_TRUE(server.Start(kHost, 0, 0));
    simulator.OnClientLoggedIn(1, "player");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "gold_coin", 5));

    Engine::Net::GenericDataPayload payload = MakeInventoryActionPayload(
        Engine::Net::InventoryActionType::Use, 0);
    ASSERT_TRUE(simulator.HandleGenericData(1, payload));
    simulator.Update(0.016f);

    EXPECT_EQ(simulator.CountOwnedItem(1, "gold_coin"), 5u);

    server.Stop();
}

TEST(AuthoritativeGameSimulatorChannelsTest, BagLayoutStillValidAfterUse) {
    NetServer server;
    AuthoritativeGameSimulator simulator(server);
    simulator.SetInventoryActionValidator(
        [](Engine::Net::InventoryActionType, const std::string& itemId) { return itemId == "maple"; });

    ASSERT_TRUE(server.Start(kHost, 0, 0));
    simulator.OnClientLoggedIn(1, "player");
    simulator.Update(0.016f);

    ASSERT_TRUE(simulator.GrantOwnedItem(1, "maple", 10));

    Engine::Net::GenericDataPayload usePayload = MakeInventoryActionPayload(
        Engine::Net::InventoryActionType::Use, 0);
    ASSERT_TRUE(simulator.HandleGenericData(1, usePayload));
    simulator.Update(0.016f);
    EXPECT_EQ(simulator.CountOwnedItem(1, "maple"), 9u);

    Engine::Net::ReplicatedItemOwnership rearranged;
    rearranged.ownerUserId = 1;
    Engine::Net::ReplicatedOwnedItem slot;
    slot.slotIndex = 3;
    slot.itemId = "maple";
    slot.quantity = 9;
    rearranged.items.push_back(slot);

    Engine::Net::GenericDataPayload layoutPayload;
    layoutPayload.channel = Engine::Net::kItemOwnershipChannel;
    layoutPayload.data = rearranged.Serialize().Data();
    ASSERT_TRUE(simulator.HandleGenericData(1, layoutPayload));
    simulator.Update(0.016f);

    const Engine::Net::ReplicatedItemOwnership* ownership = simulator.GetOwnedItems(1);
    ASSERT_NE(ownership, nullptr);
    ASSERT_EQ(ownership->items.size(), 1u);
    EXPECT_EQ(ownership->items[0].slotIndex, 3u);
    EXPECT_EQ(ownership->items[0].quantity, 9u);

    server.Stop();
}

class AuthoritativeSimulatorFixture : public ::testing::Test {
protected:
    void SetUp() override {
        tcpPort = Engine::Tests::AllocateEphemeralTestPort(40);
        udpPort = static_cast<uint16_t>(tcpPort + 1);

        server.SetAuthoritativeMode(true);
        simulator = std::make_unique<AuthoritativeGameSimulator>(server);

        server.SetClientLoggedInCallback([this](Engine::Net::UserId userId, const std::string& displayName) {
            if (simulator) {
                simulator->OnClientLoggedIn(userId, displayName);
            }
        });
        server.SetClientDisconnectedCallback([this](Engine::Net::UserId userId) {
            if (simulator) {
                simulator->OnClientDisconnected(userId);
            }
        });
        server.SetGenericDataInterceptor([this](Engine::Net::UserId userId,
                                                Engine::Net::GenericDataPayload& payload) {
            return simulator && simulator->HandleGenericData(userId, payload);
        });

        ASSERT_TRUE(server.Start(kHost, tcpPort, udpPort));

        running = true;
        tickThread = std::thread([this]() {
            constexpr float kTickSeconds = 1.0f / 60.0f;
            while (running.load(std::memory_order_acquire)) {
                if (simulator) {
                    simulator->Update(kTickSeconds);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        });
    }

    void TearDown() override {
        running = false;
        if (tickThread.joinable()) {
            tickThread.join();
        }
        server.Stop();
        simulator.reset();
    }

    NetServer server;
    std::unique_ptr<AuthoritativeGameSimulator> simulator;
    uint16_t tcpPort = 0;
    uint16_t udpPort = 0;
    std::atomic<bool> running{false};
    std::thread tickThread;
};

TEST_F(AuthoritativeSimulatorFixture, ChatBroadcastsToConnectedClients) {
    NetClient sender;
    NetClient observer;
    std::atomic<bool> observerSawChat{false};
    std::string observedText;

    ClientCallbacks observerCallbacks;
    observerCallbacks.onGenericData = [&](const Engine::Net::GenericDataPayload& payload) {
        if (payload.channel != Engine::Net::kChatChannel) {
            return;
        }
        Engine::Net::ChatMessage message;
        if (!Engine::Net::DeserializeChatMessage(Engine::Net::NetBuffer(payload.data), message)) {
            return;
        }
        observedText = message.text;
        observerSawChat = true;
    };
    observer.SetCallbacks(std::move(observerCallbacks));

    ASSERT_TRUE(observer.Connect(kHost, tcpPort));
    ASSERT_TRUE(observer.Login("observer"));
    ASSERT_TRUE(sender.Connect(kHost, tcpPort));
    ASSERT_TRUE(sender.Login("sender"));

    Engine::Net::ChatCommand command;
    command.text = "hello zone";
    Engine::Net::GenericDataPayload payload;
    payload.channel = Engine::Net::kChatChannel;
    payload.data = Engine::Net::SerializeChatCommand(command).Data();
    ASSERT_TRUE(sender.SendGenericData(payload));

    ASSERT_TRUE(WaitUntil(
        [&]() {
            sender.Poll();
            observer.Poll();
            return observerSawChat.load();
        },
        std::chrono::seconds(3)));
    EXPECT_EQ(observedText, "hello zone");

    sender.Disconnect();
    observer.Disconnect();
}
