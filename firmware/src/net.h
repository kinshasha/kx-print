#pragma once

#include <stdbool.h>
#include <stdint.h>

enum NetMode { NET_OFF, NET_AP, NET_STA_CONNECTING, NET_STA };

void net_begin(bool force_ap);
void net_poll();
NetMode net_mode();
bool net_sta_up();
void net_mdns_run();
void net_wipe_creds();             // EEPROM clear
bool net_has_creds();
void net_local_ip(char *buf, uint8_t len);
const char *net_ssid();
