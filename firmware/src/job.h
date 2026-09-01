#pragma once

#include <stdint.h>
#include <stdbool.h>

void job_init();
bool job_begin(const char *source);
bool job_feed(uint8_t c);          // through text converter into the ring
void job_end();
void job_cancel_current();         // drop the job being printed / received
void job_clear_queue();            // drop everything
void job_print_poll();             // feed typewriter when RUN
bool job_has_space();
bool job_printing();
bool job_any();
uint8_t job_queued();
uint16_t job_buffered();
const char *job_source();
bool job_error();
void job_clear_error();
bool job_paused_hw();              // RUN switch open
bool job_run_hw();
uint8_t job_burst();              // chars per loop tick, 1 = classic
void job_set_burst(uint8_t n);    // 1..PRINT_BURST_MAX
