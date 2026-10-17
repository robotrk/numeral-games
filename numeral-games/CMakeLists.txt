cmake_minimum_required(VERSION 3.16)
project(numeral_games LANGUAGES CXX VERSION 1.0)

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

install(TARGETS numeral_games RUNTIME DESTINATION .)
