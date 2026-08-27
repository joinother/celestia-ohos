# CMakeLists.txt for the Celestia headless CLI (celestia-cli).
#
# Builds a standalone command line program that links the Celestia core
# libraries plus the Qt6 Core/Gui/OpenGL/Widgets modules for off-screen
# rendering (QOffscreenSurface + FBO). No Celestia GUI frontend is needed.

option(CELESTIA_BUILD_CLI "Build the headless celestia-cli program (default: on)" ON)

if(NOT CELESTIA_BUILD_CLI)
  return()
endif()

# The CLI is a developer-machine tool; skip it on HarmonyOS app builds.
if(CELESTIA_OHOS)
  message(STATUS "celestia-cli: disabled on HarmonyOS builds")
  return()
endif()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)

add_executable(celestia-cli
  celestia-cli.cpp
  ${CMAKE_SOURCE_DIR}/src/celestia/qt/qtcli.cpp
)

target_include_directories(celestia-cli PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(celestia-cli PRIVATE
  celestia
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
)
