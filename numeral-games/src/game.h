// game.h / game.cpp — консольная реализация игр (порт логики веб-версии)
#pragma once

#include "game_config.h"
#include <chrono>
#include <string>
#include <vector>

// Цифра в ячейке: '\0' = пусто, иначе '0'..'9','A'..'F'
struct Cell { char v = '0'; };

class Game {
public:
    explicit Game(const GameConfig& cfg);

    // Запуск игры до выхода в меню. Возвращает false, если игрок вышел.
    bool run();

private:
    const GameConfig& cfg_;

    int  level_       = 1;
    int  digits_      = 0;     // текущее число разрядов ответа
    int  currentId_   = 0;     // активная ячейка (0-based)
    long value10_     = 0;     // загаданное число в десятичной системе
    std::vector<Cell> cells_;
    double timer_     = 0;
    bool finished_    = false; // все уровни пройдены
    std::chrono::steady_clock::time_point lastTick_{};

    void startLevel(int lev);
    void advanceLevel();              // переход на следующий уровень или победа
    void render(const std::string& status);
    int  checkState() const;          // 0 = неполно, 1 = верно, 2 = ошибка
    std::string expected() const;
    std::string shownNumber() const;  // число с группировкой бит для игр 3/4
    long enteredValue() const;        // режим 2->10: значение набранного двоичного числа
    void cycleCell(int idx, int delta);
};
