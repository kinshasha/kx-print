#include "text.h"
#include "config.h"

static uint8_t g_form = FORM_LENGTH;
static uint8_t g_line;     // 0 .. form-1
static bool g_last_cr;
static uint8_t g_utf_need;
static uint8_t g_utf[4];
static uint8_t g_utf_got;

void text_reset() {
  g_line = 0;
  g_last_cr = false;
  g_utf_need = 0;
  g_utf_got = 0;
}

void text_set_form_length(uint8_t lines) {
  if (lines >= 10 && lines <= 120) g_form = lines;
}

uint8_t text_form_length() { return g_form; }
uint8_t text_line_on_page() { return g_line; }

void text_save(TextState *s) {
  s->form = g_form;
  s->line = g_line;
  s->last_cr = g_last_cr ? 1 : 0;
  s->utf_need = g_utf_need;
  s->utf_got = g_utf_got;
  for (uint8_t i = 0; i < 4; i++) s->utf[i] = g_utf[i];
}

void text_load(const TextState *s) {
  g_form = s->form;
  g_line = s->line;
  g_last_cr = s->last_cr != 0;
  g_utf_need = s->utf_need;
  g_utf_got = s->utf_got;
  for (uint8_t i = 0; i < 4; i++) g_utf[i] = s->utf[i];
}


static bool text_feed_inner(uint8_t c, text_emit_fn emit, void *ctx);

static bool emit_one(uint8_t b, text_emit_fn emit, void *ctx) {
  if (!emit(b, ctx)) return false;
  if (b == '\n') {
    g_line++;
    if (g_line >= g_form) g_line = 0;
  }
  return true;
}

static bool emit_crlf(text_emit_fn emit, void *ctx) {
  if (!emit_one('\r', emit, ctx)) return false;
  return emit_one('\n', emit, ctx);
}

static bool tof_pad(text_emit_fn emit, void *ctx) {
  // CR+LF pairs until top-of-form. A daisywheel needs CR to bring the
  // carriage home; LF alone would walk a staircase.
  if (g_line == 0) return true;
  uint8_t n = (uint8_t)(g_form - g_line);
  while (n--) {
    if (!emit_crlf(emit, ctx)) return false;
  }
  return true;
}

static char latin1_fold(uint8_t c) {
  switch (c) {
    case 0xA0: return ' ';
    case 0xA1: return '!';
    case 0xA2: return 'c';
    case 0xA3: return 'L';
    case 0xA5: return 'Y';
    case 0xA7: return 'S';
    case 0xA9: return 'C';
    case 0xAB: return '"';
    case 0xBB: return '"';
    case 0xAE: return 'R';
    case 0xB0: return 'o';
    case 0xB1: return '+';
    case 0xB5: return 'u';
    case 0xB7: return '.';
    case 0xBF: return '?';
    default: break;
  }
  if (c >= 0xC0 && c <= 0xC6) return 'A';
  if (c == 0xC7) return 'C';
  if (c >= 0xC8 && c <= 0xCB) return 'E';
  if (c >= 0xCC && c <= 0xCF) return 'I';
  if (c == 0xD0) return 'D';
  if (c == 0xD1) return 'N';
  if (c >= 0xD2 && c <= 0xD6) return 'O';
  if (c == 0xD7) return 'x';
  if (c == 0xD8) return 'O';
  if (c >= 0xD9 && c <= 0xDC) return 'U';
  if (c == 0xDD) return 'Y';
  if (c == 0xDF) return 's';
  if (c >= 0xE0 && c <= 0xE6) return 'a';
  if (c == 0xE7) return 'c';
  if (c >= 0xE8 && c <= 0xEB) return 'e';
  if (c >= 0xEC && c <= 0xEF) return 'i';
  if (c == 0xF0) return 'd';
  if (c == 0xF1) return 'n';
  if (c >= 0xF2 && c <= 0xF6) return 'o';
  if (c == 0xF7) return '/';
  if (c == 0xF8) return 'o';
  if (c >= 0xF9 && c <= 0xFC) return 'u';
  if (c == 0xFD || c == 0xFF) return 'y';
  return '?';
}

static bool emit_mapped(const char *s, text_emit_fn emit, void *ctx) {
  while (*s) {
    uint8_t ch = (uint8_t)*s++;
    if (ch == '\n') {
      if (!emit_crlf(emit, ctx)) return false;
      g_last_cr = true;
    } else {
      if (!emit_one(ch, emit, ctx)) return false;
      g_last_cr = false;
    }
  }
  return true;
}

static bool finish_utf(text_emit_fn emit, void *ctx) {
  const uint8_t n = g_utf_got;
  g_utf_need = 0;
  g_utf_got = 0;

  if (n == 2 && g_utf[0] == 0xC2) {
    char a = latin1_fold(g_utf[1]);
    char tmp[2] = {a, 0};
    return emit_mapped(tmp, emit, ctx);
  }
  if (n == 2 && g_utf[0] == 0xC3) {
    char a = latin1_fold((uint8_t)(g_utf[1] + 0x40));
    char tmp[2] = {a, 0};
    return emit_mapped(tmp, emit, ctx);
  }
  if (n == 3 && g_utf[0] == 0xE2 && g_utf[1] == 0x80) {
    switch (g_utf[2]) {
      case 0x98: case 0x99: return emit_mapped("'", emit, ctx);
      case 0x9C: case 0x9D: return emit_mapped("\"", emit, ctx);
      case 0x93:            return emit_mapped("-", emit, ctx);
      case 0x94:            return emit_mapped("--", emit, ctx);
      case 0xA6:            return emit_mapped("...", emit, ctx);
      case 0xA2:            return emit_mapped("*", emit, ctx);
      default:              return emit_mapped("?", emit, ctx);
    }
  }
  if (n == 3 && g_utf[0] == 0xE2 && g_utf[1] == 0x82 && g_utf[2] == 0xAC) {
    return emit_mapped("EUR", emit, ctx);
  }
  return emit_mapped("?", emit, ctx);
}

bool text_feed(uint8_t c, text_emit_fn emit, void *ctx) {
  TextState snap;
  text_save(&snap);
  if (!text_feed_inner(c, emit, ctx)) {
    text_load(&snap);
    return false;
  }
  return true;
}

static bool text_feed_inner(uint8_t c, text_emit_fn emit, void *ctx) {
  if (g_utf_need) {
    if ((c & 0xC0) != 0x80) {
      g_utf_need = 0;
      g_utf_got = 0;
      if (!emit_mapped("?", emit, ctx)) return false;
      return text_feed_inner(c, emit, ctx);
    }
    g_utf[g_utf_got++] = c;
    if (g_utf_got >= g_utf_need) return finish_utf(emit, ctx);
    return true;
  }

  if (c == '\t') {
    for (uint8_t i = 0; i < TAB_WIDTH; i++) {
      if (!emit_one(' ', emit, ctx)) return false;
    }
    g_last_cr = false;
    return true;
  }

  if (c == '\r') {
    g_last_cr = true;
    return emit_one('\r', emit, ctx);
  }

  if (c == '\n') {
    if (!g_last_cr) {
      if (!emit_one('\r', emit, ctx)) return false;
    }
    g_last_cr = true;
    return emit_one('\n', emit, ctx);
  }

  if (c == 0x0C) {  // FF
    g_last_cr = false;
    return tof_pad(emit, ctx);
  }

  if (c == 0x00) {
    return true;  // ignore NULs in the data stream
  }

  if (c < 0x20 || c == 0x7F) {
    g_last_cr = false;
    return emit_one('?', emit, ctx);
  }

  if (c < 0x80) {
    g_last_cr = false;
    return emit_one(c, emit, ctx);
  }

  // UTF-8 lead
  g_utf[0] = c;
  g_utf_got = 1;
  if ((c & 0xE0) == 0xC0) g_utf_need = 2;
  else if ((c & 0xF0) == 0xE0) g_utf_need = 3;
  else if ((c & 0xF8) == 0xF0) g_utf_need = 4;  // folded to '?'
  else {
    g_utf_need = 0;
    g_utf_got = 0;
    g_last_cr = false;
    return emit_one('?', emit, ctx);
  }
  if (g_utf_need == 4) {
    // consume remaining 3 later; finish_utf will '?'
  }
  return true;
}
