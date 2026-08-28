#pragma once

#include <stdint.h>
#include <stdbool.h>

void lpd_begin();
void lpd_poll();
void lpd_stop_client();
bool lpd_busy();
uint8_t lpd_queue_jobs();
