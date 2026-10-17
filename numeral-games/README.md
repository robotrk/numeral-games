# Numeral Games (C++ порт)

Настольная (консольная) версия игр по системам счисления, портированная с веб-страницы.
Портативные бинарники **x86-64** для **Linux** и **Windows 10/11**, без внешних зависимостей.

## Игры (как в веб-версии)

| № | Игра            | Уровней | Особенности |
|---|-----------------|---------|-------------|
| 1 | 8 → 2           | 10      | с 7 уровня +3 разряда |
| 2 | 16 → 2          | 10      | с 7 уровня +4 разряда |
| 3 | 2 → 8           | 10      | двоичное число группируется по 3 бита |
| 4 | 2 → 16          | 10      | группировка по 4 бита, ответ hex |
| 5 | 10 → 2          | 13      | над ячейками показаны степени двойки |
| 6 | 2 → 10          | 10      | ответ десятичный |

Сохранена логика оригинала: таймер на уровень (уменьшается с каждым уровнем),
автопроверка при заполнении ячеек, «ЕСТЬ ОШИБКА», рестарт с 1 уровня при
истечении времени, выход в меню (Esc).

## Управление

- `←` / `→` — выбор ячейки (как Tab/стрелки в веб-версии)
- `↑` / `↓` — увеличить/уменьшить цифру в ячейке (циклически по основанию)
- цифры `0-9`, `A-F` — прямой ввод (принимаются только допустимые для основания)
- `Enter` — принудительная проверка
- `Esc` — вернуться в меню

## Сборка

### Linux x86-64

```bash
./build-linux.sh            # cmake + g++, результат: build/numeral_games
# или просто:
g++ -std=c++17 -O2 -static-libstdc++ -static-libgcc \
    src/main.cpp src/game.cpp src/platform_linux.cpp -o numeral_games
```

Бинарник статически линкует libstdc++/libgcc → переносим между дистрибутивами.

### Windows 10/11 x86-64

Вариант 1 — Visual Studio: откройте `x64 Native Tools Command Prompt for VS 2022`
и выполните `build-windows.bat`.

Вариант 2 — MinGW-w64:

```bat
build-windows.bat
:: или вручную:
g++ -std=c++17 -O2 -static src/main.cpp src/game.cpp src/platform_windows.cpp -o numeral_games.exe
```

Флаг `-static` делает .exe полностью самодостаточным (portable).

### Кросс-сборка .exe из-под Linux (для CI)

```bash
x86_64-w64-mingw32-g++ -std=c++17 -O2 -static \
    src/main.cpp src/game.cpp src/platform_windows.cpp -o numeral_games.exe
```

## Структура

```
src/
  platform.h            — абстракция консоли (readKey / readKeyTimeout)
  platform_linux.cpp    — termios + select
  platform_windows.cpp  — WinAPI Console (ReadConsoleInputW, UTF-8, VT-коды)
  game_config.h         — конфигурация 6 игр (порт GAME_CONFIG из JS)
  game.h/.cpp           — игровая логика (порт flevel/draw/gameState/keyDownHandler)
  main.cpp              — меню выбора игры (порт #flock)
CMakeLists.txt          — единая сборка для обеих платформ
```

На Windows поддерживается кириллический ввод раскладки (ReadConsoleInputW),
на Linux — ANSI-раскрашивание через терминал с UTF-8.
