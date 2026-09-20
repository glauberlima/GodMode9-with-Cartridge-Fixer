#include "fixerui.h"

#include <stdarg.h>

#include "ui.h"
#include "timer.h"

#define UI_W             SCREEN_WIDTH_ALT
#define UI_H             SCREEN_HEIGHT
#define FIXERUI_REFRESH_MS 120

// layout
#define Y_TITLE          0
#define Y_PHASE          18
#define Y_SUB            30
#define Y_BAR            42
#define Y_COUNTS         58
#define Y_FLAGS          70
#define Y_SEP1           82
#define Y_ELAPSED        90
#define Y_SEP2           102
#define Y_HELP1          114
#define Y_HELP2          126
#define Y_HELP3          138

static bool ui_active = false;
static FixerUiPhase ui_phase = FIXERUI_PHASE_IDLE;
static u32 ui_sub_cur = 0, ui_sub_total = 0;
static u32 ui_fixed = 0, ui_bad = 0;
static bool ui_autoskip = false, ui_log = false, ui_refresh_read = false;

static u64 ui_start = 0;
static u64 ui_last_hb = 0;
static u64 ui_last_draw = 0;
static u32 ui_spin = 0;
static u32 ui_tick_ms = 0;
static u32 ui_max_tick_ms = 0;
static u32 ui_bar_pct = 0xFFFFFFFFu;

// cached dynamic strings, to only redraw what changed
static char c_phase[64] = { 0 };
static char c_sub[64] = { 0 };
static char c_counts[64] = { 0 };
static char c_flags[64] = { 0 };
static char c_elapsed[64] = { 0 };
static char c_status[64] = { 0 };

static const char* phase_name(FixerUiPhase phase) {
    switch (phase) {
        case FIXERUI_PHASE_OPEN:       return "Opening";
        case FIXERUI_PHASE_HEADERS:    return "Reading headers";
        case FIXERUI_PHASE_EXT_HASH:   return "ExtHeader hash";
        case FIXERUI_PHASE_EXEFS_HASH: return "ExeFS hash";
        case FIXERUI_PHASE_ROMFS_HASH: return "ROMFS hash";
        case FIXERUI_PHASE_EXEFS:      return "ExeFS files";
        case FIXERUI_PHASE_ROMFS:      return "ROMFS level-3";
        case FIXERUI_PHASE_DONE:       return "Done";
        default:                       return "Idle";
    }
}

static void draw_static(void) {
    DrawRectangle(ALT_SCREEN, 0, Y_TITLE, UI_W, 13, COLOR_STD_FONT);
    DrawString(ALT_SCREEN, "CARTRIDGE FIXER", 4, Y_TITLE + 2, COLOR_STD_BG, COLOR_STD_FONT);

    DrawRectangle(ALT_SCREEN, 0, Y_SEP1, UI_W, 1, COLOR_DARKGREY);
    DrawRectangle(ALT_SCREEN, 0, Y_SEP2, UI_W, 1, COLOR_DARKGREY);

    DrawString(ALT_SCREEN, "B   cancel current block", 4, Y_HELP1, COLOR_LIGHTGREY, COLOR_STD_BG);
    DrawString(ALT_SCREEN, "Y   skip bad block (hold)", 4, Y_HELP2, COLOR_LIGHTGREY, COLOR_STD_BG);
    DrawString(ALT_SCREEN, "X/SEL  set at launch", 4, Y_HELP3, COLOR_DARKGREY, COLOR_STD_BG);
}

// Redraw a line only when its text changed (saves the bus and avoids flicker)
static void draw_cached(int x, int y, u32 color, char* cache, const char* fmt, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (!strcmp(buf, cache)) return;
    snprintf(cache, 64, "%s", buf);
    DrawRectangle(ALT_SCREEN, 0, y, UI_W, 9, COLOR_STD_BG);
    DrawString(ALT_SCREEN, buf, x, y, color, COLOR_STD_BG);
}

static void draw_bar(void) {
    u32 pct = 0;
    if (ui_sub_total)
        pct = (ui_sub_cur >= ui_sub_total) ? 100 : (u32) (((u64) ui_sub_cur * 100) / ui_sub_total);
    if (pct == ui_bar_pct) return;
    ui_bar_pct = pct;

    const int bx = 4, by = Y_BAR, bw = UI_W - 8, bh = 9;
    u32 fill = (u32) (((u64) (bw - 4) * pct) / 100);
    DrawRectangle(ALT_SCREEN, bx, by, bw, bh, COLOR_STD_FONT);
    DrawRectangle(ALT_SCREEN, bx + 2, by + 2, bw - 4, bh - 4, COLOR_STD_BG);
    if (fill) DrawRectangle(ALT_SCREEN, bx + 2, by + 2, fill, bh - 4, COLOR_STD_FONT);
}

void FixerUI_Begin(const char* path) {
    (void) path;
    ui_active = true;
    ui_phase = FIXERUI_PHASE_OPEN;
    ui_sub_cur = ui_sub_total = 0;
    ui_fixed = ui_bad = 0;
    ui_autoskip = ui_log = ui_refresh_read = false;
    ui_start = ui_last_hb = timer_start();
    ui_last_draw = 0; // force the first Tick to draw immediately
    ui_spin = 0;
    ui_tick_ms = ui_max_tick_ms = 0;
    ui_bar_pct = 0xFFFFFFFFu;
    c_phase[0] = c_sub[0] = c_counts[0] = c_flags[0] = c_elapsed[0] = c_status[0] = 0;

    ClearScreen(ALT_SCREEN, COLOR_STD_BG);
    draw_static();
    FixerUI_Tick();
}

void FixerUI_End(void) {
    if (!ui_active) return;
    ui_active = false;
    ClearScreen(ALT_SCREEN, COLOR_STD_BG);
}

void FixerUI_SetPhase(FixerUiPhase phase) {
    ui_phase = phase;
}

void FixerUI_SetSub(u32 cur, u32 total) {
    ui_sub_cur = cur;
    ui_sub_total = total;
}

void FixerUI_SetCounts(u32 fixed, u32 bad) {
    ui_fixed = fixed;
    ui_bad = bad;
}

void FixerUI_SetFlags(bool autoskip, bool log, bool refresh_every_read) {
    ui_autoskip = autoskip;
    ui_log = log;
    ui_refresh_read = refresh_every_read;
}

void FixerUI_Heartbeat(void) {
    ui_last_hb = timer_start();
}

u32 FixerUI_MaxTickMs(void) {
    return ui_max_tick_ms;
}

void FixerUI_Tick(void) {
    if (!ui_active) return;

    u64 now = timer_start();
    if (timer_msec(ui_last_draw) < FIXERUI_REFRESH_MS) return;
    ui_last_draw = now;
    ui_spin++;

    u64 tick_start = timer_start();

    u64 elapsed = timer_sec(ui_start);
    u64 idle_ms = timer_msec(ui_last_hb);

    draw_cached(4, Y_PHASE, COLOR_STD_FONT, c_phase, "PHASE  %s", phase_name(ui_phase));
    if (ui_sub_total)
        draw_cached(4, Y_SUB, COLOR_LIGHTGREY, c_sub, "       %u / %u", (unsigned) ui_sub_cur, (unsigned) ui_sub_total);
    else
        draw_cached(4, Y_SUB, COLOR_LIGHTGREY, c_sub, "       ...");
    draw_bar();

    draw_cached(4, Y_COUNTS, COLOR_STD_FONT, c_counts, "FIXED %u   UNFIXABLE %u", (unsigned) ui_fixed, (unsigned) ui_bad);
    draw_cached(4, Y_FLAGS, COLOR_LIGHTGREY, c_flags, "Autoskip[%s] Log[%s] Refresh[%s]",
        ui_autoskip ? "ON" : "OFF", ui_log ? "ON" : "OFF", ui_refresh_read ? "ON" : "OFF");

    static const char spin_chars[] = "|/-\\";
    draw_cached(4, Y_ELAPSED, COLOR_STD_FONT, c_elapsed, "ELAPSED %02u:%02u:%02u  idle %us  %c",
        (unsigned) (elapsed / 3600), (unsigned) ((elapsed / 60) % 60), (unsigned) (elapsed % 60),
        (unsigned) (idle_ms / 1000), spin_chars[ui_spin & 3]);

    if (idle_ms >= 3000)
        draw_cached(4, Y_HELP3, COLOR_RED, c_status, "** NO UPDATE FOR %us **", (unsigned) (idle_ms / 1000));
    else if (c_status[0]) {
        c_status[0] = 0;
        DrawRectangle(ALT_SCREEN, 0, Y_HELP3, UI_W, 9, COLOR_STD_BG);
        DrawString(ALT_SCREEN, "X/SEL  set at launch", 4, Y_HELP3, COLOR_DARKGREY, COLOR_STD_BG);
    }

    ui_tick_ms = (u32) timer_msec(tick_start);
    if (ui_tick_ms > ui_max_tick_ms) ui_max_tick_ms = ui_tick_ms;
}
