cmake_minimum_required(VERSION 3.22)

include(FetchContent)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

set(ENGINE_TEST_SUPPORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/scene/Scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/scene/Prefab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/scene/SceneSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/CharacterMovementSimulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/PasswordHasher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/CryptoSha256.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/SecureCompare.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/UdpMessageAuth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/CredentialStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/ScalePolicyConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/LayerInstancePolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/ReplicationTier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/ZoneScaleConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/ecs/PlayerItemOwnership.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/ecs/PlayerTrade.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/ItemOwnershipReplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/TradeReplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/TradeSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/ChatMessage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/terrain/HeightmapTerrain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/InterestManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/RateLimiter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/ServerAuthority.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/DatagramSocket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/UdpChannel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/Socket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/NetworkConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/NetClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/NetServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/TlsSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/systems/ReplicationSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/ItemOwnershipReplicationBinding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/TradeReplicationBinding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/physics/SimplePhysicsWorld.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/audio/NullAudioBackend.cpp
)

add_library(EngineTestSupport STATIC ${ENGINE_TEST_SUPPORT_SOURCES})
target_include_directories(EngineTestSupport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform
)
target_link_libraries(EngineTestSupport PUBLIC Engine glm::glm)
target_compile_features(EngineTestSupport PUBLIC cxx_std_17)

if(WIN32)
    target_link_libraries(EngineTestSupport PUBLIC ws2_32 winmm)
endif()

set(ENGINE_TEST_SOURCES
    core/NetBufferTest.cpp
    core/NetPacketTest.cpp
    core/NetworkPayloadsTest.cpp
    core/ReplicationTypesTest.cpp
    core/ECSTest.cpp
    core/DeterministicMathTest.cpp
    core/SecureCompareTest.cpp
    core/UdpMessageAuthTest.cpp
    scene/SceneTest.cpp
    scene/SceneSerializerTest.cpp
    scene/PrefabTest.cpp
    network/InterestManagerTest.cpp
    network/RateLimiterTest.cpp
    network/ServerAuthorityTest.cpp
    network/SocketTest.cpp
    network/UdpChannelTest.cpp
    network/ReplicationSystemTest.cpp
    network/ItemOwnershipReplicationBindingTest.cpp
    network/NetServerIntegrationTest.cpp
    network/TlsSessionTest.cpp
    network/CharacterMovementSimulatorTest.cpp
    network/ClientPredictionTest.cpp
    core/CredentialStoreTest.cpp
    core/PasswordHasherTest.cpp
    core/LoadoutReplicationTest.cpp
    core/ItemOwnershipReplicationTest.cpp
    core/PlayerItemOwnershipComponentTest.cpp
    core/TradeSessionTest.cpp
    core/ChatMessageTest.cpp
    core/AuthorityHardeningTest.cpp
    core/ReplicationTierTest.cpp
    core/ScalePolicyTest.cpp
    core/LayerInstancePolicyTest.cpp
    core/ZoneScaleConfigTest.cpp
    terrain/HeightmapTerrainTest.cpp
    physics/SimplePhysicsWorldTest.cpp
    audio/NullAudioBackendTest.cpp
    render/RaycastTest.cpp
    render/ShadowShaderFilesTest.cpp
)

if(BUILD_ENGINE_PLATFORM)
    list(APPEND ENGINE_TEST_SOURCES render/ModelRaycastTest.cpp network/NetworkSystemTest.cpp network/NetServerJoinLoadTest.cpp network/NetServerScalePolicyMultiUserTest.cpp network/AuthoritativeGameSimulatorOwnershipTest.cpp network/AuthoritativeGameSimulatorTradeTest.cpp)
endif()

add_executable(EngineTests ${ENGINE_TEST_SOURCES})
target_include_directories(EngineTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(EngineTests PRIVATE
    ENGINE_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../config"
)
target_link_libraries(EngineTests PRIVATE EngineTestSupport GTest::gtest_main)
target_compile_features(EngineTests PRIVATE cxx_std_17)

if(BUILD_ENGINE_PLATFORM)
    target_link_libraries(EngineTests PRIVATE Platform)
    target_compile_definitions(EngineTests PRIVATE ENGINE_PLATFORM_TESTS)
endif()

set_target_properties(EngineTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

include(GoogleTest)
gtest_discover_tests(EngineTests DISCOVERY_MODE PRE_TEST)

# Fallback so `ctest` always has at least one entry if discovery is stale on MSVC.
add_test(NAME EngineTests COMMAND EngineTests)
set_tests_properties(EngineTests PROPERTIES LABELS "engine;aggregate")
