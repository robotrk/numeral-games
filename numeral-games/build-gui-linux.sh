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
g++ -std=c++17 -O2 -Wall -o build/numeral_games_gui \
    src-gui/gui_main.cpp $(pkg-config --cflags --libs gtk+-3.0)

echo
echo "Готово: build/numeral_games_gui  (запуск: ./build/numeral_games_gui)"
