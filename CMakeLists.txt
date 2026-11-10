cmake_minimum_required(VERSION 3.20)
project(TerrainSurvivor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8 /W4 /permissive- /EHsc)
endif()

# SFML 3(rimrim 由来のビルド済みバイナリ)。DLL を同梱していないため静的リンクする。
set(SFML_STATIC_LIBRARIES TRUE)
set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SFML/lib/cmake/SFML")
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

# ヘッダも対象に入れる(Visual Studio のソリューションエクスプローラーに出すため)。System はヘッダだけのファイルが多い。
file(GLOB_RECURSE CORE_SRC CONFIGURE_DEPENDS src/ecs/*.cpp src/ecs/*.h src/core/*.cpp src/core/*.h src/game/*.cpp src/game/*.h)
add_library(ts_core STATIC ${CORE_SRC})
target_include_directories(ts_core PUBLIC src)

# デバッグ用 GUI(Dear ImGui + imgui-SFML。rimrim 由来)。ウィンドウを開く exe だけに入れ、ゲーム本体(ts_core)には入れない。
set(IMGUI_SRC
  third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp third_party/imgui-sfml/imgui-SFML.cpp)
if(MSVC)
  set_source_files_properties(${IMGUI_SRC} PROPERTIES COMPILE_OPTIONS "/W0")   # サードパーティは警告を出さない
endif()

set(APP_SRC src/app/main.cpp src/app/Renderer.cpp src/app/Renderer.h src/app/Art.cpp src/app/Art.h src/app/Effects.cpp src/app/Effects.h
  src/app/DebugUi.cpp src/app/DebugUi.h)
add_executable(terrain_survivor ${APP_SRC} ${IMGUI_SRC})
target_compile_definitions(terrain_survivor PRIVATE IMGUI_DISABLE_DEMO_WINDOWS)
target_link_libraries(terrain_survivor PRIVATE ts_core SFML::Graphics SFML::Window SFML::System opengl32)
target_include_directories(terrain_survivor PRIVATE third_party/SFML/include third_party/imgui third_party/imgui-sfml)

add_executable(ts_tests tests/tests.cpp)
target_link_libraries(ts_tests PRIVATE ts_core)

# ---- Visual Studio 向け ----
# 「フォルダーを開く」でも、build/TerrainSurvivor.sln を開いても、同じように使える。
#  ・F5 で動かすゲームは terrain_survivor(スタートアップ プロジェクト)
#  ・作業ディレクトリはプロジェクト直下(data/ や Image/ を見つけるため)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT terrain_survivor)
set_target_properties(terrain_survivor ts_tests PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "src" FILES ${CORE_SRC} ${APP_SRC})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "third_party" FILES ${IMGUI_SRC})
