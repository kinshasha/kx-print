#include <Arduino.h>
#include "config.h"
#include "app.h"
#include "kx_protocol.h"
#include "job.h"
#include "ui.h"
#include "net.h"
#include "raw9100.h"
#include "lpd.h"
#include "serial_cli.h"

static bool g_abort;
static uint8_t g_yield_depth;

bool kx_abort_requested() { return g_abort; }
void kx_request_abort() { g_abort = true; }
void kx_clear_abort() { g_abort = false; }

static void handle_buttons() {
  if (ui_cancel_hold()) {
    job_clear_queue();
    if (job_error()) job_clear_error();
    Serial.println(F("CANCEL held — queue cleared"));
  } else if (ui_cancel_short()) {
    if (job_error()) {
      job_clear_error();
      Serial.println(F("error cleared"));
    } else {
      job_cancel_current();
    }
  }
}

void kx_yield() {
  if (g_yield_depth) return;
  g_yield_depth++;
  ui_poll();
  handle_buttons();
  net_mdns_run();
  raw9100_poll();
  lpd_poll();
  serial_cli_poll();
  g_yield_depth--;
}

void setup() {
  // Outputs to safe idle BEFORE Serial, Wi-Fi, or anything else.
  kx_protocol_init();

  Serial.begin(SERIAL_BAUD);
  uint32_t t0 = millis();
  while (!Serial && (uint32_t)(millis() - t0) < 1500) { /* USB enumerate */ }

  ui_init();
  job_init();

  Serial.println();
  Serial.println(F("KX-Print " KX_FW_VERSION));
  Serial.println(F("Panasonic KX-R540 Wi-Fi printer"));
  Serial.println(F("SAFETY: Mini-DIN pin 5 is +12 V — never connect it to the UNO."));

  bool force_ap = ui_boot_hold_cancel();
  if (force_ap) {
    Serial.println(F("CANCEL held at boot — Wi-Fi config reset"));
  }

  net_begin(force_ap);
  raw9100_begin();
  lpd_begin();
  serial_cli_init();
}

void loop() {
  ui_poll();
  handle_buttons();
  net_poll();
  raw9100_poll();
  lpd_poll();
  serial_cli_poll();
  job_print_poll();
  ui_render();
}
