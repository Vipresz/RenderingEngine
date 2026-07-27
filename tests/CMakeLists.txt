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
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/InterestManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/ServerAuthority.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/DatagramSocket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/UdpChannel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/Socket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/network/systems/ReplicationSystem.cpp
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
    scene/SceneTest.cpp
    scene/SceneSerializerTest.cpp
    scene/PrefabTest.cpp
    network/InterestManagerTest.cpp
    network/ServerAuthorityTest.cpp
    network/SocketTest.cpp
    network/UdpChannelTest.cpp
    network/ReplicationSystemTest.cpp
    physics/SimplePhysicsWorldTest.cpp
    audio/NullAudioBackendTest.cpp
)

add_executable(EngineTests ${ENGINE_TEST_SOURCES})
target_link_libraries(EngineTests PRIVATE EngineTestSupport GTest::gtest_main)
target_compile_features(EngineTests PRIVATE cxx_std_17)

include(GoogleTest)
gtest_discover_tests(EngineTests DISCOVERY_MODE PRE_TEST)

# Fallback so `ctest` always has at least one entry if discovery is stale on MSVC.
add_test(NAME EngineTests COMMAND EngineTests)
set_tests_properties(EngineTests PROPERTIES LABELS "engine;aggregate")
