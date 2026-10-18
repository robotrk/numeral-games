// game_core.h — платформенно-независимое ядро игр по системам счисления.
// Используется GTK3-версией (Linux: KDE Plasma, XFCE) и Win32/GDI+ версией (Windows 10/11).
// Порт логики веб-версии: 6 режимов, уровни, таймеры, поле ячеек.
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace core {

// ===================== КОНФИГУРАЦИЯ =====================
struct GameConfig {
    int id;
    int fromBase, toBase, inputBase;
    int maxDigits, levelCount;
    double startTimer, timerStep;
    // Число ячеек ввода на уровне (если не задано — maxDigits с ростом на 7+ уровне).
    // Игра 2 → 10: уровни 1-5 — 2 ячейки, уровни 6+ — 3 ячейки.
    std::function<int(int)> digitsForLevel;
    const char* title;   // подпись кнопки в меню (UTF-8)
    const char* desc;    // описание на игровом экране (UTF-8)
    std::function<std::pair<int,int>(int)> numRange;
    bool showPowers;     // показывать веса разрядов НАД ЗАДАНИЕМ (строкой числа)
                         // (игра 2 → 10: 64 32 16 8 4 2 1 / 128 ... 1 — над двоичным числом)
    bool showPowersGrid; // показывать веса разрядов над сеткой ячеек ввода
                         // (игра 10 → 2: 128 64 32 16 8 4 2 1 над двоичными кнопками)
};

inline std::vector<GameConfig> games() {
    std::vector<GameConfig> g;
    g.push_back({1, 8, 2, 2, 9, 10, 31, 2, nullptr, "8 \xE2\x86\x92 2",
        "\xD0\x9D\xD1\x83\xD0\xB6\xD0\xBD\xD0\xBE \xD0\xBF\xD0\xB5\xD1\x80\xD0\xB5\xD0\xB2\xD0\xB5\xD1\x81\xD1\x82\xD0\xB8 \xD1\x87\xD0\xB8\xD1\x81\xD0\xBB\xD0\xBE \xD0\xB8\xD0\xB7 8 \xD0\xA1.\xD0\xA1. \xD0\xB2 2 \xD0\xA1.\xD0\xA1.",
        [](int){ return std::make_pair(64, 511); }, false, false});
    g.push_back({2, 16, 2, 2, 12, 10, 35, 1, nullptr, "16 \xE2\x86\x92 2",
        "\xD0\x9D\xD1\x83\xD0\xB6\xD0\xBD\xD0\xBE \xD0\xBF\xD0\xB5\xD1\x80\xD0\xB5\xD0\xB2\xD0\xB5\xD1\x81\xD1\x82\xD0\xB8 \xD1\x87\xD0\xB8\xD1\x81\xD0\xBB\xD0\xBE \xD0\xB8\xD0\xB7 16 \xD0\xA1.\xD0\xA1. \xD0\xB2 2 \xD0\xA1.\xD0\xA1.",
        [](int){ return std::make_pair(256, 4095); }, false, false});
    g.push_back({3, 2, 8, 8, 3, 10, 35, 2, nullptr, "2 \xE2\x86\x92 8",
        "\xD0\x9D\xD1\x83\xD0\xB6\xD0\xBD\xD0\xBE \xD0\xBF\xD0\xB5\xD1\x80\xD0\xB5\xD0\xB2\xD0\xB5\xD1\x81\xD1\x82\xD0\xB8 \xD1\x87\xD0\xB8\xD1\x81\xD0\xBB\xD0\xBE \xD0\xB8\xD0\xB7 2 \xD0\xA1.\xD0\xA1. \xD0\xB2 8 \xD0\xA1.\xD0\xA1.",
        [](int lev){ return lev >= 7 ? std::make_pair(513, 4095) : std::make_pair(64, 511); }, false, false});
    g.push_back({4, 2, 16, 16, 3, 10, 35, 2, nullptr, "2 \xE2\x86\x92 16",
        "\xD0\x9D\xD1\x83\xD0\xB6\xD0\xBD\xD0\xBE \xD0\xBF\xD0\xB5\xD1\x80\xD0\xB5\xD0\xB2\xD0\xB5\xD1\x81\xD1\x82\xD0\xB8 \xD1\x87\xD0\xB8\xD1\x81\xD0\xBB\xD0\xBE \xD0\xB8\xD0\xB7 2 \xD0\xA1.\xD0\xA1. \xD0\xB2 16 \xD0\xA1.\xD0\xA1.",
        [](int lev){ return lev >= 7 ? std::make_pair(4097, 65534) : std::make_pair(256, 4095); }, false, false});
    g.push_back({5, 10, 2, 2, 8, 13, 31, 1, nullptr, "10 \xE2\x86\x92 2",
        "\xD0\x9D\xD1\x83\xD0\xB6\xD0\xBD\xD0\xBE \xD0\xBF\xD0\xB5\xD1\x80\xD0\xB5\xD0\xB2\xD0\xB5\xD1\x81\xD1\x82\xD0\xB8 \xD1\x87\xD0\xB8\xD1\x81\xD0\xBB\xD0\xBE \xD0\xB8\xD0\xB7 10 \xD0\xA1.\xD0\xA1. \xD0\xB2 2 \xD0\xA1.\xD0\xA1.",
        [](int lev){ return lev <= 5 ? std::make_pair(17, 127) : std::make_pair(129, 255); }, false, true});
    g.push_back({6, 2, 10, 10, 2, 10, 35, 2, [](int lev){ return lev <= 5 ? 2 : 3; }, "2 \xE2\x86\x92 10",
        "\xD0\x9D\xD1\x83\xD0\xB6\xD0\xBD\xD0\xBE \xD0\xBF\xD0\xB5\xD1\x80\xD0\xB5\xD0\xB2\xD0\xB5\xD1\x81\xD1\x82\xD0\xB8 \xD1\x87\xD0\xB8\xD1\x81\xD0\xBB\xD0\xBE \xD0\xB8\xD0\xB7 2 \xD0\xA1.\xD0\xA1. \xD0\xB2 10 \xD0\xA1.\xD0\xA1.",
        [](int lev) {
            if (lev == 1) return std::make_pair(8, 16);
            if (lev == 2) return std::make_pair(17, 32);
            if (lev == 3) return std::make_pair(33, 64);
            if (lev <= 5) return std::make_pair(65, 128);
            return std::make_pair(129, 511);
        }, true, false});
    return g;
}

// ===================== УТИЛИТЫ =====================
inline long rndLong(long a, long b) {           // [a, b] включительно
    static std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<long> d(a, b);
    return d(rng);
}

inline std::string toBase(long v, int base) {
    if (v == 0) return "0";
    const char* digits = "0123456789ABCDEF";
    std::string s;
    while (v > 0) { s.insert(s.begin(), digits[v % base]); v /= base; }
    return s;
}

inline std::string padStart(const std::string& s, size_t n) {
    if (s.size() >= n) return s;
    return std::string(n - s.size(), '0') + s;
}

inline std::string groupBits(const std::string& bin, int size) {
    std::string out;
    int cnt = 0;
    for (int i = (int)bin.size() - 1; i >= 0; --i) {
        out.insert(out.begin(), bin[i]);
        if (++cnt % size == 0 && i > 0) out.insert(out.begin(), ' ');
    }
    return out;
}

inline int digitValue(const std::string& t) {   // "A" -> 10, "" -> -1
    if (t.empty()) return -1;
    char c = t[0];
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

inline std::string digitText(int d, int base) {
    char buf[4];
    if (base == 16) snprintf(buf, sizeof buf, "%X", d);
    else            snprintf(buf, sizeof buf, "%d", d);
    return buf;
}

// ===================== СОСТОЯНИЕ ИГРЫ (без виджетов) =====================
// Полностью воспроизводит логику веб-версии: генерация уровня, проверка,
// обратный отсчёт, переходы между уровнями. UI-слой только отображает поля
// и вызывает методы при действиях пользователя.
class GameEngine {
public:
    enum class Screen { Menu, Game };
    enum class Status { Idle, Correct, Wrong, TimeoutWait, NextWait, Finished };

    // --- экраны ---
    Screen screen = Screen::Menu;
    bool inGame = false;

    void openMenu() { screen = Screen::Menu; inGame = false; }
    void startGame(int gameId) {
        if (!gamesReady_) games_ = games(), gamesReady_ = true;
        for (const auto& g : games_) {
            if (g.id == gameId) { cfg = &g; break; }
        }
        if (!cfg) return;
        screen = Screen::Game;
        inGame = true;
        startLevel(1);
    }

    // --- состояние игрового экрана ---
    const GameConfig* cfg = nullptr;
    int level = 1;
    int digits = 0;                 // число ячеек текущего уровня
    int cursor = 0;                 // активная ячейка (0..digits-1)
    long value10 = 0;               // загаданное число
    double timeLeft = 0;            // остаток времени уровня
    double timeFull = 0;            // время первого уровня (для прогресс-бара)
    Status status = Status::Idle;
    int waitCounter = 0;            // отсчёт перед следующим уровнем / рестартом
    std::vector<std::string> cells; // содержимое ячеек ("0".."F")

    std::string numberText() const { // то, что показано игроку
        if (!cfg) return "";
        if (cfg->id == 3) return groupBits(toBase(value10, 2), 3);
        if (cfg->id == 4) return groupBits(toBase(value10, 2), 4);
        return toBase(value10, cfg->fromBase);
    }

    // Живое значение, которое игрок набирает ячейками ответа
    // (показывается справа от задания, рядом со стрелкой):
    //  • режим 10 → 2: десятичное число, соответствующее набранной двоичной
    //    записи (для самопроверки);
    //  • режим 2 → 10: число, которое сейчас набрано в десятичных ячейках.
    // Пока все ячейки равны нулю (состояние по умолчанию), возвращается
    // пустая строка — чтобы «0» не путало игрока.
    std::string enteredValueText() const {
        if (!cfg || (cfg->id != 5 && cfg->id != 6)) return "";
        // состояние по умолчанию (все ячейки равны нулю) — пусто,
        // чтобы не путать игрока
        bool any = false;
        for (int i = 0; i < digits; ++i)
            if (digitValue(cells[i]) > 0) { any = true; break; }
        if (!any) return "";
        if (cfg->id == 6) {
            // режим 2 → 10: двоичное число из набранного десятичного значения
            long v = 0;
            for (int i = 0; i < digits; ++i)
                v = v * 10 + digitValue(cells[i]);
            return toBase(v, 2);
        }
        // режим 10 → 2: из набранной двоичной записи — десятичное значение
        long v = 0;
        for (int i = 0; i < digits; ++i) {
            int d = digitValue(cells[i]);
            v = v * 2 + (d > 0 ? 1 : 0);
        }
        char buf[32]; snprintf(buf, sizeof buf, "%ld", v);
        return buf;
    }

    // Режим 2 → 10: веса разрядов (степени двойки) НАД ЗАДАНИЕМ — над
    // показанным двоичным числом, ровно по числу его цифр (без пробелов):
    // уровень 1-6: "64 32 16 8 4 2 1", уровень 7+: "128 ... 1".
    std::string taskWeightsText() const {
        if (!cfg || !cfg->showPowers) return "";
        std::string digitsStr;
        for (char c : numberText())
            if (c == '0' || c == '1') digitsStr += c;
        int n = (int)digitsStr.size();
        if (n == 0) return "";
        std::string out;
        for (int i = 0; i < n; ++i) {
            long w = 1L << (n - 1 - i);
            char buf[16]; snprintf(buf, sizeof buf, "%ld", w);
            if (i) out += " ";
            out += buf;
        }
        return out;
    }

    std::string expected() const {   // правильный ответ
        return padStart(toBase(value10, cfg->toBase), digits);
    }

    // 0 — неполно, 1 — верно, 2 — есть ошибка
    int check() const {
        std::string stroka;
        for (int i = 0; i < digits; ++i) {
            if (cells[i].empty()) return 0;
            stroka += cells[i];
        }
        return stroka == expected() ? 1 : 2;
    }

    // --- действия пользователя ---
    void moveCursor(int dir) {
        if (dir > 0) cursor = (cursor + 1) % digits;
        else         cursor = (cursor - 1 + digits) % digits;
    }
    void setCell(int idx, const std::string& text, bool advance = false) {
        if (idx < 0 || idx >= digits) return;
        cells[idx] = text;
        if (advance) moveCursor(+1);
    }
    void cycleCell(int idx, int delta) {
        if (idx < 0 || idx >= digits) return;
        int base = cfg->inputBase;
        // Пустая ячейка: клик/стрелки начинают набор с 1 или 0
        if (cells[idx].empty()) { cells[idx] = (delta > 0) ? "1" : "0"; return; }
        int n = digitValue(cells[idx]);
        n = (n + delta + base) % base;
        cells[idx] = digitText(n, base);
    }

    // --- тикер (вызывать из UI с реальной дельтой времени) ---
    // Возвращает true, если UI должен перерисоваться.
    bool tick(double dt) {
        if (screen != Screen::Game) return false;

        // пауза перед следующим уровнем / рестартом после таймаута
        if (status == Status::NextWait || status == Status::TimeoutWait) {
            waitAccum += dt;
            while (waitAccum >= 1.0 &&
                   (status == Status::NextWait || status == Status::TimeoutWait)) {
                waitAccum -= 1.0;
                if (--waitCounter < 0) {
                    if (status == Status::TimeoutWait) {
                        startLevel(1);
                    } else if (level + 1 <= cfg->levelCount) {
                        startLevel(level + 1);
                    } else {
                        status = Status::Finished;
                    }
                }
            }
            return true;
        }

        if (status == Status::Finished) return false;

        // проверка ввода
        int res = check();
        if (res == 1) {
            status = Status::NextWait;
            waitCounter = 3;
            waitAccum = 0;
            return true;
        }
        if (res == 2) status = Status::Wrong;
        else if (status == Status::Wrong) status = Status::Idle;

        // отсчёт времени уровня
        timeLeft -= dt;
        if (timeLeft < 0) {
            timeLeft = 0;
            status = Status::TimeoutWait;
            waitCounter = 3;
            waitAccum = 0;
        }
        return true;
    }

    bool finishedAndIdle() const { return status == Status::Finished; }

private:
    double waitAccum = 0;
    std::vector<GameConfig> games_;
    bool gamesReady_ = false;

    void startLevel(int lev) {
        level = lev;
        timeFull = cfg->startTimer;
        timeLeft = cfg->startTimer - (lev - 1) * cfg->timerStep;

        if (cfg->digitsForLevel) {
            // Динамическое число ячеек ввода по уровням
            // (игра 2 → 10: уровни 1-5 — 2 ячейки, уровни 6+ — 3 ячейки).
            digits = cfg->digitsForLevel(lev);
        } else {
            digits = cfg->maxDigits;
            if (lev >= 7) {
                if (cfg->id == 1) digits += 3;
                else if (cfg->id == 2) digits += 4;
                else if (cfg->id == 3 || cfg->id == 4) digits += 1;
            }
        }
        cursor = 0;
        // Игра 2 → 10: ячейки ответа стартуют ПУСТЫМИ (игрок сам набирает
        // десятичное число — пустые ячейки показываются как «_»); в остальных
        // режимах — «0» по умолчанию, как в веб-версии.
        // во всех режимах ячейки по умолчанию показывают «0» (как в v1.02)
        cells.assign(digits, "0");

        auto range = cfg->numRange(lev);
        value10 = rndLong(range.first, range.second);

        status = Status::Idle;
        waitCounter = 0;
        waitAccum = 0;
    }
};

} // namespace core
