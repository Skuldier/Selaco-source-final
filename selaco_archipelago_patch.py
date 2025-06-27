#!/usr/bin/env python3
"""
Selaco src/CMakeLists.txt Archipelago Integration Patcher
========================================================

This script patches Selaco's src/CMakeLists.txt to integrate the archipelago
subdirectory and link it to the main game executables.

Usage:
    python patch_src_cmake_archipelago.py <path_to_selaco_src>

The script will:
1. Add archipelago subdirectory 
2. Add archipelago include directory
3. Link archipelago library to main game executables
4. Handle multiple game targets (doom, heretic, hexen, strife)

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

def add_archipelago_subdirectory(content: str) -> str:
    """Add archipelago subdirectory and include directory."""
    
    # Look for existing add_subdirectory calls to find good insertion point
    subdir_pattern = r'foreach\s*\([^)]*SUBDIR[^)]*\)\s*\n\s*add_subdirectory\s*\([^)]*\)\s*\nendforeach\s*\(\)'
    match = re.search(subdir_pattern, content)
    
    archipelago_setup = '''
# Add Archipelago integration
add_subdirectory(archipelago)
include_directories(archipelago)
'''
    
    if match:
        # Add after the foreach loop
        insert_point = match.end()
        return content[:insert_point] + archipelago_setup + content[insert_point:]
    
    # Fallback: look for any add_subdirectory call
    subdir_matches = list(re.finditer(r'add_subdirectory\s*\([^)]+\)', content))
    if subdir_matches:
        # Add after the last add_subdirectory
        insert_point = subdir_matches[-1].end()
        return content[:insert_point] + archipelago_setup + content[insert_point:]
    
    # Final fallback: add at the beginning after any initial setup
    setup_pattern = r'(set\s*\([^)]*SOURCE_FILES[^)]*\)[^#]*)'
    setup_match = re.search(setup_pattern, content, re.DOTALL)
    if setup_match:
        insert_point = setup_match.end()
        return content[:insert_point] + archipelago_setup + content[insert_point:]
    
    print("Warning: Could not find good insertion point for archipelago subdirectory")
    return content

def find_executable_targets(content: str) -> List[str]:
    """Find all add_executable targets that might need archipelago linking."""
    
    # Look for add_executable calls
    pattern = r'add_executable\s*\(\s*"?\$?\{?([^}")\s]+)\}?"?'
    matches = re.findall(pattern, content)
    
    # Filter for likely game executables
    game_targets = []
    for match in matches:
        target = match.strip()
        # Look for doom, heretic, hexen, strife, or selaco variants
        if any(game in target.lower() for game in ['doom', 'heretic', 'hexen', 'strife', 'selaco']):
            game_targets.append(target)
        # Also include PROGRAM_PREFIX variants
        elif 'PROGRAM_PREFIX' in target:
            game_targets.append(target)
    
    return game_targets

def add_archipelago_linking(content: str) -> str:
    """Add archipelago library linking to game executables."""
    
    # Find all game executable targets
    targets = find_executable_targets(content)
    
    if not targets:
        print("Warning: No game executable targets found")
        return content
    
    modified_content = content
    
    for target in targets:
        # Look for existing target_link_libraries for this target
        escaped_target = re.escape(target)
        link_pattern = rf'target_link_libraries\s*\(\s*"?\$?\{{?{escaped_target}\}}?"?\s+([^)]*)\)'
        
        match = re.search(link_pattern, modified_content)
        if match:
            # Check if archipelago is already linked
            existing_libs = match.group(1)
            if 'archipelago' not in existing_libs:
                # Add archipelago to existing target_link_libraries
                new_libs = existing_libs.rstrip() + ' archipelago'
                new_link_call = f'target_link_libraries("{target}" {new_libs})'
                modified_content = modified_content.replace(match.group(0), new_link_call)
                print(f"  Added archipelago to existing target_link_libraries for {target}")
        else:
            # Look for the executable definition and add linking after it
            exe_pattern = rf'add_executable\s*\(\s*"?\$?\{{?{escaped_target}\}}?"?[^)]*\)'
            exe_match = re.search(exe_pattern, modified_content)
            if exe_match:
                # Find a good insertion point after the executable
                search_start = exe_match.end()
                # Look for existing target_* calls or next add_executable
                next_section = re.search(r'\n\s*(?:target_|add_)', modified_content[search_start:])
                if next_section:
                    insert_point = search_start + next_section.start()
                else:
                    insert_point = search_start
                
                archipelago_link = f'\ntarget_link_libraries("{target}" archipelago)\n'
                modified_content = modified_content[:insert_point] + archipelago_link + modified_content[insert_point:]
                print(f"  Added new target_link_libraries for {target}")
    
    return modified_content

def add_archipelago_include_to_targets(content: str) -> str:
    """Add archipelago include directory to target_include_directories calls."""
    
    targets = find_executable_targets(content)
    modified_content = content
    
    for target in targets:
        # Look for existing target_include_directories
        escaped_target = re.escape(target)
        include_pattern = rf'target_include_directories\s*\(\s*"?\$?\{{?{escaped_target}\}}?"?\s+[^)]*\)'
        
        match = re.search(include_pattern, modified_content)
        if match:
            existing_call = match.group(0)
            if 'archipelago' not in existing_call and '${PROJECT_SOURCE_DIR}/src/archipelago' not in existing_call:
                # Add archipelago to existing include directories
                # Insert before the closing parenthesis
                insertion_point = existing_call.rfind(')')
                new_call = existing_call[:insertion_point] + ' ${PROJECT_SOURCE_DIR}/src/archipelago' + existing_call[insertion_point:]
                modified_content = modified_content.replace(existing_call, new_call)
                print(f"  Added archipelago include to {target}")
    
    return modified_content

def patch_src_cmake(filepath: str) -> bool:
    """Apply all patches to src/CMakeLists.txt."""
    
    print(f"Patching {filepath}...")
    
    # Create backup
    backup_file(filepath)
    
    # Read original content
    content = read_file(filepath)
    
    # Apply patches in sequence
    print("  Adding archipelago subdirectory and includes...")
    content = add_archipelago_subdirectory(content)
    
    print("  Adding archipelago linking to game executables...")
    content = add_archipelago_linking(content)
    
    print("  Adding archipelago includes to targets...")
    content = add_archipelago_include_to_targets(content)
    
    # Write modified content
    write_file(filepath, content)
    
    print(f"Successfully patched {filepath}")
    return True

def create_manual_instructions(src_dir: str) -> None:
    """Create manual instructions in case automatic patching fails."""
    
    instructions = '''# Manual src/CMakeLists.txt Integration Instructions

If the automatic patch didn't work perfectly, manually add these lines:

## 1. Add archipelago subdirectory (after other add_subdirectory calls):

```cmake
# Add Archipelago integration
add_subdirectory(archipelago)
include_directories(archipelago)
```

## 2. Link archipelago to game executables:

Find your main game executable targets and add archipelago to their target_link_libraries:

```cmake
# For each game target, add archipelago:
target_link_libraries(doom archipelago)
target_link_libraries(heretic archipelago)  
target_link_libraries(hexen archipelago)
target_link_libraries(strife archipelago)
# or whatever your main executable is named
```

## 3. Add archipelago includes to target_include_directories:

```cmake
target_include_directories(your_target PRIVATE ${PROJECT_SOURCE_DIR}/src/archipelago)
```

## Verification:

After building, you should see:
- archipelago library compiled
- No linking errors about missing archipelago symbols
- Console commands available: archipelago_status, archipelago_connect, etc.
'''
    
    manual_file = os.path.join(src_dir, 'archipelago_src_cmake_manual.txt')
    write_file(manual_file, instructions)
    print(f"Created manual instructions: {manual_file}")

def main():
    if len(sys.argv) != 2:
        print("Usage: python patch_src_cmake_archipelago.py <path_to_selaco_src>")
        print("Example: python patch_src_cmake_archipelago.py ./src")
        print("Example: python patch_src_cmake_archipelago.py C:\\path\\to\\selaco\\src")
        sys.exit(1)
    
    src_dir = sys.argv[1]
    
    if not os.path.isdir(src_dir):
        print(f"Error: Directory {src_dir} does not exist")
        sys.exit(1)
    
    # Find src/CMakeLists.txt
    cmake_path = os.path.join(src_dir, 'CMakeLists.txt')
    if not os.path.exists(cmake_path):
        print(f"Error: {cmake_path} not found")
        print("Make sure you're pointing to the src directory containing CMakeLists.txt")
        sys.exit(1)
    
    print("=== Selaco src/CMakeLists.txt Archipelago Patcher ===")
    print(f"Source directory: {src_dir}")
    print(f"Target file: {cmake_path}")
    print()
    
    # Check if already patched
    content = read_file(cmake_path)
    if 'add_subdirectory(archipelago)' in content:
        print("WARNING: src/CMakeLists.txt appears to already contain archipelago integration!")
        response = input("Continue anyway? (y/N): ")
        if response.lower() != 'y':
            print("Aborted.")
            sys.exit(0)
    
    # Apply patches
    success = patch_src_cmake(cmake_path)
    
    if success:
        print()
        print("=== src/CMake Patch Applied Successfully ===")
        print()
        print("✅ Added archipelago subdirectory")
        print("✅ Added archipelago include directory")
        print("✅ Linked archipelago to game executables")
        print("✅ Added archipelago includes to targets")
        print()
        
        # Create manual instructions
        create_manual_instructions(src_dir)
        
        print("Next steps:")
        print("1. Make sure you've created src/archipelago/ directory")
        print("2. Copy all 6 archipelago source files to src/archipelago/")
        print("3. Run the main loop patch: python selaco_archipelago_patch.py ./src")
        print("4. Build and test!")
        print()
        print("If build fails, check archipelago_src_cmake_manual.txt for manual instructions")
        
    else:
        print("=== src/CMake Patch Failed ===")
        print("Please check the errors above")
        create_manual_instructions(src_dir)
        print("See archipelago_src_cmake_manual.txt for manual instructions")
        sys.exit(1)

if __name__ == "__main__":
    main()