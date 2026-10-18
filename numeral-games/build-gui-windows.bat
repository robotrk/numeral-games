@echo off
REM build-gui-windows.bat — сборка НЕконсольной GUI-версии (.exe без консоли) для Windows 10/11
REM Требуется: Visual Studio 2019/2022 ("Desktop development with C++")
REM           ИЛИ MinGW-w64 g++ в PATH.
REM Результат: build\numeral_games_gui.exe (нативный Win32 + GDI+, без внешних зависимостей).

where cl >nul 2>nul
if %errorlevel%==0 goto msvc

where g++ >nul 2>nul
if %errorlevel%==0 goto mingw

echo Не найден компилятор. Запустите из "x64 Native Tools Command Prompt for VS"
echo или добавьте MinGW-w64 g++ в PATH.
exit /b 1

:msvc
if not exist build mkdir build
REM Копируем иконку рядом с .rc, чтобы относительный путь в ресурсе работал
copy /Y numeral_games.ico src\win\numeral_games.ico >nul
rc /nologo /fo build\app_res.res src\win\app.rc
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /utf-8 /DUNICODE /D_UNICODE ^
   src\win\win_main.cpp ^
   /link /SUBSYSTEM:WINDOWS gdiplus.lib user32.lib gdi32.lib shell32.lib ^
   build\app_res.res ^
   /OUT:build\numeral_games_gui.exe
goto done

:mingw
if not exist build mkdir build
x86_64-w64-mingw32-windres -I src/win -I . src/win/app.rc -O coff -o build/app_res.o 2>nul || windres -I src/win -I . src/win/app.rc -O coff -o build/app_res.o || (echo Не найден windres & exit /b 1)
g++ -std=c++17 -O2 -mwindows -municode -DUNICODE -D_UNICODE -static ^
    src/win/win_main.cpp build/app_res.o ^
    -lgdiplus -luser32 -lgdi32 -lshell32 ^
    -o build/numeral_games_gui.exe

:done
echo Готово: build\numeral_games_gui.exe
