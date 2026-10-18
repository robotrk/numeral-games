cmake_minimum_required(VERSION 3.16)
project(numeral_games LANGUAGES CXX VERSION 1.0.4)

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
    # ---- Иконка Linux: PNG вшивается в бинарник как base64-заголовок ----
    set(ICON_PNG "${CMAKE_CURRENT_SOURCE_DIR}/numeral_games.png")
    set(GEN_ICON_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY ${GEN_ICON_DIR})
    if(EXISTS "${ICON_PNG}")
        add_custom_command(
            OUTPUT "${GEN_ICON_DIR}/icon_data.h"
            COMMAND ${CMAKE_COMMAND} -DPNG=${ICON_PNG} -DOUT=${GEN_ICON_DIR}/icon_data.h
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_icon_header.cmake
            DEPENDS ${ICON_PNG} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_icon_header.cmake
            COMMENT "Генерация иконки (PNG -> base64 заголовок)")
        set_source_files_properties(src-gui/gui_main.cpp PROPERTIES OBJECT_DEPENDS "${GEN_ICON_DIR}/icon_data.h")
    endif()

    add_executable(numeral_games_gui WIN32 MACOSX_BUNDLE
        src-gui/gui_main.cpp
    )
    target_include_directories(numeral_games_gui PRIVATE ${GTK3_INCLUDE_DIRS} ${GEN_ICON_DIR})
    target_link_libraries(numeral_games_gui PRIVATE ${GTK3_LIBRARIES})
    target_link_directories(numeral_games_gui PRIVATE ${GTK3_LIBRARY_DIRS})
    # общее ядро игр: только заголовки src-core/, без доп. компиляции
    target_include_directories(numeral_games_gui PRIVATE src-core)
    if(MSVC)
        target_compile_options(numeral_games_gui PRIVATE /utf-8)
    endif()
    install(TARGETS numeral_games_gui RUNTIME DESTINATION . BUNDLE DESTINATION .)
    # установка png рядом (для .desktop-файлов/тем, если понадобится)
    if(EXISTS "${ICON_PNG}")
        install(FILES ${ICON_PNG} DESTINATION share/pixmaps)
    endif()
    message(STATUS "GUI (GTK3): включена — цель numeral_games_gui")
else()
    message(STATUS "GUI (GTK3): пропущена (gtk+-3.0 не найден или это Windows)")
endif()

# ---------------------------------------------------------------------------
# GUI-версия для Windows: Win32 + GDI+ (src/win/win_main.cpp), иконка из app.rc
# ---------------------------------------------------------------------------
if(WIN32)
    set(ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/numeral_games.ico")
    set(WIN_GUI_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/win/win_main.cpp")
    if(EXISTS ${WIN_GUI_SRC})
        if(MSVC)
            # MSVC/rc.exe принимает .ico напрямую как исходник цели
            add_executable(numeral_games_gui_win WIN32
                ${WIN_GUI_SRC}
                ${ICON_ICO}
            )
        else()
            # MinGW-GCC: компилируем app.rc (windres) и прикладываем объект
            enable_language(RC)
            add_library(numeral_games_res OBJECT src/win/app.rc)
            target_include_directories(numeral_games_res PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
            add_executable(numeral_games_gui_win WIN32
                ${WIN_GUI_SRC}
                $<TARGET_OBJECTS:numeral_games_res>
            )
        endif()
        target_include_directories(numeral_games_gui_win PRIVATE src-core)
        if(MSVC)
            target_compile_options(numeral_games_gui_win PRIVATE /std:c++17 /EHsc /O2 /utf-8 /DUNICODE /D_UNICODE)
            target_link_options(numeral_games_gui_win PRIVATE /SUBSYSTEM:WINDOWS)
            target_link_libraries(numeral_games_gui_win PRIVATE gdiplus user32 gdi32 shell32)
        else()
            target_compile_options(numeral_games_gui_win PRIVATE -std=c++17 -O2 -mwindows -municode -DUNICODE -D_UNICODE)
            target_link_options(numeral_games_gui_win PRIVATE -mwindows -static)
            target_link_libraries(numeral_games_gui_win PRIVATE gdiplus user32 gdi32 shell32)
        endif()
        install(TARGETS numeral_games_gui_win RUNTIME DESTINATION .)
        message(STATUS "GUI (Win32): включена — цель numeral_games_gui_win (иконка: ${ICON_ICO})")
    endif()
endif()

install(TARGETS numeral_games RUNTIME DESTINATION .)
