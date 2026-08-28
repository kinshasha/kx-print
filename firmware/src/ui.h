#pragma once

#include <stdint.h>
#include <stdbool.h>

enum UiGlyph {
  UI_WIFI = 0,
  UI_READY,
  UI_PRINTING,
  UI_PAUSED,
  UI_ERROR,
  UI_CANCELLED
};

void ui_init();
void ui_poll();                    // debounce D8/D9
void ui_render();
void ui_show_cancelled();
bool ui_run();                     // true = RUN (toggle closed to GND)
bool ui_cancel_down();
bool ui_cancel_short();            // consumed edge: released after < hold
bool ui_cancel_hold();             // consumed: held ~3s while running
bool ui_boot_hold_cancel();        // call from setup, blocking ~BOOT_HOLD_MS
void ui_force_glyph(UiGlyph g);
