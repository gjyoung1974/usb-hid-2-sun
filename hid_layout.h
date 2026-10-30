/*
 * Minimal HID report descriptor parser for USB2Sun.
 *
 * TinyUSB puts keyboards and mice into boot protocol, which gives fixed report
 * layouts. Some devices acknowledge that request and then keep sending their
 * report-protocol reports anyway (report ID first, vendor bytes, 16-bit axes).
 * This parser extracts, from the interface's report descriptor, where the
 * fields we care about live, so such reports can still be decoded.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "class/hid/hid.h"

typedef struct {
  bool     present;
  bool     is_signed;
  uint16_t bit_offset;   // within the report, not counting the report ID byte
  uint8_t  bit_size;
} hid_field_t;

typedef struct {
  // mouse: the input report that carries X and Y
  bool        have_mouse;
  uint8_t     mouse_report_id;     // 0 when the descriptor uses no report IDs
  uint16_t    mouse_report_bytes;  // report size on the wire, including the ID byte if any
  hid_field_t x, y, wheel;
  uint16_t    buttons_bit_offset;
  uint8_t     buttons_count;

  // keyboard: the input report with the modifier bits and key array
  bool        have_keyboard;
  uint8_t     keyboard_report_id;
  uint16_t    keyboard_report_bytes;
  hid_field_t modifiers;           // 8 one-bit fields starting here (usage E0..E7)
  uint16_t    keys_bit_offset;     // array of key codes
  uint8_t     keys_count;
  uint8_t     keys_bit_size;
} hid_layout_t;

// Parse a report descriptor. Returns true if a mouse or keyboard report was found.
bool hid_layout_parse(const uint8_t *desc, uint16_t len, hid_layout_t *out);

// Decode a report with the mouse layout. Returns false (and leaves *out alone)
// when the report does not match the layout, in which case the caller should
// treat it as a boot-protocol report.
bool hid_layout_decode_mouse(const hid_layout_t *l, const uint8_t *report, uint16_t len, hid_mouse_report_t *out);

// Same for keyboard reports, producing the boot-protocol structure.
bool hid_layout_decode_keyboard(const hid_layout_t *l, const uint8_t *report, uint16_t len, hid_keyboard_report_t *out);
