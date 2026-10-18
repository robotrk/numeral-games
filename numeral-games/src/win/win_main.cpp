// win_main.cpp — неконсольная нативная Windows-версия (Windows 10/11, x86-64).
// GDI+ рендеринг: скруглённые карточки, анимированный таймер с полосой времени,
// hover/press-эффекты кнопок, плавный переход между меню и игрой.
// Логика игры — общее ядро src-core/game_core.h (то же, что в GTK-версии для Linux).
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../../src-core/game_core.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;
using namespace core;

// ===================== ЦВЕТА (как в веб-версии) =====================
static const Color COL_BG      (255, 0xEE, 0xF2, 0xFF);
static const Color COL_CARD    (255, 0xFF, 0xFF, 0xFF);
static const Color COL_PRIMARY (255, 0x4F, 0x46, 0xE5);
static const Color COL_HOVER   (255, 0x43, 0x38, 0xCA);
static const Color COL_PRESS   (255, 0x37, 0x30, 0xA3);
static const Color COL_TEXT    (255, 0x2C, 0x3E, 0x50);
static const Color COL_MUTED   (255, 0x64, 0x74, 0x8B);
static const Color COL_DESC    (255, 0x55, 0x55, 0x55);
static const Color COL_NUM     (255, 0x1E, 0x29, 0x3B);
static const Color COL_CELL_BG (255, 0xF8, 0xFA, 0xFC);
static const Color COL_CELL_BRD(255, 0xCB, 0xD5, 0xE1);
static const Color COL_ACT_BG  (255, 0xFE, 0xF3, 0xC7);
static const Color COL_ACT_BRD (255, 0xF5, 0x9E, 0x0B);
static const Color COL_OK      (255, 0x16, 0xA3, 0x4A);
static const Color COL_BAD     (255, 0xDC, 0x26, 0x26);
static const Color COL_BACK_BG (255, 0xE5, 0xE7, 0xEB);
static const Color COL_BACK_HOV(255, 0xD1, 0xD5, 0xDB);
static const Color COL_TRACK   (255, 0xE2, 0xE8, 0xF0);

// lerp цвета по t 0..1
static Color mix(const Color& a, const Color& b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return Color((BYTE)(a.GetA() + (b.GetA() - a.GetA()) * t),
                 (BYTE)(a.GetR() + (b.GetR() - a.GetR()) * t),
                 (BYTE)(a.GetG() + (b.GetG() - a.GetG()) * t),
                 (BYTE)(a.GetB() + (b.GetB() - a.GetB()) * t));
}

// ===================== UTF-8 -> UTF-16 =====================
static std::wstring u8to16(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// ===================== КНОПКА =====================
struct Button {
    RectF rect{ 0, 0, 0, 0 };
    std::wstring label;
    int gameId = 0;         // для кнопок меню
    bool isBack = false;    // кнопка «В меню»
    enum State { Normal, Hover, Press } state = Normal;
    float anim = 0;         // 0..1 плавность подсветки
};

// ===================== СОСТОЯНИЕ ПРИЛОЖЕНИЯ =====================
struct App {
    HWND hwnd = nullptr;
    GameEngine eng;

    std::vector<Button> menuButtons;
    Button backButton;

    double screenAnim = 0;        // 0 = меню, 1 = игра (плавный переход)
    bool wantGameScreen = false;

    POINT mouse{ -1000, -1000 };
    ULONGLONG lastTickMs = 0;

    float cellSize = 46, cellGap = 6;

    // геометрия игровой сетки (обновляется в layoutGame, используется в кликах)
    float gridTop = 0, gridStartX = 0;
};

// ===================== ГЕОМЕТРИЧЕСКИЕ УТИЛИТЫ =====================
static GraphicsPath* roundedRect(float x, float y, float w, float h, float r) {
    r = std::min(r, std::min(w, h) / 2);
    auto* p = new GraphicsPath();
    p->AddArc(x, y, 2 * r, 2 * r, 180, 90);
    p->AddArc(x + w - 2 * r, y, 2 * r, 2 * r, 270, 90);
    p->AddArc(x + w - 2 * r, y + h - 2 * r, 2 * r, 2 * r, 0, 90);
    p->AddArc(x, y + h - 2 * r, 2 * r, 2 * r, 90, 90);
    p->CloseFigure();
    return p;
}

static void fillRR(Graphics& g, const RectF& r, float rad, const Color& c) {
    SolidBrush b(c);
    GraphicsPath* p = roundedRect(r.X, r.Y, r.Width, r.Height, rad);
    g.FillPath(&b, p);
    delete p;
}

static void strokeRR(Graphics& g, const RectF& r, float rad, const Pen& pen) {
    GraphicsPath* p = roundedRect(r.X + pen.GetWidth() / 2, r.Y + pen.GetWidth() / 2,
                                  r.Width - pen.GetWidth(), r.Height - pen.GetWidth(), rad);
    g.DrawPath(&pen, p);
    delete p;
}

enum AlignH { AHLeft = 0, AHCenter = 1, AHRight = 2 };
enum AlignV { ATop = 0, ACenterV = 1, ABottom = 2 };

static void drawText(Graphics& g, const std::wstring& s, const RectF& area,
                     const Font* font, const Color& color,
                     AlignH ah = AHCenter, AlignV av = ACenterV) {
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetAlignment((StringAlignment)ah);
    sf.SetLineAlignment((StringAlignment)av);
    SolidBrush br(color);
    g.DrawString(s.c_str(), -1, font, area, &sf, &br);
}

static bool ptIn(const RectF& r, POINT p) {
    return p.x >= r.X && p.x < r.X + r.Width && p.y >= r.Y && p.y < r.Y + r.Height;
}

// шрифты создаются один раз при старте (GDI+ Font тяжёлый)
struct Fonts {
    Font *title, *subtitle, *gamebtn, *backbtn, *stat, *statval, *desc,
         *number, *basemark, *cell, *power, *status, *hint;
    ~Fonts() {
        delete title; delete subtitle; delete gamebtn; delete backbtn; delete stat;
        delete statval; delete desc; delete number; delete basemark; delete cell;
        delete power; delete status; delete hint;
    }
};
static Fonts* F = nullptr;

static void initFonts() {
    F = new Fonts();
    FontFamily sans(L"Segoe UI");
    FontFamily mono(L"Consolas");
    F->title    = new Font(&sans, 22, FontStyleBold);
    F->subtitle = new Font(&sans, 11);
    F->gamebtn  = new Font(&sans, 14, FontStyleBold);
    F->backbtn  = new Font(&sans, 10);
    F->stat     = new Font(&sans, 10);
    F->statval  = new Font(&sans, 10, FontStyleBold);
    F->desc     = new Font(&sans, 11, FontStyleItalic);
    F->number   = new Font(&mono, 20, FontStyleBold);
    F->basemark = new Font(&sans, 13, FontStyleBold);
    F->cell     = new Font(&mono, 15, FontStyleBold);
    F->power    = new Font(&mono, 8);
    F->status   = new Font(&sans, 13, FontStyleBold);
    F->hint     = new Font(&sans, 9);
}

// ===================== РАСКЛАДКА =====================
static void layoutMenu(App& app, float W, float H) {
    const float bw = 150, bh = 56, gap = 14;
    int cols = 3, rows = 2;
    float totalW = cols * bw + (cols - 1) * gap;
    float startX = (W - totalW) / 2;
    float startY = H / 2 - (rows * bh + (rows - 1) * gap) / 2 + 30;
    int i = 0;
    for (const auto& gcfg : games()) {
        Button b;
        b.label = u8to16(gcfg.title);
        b.gameId = gcfg.id;
        int col = i % cols, row = i / cols;
        b.rect = RectF(startX + col * (bw + gap), startY + row * (bh + gap), bw, bh);
        app.menuButtons[i] = b;
        i++;
    }
}

static void layoutGame(App& app, float W, float H) {
    app.backButton.rect = RectF(24, 20, 110, 34);

    int n = std::max(1, app.eng.digits);
    float maxRowW = W - 60;
    app.cellSize = std::min(46.f, (maxRowW - (n - 1) * app.cellGap) / (float)n);
    float totalW = n * app.cellSize + (n - 1) * app.cellGap;
    app.gridStartX = (W - totalW) / 2;
    app.gridTop = 48 + 34 + 64 + 18;   // панель + описание + строка числа (+степени)
    if (app.eng.cfg && app.eng.cfg->showPowersGrid) app.gridTop += 22;
    (void)H;
}

// ===================== ОТРИСОВКА =====================
static void renderMenu(App& app, Graphics& g, float W, float H, float alpha) {
    Color cTitle((BYTE)(alpha * 255), COL_TEXT.GetR(), COL_TEXT.GetG(), COL_TEXT.GetB());

    // заголовок поверх всей ширины
    RectF tr(0, H * 0.18f, W, 40);
    drawText(g, L"\x0418\x0433\x0440\x044B \x043F\x043E \x0441\x0438\x0441\x0442\x0435\x043C\x0430\x043C \x0441\x0447\x0438\x0441\x043B\x0435\x043D\x0438\x044F", tr, F->title, cTitle);
    RectF sr(0, H * 0.18f + 44, W, 24);
    drawText(g, L"\x0412\x044B\x0431\x0435\x0440\x0438\x0442\x0435 \x0440\x0435\x0436\x0438\x043C \x043F\x0435\x0440\x0435\x0432\x043E\x0434\x0430 \x0447\x0438\x0441\x0435\x043B", sr, F->subtitle,
             Color((BYTE)(alpha * 255), COL_MUTED.GetR(), COL_MUTED.GetG(), COL_MUTED.GetB()));

    for (auto& b : app.menuButtons) {
        Color base = COL_PRIMARY;
        Color target = (b.state == Button::Press) ? COL_PRESS : COL_HOVER;
        Color fill = mix(base, target, b.anim);
        fill = Color((BYTE)(alpha * 255), fill.GetR(), fill.GetG(), fill.GetB());
        fillRR(g, b.rect, 12, fill);
        drawText(g, b.label, b.rect, F->gamebtn, Color((BYTE)(alpha * 255), 255, 255, 255));
    }
}

static void renderGame(App& app, Graphics& g, float W, float H, float alpha) {
    const GameEngine& e = app.eng;
    if (!e.cfg) return;
    BYTE A = (BYTE)(alpha * 255);
    auto ca = [&](const Color& c) { return Color(A, c.GetR(), c.GetG(), c.GetB()); };

    // белая карточка
    float pad = 16;
    RectF card(pad, pad, W - 2 * pad, H - 2 * pad);
    fillRR(g, card, 16, ca(COL_CARD));

    // --- верхняя панель ---
    RectF backR = app.backButton.rect;
    Color backFill = mix(COL_BACK_BG, COL_BACK_HOV, app.backButton.anim);
    fillRR(g, backR, 8, ca(backFill));
    drawText(g, L"\x2190 \x0412 \x043C\x0435\x043D\x044E", backR, F->backbtn, ca(Color(255, 0x33, 0x33, 0x33)));

    wchar_t buf[64];
    swprintf(buf, 64, L"\x0423\x0440\x043E\x0432\x0435\x043D\x044C: %d", e.level);
    drawText(g, buf, RectF(W - 320, 24, 150, 26), F->statval, ca(COL_PRIMARY), AHRight);
    swprintf(buf, 64, L"\x0412\x0440\x0435\x043C\x044F: %.1f", std::max(0.0, e.timeLeft));
    drawText(g, buf, RectF(W - 165, 24, 141, 26), F->statval, ca(COL_PRIMARY), AHRight);

    // --- описание ---
    drawText(g, u8to16(e.cfg->desc), RectF(40, 64, W - 80, 26), F->desc, ca(COL_DESC));

    // --- строка «число → система» ---
    float lineY = 102;               // чуть ниже: освобождает место под веса разрядов
    std::wstring num = u8to16(e.numberText());
    // измерим ширину числа
    RectF bound;
    {
        StringFormat sf(StringFormat::GenericTypographic());
        g.MeasureString(num.c_str(), -1, F->number, PointF(0, 0), &sf, &bound);
    }
    float arrowW = 60;
    float cx = W / 2;
    // строка задания сдвинута влево, чтобы справа осталось место под
    // живое десятичное значение игрока (режим 10 -> 2)
    const float lineShift = -40.f;
    float numX = cx - bound.Width / 2 - arrowW / 2 + 14 + lineShift;
    RectF numR(numX, lineY, bound.Width, 40);
    // веса разрядов НАД ЗАДАНИЕМ (режим 2 -> 10): выровнены по цифрам числа
    if (e.cfg->showPowers) {
        int nd = 0;
        for (wchar_t c : num) if (c == L'0' || c == L'1') ++nd;
        if (nd > 0) {
                    float digitW = bound.Width / (float)num.size();
            for (int i = 0; i < nd; ++i) {
                long wgt = 1L << (nd - 1 - i);
                wchar_t wb[16]; swprintf(wb, 16, L"%ld", wgt);
                float cellCx = numX + (i + 0.5f) * digitW;
                // веса разрядов ставим чуть ниже — вплотную над цифрами числа
                RectF wr(cellCx - digitW * 1.6f, lineY - 17, digitW * 3.2f, 16);
                drawText(g, wb, wr, F->power, ca(COL_MUTED));
            }
        }
    }
    drawText(g, num, numR, F->number, ca(COL_NUM));
    swprintf(buf, 64, L"%d", e.cfg->fromBase);
    drawText(g, buf, RectF(numR.X - 40, lineY + 8, 24, 28), F->basemark, ca(COL_PRIMARY));
    drawText(g, L"\x2192", RectF(cx - arrowW / 2 + bound.Width / 2 + 10 + lineShift, lineY, arrowW, 40), F->basemark, ca(COL_PRIMARY));
    swprintf(buf, 64, L"%d", e.cfg->toBase);
    float rightEdge = cx + bound.Width / 2 + arrowW / 2 + 16 + lineShift + 24;
    drawText(g, buf, RectF(rightEdge - 24, lineY + 8, 24, 28), F->basemark, ca(COL_PRIMARY));
    // режимы 10 -> 2 и 2 -> 10: справа от задания показываем живое значение,
    // которое игрок набирает ячейками (десятичное / двоичное соответственно).
    // Пока ничего не набрано — пусто, чтобы нуль по умолчанию не путал игрока.
    if (e.cfg->id == 5 || e.cfg->id == 6) {
        std::wstring ev = u8to16(e.enteredValueText());
        if (!ev.empty()) {
            drawText(g, L"=", RectF(rightEdge + 18, lineY, 20, 40),
                     F->basemark, ca(COL_PRIMARY), AHLeft);
            drawText(g, ev, RectF(rightEdge + 44, lineY, 120, 40),
                     F->number, ca(COL_NUM), AHLeft);
        }
    }

    // --- веса разрядов над сеткой ячеек (игра 10->2: 128 64 ... 1) ---
    int n = e.digits;
    float cs = app.cellSize, gap = app.cellGap;
    float top = app.gridTop;
    if (e.cfg->showPowersGrid) {
        for (int i = 0; i < n; ++i) {
            long p = 1L << (n - 1 - i);
            swprintf(buf, 64, L"%ld", p);
            RectF pr(app.gridStartX + i * (cs + gap), top - 22, cs, 18);
            drawText(g, buf, pr, F->power, ca(COL_MUTED));
        }
    }

    // --- ячейки ---
    for (int i = 0; i < n; ++i) {
        RectF cr(app.gridStartX + i * (cs + gap), top, cs, cs);
        bool active = (i == e.cursor);
        fillRR(g, cr, 8, ca(active ? COL_ACT_BG : COL_CELL_BG));
        Pen pen(active ? COL_ACT_BRD : COL_CELL_BRD, active ? 2.5f : 1.5f);
        strokeRR(g, cr, 8, pen);
        // пустая ячейка (игра 2 -> 10) показывается как «_»
        std::wstring cellTxt = e.cells[i].empty() ? L"0" : u8to16(e.cells[i]);
        drawText(g, cellTxt, cr, F->cell, ca(COL_NUM));
    }

    // --- полоса времени ---
    float barW = std::min(420.f, W - 120), barH = 8;
    RectF bar((W - barW) / 2, top + cs + 20, barW, barH);
    float frac = std::clamp(e.timeLeft / (e.timeFull > 0 ? e.timeFull : 1.0), 0.0, 1.0);
    fillRR(g, bar, 4, ca(COL_TRACK));
    if (frac > 0.001) {
        Color barCol = frac < 0.25 ? COL_BAD : COL_PRIMARY;
        fillRR(g, RectF(bar.X, bar.Y, barW * frac, barH), 4, ca(barCol));
    }

    // --- статус ---
    std::wstring status;
    Color stCol = COL_TEXT;
    switch (e.status) {
        case GameEngine::Status::Idle:
        case GameEngine::Status::Correct: break;
        case GameEngine::Status::Wrong:
            status = L"\x2716 \x0415\x0441\x0442\x044C \x043E\x0448\x0438\x0431\x043A\x0430"; stCol = COL_BAD; break;
        case GameEngine::Status::NextWait:
            status = L"\x2714 \x0412\x0441\x0435 \x0432\x0435\x0440\x043D\x043E! "; stCol = COL_OK;
            status += std::to_wstring(std::max(0, e.waitCounter)) + L"..."; break;
        case GameEngine::Status::TimeoutWait:
            status = L"\x0412\x0440\x0435\x043C\x044F \x0432\x044B\x0448\x043B\x043E :( "; stCol = COL_BAD;
            status += std::to_wstring(std::max(0, e.waitCounter)) + L"..."; break;
        case GameEngine::Status::Finished:
            status = L"\x0418\x0413\x0420\x0410 \x0417\x0410\x0412\x0415\x0420\x0428\x0415\x041D\x0410!"; stCol = COL_OK; break;
    }
    drawText(g, status, RectF(40, bar.GetBottom() + 10, W - 80, 28), F->status, ca(stCol));

    // --- подсказка ---
    drawText(g,
        L"\x0421\x0442\x0440\x0435\x043B\x043A\x0438 \x2190 \x2192 \x2014 \x0432\x044B\x0431\x043E\x0440 "
        L"\x044F\x0447\x0435\x0439\x043A\x0438 \xB7 \x2191 \x2193 \x2014 +1/-1 \xB7 \x043A\x043B\x0438\x043A "
        L"\x2014 \x0441\x043C\x0435\x043D\x0438\x0442\x044C \x0446\x0438\x0444\x0440\x0443 \xB7 0-9/A-F "
        L"\x2014 \x0432\x0432\x043E\x0434 \xB7 Esc \x2014 \x043C\x0435\x043D\x044E",
        RectF(40, H - 46, W - 80, 20), F->hint, ca(COL_MUTED));
}

static void render(App& app, Graphics& g, float W, float H) {
    g.Clear(COL_BG);
    double t = app.screenAnim;
    if (t < 1.0) renderMenu(app, g, W, H, (float)(1.0 - t));
    if (t > 0.0) renderGame(app, g, W, H, (float)t);
}

// ===================== ВВОД =====================
static void updateHover(App& app) {
    auto upd = [&](Button& b) {
        bool inside = ptIn(b.rect, app.mouse);
        b.state = inside ? ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? Button::Press : Button::Hover)
                         : Button::Normal;
    };
    if (app.screenAnim < 0.5) for (auto& b : app.menuButtons) upd(b);
    else upd(app.backButton);
}

static void handleClick(App& app) {
    if (app.screenAnim < 0.5) {
        for (auto& b : app.menuButtons) {
            if (ptIn(b.rect, app.mouse)) {
                app.eng.startGame(b.gameId);
                app.wantGameScreen = true;
                RECT rc; GetClientRect(app.hwnd, &rc);
                layoutGame(app, (float)rc.right, (float)rc.bottom);
                break;
            }
        }
    } else {
        if (ptIn(app.backButton.rect, app.mouse)) {
            app.eng.openMenu();
            app.wantGameScreen = false;
        } else {
            int n = app.eng.digits;
            for (int i = 0; i < n; ++i) {
                RectF cr(app.gridStartX + i * (app.cellSize + app.cellGap), app.gridTop,
                         app.cellSize, app.cellSize);
                if (ptIn(cr, app.mouse)) {
                    app.eng.cursor = i;
                    app.eng.cycleCell(i, +1);
                    break;
                }
            }
        }
    }
}

static void handleKey(App& app, WPARAM vk) {
    if (!app.eng.inGame) {
        if (vk == VK_ESCAPE) PostMessage(app.hwnd, WM_CLOSE, 0, 0);
        return;
    }
    auto act = [&]() { InvalidateRect(app.hwnd, nullptr, FALSE); };
    switch (vk) {
        case VK_ESCAPE:
            app.eng.openMenu();
            app.wantGameScreen = false;
            act(); return;
        case VK_LEFT:  app.eng.moveCursor(-1); act(); return;
        case VK_RIGHT: app.eng.moveCursor(+1); act(); return;
        case VK_UP:    app.eng.cycleCell(app.eng.cursor, +1); act(); return;
        case VK_DOWN:  app.eng.cycleCell(app.eng.cursor, -1); act(); return;
    }
    // цифры с основной и дополнительной клавиатуры
    int d = -1;
    if (vk >= '0' && vk <= '9') d = (int)vk - '0';
    else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) d = (int)vk - VK_NUMPAD0;
    else if (app.eng.cfg && app.eng.cfg->inputBase == 16 && vk >= 'A' && vk <= 'F') d = (int)vk - 'A' + 10;
    if (d >= 0 && app.eng.cfg && d < app.eng.cfg->inputBase) {
        app.eng.setCell(app.eng.cursor, digitText(d, app.eng.cfg->inputBase), true);
        act();
    }
}

// ===================== ТИКЕР (SetTimer 16 мс ≈ 60 fps) =====================
static void onFrameTimer(App& app) {
    ULONGLONG now = GetTickCount64();
    double dt = (now - app.lastTickMs) / 1000.0;
    app.lastTickMs = now;
    if (dt <= 0 || dt > 0.5) dt = 1.0 / 60.0;

    bool wasInGame = app.eng.inGame;
    int prevLevel = app.eng.level;
    app.eng.tick(dt);
    if (app.eng.inGame && app.eng.level != prevLevel) {
        RECT rc; GetClientRect(app.hwnd, &rc);
        layoutGame(app, (float)rc.right, (float)rc.bottom);
    }
    (void)wasInGame;

    // плавный переход экранов
    double target = app.wantGameScreen ? 1.0 : 0.0;
    double speed = 4.0;                       // ~0.25 с на всю анимацию
    if (app.screenAnim < target)      app.screenAnim = std::min(target, app.screenAnim + speed * dt);
    else if (app.screenAnim > target) app.screenAnim = std::max(target, app.screenAnim - speed * dt);

    auto animBtn = [&](Button& b) {
        float t = (b.state == Button::Normal) ? 0.f : 1.f;
        b.anim += (t - b.anim) * (float)std::min(1.0, dt * 12.0);
    };
    for (auto& b : app.menuButtons) animBtn(b);
    animBtn(app.backButton);

    updateHover(app);
    InvalidateRect(app.hwnd, nullptr, FALSE);
}

// ===================== WNDPROC =====================
static App* g_app = nullptr;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App& app = *g_app;
    static bool needRedraw = true;

    switch (msg) {
        case WM_CREATE: {
            RECT rc; GetClientRect(hwnd, &rc);
            layoutMenu(app, (float)rc.right, (float)rc.bottom);
            app.lastTickMs = GetTickCount64();
            SetTimer(hwnd, 1, 16, nullptr);
            return 0;
        }
        case WM_SIZE: {
            RECT rc; GetClientRect(hwnd, &rc);
            layoutMenu(app, (float)rc.right, (float)rc.bottom);
            if (app.wantGameScreen) layoutGame(app, (float)rc.right, (float)rc.bottom);
            needRedraw = true;
            return 0;
        }
        case WM_MOUSEMOVE:
            app.mouse.x = GET_X_LPARAM(lp);
            app.mouse.y = GET_Y_LPARAM(lp);
            {
                // курсор-указатель над кнопками/ячейками
                bool hand = false;
                if (app.screenAnim < 0.5) {
                    for (auto& b : app.menuButtons) if (ptIn(b.rect, app.mouse)) hand = true;
                } else {
                    if (ptIn(app.backButton.rect, app.mouse)) hand = true;
                    for (int i = 0; i < app.eng.digits; ++i) {
                        RectF cr(app.gridStartX + i * (app.cellSize + app.cellGap), app.gridTop,
                                 app.cellSize, app.cellSize);
                        if (ptIn(cr, app.mouse)) hand = true;
                    }
                }
                SetCursor(LoadCursor(nullptr, hand ? IDC_HAND : IDC_ARROW));
            }
            return 0;
        case WM_LBUTTONDOWN:
            app.mouse.x = GET_X_LPARAM(lp);
            app.mouse.y = GET_Y_LPARAM(lp);
            handleClick(app);
            return 0;
        case WM_KEYDOWN:
            handleKey(app, wp);
            return 0;
        case WM_TIMER:
            if (wp == 1) onFrameTimer(app);
            return 0;
        case WM_ERASEBKGND:
            return 1; // фон рисуем сами — без мерцания
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            Bitmap bmp(rc.right, rc.bottom, PixelFormat32bppPARGB);
            Graphics g(&bmp);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
            render(app, g, (float)rc.right, (float)rc.bottom);
            Graphics wnd(hdc);
            wnd.DrawImage(&bmp, 0, 0);
            EndPaint(hwnd, &ps);
            needRedraw = false;
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ===================== MAIN =====================
// Загрузка иконки из ресурсов exe (прикреплён через app.rc / CMake как IDI_APPICON).
// Если ресурса нет — тихо используем стандартную иконку приложения.
static HICON loadAppIcon(HINSTANCE hInst) {
    if (hInst) {
        HICON big = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(101), IMAGE_ICON,
                                      GetSystemMetrics(SM_CXICON),  GetSystemMetrics(SM_CYICON),  0);
        HICON small = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(101), IMAGE_ICON,
                                        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        if (big || small) {
            // Для WM_SETICON: большая — в заголовок окна, маленькая — на панель задач
            return big ? big : small;
        }
    }
    return LoadIcon(nullptr, IDI_APPLICATION);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    GdiplusStartupInput gsi;
    ULONG_PTR token;
    GdiplusStartup(&token, &gsi, nullptr);

    initFonts();

    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = loadAppIcon(hInst);          // иконка окна (заголовок + Alt+Tab)
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(101), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!wc.hIconSm) wc.hIconSm = wc.hIcon; // иконка панели задач / проводника
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"NumeralGamesWindow";
    RegisterClassEx(&wc);

    static App app;
    app.menuButtons.resize(games().size());
    g_app = &app;

    HWND hwnd = CreateWindowEx(
        WS_EX_APPWINDOW, wc.lpszClassName,
        L"\x0418\x0433\x0440\x044B \x043F\x043E \x0441\x0438\x0441\x0442\x0435\x043C\x0430\x043C "
        L"\x0441\x0447\x0438\x0441\x043B\x0435\x043D\x0438\x044F",   // Игры по системам счисления
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 640,
        nullptr, nullptr, hInst, nullptr);
    app.hwnd = hwnd;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessage(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }

    delete F;
    GdiplusShutdown(token);
    return (int)m.wParam;
}
