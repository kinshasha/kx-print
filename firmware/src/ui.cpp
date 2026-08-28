#include "ui.h"
#include "config.h"
#include "job.h"
#include "net.h"
#include "kx_protocol.h"
#include <Arduino.h>
#include <Arduino_LED_Matrix.h>

ArduinoLEDMatrix matrix;

// Packed 12x8 frames for loadFrame(uint32_t[3]). Row-major, MSB first.
static const uint32_t FR_READY[3]    = { 0x00000200, 0x48084102, 0x20140080 };
static const uint32_t FR_PRINT0[3]   = { 0x7E041041, 0x07E04004, 0x00400400 };
static const uint32_t FR_PRINT1[3]   = { 0x7E041041, 0x07E04104, 0x08404402 };
static const uint32_t FR_PAUSE[3]    = { 0x39C39C39, 0xC39C39C3, 0x9C39C000 };
static const uint32_t FR_WIFI[3]     = { 0x80280280, 0x28428A29, 0x1260C000 };
static const uint32_t FR_ERROR[3]    = { 0x06006006, 0x00600600, 0x00060060 };
static const uint32_t FR_CANCEL[3]   = { 0xC0360630, 0xC1980F01, 0x9830C606 };

static bool run_stable = true;
static bool run_raw = true;
static uint32_t run_t0;

static bool can_stable = true;   // HIGH = not pressed (pull-up)
static bool can_raw = true;
static uint32_t can_t0;
static uint32_t can_press_start;
static bool can_held_fired;
static bool ev_short;
static bool ev_hold;

static uint32_t cancelled_until;
static UiGlyph last_glyph = UI_WIFI;

static void debounce(uint8_t pin, bool *raw, bool *stable, uint32_t *t0) {
  bool now = digitalRead(pin);  // HIGH = open (pull-up)
  if (now != *raw) {
    *raw = now;
    *t0 = millis();
  } else if ((uint32_t)(millis() - *t0) >= DEBOUNCE_MS && now != *stable) {
    *stable = now;
  }
}

void ui_init() {
  pinMode(PIN_RUN, INPUT_PULLUP);
  pinMode(PIN_CANCEL, INPUT_PULLUP);
  run_raw = run_stable = digitalRead(PIN_RUN);
  can_raw = can_stable = digitalRead(PIN_CANCEL);
  run_t0 = can_t0 = millis();
  matrix.begin();
  matrix.loadFrame(FR_WIFI);
}

bool ui_boot_hold_cancel() {
  uint32_t t0 = millis();
  bool held = true;
  while ((uint32_t)(millis() - t0) < BOOT_HOLD_MS) {
    if (digitalRead(PIN_CANCEL) != LOW) held = false;
    delay(10);
  }
  return held && digitalRead(PIN_CANCEL) == LOW;
}

void ui_poll() {
  debounce(PIN_RUN, &run_raw, &run_stable, &run_t0);
  bool prev = can_stable;
  debounce(PIN_CANCEL, &can_raw, &can_stable, &can_t0);

  if (prev == HIGH && can_stable == LOW) {
    can_press_start = millis();
    can_held_fired = false;
  }
  if (can_stable == LOW && !can_held_fired) {
    if ((uint32_t)(millis() - can_press_start) >= CANCEL_HOLD_MS) {
      can_held_fired = true;
      ev_hold = true;
    }
  }
  if (prev == LOW && can_stable == HIGH) {
    if (!can_held_fired) ev_short = true;
  }
}

bool ui_run() { return run_stable == LOW; }
bool ui_cancel_down() { return can_stable == LOW; }

bool ui_cancel_short() {
  if (!ev_short) return false;
  ev_short = false;
  return true;
}

bool ui_cancel_hold() {
  if (!ev_hold) return false;
  ev_hold = false;
  return true;
}

void ui_show_cancelled() {
  cancelled_until = millis() + CANCELLED_LED_MS;
}

void ui_force_glyph(UiGlyph g) {
  last_glyph = g;
}

void ui_render() {
  UiGlyph g;
  if (job_error()) {
    g = UI_ERROR;
  } else if (cancelled_until && (int32_t)(millis() - cancelled_until) < 0) {
    g = UI_CANCELLED;
  } else if (!net_sta_up() && net_mode() != NET_OFF) {
    // AP or connecting — W, unless we are actively printing from serial
    if (job_printing()) g = UI_PRINTING;
    else if (job_any() && !ui_run()) g = UI_PAUSED;
    else g = UI_WIFI;
  } else if (job_any() && !ui_run()) {
    g = UI_PAUSED;
  } else if (job_printing() || (job_any() && ui_run() && job_buffered())) {
    g = UI_PRINTING;
  } else {
    g = UI_READY;
  }

  if (g == UI_PRINTING) {
    const uint32_t *fr = ((millis() / 250) & 1) ? FR_PRINT1 : FR_PRINT0;
    matrix.loadFrame(fr);
    last_glyph = g;
    return;
  }

  if (g != last_glyph) {
    last_glyph = g;
    switch (g) {
      case UI_READY:     matrix.loadFrame(FR_READY); break;
      case UI_PAUSED:    matrix.loadFrame(FR_PAUSE); break;
      case UI_WIFI:      matrix.loadFrame(FR_WIFI); break;
      case UI_ERROR:     matrix.loadFrame(FR_ERROR); break;
      case UI_CANCELLED: matrix.loadFrame(FR_CANCEL); break;
      default:           matrix.loadFrame(FR_WIFI); break;
    }
  }
}
