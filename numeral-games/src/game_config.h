// game_config.h — конфигурация 6 игр по системам счисления (порт веб-версии)
#pragma once

#include <string>
#include <vector>
#include <utility>
#include <functional>

struct GameConfig {
    int id;
    int fromBase;     // основание исходного числа
    int toBase;       // основание ответа
    int inputBase;    // алфавит цифр поля ввода (2, 8, 10, 16)
    int maxDigits;    // разрядов ответа на уровнях 1..6
    int levelCount;   // всего уровней
    double startTimer;// время на 1 уровне, сек
    double timerStep; // уменьшение времени с каждым уровнем
    std::string desc;
    // диапазон значения в десятичной системе для уровня lev (включительно mn..mx)
    std::function<std::pair<int,int>(int)> numRange;
    bool showPowers;  // показывать веса разрядов над сеткой ячеек ввода (игра 10 -> 2)
};

inline std::vector<GameConfig> makeGames() {
    std::vector<GameConfig> g;

    g.push_back({1, 8, 2, 2, 9, 10, 31, 2,
        "Переведи число из 8 С.С. в 2 С.С.",
        [](int){ return std::make_pair(64, 511); }, false});

    g.push_back({2, 16, 2, 2, 12, 10, 35, 1,
        "Переведи число из 16 С.С. в 2 С.С.",
        [](int){ return std::make_pair(256, 4095); }, false});

    g.push_back({3, 2, 8, 8, 3, 10, 35, 2,
        "Переведи число из 2 С.С. в 8 С.С.",
        [](int lev){ return lev >= 7 ? std::make_pair(513, 4095)
                                     : std::make_pair(64, 511); }, false});

    g.push_back({4, 2, 16, 16, 3, 10, 35, 2,
        "Переведи число из 2 С.С. в 16 С.С.",
        [](int lev){ return lev >= 7 ? std::make_pair(4097, 65534)
                                     : std::make_pair(256, 4095); }, false});

    g.push_back({5, 10, 2, 2, 8, 13, 31, 1,
        "Переведи число из 10 С.С. в 2 С.С.",
        [](int){ return std::make_pair(1, 255); }, true});

    g.push_back({6, 2, 10, 10, 2, 10, 35, 2,
        "Переведи число из 2 С.С. в 10 С.С.",
        [](int lev){ return lev == 1 ? std::make_pair(1, 9)
                                     : std::make_pair(10, 99); }, false});

    return g;
}
