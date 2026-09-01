#include "net.h"
#include "config.h"
#include "job.h"
#include "kx_protocol.h"
#include "text.h"
#include "ui.h"
#include <Arduino.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#if __has_include(<ArduinoMDNS.h>)
#include <ArduinoMDNS.h>
#else
#include <MDNS.h>
#endif
#include <EEPROM.h>
#include <string.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

struct Creds {
  uint32_t magic;
  uint8_t flags;       // bit0 = valid
  char ssid[33];
  char pass[65];
  uint8_t crc;
};

static Creds g_creds;
static NetMode g_mode = NET_OFF;
static bool g_mdns_up;
static uint32_t g_sta_last_try;
static WiFiServer http(PORT_HTTP);
static WiFiUDP g_udp;
static MDNS g_mdns(g_udp);
static char g_ip[16];

static uint8_t crc8(const uint8_t *p, size_t n) {
  uint8_t c = 0;
  while (n--) c = (uint8_t)(c ^ *p++);
  return c;
}

static uint8_t creds_crc(const Creds *c) {
  return crc8((const uint8_t *)c, sizeof(Creds) - 1);
}

static void creds_load() {
  memset(&g_creds, 0, sizeof(g_creds));
  EEPROM.get(EEPROM_ADDR, g_creds);
  if (g_creds.magic != EEPROM_MAGIC || g_creds.crc != creds_crc(&g_creds)) {
    memset(&g_creds, 0, sizeof(g_creds));
    strncpy(g_creds.ssid, WIFI_SSID, 32);
    strncpy(g_creds.pass, WIFI_PASS, 64);
    if (g_creds.ssid[0]) g_creds.flags = 1;
  }
}

static void creds_save() {
  g_creds.magic = EEPROM_MAGIC;
  g_creds.flags = 1;
  g_creds.crc = creds_crc(&g_creds);
  EEPROM.put(EEPROM_ADDR, g_creds);
}

void net_wipe_creds() {
  memset(&g_creds, 0, sizeof(g_creds));
  g_creds.magic = EEPROM_MAGIC;
  g_creds.crc = creds_crc(&g_creds);
  EEPROM.put(EEPROM_ADDR, g_creds);
  Serial.println(F("net: EEPROM Wi-Fi creds wiped"));
}

bool net_has_creds() { return g_creds.ssid[0] != 0; }
NetMode net_mode() { return g_mode; }
bool net_sta_up() { return g_mode == NET_STA && WiFi.status() == WL_CONNECTED; }
const char *net_ssid() { return g_creds.ssid[0] ? g_creds.ssid : "(none)"; }

void net_local_ip(char *buf, uint8_t len) {
  strncpy(buf, g_ip[0] ? g_ip : "-", len - 1);
  buf[len - 1] = 0;
}

static void ip_to_buf() {
  IPAddress ip = WiFi.localIP();
  snprintf(g_ip, sizeof(g_ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

static void mdns_start() {
  if (g_mdns_up) return;
  g_mdns.begin(WiFi.localIP(), KX_HOSTNAME);
  g_mdns.addServiceRecord("KX-Print._printer", PORT_LPD, MDNSServiceTCP);
  g_mdns_up = true;
  Serial.print(F("mDNS: "));
  Serial.print(KX_HOSTNAME);
  Serial.println(F(".local  _printer._tcp"));
}

void net_mdns_run() {
  if (g_mdns_up) g_mdns.run();
}

static int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static void url_decode(char *s) {
  char *w = s;
  for (char *r = s; *r; r++) {
    if (*r == '+') {
      *w++ = ' ';
    } else if (*r == '%' && r[1] && r[2]) {
      int h = hexval(r[1]), l = hexval(r[2]);
      if (h >= 0 && l >= 0) {
        *w++ = (char)((h << 4) | l);
        r += 2;
      } else {
        *w++ = *r;
      }
    } else {
      *w++ = *r;
    }
  }
  *w = 0;
}

static bool form_get(char *body, const char *key, char *out, uint8_t outlen) {
  size_t klen = strlen(key);
  char *p = body;
  while (p && *p) {
    char *amp = strchr(p, '&');
    if (amp) *amp = 0;
    char *eq = strchr(p, '=');
    if (eq) {
      *eq = 0;
      if (strcmp(p, key) == 0) {
        url_decode(eq + 1);
        strncpy(out, eq + 1, outlen - 1);
        out[outlen - 1] = 0;
        if (amp) *amp = '&';
        *eq = '=';
        return true;
      }
      *eq = '=';
    }
    if (!amp) break;
    *amp = '&';
    p = amp + 1;
  }
  out[0] = 0;
  return false;
}


static bool req_path_is(const char *req, const char *path) {
  const char *p = req;
  while (*p && *p != ' ') p++;
  while (*p == ' ') p++;
  size_t n = strlen(path);
  if (strncmp(p, path, n) != 0) return false;
  char end = p[n];
  return end == ' ' || end == '?' || end == '\r' || end == '\n';
}

static void http_headers(WiFiClient &c, const char *ctype, int code = 200) {
  if (code == 200) c.println(F("HTTP/1.1 200 OK"));
  else if (code == 303) c.println(F("HTTP/1.1 303 See Other"));
  else c.println(F("HTTP/1.1 204 No Content"));
  c.print(F("Content-Type: "));
  c.println(ctype);
  c.println(F("Cache-Control: no-store"));
  c.println(F("Connection: close"));
}

static void json_str(WiFiClient &c, const char *s) {
  c.print('"');
  for (; s && *s; s++) {
    char ch = *s;
    if (ch == '"' || ch == '\\') c.print('\\');
    if (ch == '\r' || ch == '\n') continue;
    c.print(ch);
  }
  c.print('"');
}

static const char *net_mode_str() {
  switch (net_mode()) {
    case NET_AP: return "AP";
    case NET_STA: return "STA";
    case NET_STA_CONNECTING: return "connecting";
    default: return "off";
  }
}

static void http_status_json(WiFiClient &c) {
  char ip[16];
  net_local_ip(ip, sizeof(ip));
  bool ack_low = digitalRead(PIN_ACK) == LOW;
  http_headers(c, "application/json");
  c.println();
  c.print(F("{\"fw\":\""));
  c.print(F(KX_FW_VERSION));
  c.print(F("\",\"host\":\""));
  c.print(KX_HOSTNAME);
  c.print(F(".local\",\"queue\":\""));
  c.print(KX_QUEUE);
  c.print(F("\",\"net\":\""));
  c.print(net_mode_str());
  c.print(F("\",\"ssid\":"));
  json_str(c, net_ssid());
  c.print(F(",\"ip\":"));
  json_str(c, ip);
  c.print(F(",\"run\":\""));
  c.print(ui_run() ? "RUN" : "PAUSE");
  c.print(F("\",\"jobs\":"));
  c.print(job_queued());
  c.print(F(",\"buffered\":"));
  c.print(job_buffered());
  c.print(F(",\"source\":"));
  json_str(c, job_source());
  c.print(F(",\"pageLine\":"));
  c.print(text_line_on_page());
  c.print(F(",\"formLength\":"));
  c.print(text_form_length());
  c.print(F(",\"dryRun\":"));
  c.print(kx_is_dry_run() ? "true" : "false");
  c.print(F(",\"ack\":\""));
  c.print(ack_low ? "LOW" : "HIGH");
  c.print(F("\",\"ackIdleOk\":"));
  c.print(ack_low ? "true" : "false");
  c.print(F(",\"error\":"));
  c.print(job_error() ? "true" : "false");
  c.print(F(",\"setupUs\":"));
  c.print(kx_data_setup_us());
  c.println('}');
}

static void http_status_html(WiFiClient &c) {
  char ip[16];
  net_local_ip(ip, sizeof(ip));
  bool ack_low = digitalRead(PIN_ACK) == LOW;
  http_headers(c, "text/html");
  c.println();
  c.println(F("<!DOCTYPE html><html><head><meta name=viewport content='width=device-width'>"));
  c.println(F("<meta http-equiv=refresh content=3>"));
  c.println(F("<title>KX-Print status</title></head><body>"));
  c.println(F("<h1>KX-Print</h1><pre>"));
  c.print(F("fw      ")); c.println(F(KX_FW_VERSION));
  c.print(F("host    ")); c.print(KX_HOSTNAME); c.println(F(".local"));
  c.print(F("queue   ")); c.println(KX_QUEUE);
  c.print(F("net     ")); c.println(net_mode_str());
  c.print(F("ssid    ")); c.println(net_ssid());
  c.print(F("ip      ")); c.println(ip);
  c.print(F("run     ")); c.println(ui_run() ? F("RUN") : F("PAUSE"));
  c.print(F("jobs    ")); c.print(job_queued());
  c.print(F("  buffered ")); c.println(job_buffered());
  c.print(F("source  ")); c.println(job_source());
  c.print(F("page    line ")); c.print(text_line_on_page());
  c.print(F(" / ")); c.println(text_form_length());
  c.print(F("dry-run ")); c.println(kx_is_dry_run() ? F("on") : F("off"));
  c.print(F("setup-us ")); c.println(kx_data_setup_us());
  c.print(F("ACK pin ")); c.println(ack_low ? F("LOW (idle OK)") : F("HIGH"));
  c.print(F("error   ")); c.println(job_error() ? F("YES") : F("no"));
  c.println(F("</pre>"));
  c.println(F("<form method=POST action=/cancel><input type=submit value=Cancel></form>"));
  c.println(F("<p><a href=/status.json>json</a> · <a href=/>setup</a></p>"));
  c.println(F("</body></html>"));
}

static void http_cancel(WiFiClient &c) {
  if (job_error()) job_clear_error();
  job_cancel_current();
  c.println(F("HTTP/1.1 303 See Other"));
  c.println(F("Location: /status"));
  c.println(F("Cache-Control: no-store"));
  c.println(F("Connection: close"));
  c.println();
}

static void http_page(WiFiClient &c, bool saved) {
  c.println(F("HTTP/1.1 200 OK"));
  c.println(F("Content-Type: text/html"));
  c.println(F("Connection: close"));
  c.println();
  c.println(F("<!DOCTYPE html><html><head><meta name=viewport content='width=device-width'>"));
  c.println(F("<title>KX-Print</title></head><body>"));
  c.println(F("<h1>KX-Print setup</h1>"));
  if (saved) {
    c.println(F("<p>Saved. Rebooting into your Wi-Fi&hellip;</p>"));
  } else {
    c.println(F("<p>Join this page, type home Wi-Fi, hit Save.</p>"));
    c.println(F("<form method=POST action=/save>"));
    c.println(F("SSID<br><input name=ssid maxlength=32><br><br>"));
    c.println(F("Password<br><input type=password name=pass maxlength=64><br><br>"));
    c.println(F("<input type=submit value=Save>"));
    c.println(F("</form>"));
    c.println(F("<p><a href=/status>status</a></p>"));
  }
  c.println(F("</body></html>"));
}

static void http_poll() {
  WiFiClient c = http.available();
  if (!c) return;

  char req[HTTP_BUF];
  uint16_t n = 0;
  uint32_t t0 = millis();
  bool got_blank = false;
  int content_len = 0;
  bool is_post = false;
  bool is_save = false;

  while (c.connected() && (uint32_t)(millis() - t0) < 2000) {
    while (c.available()) {
      char ch = (char)c.read();
      if (n < sizeof(req) - 1) req[n++] = ch;
      if (n >= 4 && req[n - 4] == '\r' && req[n - 3] == '\n' &&
          req[n - 2] == '\r' && req[n - 1] == '\n') {
        got_blank = true;
        goto headers_done;
      }
    }
    delay(1);
  }
headers_done:
  req[n] = 0;
  is_post = (strncmp(req, "POST", 4) == 0);
  is_save = (strstr(req, " /save") != nullptr);
  char *cl = strstr(req, "Content-Length:");
  if (!cl) cl = strstr(req, "content-length:");
  if (cl) content_len = atoi(cl + 15);
  if (content_len < 0) content_len = 0;
  if (content_len > (int)sizeof(req) - 1) content_len = sizeof(req) - 1;

  if (req_path_is(req, "/status.json")) {
    http_status_json(c);
    c.stop();
    return;
  }
  if (req_path_is(req, "/status")) {
    http_status_html(c);
    c.stop();
    return;
  }
  if (is_post && req_path_is(req, "/cancel")) {
    int drain = content_len;
    t0 = millis();
    while (drain > 0 && (uint32_t)(millis() - t0) < 2000) {
      if (c.available()) {
        c.read();
        drain--;
      }
    }
    http_cancel(c);
    c.stop();
    return;
  }

  if (is_post && is_save && content_len > 0) {
    n = 0;
    t0 = millis();
    while (n < (uint16_t)content_len && (uint32_t)(millis() - t0) < 2000) {
      if (c.available()) req[n++] = (char)c.read();
    }
    req[n] = 0;
    char ssid[33], pass[65];
    form_get(req, "ssid", ssid, sizeof(ssid));
    form_get(req, "pass", pass, sizeof(pass));
    if (ssid[0]) {
      memset(&g_creds, 0, sizeof(g_creds));
      strncpy(g_creds.ssid, ssid, 32);
      strncpy(g_creds.pass, pass, 64);
      creds_save();
      http_page(c, true);
      c.stop();
      delay(250);
      NVIC_SystemReset();
    }
  }

  (void)got_blank;
  http_page(c, false);
  c.stop();
}

static void start_ap() {
  Serial.print(F("AP: "));
  Serial.println(F(KX_AP_SSID));
  uint8_t st = WiFi.beginAP(KX_AP_SSID);
  if (st == 0) {
    Serial.println(F("AP start failed"));
  } else {
    Serial.print(F("AP status "));
    Serial.println(st);
  }
  g_mode = NET_AP;
  ip_to_buf();
  http.begin();
  Serial.print(F("Config page: http://"));
  Serial.println(WiFi.localIP());
}

static bool start_sta() {
  Serial.print(F("STA: joining "));
  Serial.println(g_creds.ssid);
  g_mode = NET_STA_CONNECTING;
  WiFi.setHostname(KX_HOSTNAME);
  uint8_t st;
  if (g_creds.pass[0]) st = WiFi.begin(g_creds.ssid, g_creds.pass);
  else st = WiFi.begin(g_creds.ssid);
  g_sta_last_try = millis();
  if (st != WL_CONNECTED) {
    Serial.println(F("STA failed"));
    return false;
  }
  ip_to_buf();
  if (!g_ip[0] || strcmp(g_ip, "0.0.0.0") == 0) {
    Serial.println(F("STA associated, waiting for DHCP"));
    g_mode = NET_STA_CONNECTING;
    return true;
  }
  g_mode = NET_STA;
  Serial.print(F("STA IP "));
  Serial.println(g_ip);
  http.begin();
  mdns_start();
  return true;
}

void net_begin(bool force_ap) {
  // virtualEEPROM: no begin(); EEPROM.begin() is an iterator.
  creds_load();

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("WiFi module missing, serial print still works"));
    g_mode = NET_OFF;
    return;
  }

  if (force_ap || !net_has_creds()) {
    if (force_ap) net_wipe_creds();
    start_ap();
    return;
  }

  if (!start_sta()) start_ap();
}

void net_poll() {
  http_poll();
  net_mdns_run();

  if (g_mode == NET_STA_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      ip_to_buf();
      if (g_ip[0] && strcmp(g_ip, "0.0.0.0") != 0) {
        g_mode = NET_STA;
        Serial.print(F("STA IP "));
        Serial.println(g_ip);
        http.begin();
        mdns_start();
      }
    } else if ((uint32_t)(millis() - g_sta_last_try) > 15000) {
      Serial.println(F("STA retry"));
      start_sta();
    }
    return;
  }

  if (g_mode == NET_STA && WiFi.status() != WL_CONNECTED) {
    Serial.println(F("STA dropped"));
    g_mdns_up = false;
    g_mode = NET_STA_CONNECTING;
    g_sta_last_try = millis();
  }
}
