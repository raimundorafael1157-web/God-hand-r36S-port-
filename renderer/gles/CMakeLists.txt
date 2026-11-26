# RK3326 port: GLES 3 GS backend and its replay benchmark. Built from runtime/ps2xTest (GODHAND_BUILD_TOOLS=ON),
# which adds this folder when setup_base.sh has copied it into the tree.
find_library(GLES_EGL_LIBRARY EGL REQUIRED)
find_library(GLES_GLESV2_LIBRARY GLESv2 REQUIRED)
find_library(GLES_GBM_LIBRARY gbm REQUIRED)
add_executable(gles_replay gles_replay.cpp gs_gles_backend.cpp egl_headless.cpp)
target_link_libraries(gles_replay PRIVATE ps2_gs_reference ${GLES_EGL_LIBRARY} ${GLES_GLESV2_LIBRARY} ${GLES_GBM_LIBRARY})
