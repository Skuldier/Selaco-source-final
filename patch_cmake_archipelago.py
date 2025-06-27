#!/usr/bin/env python3
"""
Selaco CMakeLists.txt Archipelago Integration Patcher
=====================================================

This script automatically patches Selaco's CMakeLists.txt to add Archipelago
integration with libwebsockets and all necessary dependencies.

Usage:
    python patch_cmake_archipelago.py <path_to_selaco_root>

The script will:
1. Locate the main CMakeLists.txt file
2. Create a backup of the original
3. Add libwebsockets and JSON dependencies via FetchContent
4. Create the selaco_archipelago library target
5. Link it to the main Selaco executable
6. Configure Windows-specific libraries and settings

"""

import os
import sys
import re
import shutil
from typing import List, Tuple, Optional

def backup_file(filepath: str) -> str:
    """Create a backup of the original file."""
    backup_path = filepath + '.backup'
    if not os.path.exists(backup_path):
        shutil.copy2(filepath, backup_path)
        print(f"Created backup: {backup_path}")
    return backup_path

def read_file(filepath: str) -> str:
    """Read file contents."""
    with open(filepath, 'r', encoding='utf-8') as f:
        return f.read()

def write_file(filepath: str, content: str) -> None:
    """Write content to file."""
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

def find_cmake_minimum_required(content: str) -> Optional[int]:
    """Find the cmake_minimum_required line."""
    match = re.search(r'cmake_minimum_required\s*\([^)]+\)', content)
    return match.end() if match else None

def find_project_declaration(content: str) -> Optional[int]:
    """Find the project() declaration."""
    match = re.search(r'project\s*\([^)]+\)', content)
    return match.end() if match else None

def find_main_target(content: str) -> Optional[Tuple[str, int]]:
    """Find the main executable target (likely 'selaco' or similar)."""
    # Look for add_executable with likely target names
    target_patterns = [
        r'add_executable\s*\(\s*(selaco)\s+',
        r'add_executable\s*\(\s*(Selaco)\s+',
        r'add_executable\s*\(\s*(zdoom)\s+',
        r'add_executable\s*\(\s*(\${ZDOOM_EXE_NAME})\s+',
    ]
    
    for pattern in target_patterns:
        match = re.search(pattern, content, re.IGNORECASE)
        if match:
            target_name = match.group(1)
            return target_name, match.start()
    
    # Fallback: look for any add_executable
    match = re.search(r'add_executable\s*\(\s*([^\s)]+)', content)
    if match:
        target_name = match.group(1)
        return target_name, match.start()
    
    return None

def add_archipelago_dependencies(content: str) -> str:
    """Add libwebsockets and JSON dependencies."""
    
    archipelago_deps = '''
# =============================================================================
# ARCHIPELAGO INTEGRATION DEPENDENCIES
# =============================================================================

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

'''
    
    # Find a good insertion point after project() declaration
    project_end = find_project_declaration(content)
    if project_end:
        # Look for a good spot after project - maybe after some initial setup
        insert_point = project_end
        
        # Try to find a better spot after existing dependencies or includes
        dependency_patterns = [
            r'find_package\s*\([^)]+\)\s*\n',
            r'include\s*\([^)]+\)\s*\n',
            r'set\s*\([^)]+\)\s*\n.*?CMAKE.*?\n',
        ]
        
        search_start = project_end
        search_end = min(project_end + 2000, len(content))  # Look in next 2000 chars
        
        last_match_end = project_end
        for pattern in dependency_patterns:
            for match in re.finditer(pattern, content[search_start:search_end]):
                absolute_pos = search_start + match.end()
                if absolute_pos > last_match_end:
                    last_match_end = absolute_pos
        
        insert_point = last_match_end
        
    else:
        # Fallback: insert after cmake_minimum_required
        cmake_end = find_cmake_minimum_required(content)
        insert_point = cmake_end if cmake_end else 0
    
    return content[:insert_point] + archipelago_deps + content[insert_point:]

def add_archipelago_library(content: str) -> str:
    """Add the Archipelago library target."""
    
    archipelago_lib = '''
# =============================================================================
# ARCHIPELAGO INTEGRATION LIBRARY
# =============================================================================

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

'''
    
    # Find main target to insert library before it
    target_info = find_main_target(content)
    if target_info:
        target_name, target_pos = target_info
        return content[:target_pos] + archipelago_lib + content[target_pos:]
    else:
        # Fallback: append before the end
        return content + archipelago_lib

def add_target_link(content: str) -> str:
    """Add target_link_libraries call to link Archipelago to main target."""
    
    target_info = find_main_target(content)
    if not target_info:
        print("Warning: Could not find main executable target")
        return content
    
    target_name, _ = target_info
    
    # Look for existing target_link_libraries for this target
    link_pattern = rf'target_link_libraries\s*\(\s*{re.escape(target_name)}\s+[^)]*\)'
    match = re.search(link_pattern, content)
    
    archipelago_link = f'\n# Add Archipelago support to main executable\ntarget_link_libraries({target_name} PRIVATE selaco_archipelago)\n'
    
    if match:
        # Add after existing target_link_libraries
        insert_point = match.end()
        return content[:insert_point] + archipelago_link + content[insert_point:]
    else:
        # Find the end of the target definition and add there
        target_pattern = rf'add_executable\s*\(\s*{re.escape(target_name)}\s+[^)]*\)'
        target_match = re.search(target_pattern, content)
        if target_match:
            # Look for the end of the target definition (next blank line or next add_* command)
            search_start = target_match.end()
            next_section = re.search(r'\n\s*\n|\nadd_', content[search_start:])
            if next_section:
                insert_point = search_start + next_section.start()
            else:
                insert_point = search_start
            
            return content[:insert_point] + archipelago_link + content[insert_point:]
        
        # Fallback: append at end
        return content + archipelago_link

def add_success_message(content: str) -> str:
    """Add a success message at the end."""
    
    success_msg = '''
message(STATUS "Selaco Archipelago integration configured successfully")
'''
    
    return content + success_msg

def patch_cmake_file(filepath: str) -> bool:
    """Apply all patches to CMakeLists.txt."""
    
    print(f"Patching {filepath}...")
    
    # Create backup
    backup_file(filepath)
    
    # Read original content
    content = read_file(filepath)
    
    # Apply patches in sequence
    print("  Adding Archipelago dependencies...")
    content = add_archipelago_dependencies(content)
    
    print("  Adding Archipelago library target...")
    content = add_archipelago_library(content)
    
    print("  Adding target linking...")
    content = add_target_link(content)
    
    print("  Adding success message...")
    content = add_success_message(content)
    
    # Write modified content
    write_file(filepath, content)
    
    print(f"Successfully patched {filepath}")
    return True

def create_helper_info(root_dir: str) -> None:
    """Create a helper file with next steps."""
    
    helper_content = '''# Selaco Archipelago Integration - Next Steps

## What was added to CMakeLists.txt:

1. **Dependencies:**
   - libwebsockets v4.3.3 (via FetchContent)
   - nlohmann/json v3.11.3 (via FetchContent)
   - Windows libraries: ws2_32, iphlpapi, psapi, userenv, crypt32

2. **Library Target:**
   - `selaco_archipelago` static library
   - Includes all Archipelago source files
   - Configured with proper include directories

3. **Integration:**
   - Linked to main Selaco executable
   - Windows-specific configuration
   - Debug/Release build settings

## Next Steps:

1. Create the archipelago directory:
   ```
   mkdir src/archipelago
   ```

2. Copy the Archipelago source files to src/archipelago/

3. Run the main loop patch:
   ```
   python selaco_archipelago_patch.py ./src
   ```

4. Build and test:
   ```
   mkdir build && cd build
   cmake -G "Visual Studio 17 2022" -A x64 ..
   cmake --build . --config Release
   ```

## Testing:

Once built, test with these console commands:
- `archipelago_status`
- `archipelago_connect archipelago.gg 38281 YourSlotName`
- `archipelago_test`
- `archipelago_disconnect`
'''
    
    helper_file = os.path.join(root_dir, 'archipelago_cmake_integration.txt')
    write_file(helper_file, helper_content)
    print(f"Created helper file: {helper_file}")

def main():
    if len(sys.argv) != 2:
        print("Usage: python patch_cmake_archipelago.py <path_to_selaco_root>")
        print("Example: python patch_cmake_archipelago.py ./")
        print("Example: python patch_cmake_archipelago.py ../selaco")
        sys.exit(1)
    
    root_dir = sys.argv[1]
    
    if not os.path.isdir(root_dir):
        print(f"Error: Directory {root_dir} does not exist")
        sys.exit(1)
    
    # Find CMakeLists.txt
    cmake_path = os.path.join(root_dir, 'CMakeLists.txt')
    if not os.path.exists(cmake_path):
        print(f"Error: {cmake_path} not found")
        print("Make sure you're pointing to the root directory containing CMakeLists.txt")
        sys.exit(1)
    
    print("=== Selaco CMakeLists.txt Archipelago Patcher ===")
    print(f"Root directory: {root_dir}")
    print(f"Target file: {cmake_path}")
    print()
    
    # Check if already patched
    content = read_file(cmake_path)
    if 'selaco_archipelago' in content:
        print("WARNING: CMakeLists.txt appears to already contain Archipelago integration!")
        response = input("Continue anyway? (y/N): ")
        if response.lower() != 'y':
            print("Aborted.")
            sys.exit(0)
    
    # Apply patches
    success = patch_cmake_file(cmake_path)
    
    if success:
        print()
        print("=== CMake Patch Applied Successfully ===")
        print()
        print("✅ Added libwebsockets and JSON dependencies")
        print("✅ Created selaco_archipelago library target")
        print("✅ Configured Windows-specific settings")
        print("✅ Linked to main Selaco executable")
        print()
        
        # Create helper info
        create_helper_info(root_dir)
        
        print("Next steps:")
        print("1. Create the archipelago directory: mkdir src/archipelago")
        print("2. Copy all Archipelago source files to src/archipelago/")
        print("3. Run the main loop patch: python selaco_archipelago_patch.py ./src")
        print("4. Build and test the integration")
        print()
        print("See archipelago_cmake_integration.txt for detailed next steps!")
        
    else:
        print("=== CMake Patch Failed ===")
        print("Please check the errors above and try again")
        sys.exit(1)

if __name__ == "__main__":
    main()