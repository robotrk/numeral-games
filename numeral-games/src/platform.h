// platform.h — кроссплатформенные примитивы (Linux x86-64 / Windows 10-11 x86-64)
#pragma once

#include <string>

#if defined(_WIN32)
  #define NG_WINDOWS 1
#else
  #define NG_LINUX 1
#endif

namespace platform {

// Инициализация консоли (UTF-8, включение ANSI/VT-кодов).
void initConsole();

// Чтение одного нажатия клавиши без ожидания Enter.
// Возвращает:
//   - код символа (для печатаемых клавиш, в т.ч. кириллица через ReadConsoleW)
//   - спецкоды: KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_ESC, KEY_BACKSPACE
// Блокируется до нажатия.
int readKey();

// То же, но с таймаутом; возвращает KEY_NONE, если за timeoutMs мс ничего не нажато.
int readKeyTimeout(int timeoutMs);

constexpr int KEY_NONE      = 0;
constexpr int KEY_UP        = -1;
constexpr int KEY_DOWN      = -2;
constexpr int KEY_LEFT      = -3;
constexpr int KEY_RIGHT     = -4;
constexpr int KEY_ENTER     = -5;
constexpr int KEY_ESC       = -6;
constexpr int KEY_BACKSPACE = -7;

} // namespace platform
