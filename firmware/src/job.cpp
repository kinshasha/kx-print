#include "job.h"
#include "config.h"
#include "text.h"
#include "kx_protocol.h"
#include "ui.h"
#include "app.h"
#include <Arduino.h>
#include <string.h>

struct Job {
  uint16_t bytes;   // remaining in the ring
  bool open;        // still receiving
  bool discard;     // cancel: drop incoming / queued bytes
  char source[12];
};

static uint8_t ring[RING_SIZE];
static uint16_t r_head, r_count;
static Job jobs[MAX_JOBS];
static uint8_t j_head, j_n;
static bool g_error;
static bool g_probed;
static bool g_printing;
static uint8_t g_burst = PRINT_BURST_DEFAULT;

uint8_t job_burst() { return g_burst; }

void job_set_burst(uint8_t n) {
  if (n < 1) n = 1;
  if (n > PRINT_BURST_MAX) n = PRINT_BURST_MAX;
  g_burst = n;
}

static uint16_t space() {
  return (uint16_t)(RING_SIZE - r_count);
}

static bool ring_push(uint8_t b) {
  if (r_count >= RING_SIZE) return false;
  uint16_t i = (uint16_t)((r_head + r_count) % RING_SIZE);
  ring[i] = b;
  r_count++;
  return true;
}

static bool ring_pop(uint8_t *b) {
  if (!r_count) return false;
  *b = ring[r_head];
  r_head = (uint16_t)((r_head + 1) % RING_SIZE);
  r_count--;
  return true;
}

static Job *head_job() {
  return j_n ? &jobs[j_head] : nullptr;
}

static Job *tail_job() {
  if (!j_n) return nullptr;
  return &jobs[(j_head + j_n - 1) % MAX_JOBS];
}

static void pop_job_if_done() {
  Job *j = head_job();
  if (!j) return;
  if (!j->open && j->bytes == 0) {
    j_head = (uint8_t)((j_head + 1) % MAX_JOBS);
    j_n--;
    g_probed = false;
  }
}

static uint8_t g_tmp[140];
static uint8_t g_tmpn;

static bool collect_byte(uint8_t b, void *ctx) {
  (void)ctx;
  if (g_tmpn >= sizeof(g_tmp)) return false;
  g_tmp[g_tmpn++] = b;
  return true;
}

void job_init() {
  r_head = r_count = 0;
  j_head = j_n = 0;
  g_error = false;
  g_probed = false;
  g_printing = false;
  text_reset();
  memset(jobs, 0, sizeof(jobs));
}

bool job_begin(const char *source) {
  if (j_n >= MAX_JOBS) return false;
  uint8_t i = (uint8_t)((j_head + j_n) % MAX_JOBS);
  jobs[i].bytes = 0;
  jobs[i].open = true;
  jobs[i].discard = false;
  strncpy(jobs[i].source, source ? source : "?", sizeof(jobs[i].source) - 1);
  jobs[i].source[sizeof(jobs[i].source) - 1] = 0;
  j_n++;
  text_reset();
  return true;
}

bool job_has_space() {
  Job *j = tail_job();
  if (j && j->discard) return true;
  // One input byte can expand to a full form-feed (CR+LF * 65).
  return space() >= 140;
}

bool job_feed(uint8_t c) {
  Job *j = tail_job();
  if (!j || !j->open) return false;
  if (j->discard) return true;
  TextState snap;
  text_save(&snap);
  g_tmpn = 0;
  if (!text_feed(c, collect_byte, nullptr)) {
    text_load(&snap);
    return false;
  }
  if (space() < g_tmpn) {
    text_load(&snap);
    return false;
  }
  for (uint8_t i = 0; i < g_tmpn; i++) {
    ring_push(g_tmp[i]);
    j->bytes++;
  }
  return true;
}

void job_end() {
  Job *j = tail_job();
  if (!j || !j->open) return;
  j->open = false;
  pop_job_if_done();
}

void job_cancel_current() {
  kx_request_abort();
  Job *j = head_job();
  if (!j) {
    kx_safe_idle();
    return;
  }
  j->discard = true;
  while (j->bytes && r_count) {
    uint8_t dump;
    ring_pop(&dump);
    j->bytes--;
  }
  if (!j->open) {
    pop_job_if_done();
  }
  kx_safe_idle();
  ui_show_cancelled();
  Serial.println(F("job: cancelled"));
}

void job_clear_queue() {
  kx_request_abort();
  for (uint8_t n = 0; n < j_n; n++) {
    jobs[(j_head + n) % MAX_JOBS].discard = true;
    jobs[(j_head + n) % MAX_JOBS].open = false;
    jobs[(j_head + n) % MAX_JOBS].bytes = 0;
  }
  r_head = r_count = 0;
  j_head = j_n = 0;
  g_printing = false;
  kx_safe_idle();
  ui_show_cancelled();
  Serial.println(F("job: queue cleared"));
}

bool job_run_hw() { return ui_run(); }
bool job_paused_hw() { return !ui_run(); }

void job_print_poll() {
  if (g_error) return;
  if (!ui_run()) {
    g_printing = false;
    return;
  }

  Job *j = head_job();
  if (!j) {
    g_printing = false;
    return;
  }

  if (j->discard) {
    while (j->bytes && r_count) {
      uint8_t dump;
      ring_pop(&dump);
      j->bytes--;
    }
    pop_job_if_done();
    return;
  }

  if (!j->bytes) {
    g_printing = false;
    pop_job_if_done();
    return;
  }

  if (!g_probed && !kx_is_dry_run()) {
    if (!kx_ack_idle_low()) {
      Serial.println(F("WARN: ACK is not idle LOW. Check cable, 10k pulldown,"));
      Serial.println(F("      printer mode CODE+E (LCD: ON LINE). Not a Mac cable?"));
    } else {
      Serial.println(F("ACK idle LOW. KX-R60-class, good to send."));
    }
    g_probed = true;
  }

  g_printing = true;
  kx_clear_abort();
  uint8_t n = g_burst;
  if (n < 1) n = 1;
  while (n--) {
    j = head_job();
    if (!j || j->discard || !j->bytes) break;
    uint8_t b;
    if (!ring_pop(&b)) break;
    if (j->bytes) j->bytes--;
    if (!kx_send_byte(b)) {
      g_printing = false;
      if (kx_last_timeout()) {
        g_error = true;
        Serial.println(F("ERROR: ACK timeout, safe idle. CANCEL to recover."));
        job_cancel_current();
      }
      return;
    }
    pop_job_if_done();
    if (kx_abort_requested() || !ui_run()) break;
  }
}

bool job_printing() { return g_printing; }
bool job_any() { return j_n > 0; }
uint8_t job_queued() { return j_n; }
uint16_t job_buffered() { return r_count; }
bool job_error() { return g_error; }

void job_clear_error() {
  g_error = false;
  kx_clear_timeout();
  kx_safe_idle();
}

const char *job_source() {
  Job *j = head_job();
  return j ? j->source : "-";
}
