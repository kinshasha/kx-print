#include "raw9100.h"
#include "config.h"
#include "job.h"
#include <Arduino.h>
#include <WiFiS3.h>

static WiFiServer server(PORT_RAW);
static WiFiClient client;
static bool job_open;
static bool started;

void raw9100_begin() {
  server.begin();
  started = true;
  Serial.println(F("raw 9100 listening"));
}

void raw9100_stop_client() {
  if (job_open) {
    job_end();
    job_open = false;
  }
  if (client) client.stop();
}

bool raw9100_busy() { return client && client.connected(); }

void raw9100_poll() {
  if (!started) return;

  if (!client || !client.connected()) {
    if (job_open) {
      job_end();
      job_open = false;
      Serial.println(F("9100: job end (disconnect)"));
    }
    if (client) client.stop();
    client = server.available();
    if (client) {
      if (!job_begin("raw9100")) {
        Serial.println(F("9100: no job slot"));
        client.stop();
        return;
      }
      job_open = true;
      Serial.println(F("9100: job begin"));
    }
  }

  if (!client || !job_open) return;

  uint16_t n = 0;
  while (client.available() && job_has_space() && n < 64) {
    int ch = client.read();
    if (ch < 0) break;
    if (!job_feed((uint8_t)ch)) break;
    n++;
  }
}
