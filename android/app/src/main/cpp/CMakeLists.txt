cmake_minimum_required(VERSION 3.22.1)
project(ivan_android LANGUAGES C CXX)

get_filename_component(IVAN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../" ABSOLUTE)
set(VENDOR_ROOT "${IVAN_ROOT}/android/vendor")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_definitions(
    ANDROID
    UNIX
    USE_SDL
    IVAN_VERSION="0.59-android.5"
    PCRE_STATIC
    XBRZLIB_NOINLINEGETSETPIX
)

# SDL must remain shared because SDLActivity loads libSDL2 before libmain.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${VENDOR_ROOT}/SDL" "${CMAKE_BINARY_DIR}/vendor/SDL")

# IVAN only needs WAV sound effects and OGG music on Android.
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(SDL2MIXER_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_CMD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VENDORED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_GME OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3 OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VORBIS STB CACHE STRING "" FORCE)
set(SDL2MIXER_WAVE ON CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
add_subdirectory("${VENDOR_ROOT}/SDL_mixer" "${CMAKE_BINARY_DIR}/vendor/SDL_mixer")

# Build the small dependency surface directly, avoiding host package discovery.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory("${VENDOR_ROOT}/zlib" "${CMAKE_BINARY_DIR}/vendor/zlib")

configure_file(
    "${VENDOR_ROOT}/libpng/scripts/pnglibconf.h.prebuilt"
    "${CMAKE_BINARY_DIR}/generated/pnglibconf.h"
    COPYONLY
)
set(PNG_SOURCES
    png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c
    pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c
    pngwtran.c pngwutil.c
)
list(TRANSFORM PNG_SOURCES PREPEND "${VENDOR_ROOT}/libpng/")
if(ANDROID_ABI STREQUAL "arm64-v8a")
    list(APPEND PNG_SOURCES
        "${VENDOR_ROOT}/libpng/arm/arm_init.c"
        "${VENDOR_ROOT}/libpng/arm/filter_neon_intrinsics.c"
        "${VENDOR_ROOT}/libpng/arm/palette_neon_intrinsics.c"
    )
elseif(ANDROID_ABI STREQUAL "x86_64")
    list(APPEND PNG_SOURCES
        "${VENDOR_ROOT}/libpng/intel/intel_init.c"
        "${VENDOR_ROOT}/libpng/intel/filter_sse2_intrinsics.c"
    )
endif()
add_library(png_ivan STATIC ${PNG_SOURCES})
target_include_directories(png_ivan PUBLIC
    "${VENDOR_ROOT}/libpng"
    "${CMAKE_BINARY_DIR}/generated"
    "${VENDOR_ROOT}/zlib"
)
target_link_libraries(png_ivan PUBLIC zlibstatic)

set(PCRE_BUILD_PCRE8 ON CACHE BOOL "" FORCE)
set(PCRE_BUILD_PCRE16 OFF CACHE BOOL "" FORCE)
set(PCRE_BUILD_PCRE32 OFF CACHE BOOL "" FORCE)
set(PCRE_BUILD_PCRECPP OFF CACHE BOOL "" FORCE)
set(PCRE_BUILD_PCREGREP OFF CACHE BOOL "" FORCE)
set(PCRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${VENDOR_ROOT}/pcre" "${CMAKE_BINARY_DIR}/vendor/pcre")

file(GLOB IVAN_MAIN_SOURCES CONFIGURE_DEPENDS "${IVAN_ROOT}/Main/Source/*.cpp")
set(IVAN_INCLUDED_CPP
    action.cpp actions.cpp area.cpp bodypart.cpp char.cpp cont.cpp database.cpp
    entity.cpp fluid.cpp gear.cpp god.cpp gods.cpp human.cpp item.cpp level.cpp
    lsquare.cpp lterra.cpp lterras.cpp materia.cpp materias.cpp miscitem.cpp
    nonhuman.cpp pool.cpp proto.cpp rain.cpp room.cpp rooms.cpp slot.cpp smoke.cpp
    square.cpp stack.cpp team.cpp terra.cpp trap.cpp traps.cpp worldmap.cpp
    wsquare.cpp wterra.cpp wterras.cpp hiteffect.cpp cmdcraft.cpp
    cmdcraftfilters.cpp cmdswapweap.cpp
)
foreach(included_source IN LISTS IVAN_INCLUDED_CPP)
    list(REMOVE_ITEM IVAN_MAIN_SOURCES "${IVAN_ROOT}/Main/Source/${included_source}")
endforeach()

file(GLOB IVAN_FELIB_SOURCES CONFIGURE_DEPENDS "${IVAN_ROOT}/FeLib/Source/*.cpp")

add_library(main SHARED
    ${IVAN_MAIN_SOURCES}
    ${IVAN_FELIB_SOURCES}
    "${IVAN_ROOT}/audio/audio_android.cpp"
    "${IVAN_ROOT}/fantasyname/namegen.cc"
    "${IVAN_ROOT}/FastNoise/FastNoise.cpp"
    "${IVAN_ROOT}/xbrzscale/libxbrzscale.cpp"
    "${IVAN_ROOT}/xbrzscale/xbrz/xbrz.cpp"
)

target_include_directories(main PRIVATE
    "${IVAN_ROOT}/Main/Include"
    "${IVAN_ROOT}/FeLib/Include"
    "${IVAN_ROOT}/audio"
    "${IVAN_ROOT}/fantasyname"
    "${IVAN_ROOT}/FastNoise"
    "${IVAN_ROOT}/xbrzscale"
    "${IVAN_ROOT}/xbrzscale/xbrz"
    "${VENDOR_ROOT}/pcre"
    "${CMAKE_BINARY_DIR}/vendor/pcre"
    "${VENDOR_ROOT}/SDL_mixer/include"
)

target_compile_options(main PRIVATE
    -Wall
    -Wno-format-security
    -Wno-unused-parameter
    -Wno-sign-compare
)

target_link_libraries(main PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    SDL2_mixer
    png_ivan
    pcre
    zlibstatic
    android
    log
)
