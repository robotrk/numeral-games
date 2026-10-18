// game.cpp — логика консольной игры (порт логики веб-версии)
#include "game.h"
#include "platform.h"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>
#include <thread>

using namespace std::chrono;

// ---------- утилиты ----------
static long rndLong(long a, long b) { // [a, b] включительно
    static std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<long> d(a, b);
    return d(rng);
}

static std::string toBase(long v, int base) {
    if (v == 0) return "0";
    const char* digits = "0123456789ABCDEF";
    // Собираем в обратном порядке и разворачиваем — без O(n^2) вставок в начало.
    std::string s;
    while (v > 0) { s += digits[v % base]; v /= base; }
    std::reverse(s.begin(), s.end());
    return s;
}

static std::string padStart(const std::string& s, size_t n) {
    if (s.size() >= n) return s;
    return std::string(n - s.size(), '0') + s;
}

// группировка бит пробелами с конца (как groupBits в JS-версии)
static std::string groupBits(const std::string& bin, int size) {
    // Результат заведомо длиннее исходной строки — резервируем место сразу
    // и идём по одному проходу (без O(n^2) вставок в начало).
    std::string out;
    out.reserve(bin.size() + bin.size() / size + 1);
    int cnt = 0;
    for (auto it = bin.rbegin(); it != bin.rend(); ++it) {
        if (cnt && cnt % size == 0) out += ' ';
        out += *it;
        ++cnt;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

// ---------- Game ----------
Game::Game(const GameConfig& cfg) : cfg_(cfg) {}

void Game::startLevel(int lev) {
    level_ = lev;
    timer_ = cfg_.startTimer - (lev - 1) * cfg_.timerStep;
    shownCacheKey_ = -1;   // новое число уровня — сбрасываем кэши строк
    expectedCacheKey_ = -1;

    digits_ = cfg_.maxDigits;
    if (lev >= 7) {
        if (cfg_.id == 1) digits_ += 3;
        else if (cfg_.id == 2) digits_ += 4;
        else if (cfg_.id == 3 || cfg_.id == 4) digits_ += 1;
    }
    currentId_ = 0;

    auto [mn, mx] = cfg_.numRange(lev);
    value10_ = rndLong(mn, mx);
    lastTick_ = steady_clock::now();

    cells_.assign(digits_, Cell{'0'});
}

std::string Game::shownNumber() const {
    // Кэш: строка задания меняется только вместе с загаданным числом уровня.
    if (shownCacheKey_ != value10_) {
        if (cfg_.id == 3)       shownCache_ = groupBits(toBase(value10_, 2), 3);
        else if (cfg_.id == 4)  shownCache_ = groupBits(toBase(value10_, 2), 4);
        else                    shownCache_ = toBase(value10_, cfg_.fromBase);
        shownCacheKey_ = value10_;
    }
    return shownCache_;
}

long Game::enteredValue() const {   // режим 10->2: десятичное значение, набранное игроком
    if (cfg_.id != 5) return -1;
    bool any = false;
    for (int i = 0; i < digits_; ++i)
        if (cells_[i].v > '0') { any = true; break; }
    if (!any) return -1;            // ничего не набрано — поле справа пустое
    long v = 0;
    for (int i = 0; i < digits_; ++i)
        v = v * 2 + (cells_[i].v > '0' ? 1 : 0);
    return v;
}

std::string Game::expected() const {
    // Кэш эталонной строки: зависит только от числа уровня и числа разрядов.
    if (expectedCacheKey_ != value10_ || expectedCacheDigits_ != digits_) {
        expectedCache_ = padStart(toBase(value10_, cfg_.toBase), digits_);
        expectedCacheKey_ = value10_;
        expectedCacheDigits_ = digits_;
    }
    return expectedCache_;
}

int Game::checkState() const {
    std::string stroka;
    stroka.reserve(cells_.size());
    for (auto& c : cells_) stroka += c.v;
    return stroka == expected() ? 1 : 2;
}

void Game::cycleCell(int idx, int delta) {
    const int base = cfg_.inputBase;
    char t = cells_[idx].v;
    int n;
    if (t >= 'A' && t <= 'F') n = t - 'A' + 10;
    else n = t - '0';
    n = ((n + delta) % base + base) % base;
    cells_[idx].v = (base == 16) ? toBase(n, 16)[0] : char('0' + n);
}

void Game::advanceLevel() {
    if (level_ + 1 <= cfg_.levelCount) {
        render("ok");
        std::this_thread::sleep_for(seconds(1));
        startLevel(level_ + 1);
    } else {
        finished_ = true;
    }
}

void Game::render(const std::string& status) {
    std::ostringstream os;
    os << "\x1b[2J\x1b[H"; // clear screen
    os << "\x1b[1m=== Игра: перевод из " << cfg_.fromBase
       << " С.С. в " << cfg_.toBase << " С.С. ===\x1b[0m\n";
    os << cfg_.desc << "\n\n";
    os << "Уровень: " << level_ << "/" << cfg_.levelCount
       << "   Время: " << std::fixed << std::setprecision(1)
       << (timer_ < 0 ? 0.0 : timer_) << " сек.\n\n";

    os << "  Число:  \x1b[1;36m" << shownNumber() << "\x1b[0m"
       << "  (" << cfg_.fromBase << " С.С.)  ->  ответ в "
       << cfg_.toBase << " С.С.";
    if (cfg_.id == 5 && enteredValue() >= 0)
       os << "   = у вас получилось: " << enteredValue();
    os << "\n\n";

    if (cfg_.showPowers) {   // веса разрядов над двоичным полем ввода (игра 10->2)
        os << "          ";
        for (int i = digits_ - 1; i >= 0; --i) {
            os.width(4);
            os << (1u << i);
        }
        os << "\n";
    }

    os << "  Ответ:  ";
    for (int i = 0; i < digits_; ++i) {
        if (i == currentId_)
            os << "\x1b[7;1m[ " << cells_[i].v << " ]\x1b[0m";
        else
            os << "[ " << cells_[i].v << " ]";
    }
    os << "\n\n";

    if (!status.empty()) {
        const char* col = (status == "ok")   ? "\x1b[1;32mВСЕ ВЕРНО!\x1b[0m"
                        : (status == "err") ? "\x1b[1;31mЕСТЬ ОШИБКА\x1b[0m"
                        : (status == "time")? "\x1b[1;31mВремя вышло :(  Перезапуск с 1 уровня\x1b[0m"
                        : (status == "win") ? "\x1b[1;32m*** ИГРА ЗАВЕРШЕНА — ПОЗДРАВЛЯЕМ! ***\x1b[0m"
                        : "";
        os << "  " << col << "\n\n";
    }

    os << "  Управление: ←/→ или A/D — ячейка, ↑/↓ или W/S — изменить цифру,\n"
          "  цифры 0-9/A-F — ввод, Enter — проверить, Esc — в меню.\n";

    std::string s = os.str();
    fwrite(s.data(), 1, s.size(), stdout);
    fflush(stdout);
}

bool Game::run() {
    startLevel(1);
    finished_ = false;

    bool timedOut   = false; // время вышло — нужен рестарт с 1 уровня
    bool needRender = true;

    for (;;) {
        if (needRender)
            render(timedOut ? "time" : (finished_ ? "win" : ""));
        needRender = false;

        // ждём клавишу с коротким таймаутом, чтобы таймер тикал сам
        int key = platform::readKeyTimeout(100);

        if (key == platform::KEY_NONE) {
            // истёк таймаут ожидания — обновляем часы
            if (!finished_ && !timedOut) {
                auto now = steady_clock::now();
                timer_ -= duration_cast<duration<double>>(now - lastTick_).count();
                lastTick_ = now;
                if (timer_ <= 0) { timedOut = true; }
                else { render(""); needRender = false; } // перерисовать часы
            }
            continue;
        }

        // была нажата клавиша — сначала учтём прошедшее время
        if (!finished_ && !timedOut) {
            auto now = steady_clock::now();
            timer_ -= duration_cast<duration<double>>(now - lastTick_).count();
            lastTick_ = now;
            if (timer_ <= 0) { timedOut = true; continue; }
        }

        if (key == platform::KEY_ESC) return true; // в меню

        if (finished_) { return true; } // любая клавиша — в меню

        if (timedOut) {
            // пауза 3 секунды перед рестартом; Esc отменяет и выходит в меню
            bool cancelled = false;
            for (int cdown = 3; cdown > 0 && !cancelled; --cdown) {
                std::ostringstream p;
                p << "\x1b[2J\x1b[HВремя вышло! Рестарт через " << cdown
                  << "... (Esc — в меню)";
                std::string s = p.str();
                fwrite(s.data(), 1, s.size(), stdout); fflush(stdout);
                auto end = steady_clock::now() + seconds(1);
                while (steady_clock::now() < end) {
                    int k = platform::readKey();
                    if (k == platform::KEY_ESC) { cancelled = true; break; }
                }
            }
            if (cancelled) return true;
            timedOut = false;
            startLevel(1);
            needRender = true;
            continue;
        }

        const int base = cfg_.inputBase;

        switch (key) {
            case platform::KEY_LEFT:
                currentId_ = (currentId_ + digits_ - 1) % digits_;
                needRender = true; break;
            case platform::KEY_RIGHT:
                currentId_ = (currentId_ + 1) % digits_;
                needRender = true; break;
            case platform::KEY_UP:
                cycleCell(currentId_, +1);
                needRender = true; break;
            case platform::KEY_DOWN:
                cycleCell(currentId_, -1);
                needRender = true; break;
            case platform::KEY_ENTER: {
                if (checkState() == 1) advanceLevel();
                else { render("err"); std::this_thread::sleep_for(milliseconds(700)); render(""); }
                needRender = true;
                break;
            }
            default: {
                char c = (char)key;
                int d = -1;
                if      (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                if (d >= 0 && d < base) {
                    cells_[currentId_].v = (base == 16) ? toBase(d, 16)[0] : char('0' + d);
                    currentId_ = (currentId_ + 1) % digits_;
                    // автопроверка, как draw() в веб-версии
                    if (checkState() == 1) advanceLevel();
                    else if (checkState() == 2) render("err");
                    needRender = true;
                }
                break;
            }
        }

        if (finished_) {
            render("win");
            platform::readKey();
            return true;
        }
    }
}
