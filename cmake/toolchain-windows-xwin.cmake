# CMake toolchain file for cross-compiling to Windows using clang + xwin
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-windows-xwin.cmake ..

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# Use Homebrew LLVM
set(LLVM_PATH "/opt/homebrew/opt/llvm")
set(XWIN_PATH "$ENV{HOME}/.xwin")

# Compilers
set(CMAKE_C_COMPILER "${LLVM_PATH}/bin/clang")
set(CMAKE_CXX_COMPILER "${LLVM_PATH}/bin/clang++")
set(CMAKE_RC_COMPILER "${LLVM_PATH}/bin/llvm-rc")
set(CMAKE_LINKER "${LLVM_PATH}/bin/lld-link")
set(CMAKE_AR "${LLVM_PATH}/bin/llvm-ar")
set(CMAKE_RANLIB "${LLVM_PATH}/bin/llvm-ranlib")

# Target triple
set(TARGET_TRIPLE "x86_64-pc-windows-msvc")

# Compiler flags
set(CMAKE_C_FLAGS_INIT "-target ${TARGET_TRIPLE} -fms-compatibility-version=19.40 -fms-extensions")
set(CMAKE_CXX_FLAGS_INIT "-target ${TARGET_TRIPLE} -fms-compatibility-version=19.40 -fms-extensions")

# Include paths (Windows SDK + CRT)
# Use -isystem for system headers to suppress warnings
set(XWIN_INCLUDE_FLAGS
    "-isystem ${XWIN_PATH}/crt/include"
    "-isystem ${XWIN_PATH}/sdk/include/ucrt"
    "-isystem ${XWIN_PATH}/sdk/include/um"
    "-isystem ${XWIN_PATH}/sdk/include/shared"
    "-isystem ${XWIN_PATH}/sdk/include/winrt"
)
string(REPLACE ";" " " XWIN_INCLUDE_FLAGS_STR "${XWIN_INCLUDE_FLAGS}")

# MSVC uses 'interface' as a keyword (struct alias) - define it for clang
set(MSVC_COMPAT_FLAGS "-Dinterface=struct")

set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} ${XWIN_INCLUDE_FLAGS_STR} ${MSVC_COMPAT_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} ${XWIN_INCLUDE_FLAGS_STR} ${MSVC_COMPAT_FLAGS}")

# Library paths for linking - use -Xlinker to pass flags to lld-link
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-fuse-ld=lld-link \
    -Xlinker /libpath:${XWIN_PATH}/crt/lib/x86_64 \
    -Xlinker /libpath:${XWIN_PATH}/sdk/lib/um/x86_64 \
    -Xlinker /libpath:${XWIN_PATH}/sdk/lib/ucrt/x86_64"
)

set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT}")

# Don't look for programs on the host
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Windows-specific settings
set(WIN32 TRUE)
set(MSVC TRUE)
