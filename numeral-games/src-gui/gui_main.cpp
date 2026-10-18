// gui_main.cpp — неконсольная GTK3-версия игр по системам счисления (Linux: KDE Plasma, XFCE и др.)
// Динамический интерфейс: анимированный таймер, кнопки, поле ячеек, плавные переходы экранов.
// Логика игры вынесена в общее ядро src-core/game_core.h (оно же используется Win32-версией).
#include <gtk/gtk.h>

#include "../src-core/game_core.h"
// Иконка приложения, вшитая в бинарник (base64 PNG).
// Файл resources/icon_data.h генерируется CMake-командой gen_icon_header.cmake
// из numeral_games.png (или скриптом сборки вне CMake).
#if __has_include("icon_data.h")
#  include "icon_data.h"
#else
static const char ICON_PNG_BASE64[] = "";
#endif

using namespace core;

// ===================== СОСТОЯНИЕ ПРИЛОЖЕНИЯ =====================
struct AppState {
    GtkWidget* window   = nullptr;
    GtkWidget* stack    = nullptr;   // menu <-> game
    GtkWidget* gameBox  = nullptr;   // карточка игры (пересобирается при старте)

    // игровые виджеты
    GtkWidget* lblLevel    = nullptr;
    GtkWidget* lblTimer    = nullptr;
    GtkWidget* lblDesc     = nullptr;
    GtkWidget* lblNumber   = nullptr;  // исходное число
    GtkWidget* lblWeights  = nullptr;  // режим 2 -> 10: веса разрядов над заданием
    GtkWidget* lblEntered  = nullptr;  // режим 10 -> 2: значение, набранное игроком
    std::string lastNumberShown;       // чтобы не пересобирать строку каждый тик
    std::string lastEnteredShown;      // живое значение справа от задания
    std::string lastStateText;         // текст строки состояния (не обновляем без изменения)
    int lastCellsSig = -1;             // подпись содержимого ячеек (см. cellsSignature)
    GtkWidget* cellsGrid   = nullptr;
    GtkWidget* lblState    = nullptr;
    GtkWidget* progressBar = nullptr;
    std::vector<GtkWidget*> cells;     // метки-ячейки (слева направо)

    GameEngine eng;                    // общее ядро игры
};

static void rebuildGameView(AppState& st);
static void buildCellsGrid(AppState& st);

// ===================== СТИЛИ (повторяют дизайн веб-версии) =====================
static const char* CSS = R"CSS(
window { background: #eef2ff; }
.card {
  background: #ffffff;
  border-radius: 16px;
  padding: 20px;
}
.title { color: #2c3e50; font-size: 26px; font-weight: 700; }
.subtitle { color: #64748b; font-size: 14px; }
.gamebtn {
  padding: 14px 26px;
  margin: 6px;
  font-size: 18px;
  font-weight: 700;
  border: none;
  border-radius: 12px;
  background: #4f46e5;
  color: #ffffff;
}
.gamebtn:hover  { background: #4338ca; }
.gamebtn:active { background: #3730a3; }
.backbtn {
  padding: 8px 16px;
  font-size: 14px;
  border: none;
  border-radius: 8px;
  background: #e5e7eb;
  color: #333333;
}
.backbtn:hover { background: #d1d5db; }
.stat    { color: #444444; font-size: 15px; }
.statval { color: #4f46e5; font-size: 15px; font-weight: 700; }
.desc    { color: #555555; font-style: italic; font-size: 16px; }
.number {
  color: #1e293b;
  font-family: monospace;
  font-size: 30px;
  font-weight: 700;
  letter-spacing: 2px;
}
.basemark { color: #4f46e5; font-size: 20px; font-weight: 700; }
.cell {
  background: #f8fafc;
  border: 2px solid #cbd5e1;
  border-radius: 8px;
  color: #1e293b;
  font-family: monospace;
  font-size: 22px;
  font-weight: 700;
}
.cell:hover { border-color: #94a3b8; }
.cell.active {
  background: #fef3c7;
  border-color: #f59e0b;
}
.power { color: #64748b; font-size: 12px; font-family: monospace; }
.ok   { color: #16a34a; font-weight: 700; font-size: 18px; }
.bad  { color: #dc2626; font-weight: 700; font-size: 18px; }
.info { color: #4f46e5; font-weight: 700; font-size: 18px; }
progressbar trough { min-height: 8px; border-radius: 6px; background: #e2e8f0; }
progressbar progress { min-height: 8px; border-radius: 6px; background: #4f46e5; }
.low progress { background: #dc2626; }
.hint { color: #94a3b8; font-size: 13px; }
)CSS";

// ===================== ТАЙМЕР / СТАТУС =====================
static void updateTimerLabel(AppState& st) {
    if (!st.lblTimer) return;
    char buf[32];
    snprintf(buf, sizeof buf, "%.1f", std::max(0.0, st.eng.timeLeft));
    // метку обновляем только когда отображаемое значение (0.1 с) изменилось —
    // это отсеивает большинство перерисовок между тиками
    static thread_local std::string lastShown;
    if (lastShown != buf) {
        lastShown = buf;
        gtk_label_set_text(GTK_LABEL(st.lblTimer), buf);
    }

    if (st.progressBar && st.eng.cfg) {
        double frac = std::clamp(st.eng.timeLeft / st.eng.timeFull, 0.0, 1.0);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(st.progressBar), frac);
        GtkStyleContext* sc = gtk_widget_get_style_context(st.progressBar);
        if (frac < 0.25) gtk_style_context_add_class(sc, "low");
        else             gtk_style_context_remove_class(sc, "low");
    }
}

static void setStateText(AppState& st, const std::string& text, const char* styleClass) {
    if (!st.lblState) return;
    GtkStyleContext* sc = gtk_widget_get_style_context(st.lblState);
    for (const char* c : {"ok", "bad", "info"}) gtk_style_context_remove_class(sc, c);
    if (styleClass) gtk_style_context_add_class(sc, styleClass);
    gtk_label_set_text(GTK_LABEL(st.lblState), text.c_str());
}

// ===================== ПОЛЕ ВВОДА =====================
static void refreshCellStyles(AppState& st) {
    for (size_t i = 0; i < st.cells.size(); ++i) {
        GtkStyleContext* sc = gtk_widget_get_style_context(st.cells[i]);
        if ((int)i == st.eng.cursor) gtk_style_context_add_class(sc, "active");
        else                        gtk_style_context_remove_class(sc, "active");
    }
}

// Компактная подпись содержимого ячеек: позволяет вызывающему коду
// пропускать синхронизацию виджетов, когда набор цифр не менялся.
static int cellsSignature(const AppState& st) {
    int sig = st.eng.cursor * 131 + (int)st.eng.cells.size() * 7919;
    for (const auto& c : st.eng.cells)
        sig = sig * 33 + (c.empty() ? -1 : (unsigned char)c[0]);
    return sig;
}

static void syncCellsFromEngine(AppState& st) {
    for (size_t i = 0; i < st.cells.size() && i < st.eng.cells.size(); ++i) {
        // пустая ячейка (игра 2 -> 10) показывается как «_»
        const char* txt = st.eng.cells[i].empty() ? "0" : st.eng.cells[i].c_str();
        gtk_label_set_text(GTK_LABEL(st.cells[i]), txt);
    }
    // режим 10 -> 2: справа от задания показываем десятичное значение,
    // которое игрок набирает двоичными ячейками. Пока ничего не набрано —
    // строка пустая (нуль по умолчанию путает игрока).
    if (st.lblEntered) {
        std::string v = st.eng.enteredValueText();
        gtk_label_set_text(GTK_LABEL(st.lblEntered), v.c_str());
        gtk_widget_set_opacity(st.lblEntered, v.empty() ? 0.0 : 1.0);
        st.lastEnteredShown = v;
    }
    st.lastCellsSig = cellsSignature(st);
}

// Обновление строки задания: число + веса разрядов над ним (2 -> 10)
// + живое значение игрока (10 -> 2). Вызывается при старте уровня.
static void refreshNumberLine(AppState& st) {
    std::string num = st.eng.numberText();
    st.lastNumberShown = num;
    if (st.lblNumber) gtk_label_set_text(GTK_LABEL(st.lblNumber), num.c_str());

    // веса разрядов НАД ЗАДАНИЕМ (режим 2 -> 10): пересобираем разметку,
    // чтобы ширина букв совпала с шириной цифр числа
    if (st.lblWeights) {
        GList* kids = gtk_container_get_children(GTK_CONTAINER(st.lblWeights));
        for (GList* l = kids; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
        g_list_free(kids);
        GtkWidget* w = st.lblWeights;
        const char* mono = "monospace";
        PangoAttrList* attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, pango_attr_family_new(mono));
        pango_attr_list_insert(attrs, pango_attr_size_new(13 * PANGO_SCALE));
        pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_NORMAL));
        pango_attr_list_insert(attrs, pango_attr_foreground_new(0x6474, 0x8b8b, 0x9c9c));
        int n = 0;
        for (char c : num) if (c == '0' || c == '1') ++n;
        for (int i = 0; i < n; ++i) {
            long weight = 1L << (n - 1 - i);
            char buf[16]; snprintf(buf, sizeof buf, "%ld", weight);
            GtkWidget* l = gtk_label_new(buf);
            gtk_label_set_attributes(GTK_LABEL(l), attrs);
            gtk_box_pack_start(GTK_BOX(w), l, FALSE, FALSE, 0);
        }
        pango_attr_list_unref(attrs);
        gtk_widget_show_all(w);
    }
    syncCellsFromEngine(st);
}

// ===================== ТИКЕР =====================
static guint tickSource = 0;
static gint64 lastTickMs = 0;

static gboolean onTick(gpointer data) {
    AppState& st = *(AppState*)data;
    if (!st.eng.inGame || st.eng.status == GameEngine::Status::Idle && false) { /* noop */ }

    gint64 now = g_get_monotonic_time();
    double dt = (now - lastTickMs) / 1000000.0;
    lastTickMs = now;
    if (dt <= 0 || dt > 0.5) dt = 0.1;   // защита от сна системы / паузы отладчика

    st.eng.tick(dt);

    switch (st.eng.status) {
        case GameEngine::Status::Idle:
            if (!st.lastStateText.empty()) {   // не трогаем виджет без изменения текста
                st.lastStateText.clear();
                setStateText(st, "", nullptr);
            }
            break;
        case GameEngine::Status::Wrong: {
            // ✖ Есть ошибка
            static const std::string kWrong =
                "\xE2\x9C\x96 \xD0\x95\xD1\x81\xD1\x82\xD1\x8C \xD0\xBE\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBA\xD0\xB0";
            if (st.lastStateText != kWrong) {
                st.lastStateText = kWrong;
                setStateText(st, kWrong, "bad");
            }
            break;
        }
        case GameEngine::Status::NextWait: {
            // Все верно! N...
            std::string t = "\xD0\x92\xD1\x81\xD0\xB5 \xD0\xB2\xD0\xB5\xD1\x80\xD0\xBD\xD0\xBE! " +
                std::to_string(std::max(0, st.eng.waitCounter)) + "...";
            if (st.lastStateText != t) { st.lastStateText = t; setStateText(st, t, "ok"); }
            break;
        }
        case GameEngine::Status::TimeoutWait: {
            // Время вышло :( N...
            std::string t = "\xD0\x92\xD1\x80\xD0\xB5\xD0\xBC\xD1\x8F \xD0\xB2\xD1\x8B\xD1\x88\xD0\xBB\xD0\xBE :(" +
                std::to_string(std::max(0, st.eng.waitCounter)) + "...";
            if (st.lastStateText != t) { st.lastStateText = t; setStateText(st, t, "bad"); }
            break;
        }
        case GameEngine::Status::Finished: {
            // ИГРА ЗАВЕРШЕНА!
            static const std::string kFin =
                "\xD0\x98\xD0\x93\xD0\xA0\xD0\x90 \xD0\x97\xD0\x90\xD0\x92\xD0\x95\xD0\xA0\xD0\xA8\xD0\x95\xD0\x9D\xD0\x90!";
            if (st.lastStateText != kFin) {
                st.lastStateText = kFin;
                setStateText(st, kFin, "ok");
            }
            break;
        }
    }

    // уровень/число могли смениться — обновляем заголовок и ячейки
    char buf[16];
    snprintf(buf, sizeof buf, "%d", st.eng.level);
    if (st.lblLevel)  gtk_label_set_text(GTK_LABEL(st.lblLevel), buf);
    if (st.eng.numberTextCached() != st.lastNumberShown ||
        st.eng.enteredValueText() != st.lastEnteredShown) refreshNumberLine(st);
    // виджеты ячеек трогаем только когда набор цифр или курсор реально изменились
    if (cellsSignature(st) != st.lastCellsSig) {
        syncCellsFromEngine(st);
        refreshCellStyles(st);
    }
    updateTimerLabel(st);
    return G_SOURCE_CONTINUE;
}

// ===================== ДЕЙСТВИЯ ПОЛЬЗОВАТЕЛЯ =====================
static gboolean onCellClick(GtkWidget* w, GdkEventButton*, gpointer data) {
    AppState& st = *(AppState*)data;
    if (!st.eng.inGame) return TRUE;
    for (size_t i = 0; i < st.cells.size(); ++i) {
        if (gtk_widget_get_parent(st.cells[i]) == w) {
            st.eng.cursor = (int)i;
            st.eng.cycleCell((int)i, +1);
            syncCellsFromEngine(st);
            refreshCellStyles(st);
            break;
        }
    }
    return TRUE;
}

static gboolean onKeyPress(GtkWidget*, GdkEventKey* e, gpointer data) {
    AppState& st = *(AppState*)data;
    bool inGame = (gtk_stack_get_visible_child(GTK_STACK(st.stack)) == st.gameBox);

    if (!inGame) {
        if (e->keyval == GDK_KEY_Escape) gtk_main_quit();
        return FALSE;
    }
    if (!st.eng.cfg) return FALSE;

    auto act = [&](bool changedCells = true) {
        if (changedCells) syncCellsFromEngine(st);
        refreshCellStyles(st);
    };

    switch (e->keyval) {
        case GDK_KEY_Escape:
            st.eng.openMenu();
            gtk_stack_set_visible_child_name(GTK_STACK(st.stack), "menu");
            return TRUE;
        case GDK_KEY_Left:
            st.eng.moveCursor(-1); act(false); return TRUE;
        case GDK_KEY_Right:
            st.eng.moveCursor(+1); act(false); return TRUE;
        case GDK_KEY_Up: case GDK_KEY_w: case GDK_KEY_W:
            st.eng.cycleCell(st.eng.cursor, +1); act(); return TRUE;
        case GDK_KEY_Down: case GDK_KEY_s: case GDK_KEY_S:
            st.eng.cycleCell(st.eng.cursor, -1); act(); return TRUE;
    }

    guint kv = e->keyval;
    int d = -1;
    if (kv >= GDK_KEY_0 && kv <= GDK_KEY_9)            d = kv - GDK_KEY_0;
    else if (kv >= GDK_KEY_KP_0 && kv <= GDK_KEY_KP_9) d = kv - GDK_KEY_KP_0;
    else if (st.eng.cfg->inputBase == 16 && kv >= GDK_KEY_a && kv <= GDK_KEY_f) d = kv - GDK_KEY_a + 10;
    else if (st.eng.cfg->inputBase == 16 && kv >= GDK_KEY_A && kv <= GDK_KEY_F) d = kv - GDK_KEY_A + 10;

    if (d >= 0 && d < st.eng.cfg->inputBase) {
        st.eng.setCell(st.eng.cursor, digitText(d, st.eng.cfg->inputBase), /*advance=*/true);
        act();
        return TRUE;
    }
    return FALSE;
}

// ===================== ЭКРАН ИГРЫ =====================
static void clearBox(GtkWidget* box) {
    GList* kids = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList* l = kids; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(kids);
}

static void backToMenu(GtkButton*, gpointer data) {
    AppState& st = *(AppState*)data;
    st.eng.openMenu();
    gtk_stack_set_visible_child_name(GTK_STACK(st.stack), "menu");
}

static void buildCellsGrid(AppState& st) {
    if (st.cellsGrid) {
        gtk_widget_destroy(st.cellsGrid);
        st.cellsGrid = nullptr;
    }
    st.cells.clear();

    st.cellsGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(st.cellsGrid), 6);
    gtk_grid_set_row_spacing(GTK_GRID(st.cellsGrid), 4);
    gtk_widget_set_halign(st.cellsGrid, GTK_ALIGN_CENTER);

    int row = 0;
    if (st.eng.cfg->showPowersGrid) {   // веса разрядов над сеткой (игра 10 -> 2: 128..1)
        for (int i = 0; i < st.eng.digits; ++i) {
            long p = 1L << (st.eng.digits - 1 - i);
            char buf[16]; snprintf(buf, sizeof buf, "%ld", p);
            GtkWidget* l = gtk_label_new(buf);
            gtk_style_context_add_class(gtk_widget_get_style_context(l), "power");
            gtk_grid_attach(GTK_GRID(st.cellsGrid), l, i, row, 1, 1);
        }
        row = 1;
    }

    for (int i = 0; i < st.eng.digits; ++i) {
        GtkWidget* ev = gtk_event_box_new();
        // пустая ячейка (игра 2 -> 10, старт уровня) показывается как «_»
        const char* txt = st.eng.cells[i].empty() ? "0" : st.eng.cells[i].c_str();
        GtkWidget* lbl = gtk_label_new(txt);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "cell");
        gtk_widget_set_size_request(lbl, 46, 46);
        gtk_container_add(GTK_CONTAINER(ev), lbl);
        gtk_widget_add_events(ev, GDK_BUTTON_PRESS_MASK);
        g_signal_connect(ev, "button-press-event", G_CALLBACK(onCellClick), &st);
        gtk_grid_attach(GTK_GRID(st.cellsGrid), ev, i, row, 1, 1);
        st.cells.push_back(lbl);
    }
    // сетка всегда занимает фиксированную позицию 3 в вертикальном боксе
    gtk_box_pack_start(GTK_BOX(st.gameBox), st.cellsGrid, FALSE, FALSE, 6);
    gtk_box_reorder_child(GTK_BOX(st.gameBox), st.cellsGrid, 3);
    gtk_widget_show_all(st.cellsGrid);
}

static void rebuildGameView(AppState& st) {
    clearBox(st.gameBox);
    gtk_widget_set_size_request(st.gameBox, 680, -1);

    // 0 — верхняя панель
    GtkWidget* top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* back = gtk_button_new_with_label("\xE2\x86\x90 \xD0\x92 \xD0\xBC\xD0\xB5\xD0\xBD\xD1\x8E"); // ← В меню
    gtk_style_context_add_class(gtk_widget_get_style_context(back), "backbtn");
    g_signal_connect(back, "clicked", G_CALLBACK(backToMenu), &st);
    gtk_box_pack_start(GTK_BOX(top), back, FALSE, FALSE, 0);

    GtkWidget* sl1 = gtk_label_new("\xD0\xA3\xD1\x80\xD0\xBE\xD0\xB2\xD0\xB5\xD0\xBD\xD1\x8C:"); // Уровень:
    gtk_style_context_add_class(gtk_widget_get_style_context(sl1), "stat");
    st.lblLevel = gtk_label_new("1");
    gtk_style_context_add_class(gtk_widget_get_style_context(st.lblLevel), "statval");
    GtkWidget* sp = gtk_label_new("      ");
    GtkWidget* sl2 = gtk_label_new("\xD0\x92\xD1\x80\xD0\xB5\xD0\xBC\xD1\x8F:"); // Время:
    gtk_style_context_add_class(gtk_widget_get_style_context(sl2), "stat");
    st.lblTimer = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(st.lblTimer), "statval");
    gtk_box_pack_end(GTK_BOX(top), st.lblTimer, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top), sl2, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top), sp, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top), st.lblLevel, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top), sl1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(st.gameBox), top, FALSE, FALSE, 0);

    // 1 — описание
    st.lblDesc = gtk_label_new(st.eng.cfg->desc);
    gtk_style_context_add_class(gtk_widget_get_style_context(st.lblDesc), "desc");
    gtk_box_pack_start(GTK_BOX(st.gameBox), st.lblDesc, FALSE, FALSE, 4);

    // 2 — строка «число → система»; над числом — веса разрядов (режим 2 -> 10),
    // с увеличенным отступом, чтобы веса стояли чуть ниже, ближе к цифрам
    GtkWidget* numCol = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(numCol, GTK_ALIGN_CENTER);
    st.lblWeights = nullptr;
    if (st.eng.cfg && st.eng.cfg->showPowers) {   // только игра 2 -> 10
        st.lblWeights = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_margin_bottom(st.lblWeights, 4); // разряды чуть пониже
        gtk_box_pack_start(GTK_BOX(numCol), st.lblWeights, FALSE, FALSE, 0);
    }
    GtkWidget* numLine = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(numLine, GTK_ALIGN_CENTER);
    char sub[96];
    snprintf(sub, sizeof sub, "<span foreground='#4f46e5' size='large' weight='bold'>%d</span>", st.eng.cfg->fromBase);
    GtkWidget* l1 = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(l1), sub);
    st.lblNumber = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(st.lblNumber), "number");
    GtkWidget* arr = gtk_label_new("\xE2\x86\x92"); // →
    gtk_style_context_add_class(gtk_widget_get_style_context(arr), "basemark");
    snprintf(sub, sizeof sub, "<span foreground='#4f46e5' size='large' weight='bold'>%d</span>", st.eng.cfg->toBase);
    GtkWidget* l2 = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(l2), sub);
    gtk_box_pack_start(GTK_BOX(numLine), l1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(numLine), st.lblNumber, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(numLine), arr, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(numLine), l2, FALSE, FALSE, 0);
    // режимы 10 -> 2 и 2 -> 10: после стрелки показываем живое значение,
    // которое игрок набирает ячейками (десятичное / двоичное соответственно).
    // Пусто, пока ничего не набрано.
    st.lblEntered = nullptr;
    if (st.eng.cfg && (st.eng.cfg->id == 5 || st.eng.cfg->id == 6)) {
        GtkWidget* eq = gtk_label_new("=");
        gtk_style_context_add_class(gtk_widget_get_style_context(eq), "basemark");
        st.lblEntered = gtk_label_new("");
        gtk_style_context_add_class(gtk_widget_get_style_context(st.lblEntered), "number");
        gtk_widget_set_opacity(st.lblEntered, 0.0);
        gtk_box_pack_start(GTK_BOX(numLine), eq, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(numLine), st.lblEntered, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(numCol), numLine, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(st.gameBox), numCol, FALSE, FALSE, 6);

    // 3 — placeholder под сетку ячеек (buildCellsGrid вставит её на это место)
    GtkWidget* placeholder = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(st.gameBox), placeholder, FALSE, FALSE, 6);

    // 4 — полоса времени
    st.progressBar = gtk_progress_bar_new();
    gtk_widget_set_size_request(st.progressBar, 420, 8);
    gtk_widget_set_halign(st.progressBar, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(st.gameBox), st.progressBar, FALSE, FALSE, 2);

    // 5 — статус
    st.lblState = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(st.gameBox), st.lblState, FALSE, FALSE, 4);

    // 6 — подсказка
    GtkWidget* hint = gtk_label_new(
        "\xD0\xA1\xD1\x82\xD1\x80\xD0\xB5\xD0\xBB\xD0\xBA\xD0\xB8 \xE2\x86\x90 \xE2\x86\x92 \xE2\x80\x94 \xD0\xB2\xD1\x8B\xD0\xB1\xD0\xBE\xD1\x80 "
        "\xD1\x8F\xD1\x87\xD0\xB5\xD0\xB9\xD0\xBA\xD0\xB8 \xC2\xB7 \xE2\x86\x91 \xE2\x86\x93 \xE2\x80\x94 +1/-1 \xC2\xB7 "
        "\xD0\xBA\xD0\xBB\xD0\xB8\xD0\xBA \xE2\x80\x94 \xD1\x81\xD0\xBC\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x82\xD1\x8C "
        "\xD1\x86\xD0\xB8\xD1\x84\xD1\x80\xD1\x83 \xC2\xB7 0-9/A-F \xE2\x80\x94 \xD0\xB2\xD0\xB2\xD0\xBE\xD0\xB4 "
        "\xC2\xB7 Esc \xE2\x80\x94 \xD0\xBC\xD0\xB5\xD0\xBD\xD1\x8E");
    gtk_style_context_add_class(gtk_widget_get_style_context(hint), "hint");
    gtk_box_pack_start(GTK_BOX(st.gameBox), hint, FALSE, FALSE, 0);

    gtk_widget_show_all(st.gameBox);
}

static void startGame(GtkButton* b, gpointer data) {
    AppState& st = *(AppState*)data;
    int gameId = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "gameid"));
    st.eng.startGame(gameId);
    rebuildGameView(st);
    buildCellsGrid(st);
    refreshCellStyles(st);
    updateTimerLabel(st);
    refreshNumberLine(st);
    gtk_stack_set_visible_child_name(GTK_STACK(st.stack), "game");
}

// ===================== МЕНЮ =====================
static void buildMenu(AppState& st) {
    GtkWidget* menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(menu), "card");
    gtk_widget_set_halign(menu, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(menu, GTK_ALIGN_CENTER);

    // Игры по системам счисления
    GtkWidget* title = gtk_label_new(
        "\xD0\x98\xD0\xB3\xD1\x80\xD1\x8B \xD0\xBF\xD0\xBE \xD1\x81\xD0\xB8\xD1\x81\xD1\x82\xD0\xB5\xD0\xBC\xD0\xB0\xD0\xBC "
        "\xD1\x81\xD1\x87\xD0\xB8\xD1\x81\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x8F");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title");
    gtk_box_pack_start(GTK_BOX(menu), title, FALSE, FALSE, 4);

    // Выберите режим перевода чисел
    GtkWidget* subtitle = gtk_label_new(
        "\xD0\x92\xD1\x8B\xD0\xB1\xD0\xB5\xD1\x80\xD0\xB8\xD1\x82\xD0\xB5 \xD1\x80\xD0\xB5\xD0\xB6\xD0\xB8\xD0\xBC "
        "\xD0\xBF\xD0\xB5\xD1\x80\xD0\xB5\xD0\xB2\xD0\xBE\xD0\xB4\xD0\xB0 \xD1\x87\xD0\xB8\xD1\x81\xD0\xB5\xD0\xBB");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "subtitle");
    gtk_box_pack_start(GTK_BOX(menu), subtitle, FALSE, FALSE, 0);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);

    int i = 0;
    for (const auto& g : games()) {
        GtkWidget* b = gtk_button_new_with_label(g.title);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "gamebtn");
        g_object_set_data(G_OBJECT(b), "gameid", GINT_TO_POINTER(g.id));
        g_signal_connect(b, "clicked", G_CALLBACK(startGame), &st);
        gtk_grid_attach(GTK_GRID(grid), b, i % 3, i / 3, 1, 1);
        i++;
    }
    gtk_box_pack_start(GTK_BOX(menu), grid, FALSE, FALSE, 10);
    gtk_stack_add_named(GTK_STACK(st.stack), menu, "menu");
}

// ===================== MAIN =====================
int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    // Пользовательская тема GTK (на KDE — Breeze, на XFCE — Adwaita/Xfwm) остаётся
    // активной: наши стили добавляются поверх неё с приоритетом APPLICATION.
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, CSS, -1, nullptr);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    AppState* st = new AppState();
    st->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    // Игры по системам счисления
    gtk_window_set_title(GTK_WINDOW(st->window),
        "\xD0\x98\xD0\xB3\xD1\x80\xD1\x8B \xD0\xBF\xD0\xBE \xD1\x81\xD0\xB8\xD1\x81\xD1\x82\xD0\xB5\xD0\xBC\xD0\xB0\xD0\xBC "
        "\xD1\x81\xD1\x87\xD0\xB8\xD1\x81\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x8F");
    // Иконка окна (панель задач KDE/XFCE, alt+Tab): PNG вшит в бинарник
    // как base64-строка (см. resources/icon_data.h, генерируется CMake/скриптом).
    if (ICON_PNG_BASE64[0]) {
        gsize olen = 0;
        gchar* raw = reinterpret_cast<gchar*>(g_base64_decode(ICON_PNG_BASE64, &olen));
        GdkPixbufLoader* loader = gdk_pixbuf_loader_new_with_type("png", nullptr);
        gdk_pixbuf_loader_write(loader, (const guchar*)(const guint8*)raw, olen, nullptr);
        gdk_pixbuf_loader_close(loader, nullptr);
        GdkPixbuf* icon = gdk_pixbuf_loader_get_pixbuf(loader);
        if (icon) gtk_window_set_icon(GTK_WINDOW(st->window), icon);
        g_object_unref(loader);
        g_free(raw);
    }
    gtk_window_set_default_size(GTK_WINDOW(st->window), 900, 640);
    g_signal_connect(st->window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    st->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(st->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(st->stack), 250);
    gtk_container_add(GTK_CONTAINER(st->window), st->stack);

    buildMenu(*st);

    // placeholder страницы "game" (пересобирается при старте)
    st->gameBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_stack_add_named(GTK_STACK(st->stack), st->gameBox, "game");

    g_signal_connect(st->window, "key-press-event", G_CALLBACK(onKeyPress), st);

    lastTickMs = g_get_monotonic_time();
    tickSource = g_timeout_add(100, onTick, st);

    gtk_widget_show_all(st->window);
    gtk_main();
    if (tickSource) g_source_remove(tickSource);
    delete st;
    return 0;
}
