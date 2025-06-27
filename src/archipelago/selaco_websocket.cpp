# WebSocket Integration for Selaco - Add to main CMakeLists.txt

# =============================================================================
# LIBWEBSOCKETS INTEGRATION
# =============================================================================

# Add this section to your main CMakeLists.txt after existing dependencies

cmake_minimum_required(VERSION 3.15)

# Use FetchContent for libwebsockets and JSON library
include(FetchContent)

# Fetch libwebsockets
FetchContent_Declare(
    libwebsockets
    GIT_REPOSITORY https://github.com/warmcat/libwebsockets.git
    GIT_TAG v4.3.3  # Stable version
)

# Configure libwebsockets options BEFORE fetching
set(LWS_WITHOUT_TESTAPPS ON CACHE BOOL "Don't build test apps")
set(LWS_WITHOUT_TEST_SERVER ON CACHE BOOL "Don't build test server")
set(LWS_WITHOUT_TEST_CLIENT ON CACHE BOOL "Don't build test client")
set(LWS_WITH_MINIMAL_EXAMPLES OFF CACHE BOOL "Don't build examples")
set(LWS_WITH_SSL ON CACHE BOOL "Enable SSL support")
set(LWS_WITH_BUNDLED_ZLIB ON CACHE BOOL "Use bundled zlib on Windows")
set(LWS_WITH_LIBEV OFF CACHE BOOL "Disable libev")
set(LWS_WITH_LIBUV OFF CACHE BOOL "Disable libuv") 

# Fetch JSON library (lightweight, header-only)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

# Make both available
FetchContent_MakeAvailable(libwebsockets json)

# Create Archipelago integration library for Selaco
add_library(selaco_archipelago STATIC
    src/archipelago/selaco_websocket.cpp
    src/archipelago/selaco_websocket.h
    src/archipelago/archipelago_client.cpp
    src/archipelago/archipelago_client.h
    src/archipelago/archipelago_integration.cpp
    src/archipelago/archipelago_integration.h
)

# Configure include directories
target_include_directories(selaco_archipelago
    PUBLIC
    ${libwebsockets_SOURCE_DIR}/include
    ${libwebsockets_BINARY_DIR}/include
    src/archipelago
)

# Link libraries
target_link_libraries(selaco_archipelago
    PUBLIC
    websockets
    nlohmann_json::nlohmann_json
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(selaco_archipelago 
        PUBLIC 
        ws2_32 
        iphlpapi 
        psapi 
        userenv
        crypt32  # For SSL certificate validation
    )
    
    # Windows-specific defines
    target_compile_definitions(selaco_archipelago 
        PRIVATE 
        _WIN32_WINNT=0x0600
        WIN32_LEAN_AND_MEAN
    )
endif()

# Add Archipelago support to main Selaco executable
# IMPORTANT: Add this line to your main target
target_link_libraries(selaco PRIVATE selaco_archipelago)

# =============================================================================
# BUILD CONFIGURATION
# =============================================================================

# Ensure proper build configuration for Archipelago integration
if(MSVC)
    target_compile_options(selaco_archipelago PRIVATE /W3)
    target_compile_definitions(selaco_archipelago PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(selaco_archipelago PRIVATE -Wall -Wextra)
endif()

# Debug configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(selaco_archipelago PRIVATE SELACO_AP_DEBUG=1)
endif()

message(STATUS "Selaco Archipelago integration configured successfully")