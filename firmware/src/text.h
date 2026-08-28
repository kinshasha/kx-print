#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef bool (*text_emit_fn)(uint8_t b, void *ctx);

struct TextState {
  uint8_t form;
  uint8_t line;
  uint8_t last_cr;
  uint8_t utf_need;
  uint8_t utf_got;
  uint8_t utf[4];
};

void text_reset();
void text_set_form_length(uint8_t lines);
uint8_t text_form_length();
uint8_t text_line_on_page();
void text_save(TextState *s);
void text_load(const TextState *s);

// Stream one incoming byte. May emit 0..N output bytes via emit().
// Returns false if emit() fails — state is unchanged (caller may retry).
bool text_feed(uint8_t c, text_emit_fn emit, void *ctx);
