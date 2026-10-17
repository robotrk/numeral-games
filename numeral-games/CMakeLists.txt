cmake_minimum_required(VERSION 3.16)
project(numeral_games LANGUAGES CXX VERSION 1.0.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(numeral_games
    src/main.cpp
    src/game.cpp
    src/platform_linux.cpp
    src/platform_windows.cpp
)

if(MSVC)
    target_compile_options(numeral_games PRIVATE /W4 /utf-8 /O2)
else()
    target_compile_options(numeral_games PRIVATE -Wall -Wextra -O2)
endif()

# Статическая линковка libstdc++/libgcc на Linux для переносимости
if(UNIX AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(numeral_games PRIVATE -static-libstdc++ -static-libgcc)
endif()

# ---------------------------------------------------------------------------
# GUI-версия (неконсольное приложение): Linux — GTK3 (KDE Plasma, XFCE и др.)
# ---------------------------------------------------------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(GTK3 QUIET gtk+-3.0)
endif()

if(GTK3_FOUND AND NOT WIN32)
    add_executable(numeral_games_gui WIN32 MACOSX_BUNDLE
        src-gui/gui_main.cpp
    )
    target_include_directories(numeral_games_gui PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(numeral_games_gui PRIVATE ${GTK3_LIBRARIES})
    target_link_directories(numeral_games_gui PRIVATE ${GTK3_LIBRARY_DIRS})
    # общее ядро игр: только заголовки src-core/, без доп. компиляции
    target_include_directories(numeral_games_gui PRIVATE src-core)
    if(MSVC)
        target_compile_options(numeral_games_gui PRIVATE /utf-8)
    endif()
    install(TARGETS numeral_games_gui RUNTIME DESTINATION . BUNDLE DESTINATION .)
    message(STATUS "GUI (GTK3): включена — цель numeral_games_gui")
else()
    message(STATUS "GUI (GTK3): пропущена (gtk+-3.0 не найден или это Windows)")
endif()

install(TARGETS numeral_games RUNTIME DESTINATION .)
