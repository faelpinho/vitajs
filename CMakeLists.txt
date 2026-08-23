cmake_minimum_required(VERSION 3.6)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

# Name of the project
set(SHORT_NAME vitajs)

project(${SHORT_NAME})
# This line adds Vita helper macros, must go after project definition in order to build Vita specific artifacts (self/vpk).
include("${VITASDK}/share/vita.cmake" REQUIRED)

find_package(ZLIB REQUIRED)

## Configuration options for this app
# Display name (under bubble in LiveArea)
set(VITA_APP_NAME ${SHORT_NAME})
# unique string of developer and Y = a unique number for this app
set(VITA_TITLEID  "VITAJS001")
set(VITA_VERSION  "01.00")

## Flags and includes for building
# Note that we make sure not to overwrite previous flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu17 -Wno-all -Wno-int-conversion -DPATH_MAX=256")

# Read the version directly from the upstream QuickJS directory.
# Expected after this upgrade: 2026-06-04.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/quickjs/VERSION" QUICKJS_VERSION LIMIT_COUNT 1)
if(NOT QUICKJS_VERSION)
    message(FATAL_ERROR "quickjs/VERSION is missing or empty")
endif()
message(STATUS "Building with QuickJS ${QUICKJS_VERSION}")

add_definitions(-DCONFIG_VERSION="${QUICKJS_VERSION}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

# Add any additional include paths here
include_directories(
    src/assets
    quickjs
)

## Build and link
# Native VitaJS modules are discovered automatically.
# net_async.c is experimental and intentionally excluded until it replaces net.c.
file(GLOB VITAJS_MODULE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/modules/*.c"
)
list(FILTER VITAJS_MODULE_SOURCES EXCLUDE REGEX "/net_async\\.c$")

# Add all the files needed to compile here
add_executable(${PROJECT_NAME}
    src/main.c
    ${VITAJS_MODULE_SOURCES}
    src/env.c
    src/graphics.c
    #src/taskman.c
    # QuickJS 2026 core.
    # Keep PSVitaJS' existing Vita-patched quickjs-libc.c / quickjs-libc.h.
    quickjs/cutils.c
    quickjs/dtoa.c
    quickjs/libregexp.c
    quickjs/libunicode.c
    quickjs/quickjs.c
    quickjs/quickjs-libc.c
    src/common/debugScreen.c
)

# Keep upstream QuickJS optimizer assumptions explicit.
# -fwrapv is required by upstream QuickJS; -O2 matches the upstream optimized build.
set_source_files_properties(
    quickjs/cutils.c
    quickjs/dtoa.c
    quickjs/libregexp.c
    quickjs/libunicode.c
    quickjs/quickjs.c
    quickjs/quickjs-libc.c
    PROPERTIES COMPILE_FLAGS "-O2 -fwrapv"
)

# The audio callback/mixer is real-time code on a 444 MHz ARM CPU.
# Build this module optimized even when the rest of the project is a debug/default build.
set_source_files_properties(
    src/modules/audio.c
    PROPERTIES COMPILE_FLAGS "-O2"
)

# Library to link to (drop the -l prefix). This will mostly be stubs.
# NOTE: png[lib] should stay BEFORE z[lib] here.
target_link_libraries(${PROJECT_NAME}
    vita2d

    SceGxm_stub
    SceDisplay_stub
    SceSysmodule_stub
    SceAppMgr_stub
    SceAppUtil_stub
    SceProcessmgr_stub
    SceCommonDialog_stub
    SceIme_stub
    ScePower_stub
    SceCtrl_stub
    SceTouch_stub
    SceSysmem_stub
    ScePvf_stub

    SceNet_stub
    SceNetCtl_stub
    SceHttp_stub
    SceAudio_stub
    freetype
    png
    jpeg
    bz2
    z
    c
    m
    pthread
    vorbisidec
    ogg
)

## Create Vita files
vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})

# The FILE directive lets you add additional files to the VPK, the syntax is
# FILE src_path dst_path_in_vpk. In this case, we add the LiveArea paths.
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    FILE
          src/assets/segoeui.ttf assets/segoeui.ttf
          src/assets/vitajs.d.ts assets/vitajs.d.ts
          src/assets/main.js assets/main.js
          src/assets/game.js assets/game.js
          src/assets/test1.png assets/test1.png
          src/assets/audio.wav assets/audio.wav
          src/assets/bg.wav assets/bg.wav
          src/assets/coin.wav assets/coin.wav
          sce_sys/icon0.png sce_sys/icon0.png
          sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
          sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
          sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
