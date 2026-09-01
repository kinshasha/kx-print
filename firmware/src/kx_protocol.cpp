#include "kx_protocol.h"
#include "config.h"
#include "app.h"
#include <Arduino.h>

// KX-R60-class Mini-DIN-8 (xunker, tested KX-R435). KX-R540 treated the same.
//
// Idle: /ONLINE HIGH, /STB HIGH, DATA HIGH. ACK idles LOW (typewriter 10k pulldown).
// Do NOT enable INPUT_PULLUP on ACK. It fights that pulldown.
//
// Per bit: DATA, 50 us, /STB HIGH→LOW, wait ACK HIGH, /STB LOW→HIGH, wait ACK LOW.
// This is the opposite of RP-K105 / DE-9 thermal writers. Do not wait ACK-low-then-high.
//
// ACK timeout 1500 ms is our watchdog; original adapter code waits forever.

static bool g_dry;
static bool g_timeout;
static uint16_t g_setup_us = DATA_SETUP_US;

void kx_set_dry_run(bool on) { g_dry = on; }
bool kx_is_dry_run() { return g_dry; }

uint16_t kx_data_setup_us() { return g_setup_us; }

void kx_set_data_setup_us(uint16_t us) {
  if (us < 5) us = 5;
  if (us > 500) us = 500;
  g_setup_us = us;
}
bool kx_last_timeout() { return g_timeout; }
void kx_clear_timeout() { g_timeout = false; }

void kx_safe_idle() {
  digitalWrite(PIN_ONLINE, HIGH);
  digitalWrite(PIN_STB, HIGH);
  digitalWrite(PIN_DATA, HIGH);
}

void kx_protocol_init() {
  // Drive idle levels BEFORE pinMode so the pins never glitch LOW at boot.
  digitalWrite(PIN_DATA, HIGH);
  digitalWrite(PIN_STB, HIGH);
  digitalWrite(PIN_ONLINE, HIGH);
  pinMode(PIN_DATA, OUTPUT);
  pinMode(PIN_STB, OUTPUT);
  pinMode(PIN_ONLINE, OUTPUT);
  pinMode(PIN_ACK, INPUT);  // no pull-up
  kx_safe_idle();
#if KX_DRY_RUN_DEFAULT
  g_dry = true;
#endif
}

bool kx_ack_idle_low() {
  return digitalRead(PIN_ACK) == LOW;
}

static bool wait_ack(uint8_t want) {
  const uint32_t t0 = millis();
  uint32_t last_yield = t0;
  while (digitalRead(PIN_ACK) != want) {
    uint32_t now = millis();
    if ((now - t0) >= ACK_TIMEOUT_MS) {
      return false;
    }
    if (kx_abort_requested()) {
      return false;
    }
    if ((now - last_yield) >= ACK_YIELD_MS) {
      last_yield = now;
      kx_yield();
    }
  }
  return true;
}

bool kx_send_byte(uint8_t b) {
  g_timeout = false;

  if (g_dry) {
    Serial.print(F("dry "));
    if (b >= 0x20 && b < 0x7F) {
      Serial.print((char)b);
    } else {
      Serial.print(F("0x"));
      if (b < 16) Serial.print('0');
      Serial.print(b, HEX);
    }
    Serial.println();
    digitalWrite(PIN_ONLINE, LOW);
    for (uint8_t i = 0; i < 8; i++) {
      digitalWrite(PIN_DATA, (b & 1) ? HIGH : LOW);
      delayMicroseconds(g_setup_us);
      digitalWrite(PIN_STB, LOW);
      delayMicroseconds(g_setup_us);
      digitalWrite(PIN_STB, HIGH);
      delayMicroseconds(g_setup_us);
      b >>= 1;
      if (kx_abort_requested()) {
        kx_safe_idle();
        return false;
      }
    }
    kx_safe_idle();
    return true;
  }

  digitalWrite(PIN_ONLINE, LOW);

  for (uint8_t i = 0; i < 8; i++) {
    if (kx_abort_requested()) {
      kx_safe_idle();
      return false;
    }

    digitalWrite(PIN_DATA, (b & 1) ? HIGH : LOW);
    delayMicroseconds(g_setup_us);

    digitalWrite(PIN_STB, LOW);
    if (!wait_ack(HIGH)) {
      g_timeout = !kx_abort_requested();
      kx_safe_idle();
      return false;
    }

    digitalWrite(PIN_STB, HIGH);
    if (!wait_ack(LOW)) {
      g_timeout = !kx_abort_requested();
      kx_safe_idle();
      return false;
    }

    b >>= 1;
  }

  kx_safe_idle();
  return true;
}
