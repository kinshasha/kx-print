#include "lpd.h"
#include "config.h"
#include "job.h"
#include <Arduino.h>
#include <WiFiS3.h>
#include <string.h>
#include <stdlib.h>

// RFC 1179 receive-job. Stream the data file. Do not spool it.
// Accept control-file-first OR data-file-first (CUPS often sends df first).
// ACK is a single 0x00 octet. Source ports 721-731 are NOT enforced.

static WiFiServer server(PORT_LPD);
static WiFiClient client;
static bool started;

enum LpdState {
  LPD_IDLE,
  LPD_LINE,
  LPD_SKIP,         // control file body
  LPD_STREAM,       // data file body (known length)
  LPD_STREAM_OPEN,  // data file, count=0, until close
  LPD_WAIT_NUL,
  LPD_QUEUE
};

static LpdState st;
static char line[LINE_BUF];
static uint8_t llen;
static uint32_t left;          // bytes remaining in this file
static uint8_t after;          // LPD_SKIP or LPD_STREAM → then WAIT_NUL
static bool in_receive;
static bool df_open;
static bool started_job;

static void ack_ok() {
  if (client) client.write((uint8_t)0x00);
}

static void ack_nak() {
  if (client) client.write((uint8_t)0x01);
}

static void reset_conn() {
  if (df_open) {
    job_end();
    df_open = false;
  }
  started_job = false;
  in_receive = false;
  llen = 0;
  left = 0;
  st = LPD_IDLE;
  if (client) client.stop();
}

static void begin_df() {
  if (!df_open) {
    if (!job_begin("lpr")) {
      ack_nak();
      reset_conn();
      return;
    }
    df_open = true;
    started_job = true;
    Serial.println(F("lpd: data file → printer"));
  }
}

static void end_df() {
  if (df_open) {
    job_end();
    df_open = false;
    Serial.println(F("lpd: data file complete"));
  }
}

static bool parse_count_name(const char *s, uint32_t *count) {
  // s is "1234 filename" after the command byte
  while (*s == ' ') s++;
  char *end = nullptr;
  unsigned long v = strtoul(s, &end, 10);
  *count = (uint32_t)v;
  return true;
}

static void handle_top_cmd(const uint8_t *cmd, uint8_t n) {
  if (n < 1) return;
  uint8_t code = cmd[0];
  // queue name follows, LF already stripped
  switch (code) {
    case 0x01:  // print any waiting jobs (we already print as we stream)
      client.stop();
      st = LPD_IDLE;
      break;
    case 0x02:  // receive a printer job
      ack_ok();
      in_receive = true;
      st = LPD_LINE;
      llen = 0;
      Serial.println(F("lpd: receive-job"));
      break;
    case 0x03:  // short queue
    case 0x04: {  // long queue
      char buf[96];
      snprintf(buf, sizeof(buf), "%s: %u job(s), %u bytes buffered\n",
               KX_QUEUE, job_queued(), job_buffered());
      client.print(buf);
      client.stop();
      st = LPD_IDLE;
      break;
    }
    case 0x05:  // remove jobs
      job_cancel_current();
      client.stop();
      st = LPD_IDLE;
      break;
    default:
      Serial.print(F("lpd: unknown cmd 0x"));
      Serial.println(code, HEX);
      client.stop();
      st = LPD_IDLE;
      break;
  }
}

static void handle_sub(const uint8_t *cmd, uint8_t n) {
  if (n < 1) return;
  uint8_t code = cmd[0];
  if (code == 0x01) {  // abort job
    Serial.println(F("lpd: abort"));
    if (df_open) job_cancel_current();
    reset_conn();
    return;
  }
  if (code == 0x02 || code == 0x03) {
    uint32_t count = 0;
    parse_count_name((const char *)cmd + 1, &count);
    ack_ok();
    left = count;
    if (code == 0x02) {
      Serial.print(F("lpd: control file "));
      Serial.println(count);
      after = LPD_SKIP;
      st = (count == 0) ? LPD_WAIT_NUL : LPD_SKIP;
    } else {
      Serial.print(F("lpd: data file "));
      Serial.println(count);
      begin_df();
      if (count == 0) {
        st = LPD_STREAM_OPEN;
      } else {
        after = LPD_STREAM;
        st = LPD_STREAM;
      }
    }
    llen = 0;
    return;
  }
  Serial.print(F("lpd: unknown sub 0x"));
  Serial.println(code, HEX);
}

void lpd_begin() {
  server.begin();
  started = true;
  st = LPD_IDLE;
  Serial.println(F("lpd 515 listening"));
}

void lpd_stop_client() { reset_conn(); }
bool lpd_busy() { return client && client.connected(); }
uint8_t lpd_queue_jobs() { return job_queued(); }

void lpd_poll() {
  if (!started) return;

  if (st == LPD_IDLE) {
    if (client && !client.connected()) {
      client.stop();
    }
    if (!client || !client.connected()) {
      client = server.available();
      if (!client) return;
      llen = 0;
      in_receive = false;
      st = LPD_LINE;
    }
  }

  if (!client) return;

  if (!client.connected()) {
    if (st == LPD_STREAM_OPEN) end_df();
    else if (df_open) job_cancel_current();
    reset_conn();
    return;
  }

  if (st == LPD_LINE) {
    while (client.available()) {
      int ch = client.read();
      if (ch < 0) break;
      if (ch == '\n') {
        line[llen] = 0;
        if (in_receive) handle_sub((const uint8_t *)line, llen);
        else handle_top_cmd((const uint8_t *)line, llen);
        llen = 0;
        return;
      }
      if (llen < LINE_BUF - 1) line[llen++] = (char)ch;
    }
    return;
  }

  if (st == LPD_SKIP) {
    while (client.available() && left) {
      client.read();
      left--;
    }
    if (left == 0) st = LPD_WAIT_NUL;
    return;
  }

  if (st == LPD_STREAM) {
    while (client.available() && left && job_has_space()) {
      int ch = client.read();
      if (ch < 0) break;
      job_feed((uint8_t)ch);
      left--;
    }
    if (left == 0) st = LPD_WAIT_NUL;
    return;
  }

  if (st == LPD_STREAM_OPEN) {
    while (client.available() && job_has_space()) {
      int ch = client.read();
      if (ch < 0) break;
      job_feed((uint8_t)ch);
    }
    return;
  }

  if (st == LPD_WAIT_NUL) {
    if (!client.available()) return;
    int ch = client.read();
    (void)ch;  // RFC: a 0x00 completeness octet
    if (after == LPD_STREAM) end_df();
    ack_ok();
    st = LPD_LINE;
    llen = 0;
    after = 0;
    return;
  }
}
