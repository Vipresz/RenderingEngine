# Engine Library - Core simulation engine (ECS, timestep, jobs, etc.)

# ============================================================================
# Engine Library (Header-only INTERFACE library for now)
# ============================================================================

# Build Platform library first if it exists (needed for examples)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/CMakeLists.txt")
    add_subdirectory(platform)
endif()

# Create INTERFACE library (since engine is currently header-only)
# When .cpp files are added later, we can convert this to a STATIC library
add_library(Engine INTERFACE)

# Include directories - point to engine root so includes work like:
# #include "core/ecs/Registry.h"
target_include_directories(Engine INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# C++ standard
target_compile_features(Engine INTERFACE cxx_std_17)

# Platform-specific libraries
find_package(Threads REQUIRED)
target_link_libraries(Engine INTERFACE Threads::Threads)

# Windows-specific settings
if(WIN32)
    # Windows needs this for Sleep() and other functions
    target_link_libraries(Engine INTERFACE winmm)
endif()

# ============================================================================
# Example/Simulation Executables
# ============================================================================

# Option to build examples
option(BUILD_ENGINE_EXAMPLES "Build engine example programs" ON)

if(BUILD_ENGINE_EXAMPLES)
    # Basic Example - Demonstrates rendering with grid, sphere, WASD controls, and lighting
    add_executable(BasicExample
        examples/BasicExample.cpp
    )

    target_link_libraries(BasicExample PRIVATE Engine)
    
    # Link Platform library if it exists (for rendering, input, etc.)
    if(TARGET Platform)
        target_link_libraries(BasicExample PRIVATE Platform)
    else()
        # If Platform library doesn't exist, we need to set up include directories manually
        # This happens when building just the engine directory in isolation
        target_include_directories(BasicExample PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/platform
        )
    endif()

    # Set output name - CMake automatically adds .exe on Windows
    set_target_properties(BasicExample PROPERTIES
        OUTPUT_NAME "basic_example"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # Set working directory for running from IDE (so shaders can be found)
    set_target_properties(BasicExample PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/platform/render/shaders"
    )
    
    # Copy shader files to output directory so they can be found at runtime
    # On Windows, executables go to bin/Release or bin/Debug, so we need to handle both
    file(GLOB SHADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/render/shaders/*.vert" 
                            "${CMAKE_CURRENT_SOURCE_DIR}/platform/render/shaders/*.frag")
    foreach(SHADER ${SHADER_FILES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        # Copy to both Release and Debug directories (Windows multi-config generators)
        foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
            add_custom_command(TARGET BasicExample POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/shaders"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SHADER}"
                "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/shaders/${SHADER_NAME}"
                COMMENT "Copying shader: ${SHADER_NAME} to $<CONFIG>"
            )
        endforeach()
        # Also copy to base bin/shaders for single-config generators (Unix)
        add_custom_command(TARGET BasicExample POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHADER}"
            "${CMAKE_BINARY_DIR}/bin/shaders/${SHADER_NAME}"
            COMMENT "Copying shader: ${SHADER_NAME}"
        )
    endforeach()
    
    # Copy GLEW DLL to output directory (needed on Windows)
    # Try to find GLEW DLL from the main build directory or game_libs
    if(WIN32)
        # Check main build directory first (if building from root project)
        set(GLEW_DLL_SOURCE "")
        if(EXISTS "${CMAKE_SOURCE_DIR}/../build/bin/Release/glew32.dll")
            set(GLEW_DLL_SOURCE "${CMAKE_SOURCE_DIR}/../build/bin/Release/glew32.dll")
        elseif(EXISTS "${CMAKE_SOURCE_DIR}/../game_libs/glew-2.1.0/bin/Release/x64/glew32.dll")
            set(GLEW_DLL_SOURCE "${CMAKE_SOURCE_DIR}/../game_libs/glew-2.1.0/bin/Release/x64/glew32.dll")
        endif()
        
        if(GLEW_DLL_SOURCE)
            foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
                add_custom_command(TARGET BasicExample POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${GLEW_DLL_SOURCE}"
                    "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/glew32.dll"
                    COMMENT "Copying GLEW DLL to $<CONFIG>"
                )
            endforeach()
            # For single-config generators
            add_custom_command(TARGET BasicExample POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${GLEW_DLL_SOURCE}"
                "${CMAKE_BINARY_DIR}/bin/glew32.dll"
                COMMENT "Copying GLEW DLL"
            )
        endif()
    endif()
endif()

if(BUILD_ENGINE_EXAMPLES)
    add_executable(NetworkExample
        examples/NetworkExample.cpp
    )
    target_link_libraries(NetworkExample PRIVATE Engine Platform)
    set_target_properties(NetworkExample PROPERTIES
        OUTPUT_NAME "network_example"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# ============================================================================
# Unit tests
# ============================================================================

option(BUILD_ENGINE_TESTS "Build RenderingEngine unit tests" ON)

if(BUILD_ENGINE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Future: Add source files here when creating .cpp implementations
# ============================================================================

# Example for when you add .cpp files:
# set(ENGINE_CORE_SOURCES
#     core/time/GameLoop.cpp
#     core/jobs/JobSystem.cpp
# )
# 
# # Convert to STATIC library
# set_target_properties(Engine PROPERTIES TYPE STATIC)
# target_sources(Engine PRIVATE ${ENGINE_CORE_SOURCES})

# ============================================================================
# Third-party dependencies (commented out for future use)
# ============================================================================

# OpenGL support (uncomment when needed)
# find_package(OpenGL REQUIRED)
# target_link_libraries(Engine INTERFACE OpenGL::GL)

# JSON library (example: nlohmann/json)
# add_subdirectory(third_party/json)
