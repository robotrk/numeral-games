@echo off
REM build-windows.bat — сборка portable x86-64 .exe на Windows 10/11
REM Требуется: Visual Studio 2019/2022 (Desktop C++) или MinGW-w64 g++ в PATH.

where cl >nul 2>nul
if %errorlevel%==0 goto msvc

where g++ >nul 2>nul
if %errorlevel%==0 goto mingw

echo Не найден компилятор. Запустите из "x64 Native Tools Command Prompt for VS"
echo или добавьте MinGW-w64 g++ в PATH.
exit /b 1

:msvc
if not exist build mkdir build
cl /nologo /EHsc /std:c++17 /O2 /utf-8 /W4 ^
   src\main.cpp src\game.cpp src\platform_windows.cpp ^
   /Fe:build\numeral_games.exe /Fo:build\
goto done

:mingw
if not exist build mkdir build
g++ -std=c++17 -O2 -Wall -Wextra -static ^
    src/main.cpp src/game.cpp src/platform_windows.cpp ^
    -o build/numeral_games.exe

:done
echo Готово: build\numeral_games.exe
