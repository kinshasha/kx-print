#pragma once

#include <stdint.h>
#include <stdbool.h>

// Cooperative yield from ACK waits so TCP/mDNS/buttons keep moving.
void kx_yield();

bool kx_abort_requested();
void kx_request_abort();
void kx_clear_abort();
