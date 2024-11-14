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
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu17 -Wno-all -Wno-int-conversion -DCONFIG_VERSION=\"$(shell cat VERSION)\" -DPATH_MAX=256")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

# Add any additional include paths here
include_directories(
    src/assets
    quickjs
)

# Add any additional library paths here

## Build and link
# Add all the files needed to compile here
add_executable(${PROJECT_NAME}
    src/main.c
    src/modules/screen.c
    src/modules/system.c
    src/modules/pads.c
    src/modules/font.c
    src/env.c
    src/graphics.c
    #src/taskman.c
    quickjs/cutils.c
    quickjs/libbf.c
    quickjs/libregexp.c
    quickjs/libunicode.c
    quickjs/quickjs.c
    quickjs/quickjs-libc.c
    src/common/debugScreen.c
)

# Library to link to (drop the -l prefix). This will mostly be stubs.
# NOTE: png[lib] should stay BEFORE z[lib] here.
target_link_libraries(${PROJECT_NAME}
    vita2d
    SceGxm_stub
    SceDisplay_stub   # used by debugScreenPrintf()
    SceSysmodule_stub
    SceAppMgr_stub
    SceCommonDialog_stub
    SceCtrl_stub
    SceSysmem_stub
    ScePvf_stub
    freetype
    png
    jpeg
    bz2
    z
    c
    m
    pthread
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
          src/assets/main.js assets/main.js
          src/assets/test1.png assets/test1.png
          sce_sys/icon0.png sce_sys/icon0.png
          sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
          sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
          sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
