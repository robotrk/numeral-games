// main.cpp — меню выбора игр (порт веб-страницы «flock» с кнопками)
#include "game.h"
#include "game_config.h"
#include "platform.h"

#include <cstdio>
#include <string>
#include <vector>

static void showMenu(const std::vector<GameConfig>& games) {
    const char* clear = "\x1b[2J\x1b[H";
    std::string s = clear;
    s += "\x1b[1m╔══════════════════════════════════════════╗\x1b[0m\n";
    s += "\x1b[1m║   ИГРЫ ПО СИСТЕМАМ СЧИСЛЕНИЯ (C++ порт)  ║\x1b[0m\n";
    s += "\x1b[1m╚══════════════════════════════════════════╝\x1b[0m\n\n";
    for (size_t i = 0; i < games.size(); ++i) {
        char btn[64];
        snprintf(btn, sizeof(btn), "%d → %d", games[i].fromBase, games[i].toBase);
        s += "  \x1b[1;35m[" + std::to_string(i + 1) + "]\x1b[0m  " + btn +
             "   (" + games[i].desc + ", уровней: " +
             std::to_string(games[i].levelCount) + ")\n";
    }
    s += "\n  \x1b[1;35m[Q]\x1b[0m  Выход\n";
    s += "\nВыберите игру (цифра) или Q для выхода: ";
    fwrite(s.data(), 1, s.size(), stdout);
    fflush(stdout);
}

int main() {
    platform::initConsole();

    auto games = makeGames();

    for (;;) {
        showMenu(games);
        int key = platform::readKey();

        if (key == platform::KEY_ESC) break;
        char c = (char)key;
        if (c == 'q' || c == 'Q') break; // Q — выход (и 'й' на русской раскладке в Windows)

        int idx = -1;
        if (c >= '1' && c <= '9') idx = (c - '1');
        if (idx >= 0 && idx < (int)games.size()) {
            Game g(games[idx]);
            g.run();
        }
    }

    std::string bye = "\x1b[2J\x1b[HДо свидания!\n";
    fwrite(bye.data(), 1, bye.size(), stdout);
    return 0;
}
