# RK3326 build with zig as the cross compiler: glibc 2.30 symbols (ArkOS is Ubuntu 19.10 / glibc 2.30; dArkOS is
# newer), libc++ linked statically by zig. Device libs (DRM/GBM/EGL/GLES) come from RK3326_SYSROOT.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
set(_RK_SRC "${CMAKE_CURRENT_LIST_DIR}/..")
set(CMAKE_C_COMPILER "${_RK_SRC}/platform/zig/zig-cc")
set(CMAKE_CXX_COMPILER "${_RK_SRC}/platform/zig/zig-c++")
set(CMAKE_AR "${_RK_SRC}/platform/zig/zig-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${_RK_SRC}/platform/zig/zig-ranlib" CACHE FILEPATH "")
set(RK3326_SYSROOT "$ENV{RK3326_SYSROOT}" CACHE PATH "Sysroot with the device libraries")
set(CMAKE_FIND_ROOT_PATH "${RK3326_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_C_FLAGS_INIT "-I${_RK_SRC}/platform/arm64-intrin -isystem ${RK3326_SYSROOT}/usr/include -isystem ${RK3326_SYSROOT}/usr/include/libdrm")
set(CMAKE_CXX_FLAGS_INIT "-DUSE_SSE2NEON -I${_RK_SRC}/platform/arm64-intrin -I${_RK_SRC}/runtime/ps2xRuntime/include -isystem ${RK3326_SYSROOT}/usr/include -isystem ${RK3326_SYSROOT}/usr/include/libdrm")
# Only the device libs' own folders: never the sysroot's /usr/include as a whole (its glibc headers are 2.39).
# The sysroot .so files only stand in for the device's own at link time: their newer glibc references are not ours.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-L${RK3326_SYSROOT}/usr/lib/aarch64-linux-gnu -Wl,--allow-shlib-undefined")
