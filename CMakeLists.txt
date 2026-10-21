cmake_minimum_required(VERSION 3.21)
project(blueui VERSION 0.2.0 LANGUAGES C)

# BlueUI: a talking game UI in C. One library: input in; its state, its
# geometry and its voice (WAVs, Flite built in) out. Built alone (its demo,
# its tests) or inside a game (add_subdirectory / FetchContent: the library).
set(BLUEUI_TOP_LEVEL ${PROJECT_IS_TOP_LEVEL})
option(BLUEUI_WITH_FLITE "The voice built in: Flite (else lines handed over as text)" ON)
option(BLUEUI_SHARED "A shared library (libblueui.so), linked by the game at run time" ON)
option(BLUEUI_BUILD_DEMO "The demo (SDL3: a window, its audio)" ${BLUEUI_TOP_LEVEL})
option(BLUEUI_BUILD_TESTS "The tests (ctest)" ${BLUEUI_TOP_LEVEL})

if(BLUEUI_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(BLUEUI_SHARED)
  set(BLUEUI_LINKAGE SHARED)
else()
  set(BLUEUI_LINKAGE STATIC)
endif()
find_package(Threads REQUIRED)

# ------------------------------------------------------------ speech: Flite
if(BLUEUI_WITH_FLITE)
  include(FetchContent)
  # CMU's Flite (BSD-like), its sources: no audio device of its own, no
  # sockets (its waves handed to the game's). The engine, US English, the
  # CMU lexicon, one voice (slt). (The lexicon's data files #included by
  # others, not built alone.)
  FetchContent_Declare(
    blueui_flite_src
    URL      https://github.com/festvox/flite/archive/refs/tags/v2.2.tar.gz
    URL_HASH SHA256=ab1555fe5adc3f99f1d4a1a0eb1596d329fd6d74f1464a0097c81f53c0cf9e5c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR none
  )
  FetchContent_MakeAvailable(blueui_flite_src)
  set(FLITE_DIR "${blueui_flite_src_SOURCE_DIR}")
  # (Its notice: for a game to ship beside it, as its licence asks.)
  set(BLUEUI_FLITE_LICENSE "${FLITE_DIR}/COPYING" CACHE INTERNAL "Flite's licence (COPYING)")
  set(FLITE_DIRS src/utils src/hrg src/lexicon src/regex src/speech src/stats src/synth src/wavesynth src/cg
                 lang/usenglish lang/cmulex lang/cmu_us_slt)
  set(FLITE_SOURCES)
  foreach(d IN LISTS FLITE_DIRS)
    file(GLOB d_sources "${FLITE_DIR}/${d}/*.c")
    list(APPEND FLITE_SOURCES ${d_sources})
  endforeach()
  list(FILTER FLITE_SOURCES EXCLUDE REGEX "/cst_(file|mmap)_[a-z0-9]+\\.c$|/cmu_lex_(data_raw|num_bytes|entries_huff_table|phones_huff_table)\\.c$")
  list(APPEND FLITE_SOURCES
    "${FLITE_DIR}/src/utils/cst_file_stdio.c"
    "${FLITE_DIR}/src/utils/cst_mmap_none.c"
    "${FLITE_DIR}/src/audio/audio.c"
    "${FLITE_DIR}/src/audio/au_streaming.c"
    "${FLITE_DIR}/src/audio/au_none.c")
  add_library(blueui_flite_engine STATIC ${FLITE_SOURCES})
  target_include_directories(blueui_flite_engine SYSTEM PUBLIC "${FLITE_DIR}/include" "${FLITE_DIR}/lang/cmu_us_slt")
  target_include_directories(blueui_flite_engine PRIVATE "${FLITE_DIR}/src/audio" "${FLITE_DIR}/src/regex"
    "${FLITE_DIR}/lang/usenglish" "${FLITE_DIR}/lang/cmulex")
  target_compile_definitions(blueui_flite_engine PRIVATE CST_AUDIO_NONE CST_NO_SOCKETS)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(blueui_flite_engine PRIVATE -w)
  endif()
  set_target_properties(blueui_flite_engine PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
endif()

# ------------------------------------------------------------ the library
add_library(blueui ${BLUEUI_LINKAGE} src/context.c src/voice.c src/font.c src/widgets.c)
target_include_directories(blueui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_target_properties(blueui PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON
  C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON
  VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_compile_definitions(blueui PRIVATE BLUEUI_BUILDING)
if(BLUEUI_SHARED)
  target_compile_definitions(blueui INTERFACE BLUEUI_DLL)
endif()
target_link_libraries(blueui PRIVATE Threads::Threads)
if(UNIX)
  target_link_libraries(blueui PRIVATE m)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(blueui PRIVATE -Wall -Wextra)
endif()
if(BLUEUI_WITH_FLITE)
  target_compile_definitions(blueui PRIVATE BLUEUI_WITH_FLITE)
  target_link_libraries(blueui PRIVATE blueui_flite_engine)
endif()

# ------------------------------------------------------------ the demo, the tests
if(BLUEUI_BUILD_DEMO)
  find_package(SDL3 CONFIG QUIET)
  if(TARGET SDL3::SDL3)
    set(BLUEUI_DEMO_SDL3 SDL3::SDL3)
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(BLUEUI_DEMO_SDL3_PC REQUIRED IMPORTED_TARGET sdl3)
    set(BLUEUI_DEMO_SDL3 PkgConfig::BLUEUI_DEMO_SDL3_PC)
  endif()
  add_executable(blueui_demo demo/main.c)
  set_target_properties(blueui_demo PROPERTIES C_STANDARD 11)
  target_link_libraries(blueui_demo PRIVATE blueui ${BLUEUI_DEMO_SDL3} m)
endif()

if(BLUEUI_BUILD_TESTS)
  enable_testing()
  add_executable(blueui_tests tests/tests.c)
  set_target_properties(blueui_tests PROPERTIES C_STANDARD 11)
  target_link_libraries(blueui_tests PRIVATE blueui m)
  add_test(NAME blueui_tests COMMAND blueui_tests)
endif()
