#pragma once

// Pins, timeouts, hostnames. Timing values are safety-critical. See docs/protocol.md.

#ifndef KX_FW_VERSION
#define KX_FW_VERSION "1.0.2"
#endif

#define PIN_ACK      4   // D4  /ACK from typewriter. INPUT, no pull-up.
#define PIN_DATA     5   // D5  DATA to typewriter. Idle HIGH.
#define PIN_STB      6   // D6  /STB to typewriter. Idle HIGH, pulse LOW.
#define PIN_ONLINE   7   // D7  /ONLINE to typewriter. Idle HIGH, LOW during a byte.
#define PIN_RUN      8   // D8  RUN/PAUSE toggle. INPUT_PULLUP, closed to GND = RUN.
#define PIN_CANCEL   9   // D9  CANCEL momentary. INPUT_PULLUP, active low.

#define ACK_TIMEOUT_MS     1500
#define DATA_SETUP_US        50
#define DEBOUNCE_MS          25
#define CANCEL_HOLD_MS     3000
#define BOOT_HOLD_MS       2000
#define CANCELLED_LED_MS   1200

#define FORM_LENGTH          66
#define TAB_WIDTH             4

#define PORT_RAW           9100
#define PORT_LPD            515
#define PORT_HTTP            80

#define KX_HOSTNAME        "kxr540"
#define KX_QUEUE           "kxr540"
#define KX_AP_SSID         "KX-Print-Setup"
#define KX_DEVICE_NAME     "KX-Print"

// 32 kB SRAM: stream, do not spool whole jobs.
#define RING_SIZE          2048
#define MAX_JOBS              4
#define LINE_BUF            160
#define HTTP_BUF            384

#define SERIAL_BAUD      115200

#define EEPROM_MAGIC     0x4B585052u  // 'KXPR'
#define EEPROM_ADDR           0

#ifndef KX_DRY_RUN_DEFAULT
#define KX_DRY_RUN_DEFAULT    0
#endif
