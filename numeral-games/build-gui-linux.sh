#!/usr/bin/env bash
# build-gui-linux.sh — сборка НЕконсольной GUI-версии (GTK3) для Linux
# Подходит для KDE Plasma, XFCE и любых других DE/WM с X11 или Wayland.
#
# Зависимости (Debian/Ubuntu/KDE Neon):   sudo apt install libgtk-3-dev
# Зависимости (Fedora):                   sudo dnf install gtk3-devel
# Зависимости (openSUSE):                 sudo zypper install gtk3-devel
# Зависимости (Arch):                     sudo pacman -S gtk3
set -e
cd "$(dirname "$0")"

if ! pkg-config --exists gtk+-3.0; then
    echo "Ошибка: не найдены заголовки GTK3 (gtk+-3.0)." >&2
    echo "Установите пакет libgtk-3-dev / gtk3-devel и повторите." >&2
    exit 1
fi

mkdir -p build

# Иконка Linux: numeral_games.png -> base64-заголовок icon_data.h (вшивается в бинарник)
if [ -f numeral_games.png ]; then
    {
        echo "// Автогенерация build-gui-linux.sh из numeral_games.png. Не редактировать вручную."
        echo "#pragma once"
        echo "static const char ICON_PNG_BASE64[] ="
        base64 -w 960 numeral_games.png | sed 's/.*/    "&"/'
        echo ";"
    } > build/icon_data.h
    INC="-Ibuild"
else
    echo "Внимание: numeral_games.png не найден — иконка окна использована не будет." >&2
    INC=""
fi

g++ -std=c++17 -O2 -Wall $INC -o build/numeral_games_gui \
    src-gui/gui_main.cpp $(pkg-config --cflags --libs gtk+-3.0)

echo
echo "Готово: build/numeral_games_gui  (запуск: ./build/numeral_games_gui)"
