#include "serial_cli.h"
#include "config.h"
#include "job.h"
#include "kx_protocol.h"
#include "net.h"
#include "text.h"
#include "ui.h"
#include "app.h"
#include <Arduino.h>
#include <string.h>

static char line[LINE_BUF];
static uint8_t llen;
static bool paste;

static void help() {
  Serial.println(F("KX-Print commands:"));
  Serial.println(F("  help              this text"));
  Serial.println(F("  status            Wi-Fi, queue, RUN/PAUSE, ACK"));
  Serial.println(F("  print [text]      print rest of line (CR+LF)"));
  Serial.println(F("  print             paste mode; end with a line of ."));
  Serial.println(F("  dry-run [on|off]  protocol log, no ACK required"));
  Serial.println(F("  cancel            cancel current job"));
}

void serial_cli_print_status() {
  char ip[16];
  net_local_ip(ip, sizeof(ip));
  Serial.println(F("---- KX-Print ----"));
  Serial.print(F("fw      ")); Serial.println(F(KX_FW_VERSION));
  Serial.print(F("host    ")); Serial.print(KX_HOSTNAME); Serial.println(F(".local"));
  Serial.print(F("queue   ")); Serial.println(KX_QUEUE);
  Serial.print(F("net     "));
  switch (net_mode()) {
    case NET_AP: Serial.println(F("AP config")); break;
    case NET_STA: Serial.println(F("STA")); break;
    case NET_STA_CONNECTING: Serial.println(F("connecting")); break;
    default: Serial.println(F("off")); break;
  }
  Serial.print(F("ssid    ")); Serial.println(net_ssid());
  Serial.print(F("ip      ")); Serial.println(ip);
  Serial.print(F("run     ")); Serial.println(ui_run() ? F("RUN") : F("PAUSE"));
  Serial.print(F("jobs    ")); Serial.print(job_queued());
  Serial.print(F("  buffered ")); Serial.println(job_buffered());
  Serial.print(F("source  ")); Serial.println(job_source());
  Serial.print(F("page    line ")); Serial.print(text_line_on_page());
  Serial.print(F(" / ")); Serial.println(text_form_length());
  Serial.print(F("dry-run ")); Serial.println(kx_is_dry_run() ? F("on") : F("off"));
  Serial.print(F("ACK pin ")); Serial.println(digitalRead(PIN_ACK) == LOW ? F("LOW (idle OK)") : F("HIGH"));
  Serial.print(F("error   ")); Serial.println(job_error() ? F("YES") : F("no"));
}

static void feed_str(const char *s) {
  while (*s) {
    job_feed((uint8_t)*s++);
  }
}

static void cmd(char *s) {
  while (*s == ' ') s++;
  if (!*s) return;

  if (paste) {
    if (s[0] == '.' && s[1] == 0) {
      job_end();
      paste = false;
      Serial.println(F("print: ended paste"));
      return;
    }
    feed_str(s);
    job_feed('\n');
    return;
  }

  char *arg = s;
  while (*arg && *arg != ' ') arg++;
  if (*arg) {
    *arg++ = 0;
    while (*arg == ' ') arg++;
  }

  if (!strcmp(s, "help") || !strcmp(s, "?")) {
    help();
  } else if (!strcmp(s, "status")) {
    serial_cli_print_status();
  } else if (!strcmp(s, "cancel")) {
    if (job_error()) job_clear_error();
    job_cancel_current();
  } else if (!strcmp(s, "dry-run")) {
    if (!strcmp(arg, "on") || !strcmp(arg, "1")) kx_set_dry_run(true);
    else if (!strcmp(arg, "off") || !strcmp(arg, "0")) kx_set_dry_run(false);
    else kx_set_dry_run(!kx_is_dry_run());
    Serial.print(F("dry-run "));
    Serial.println(kx_is_dry_run() ? F("on") : F("off"));
  } else if (!strcmp(s, "print")) {
    if (!*arg) {
      if (!job_begin("serial")) {
        Serial.println(F("print: no job slot"));
        return;
      }
      paste = true;
      Serial.println(F("paste text, end with a line of ."));
    } else {
      if (!job_begin("serial")) {
        Serial.println(F("print: no job slot"));
        return;
      }
      feed_str(arg);
      job_feed('\n');
      job_end();
    }
  } else {
    Serial.println(F("unknown, try help"));
  }
}

void serial_cli_init() {
  llen = 0;
  paste = false;
  Serial.println();
  Serial.println(F("KX-Print ready. Type help"));
}

void serial_cli_poll() {
  while (Serial.available()) {
    int ch = Serial.read();
    if (ch < 0) break;
    if (ch == '\r') continue;
    if (ch == '\n') {
      line[llen] = 0;
      llen = 0;
      cmd(line);
      continue;
    }
    if (llen < LINE_BUF - 1) line[llen++] = (char)ch;
  }
}
