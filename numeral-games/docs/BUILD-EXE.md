# Как создать .exe файл (Windows 10/11, x86-64)

Портативный `.exe` собирается двумя способами: через **MSYS2/MinGW-w64** (рекомендуется, работает и из-под Linux — кросс-компиляция) или через **Visual Studio**.

---

## Способ 1. MSYS2 + MinGW-w64 (рекомендуемый)

### 1. Установка инструментов (на Windows)

1. Скачайте и установите [MSYS2](https://www.msys2.org/) (`msys2-x86_64-*.exe`).
2. Откройте ярлык **"MSYS2 MinGW x64"** и выполните:

```bash
pacman -Syu                       # обновление базы (перезапустите терминал, если попросит)
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake make
```

### 2. Сборка консольной версии

```bash
cd numeral-games
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build . --config Release
```

Готовый файл: `build/numeral_games.exe`.

Чтобы exe был **полностью портативным** (не требовал DLL компилятора), соберите со статической линковкой:

```bash
g++ -std=c++17 -O2 -static -mwindows ../src/*.cpp -o numeral_games.exe
```

Флаг `-static` вшивает libgcc/libstdc++ в exe; `-mwindows` убирает чёрное окно консоли (для GUI-версии).

### 3. Сборка GUI-версии (GTK3)

```bash
pacman -S mingw-w64-x86_64-gtk3
cd numeral-games
g++ -std=c++17 -O2 -mwindows src-gui/gui_main.cpp $(pkg-config --cflags --libs gtk+-3.0) -o numeral_games_gui.exe
```

GUI на GTK3 требует рядом DLL GTK. Чтобы получить один portable-файл, либо распространяйте папку с exe + DLL из `/mingw64/bin/`, либо используйте способ 2 (Win32 API — не требует никаких DLL).

### 4. Кросс-компиляция .exe из-под Linux (так собран `release/numeral_games-win-x64.exe`)

```bash
sudo apt install g++-mingw-w64-x86-64
x86_64-w64-mingw32-g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ \
    src/*.cpp -o release/numeral_games-win-x64.exe -lpthread
```

---

## Способ 2. Visual Studio 2022

1. Установите VS 2022 с workload **"Desktop development with C++"**.
2. Откройте **"Developer Command Prompt for VS 2022"**:

```bat
cd numeral-games
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

3. Готовый exe: `build/Release/numeral_games.exe`.

Для портативности без VC++ Redistributable добавьте статическую линковку рантайма:

```bat
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_CXX_FLAGS="/MT"
```

---

## Способ 3. Чистый Win32 API (один exe без DLL, неконсольный)

Если нужен GUI-экран без установки GTK, используйте `src-gui/win32_shell.c`:

```bat
:: из MSYS2 MinGW x64:
x86_64-w64-mingw32-gcc -O2 -mwindows win32_shell.c -o numeral_games_win32.exe -lgdi32 -luser32 -lcomctl32
```

или в cl.exe (Visual Studio):

```bat
cl /O2 /W3 win32_shell.c /Fe:numeral_games_win32.exe user32.lib gdi32.lib comctl32.lib /link /SUBSYSTEM:WINDOWS
```

Такой exe весит ~50–100 КБ, запускается двойным кликом и не требует ничего, кроме Windows 10/11.

---

## Проверка результата

```bash
file numeral_games.exe          # должен показать: PE32+ executable (console/GUI) x86-64
```

Перенесите exe на другую машину с чистой Windows 10/11 — он обязан запуститься без установки чего-либо дополнительно (это и есть «портативная версия»).

## Типичные ошибки

| Ошибка | Причина / решение |
|---|---|
| `libgcc_s_seh-1.dll not found` | Собрано без `-static`. Пересоберите с флагом `-static`. |
| `VCRUNTIME140.dll missing` | В VS используйте флаг `/MT` вместо `/MD`. |
| Окно консоли мелькает перед запуском | Добавьте `-mwindows` (MinGW) или `/SUBSYSTEM:WINDOWS` (MSVC). |
| Антивирус ругается на exe | Обычная реакция на не подписанные static-бинарники; убедитесь, что собираете из исходников проекта. |
