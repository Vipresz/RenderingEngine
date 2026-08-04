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
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/scene/SceneSpawner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/scene/SceneSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/CharacterMovementSimulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/PasswordHasher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/CryptoSha256.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/SecureCompare.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/UdpMessageAuth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/UdpDatagramCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/CredentialStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/ScalePolicyConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/LayerInstancePolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/ReplicationTier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/ReplicationTierThrottle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/ShardRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/network/MigrationToken.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/NetGateway.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/CellLayerAssigner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/ShardPopulationReporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/systems/ReplicationSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/ItemOwnershipReplicationBinding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/TradeReplicationBinding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/physics/SimplePhysicsWorld.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/audio/NullAudioBackend.cpp
)
if(NOT BUILD_ENGINE_PLATFORM)
    list(APPEND ENGINE_TEST_SUPPORT_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/TlsSession.cpp)
endif()

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
    core/SecurityHardeningTest.cpp
    core/SecureCompareTest.cpp
    core/UdpMessageAuthTest.cpp
    scene/SceneTest.cpp
    scene/SceneSerializerTest.cpp
    scene/SceneSpawnerTest.cpp
    scene/PrefabTest.cpp
    network/InterestManagerTest.cpp
    network/RateLimiterTest.cpp
    network/ServerAuthorityTest.cpp
    network/SocketTest.cpp
    network/UdpChannelTest.cpp
    network/ReplicationSystemTest.cpp
    network/ItemOwnershipReplicationBindingTest.cpp
    network/NetServerIntegrationTest.cpp
    network/PingPongTest.cpp
    network/LoginEdgeCasesTest.cpp
    network/NetServerSecurityTest.cpp
    network/UdpDatagramCipherTest.cpp
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
    core/ShardRegistryTest.cpp
    core/MigrationTokenTest.cpp
    core/ReplicationTierThrottleTest.cpp
    core/ZoneScaleConfigTest.cpp
    network/CellLayerAssignerTest.cpp
    network/NetGatewayTest.cpp
    network/TransformBatchTest.cpp
    terrain/HeightmapTerrainTest.cpp
    physics/SimplePhysicsWorldTest.cpp
    audio/NullAudioBackendTest.cpp
    render/RaycastTest.cpp
    render/ShadowShaderFilesTest.cpp
    ui/UILayerTest.cpp
)

if(BUILD_ENGINE_PLATFORM)
    list(APPEND ENGINE_TEST_SOURCES
        render/FrustumTest.cpp
        render/ModelRaycastTest.cpp
        render/RenderLodSelectionTest.cpp
        render/AssetManagerTest.cpp
        render/ModelRaycastWorldTest.cpp
        render/RenderPipelinePassTest.cpp
        network/NetworkSystemTest.cpp
        network/NetworkSystemIntegrationTest.cpp
        network/NetServerJoinLoadTest.cpp
        network/NetServerScalePolicyMultiUserTest.cpp
        network/AuthoritativeGameSimulatorOwnershipTest.cpp
        network/AuthoritativeGameSimulatorTradeTest.cpp
        network/AuthoritativeGameSimulatorChannelsTest.cpp
        network/AuthoritativeGameSimulatorGameplayTest.cpp
        network/TlsHandshakeTest.cpp)
    if(DEFINED NUKLEAR_DIR)
        list(APPEND ENGINE_TEST_SOURCES
            ui/FontLibraryTest.cpp
            ui/ItemTooltipLayoutTest.cpp
            ui/UISystemTest.cpp)
    endif()
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
    if(DEFINED NUKLEAR_DIR)
        target_compile_definitions(EngineTests PRIVATE PLATFORM_HAS_NUKLEAR)
        target_include_directories(EngineTests PRIVATE ${NUKLEAR_DIR} ${NUKLEAR_DIR}/demo/glfw_opengl3)
    endif()
endif()

if(ENGINE_ENABLE_TLS)
    target_compile_definitions(EngineTestSupport PUBLIC ENGINE_ENABLE_TLS)
    target_link_libraries(EngineTestSupport PUBLIC mbedtls)
    target_link_libraries(EngineTests PRIVATE mbedtls)
endif()

set_target_properties(EngineTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(BUILD_ENGINE_PLATFORM)
    file(GLOB ENGINE_TEST_SHADER_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../platform/render/shaders/*.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/../platform/render/shaders/*.frag")
    foreach(SHADER ${ENGINE_TEST_SHADER_FILES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        add_custom_command(TARGET EngineTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EngineTests>/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SHADER}"
                "$<TARGET_FILE_DIR:EngineTests>/shaders/${SHADER_NAME}"
            COMMENT "Copying shader ${SHADER_NAME} for EngineTests"
        )
    endforeach()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tls/test_server.crt")
        add_custom_command(TARGET EngineTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EngineTests>/fixtures/tls"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tls/test_server.crt"
                "$<TARGET_FILE_DIR:EngineTests>/fixtures/tls/test_server.crt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tls/test_server.key"
                "$<TARGET_FILE_DIR:EngineTests>/fixtures/tls/test_server.key"
            COMMENT "Copying TLS fixtures for EngineTests"
        )
    endif()
endif()

include(GoogleTest)
gtest_discover_tests(EngineTests DISCOVERY_MODE PRE_TEST)

# Fallback so `ctest` always has at least one entry if discovery is stale on MSVC.
add_test(NAME EngineTests COMMAND EngineTests)
set_tests_properties(EngineTests PROPERTIES LABELS "engine;aggregate")
