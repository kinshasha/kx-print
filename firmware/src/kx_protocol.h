#pragma once

#include <stdint.h>
#include <stdbool.h>

void kx_protocol_init();          // safe idle FIRST. Call before anything else
void kx_safe_idle();
bool kx_send_byte(uint8_t b);     // false on ACK timeout or abort
bool kx_ack_idle_low();           // probe: KX-R60-class ACK idles LOW
bool kx_last_timeout();
void kx_clear_timeout();
void kx_set_dry_run(bool on);
bool kx_is_dry_run();
uint16_t kx_data_setup_us();     // DATA setup before /STB, microseconds
void kx_set_data_setup_us(uint16_t us);  // clamped 5..500, not persisted
