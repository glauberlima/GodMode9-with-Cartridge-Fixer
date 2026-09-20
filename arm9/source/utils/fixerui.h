#pragma once

#include "common.h"

// Dashboard for the cartridge fixer, rendered on ALT_SCREEN (the screen the
// fixer/progress UI does not use). Renders incrementally and is time-throttled.

typedef enum {
    FIXERUI_PHASE_IDLE = 0,
    FIXERUI_PHASE_OPEN,
    FIXERUI_PHASE_HEADERS,
    FIXERUI_PHASE_EXT_HASH,
    FIXERUI_PHASE_EXEFS_HASH,
    FIXERUI_PHASE_ROMFS_HASH,
    FIXERUI_PHASE_EXEFS,
    FIXERUI_PHASE_ROMFS,
    FIXERUI_PHASE_DONE,
} FixerUiPhase;

void FixerUI_Begin(const char* path);
void FixerUI_End(void);
void FixerUI_SetPhase(FixerUiPhase phase);
void FixerUI_SetSub(u32 cur, u32 total);
void FixerUI_SetCounts(u32 fixed, u32 bad);
void FixerUI_SetFlags(bool autoskip, bool log, bool refresh_every_read);
void FixerUI_Heartbeat(void);
void FixerUI_Tick(void);
u32  FixerUI_MaxTickMs(void);
