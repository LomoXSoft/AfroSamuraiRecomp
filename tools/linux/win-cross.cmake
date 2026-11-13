# Cross-compile for x86_64 Windows (MSVC ABI) from Linux with LLVM.
# Headers and import libraries come from xwin through the INCLUDE and LIB
# environment variables, which clang and lld-link read like cl and link do.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_AR llvm-ar)
set(CMAKE_LINKER_TYPE LLD)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# xwin splats only the release CRT (no msvcrtd.lib): compiler checks link Release.
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
