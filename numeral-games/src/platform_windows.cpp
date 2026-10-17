// platform_windows.cpp — реализация для Windows 10/11 (Console API, Wide-ввод для кириллицы)
#include "platform.h"

#if defined(NG_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

namespace platform {

void initConsole() {
    // UTF-8 для вывода (кириллица в исходниках в кодировке UTF-8)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Включаем ANSI/VT-последовательности (Windows 10+)
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // Отключаем quick-edit, чтобы клики мыши не блокировали ввод
    HANDLE hi = GetStdHandle(STD_INPUT_HANDLE);
    DWORD imode = 0;
    if (GetConsoleMode(hi, &imode))
        SetConsoleMode(hi, (imode & ~(ENABLE_QUICK_EDIT_MODE | ENABLE_INSERT_MODE))
                              | ENABLE_EXTENDED_FLAGS);
}

int readKey() {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD rec;
    DWORD got = 0;

    for (;;) {
        if (!ReadConsoleInputW(h, &rec, 1, &got) || got != 1) return KEY_NONE;
        if (rec.EventType != KEY_EVENT) continue;
        const KEY_EVENT_RECORD& k = rec.Event.KeyEvent;
        if (!k.bKeyDown) continue;

        WORD vk = k.wVirtualKeyCode;
        switch (vk) {
            case VK_UP:     return KEY_UP;
            case VK_DOWN:   return KEY_DOWN;
            case VK_LEFT:   return KEY_LEFT;
            case VK_RIGHT:  return KEY_RIGHT;
            case VK_RETURN: return KEY_ENTER;
            case VK_ESCAPE: return KEY_ESC;
            case VK_BACK:   return KEY_BACKSPACE;
            default: break;
        }

        // Символ через Unicode-чтение — корректно работает и с русской раскладкой
        WCHAR uc = k.uChar.UnicodeChar;
        if (uc >= 32 && uc < 127) return (int)uc;
        return KEY_NONE;
    }
}

int readKeyTimeout(int timeoutMs) {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD avail = 0;
    auto deadline = GetTickCount64() + (DWORD)timeoutMs;
    for (;;) {
        if (GetNumberOfConsoleInputEvents(h, &avail) && avail > 0)
            return readKey();
        if (GetTickCount64() >= deadline) return KEY_NONE;
        Sleep(10);
    }
}

} // namespace platform

#endif // NG_WINDOWS
