// platform_linux.cpp — реализация для Linux (termios raw-mode)
#include "platform.h"

#if defined(NG_LINUX)

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <cstdio>
#include <cstring>

namespace platform {

void initConsole() {
    // UTF-8 в терминале обычно по умолчанию; ничего специального не нужно.
}

int readKey() {
    termios oldt{}, newt{};
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN]  = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    unsigned char buf[8];
    int n = 0;
    while (n < 1) { if (read(STDIN_FILENO, buf + n, 1) <= 0) break; n++; }

    // Escape-последовательности: ESC [ A/B/C/D
    if (n == 1 && buf[0] == 0x1b) {
        unsigned char rest[2] = {0, 0};
        termios t{};
        tcgetattr(STDIN_FILENO, &t);
        t.c_lflag &= ~ICANON;
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 1;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
        int r1 = read(STDIN_FILENO, rest, 1);
        int r2 = r1 == 1 ? read(STDIN_FILENO, rest + 1, 1) : 0;

        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);

        if (r1 == 1 && rest[0] == '[' && r2 == 1) {
            switch (rest[1]) {
                case 'A': tcsetattr(STDIN_FILENO, TCSANOW, &oldt); return KEY_UP;
                case 'B': tcsetattr(STDIN_FILENO, TCSANOW, &oldt); return KEY_DOWN;
                case 'C': tcsetattr(STDIN_FILENO, TCSANOW, &oldt); return KEY_RIGHT;
                case 'D': tcsetattr(STDIN_FILENO, TCSANOW, &oldt); return KEY_LEFT;
            }
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return KEY_ESC;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    unsigned char c = buf[0];
    if (c == '\r' || c == '\n') return KEY_ENTER;
    if (c == 0x7f || c == 0x08) return KEY_BACKSPACE;
    if (c == 0x1b) return KEY_ESC;
    return (int)c;
}

int readKeyTimeout(int timeoutMs) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    int r = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    if (r <= 0) return KEY_NONE;
    return readKey();
}

} // namespace platform

#endif // NG_LINUX
