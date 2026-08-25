cmake_minimum_required(VERSION 3.5)
project(ivan CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 59)
set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
#set(VERSION_PATCH 0)
#set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
add_definitions(-DIVAN_VERSION="${PROJECT_VERSION}" -DUSE_SDL -DADAPTIVE_UI)

option(BUILD_MAC_APP "Build standalone application for MacOS" OFF)
option(USE_HOME_FOR_STATE_DIR "Statedir will be /.ivan/ in current user's homedir" OFF)
option(WIZARD "Enable Wizard Mode" OFF)
option(PORTABLE_BUILD "Keep game and user data in the directory IVAN is launched from" OFF)

if(UNIX)
  add_definitions(-DUNIX)
  if(NOT PORTABLE_BUILD)
  include(GNUInstallDirs)

  if(BUILD_MAC_APP)
    install(DIRECTORY Graphics Script Music Sound DESTINATION "ivan")
    install(FILES COPYING INSTALL LICENSING NEWS MANUAL README.md DESTINATION "docs")
    add_definitions(-DMAC_APP)
  else()
    install(DIRECTORY Graphics Script Music Sound DESTINATION "${CMAKE_INSTALL_DATADIR}/ivan")
    add_definitions(-DDATADIR="${CMAKE_INSTALL_FULL_DATADIR}")

    message("\n*NOTICE*\n"
            "    Highscore files and Bone files are now stored at \"\$HOME/.ivan\".\n"
            "    Your HOME directory is \"$ENV{HOME}\".\n")
    if(IS_DIRECTORY "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/ivan")
      message(WARNING
              "If you have game data in \"${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/ivan\","
              " please move them to \"$ENV{HOME}/.ivan\".")
    endif()
  endif()
  endif()

elseif(WIN32)
  install(DIRECTORY Graphics Script Music Sound DESTINATION "ivan")
  install(FILES COPYING INSTALL LICENSING NEWS MANUAL README.md DESTINATION "ivan")
  add_definitions(-DWIN32)

  if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS -mwindows)
  endif(MINGW)

  # This is bad. To remove below defines, FeLib should switch to std::chrono and serialize time since common epoch.
  # 32 bit TIME_T has year 2038 problem. Moreover, serialized value is not portable for time_t in general.
  # Switching to std::chrono should not be hard, but it's not a topic for this pull request.
  add_definitions(-D_USE_32BIT_TIME_T)
endif(UNIX)

# In visual studio 2017, add an option to hide the console window (useful where debugging is not intended - i.e. releases)
if(MSVC AND MSVC_HIDE_CONSOLE_WINDOW)
  # Hide the console window in visual studio projects
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

if(WIZARD)
  add_definitions(-DWIZARD)
endif()

if(PORTABLE_BUILD)
  add_definitions(-DPORTABLE_BUILD)
endif()

find_path(EXECINFO_INCLUDE_DIR NAMES execinfo.h)
if(NOT EXECINFO_INCLUDE_DIR MATCHES "-NOTFOUND$")
  add_definitions(-DBACKTRACE)
endif()

add_subdirectory(FeLib)
add_subdirectory(audio)
add_subdirectory(Main)
add_subdirectory(igor)
add_subdirectory(mihail)
add_subdirectory(xbrzscale)
add_subdirectory(fantasyname)
add_subdirectory(FastNoise)

include(CTest)
if(BUILD_TESTING)
  find_package(SDL2 REQUIRED)
  add_executable(adaptiveui_layout_test
                 Tests/adaptiveui_layout_test.cpp
                 FeLib/Source/adaptiveui.cpp)
  target_include_directories(adaptiveui_layout_test PRIVATE
                             FeLib/Include ${SDL2_INCLUDE_DIR})
  target_link_libraries(adaptiveui_layout_test ${SDL2_LIBRARY})
  if(MSVC)
    target_compile_options(adaptiveui_layout_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(adaptiveui_layout_test PRIVATE -UNDEBUG)
  endif()
  add_test(NAME adaptiveui_layout_test COMMAND adaptiveui_layout_test)
  if(WIN32)
    # The portable i686 runtime DLLs are staged beside ivan.exe. Run the test
    # there as well so Windows does not pick up incompatible DLLs from PATH.
    set_tests_properties(adaptiveui_layout_test PROPERTIES
                         WORKING_DIRECTORY "$<TARGET_FILE_DIR:ivan>")
  endif()
endif()
