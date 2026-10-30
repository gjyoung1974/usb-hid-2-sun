/*
 * Minimal HID report descriptor parser for USB2Sun. See hid_layout.h.
 */
#include <string.h>

#include "hid_layout.h"

#define MAX_USAGES   24
#define MAX_REPORTS  8

enum { PAGE_DESKTOP = 0x01, PAGE_KEYBOARD = 0x07, PAGE_BUTTON = 0x09 };
enum { USAGE_X = 0x30, USAGE_Y = 0x31, USAGE_WHEEL = 0x38 };

typedef struct {
  uint8_t  id;
  uint16_t bits;      // input bits used so far in this report
} report_size_t;

static report_size_t *report_slot(report_size_t *tab, int *n, uint8_t id) {
  for (int i = 0; i < *n; i++) if (tab[i].id == id) return &tab[i];
  if (*n >= MAX_REPORTS) return NULL;
  tab[*n].id = id;
  tab[*n].bits = 0;
  return &tab[(*n)++];
}

static uint16_t report_bytes(report_size_t *tab, int n, uint8_t id) {
  for (int i = 0; i < n; i++) if (tab[i].id == id) return (uint16_t) ((tab[i].bits + 7) / 8 + (id ? 1 : 0));
  return 0;
}

bool hid_layout_parse(const uint8_t *d, uint16_t len, hid_layout_t *out) {
  memset(out, 0, sizeof(*out));

  // global state
  uint16_t usage_page = 0;
  int32_t  logical_min = 0;
  uint8_t  report_size = 0, report_count = 0, report_id = 0;
  // local state
  uint32_t usages[MAX_USAGES];   // (page << 16) | usage
  int      nusages = 0;
  uint32_t usage_min = 0, usage_max = 0;
  bool     have_range = false;

  report_size_t sizes[MAX_REPORTS];
  int nsizes = 0;
  uint8_t buttons_report_id = 0;
  bool have_buttons = false;

  uint16_t i = 0;
  while (i < len) {
    uint8_t prefix = d[i++];
    if (prefix == 0xFE) {                 // long item: skip
      if (i + 2 > len) return false;
      i = (uint16_t) (i + 2 + d[i]);
      continue;
    }
    uint8_t size = prefix & 3;
    if (size == 3) size = 4;
    uint8_t type = (prefix >> 2) & 3;
    uint8_t tag  = prefix >> 4;
    if (i + size > len) return false;

    uint32_t val = 0;
    for (uint8_t k = 0; k < size; k++) val |= (uint32_t) d[i + k] << (8 * k);
    int32_t sval = (int32_t) val;
    if (size == 1) sval = (int8_t) val;
    else if (size == 2) sval = (int16_t) val;
    i = (uint16_t) (i + size);

    if (type == 1) {                      // global
      switch (tag) {
        case 0: usage_page = (uint16_t) val; break;
        case 1: logical_min = sval; break;
        case 7: report_size = (uint8_t) val; break;
        case 8: report_id = (uint8_t) val; break;
        case 9: report_count = (uint8_t) val; break;
        default: break;                   // logical max, physical, unit, push/pop: not needed
      }
    } else if (type == 2) {               // local
      uint32_t full = (size == 4) ? val : (((uint32_t) usage_page << 16) | val);
      switch (tag) {
        case 0: if (nusages < MAX_USAGES) usages[nusages++] = full; break;
        case 1: usage_min = full; break;
        case 2: usage_max = full; have_range = true; break;
        default: break;
      }
    } else if (type == 0) {               // main
      if (tag == 8) {                     // Input
        report_size_t *rs = report_slot(sizes, &nsizes, report_id);
        if (!rs) return false;
        bool constant = val & 1;
        bool is_array = !(val & 2);
        uint16_t offset = rs->bits;

        if (have_range) {
          for (uint32_t u = usage_min; u <= usage_max && nusages < MAX_USAGES; u++) usages[nusages++] = u;
        }

        if (!constant && is_array) {
          // Key code array (boot keyboards: 6 x 8 bits, usage page 7)
          if (usage_page == PAGE_KEYBOARD || (nusages && (usages[0] >> 16) == PAGE_KEYBOARD)) {
            if (!out->keys_count) {
              out->keys_bit_offset = offset;
              out->keys_count = report_count;
              out->keys_bit_size = report_size;
              out->have_keyboard = true;
              out->keyboard_report_id = report_id;
            }
          }
        } else if (!constant) {
          for (uint8_t f = 0; f < report_count; f++) {
            uint32_t u = nusages ? usages[f < nusages ? f : nusages - 1] : 0;
            uint16_t page = (uint16_t) (u >> 16), usage = (uint16_t) u;
            uint16_t off = (uint16_t) (offset + f * report_size);
            hid_field_t *fld = NULL;
            if (page == PAGE_DESKTOP && usage == USAGE_X) fld = &out->x;
            else if (page == PAGE_DESKTOP && usage == USAGE_Y) fld = &out->y;
            else if (page == PAGE_DESKTOP && usage == USAGE_WHEEL) fld = &out->wheel;
            else if (page == PAGE_BUTTON) {
              if (!have_buttons) {
                have_buttons = true;
                buttons_report_id = report_id;
                out->buttons_bit_offset = off;
                out->buttons_count = 0;
              }
              if (report_id == buttons_report_id && out->buttons_count < 8) out->buttons_count++;
            } else if (page == PAGE_KEYBOARD && usage == 0xE0 && !out->modifiers.present) {
              out->modifiers.present = true;
              out->modifiers.bit_offset = off;
              out->modifiers.bit_size = 1;
              out->have_keyboard = true;
              out->keyboard_report_id = report_id;
            }
            if (fld && !fld->present) {
              fld->present = true;
              fld->bit_offset = off;
              fld->bit_size = report_size;
              fld->is_signed = logical_min < 0;
            }
          }
          if (out->x.present && out->y.present && !out->have_mouse) {
            out->have_mouse = true;
            out->mouse_report_id = report_id;
          }
        }
        rs->bits = (uint16_t) (rs->bits + report_size * report_count);
      }
      // Output/Feature/Collection/End: nothing to track. Every main item clears local state.
      nusages = 0;
      have_range = false;
    }
  }

  if (out->have_mouse) {
    out->mouse_report_bytes = report_bytes(sizes, nsizes, out->mouse_report_id);
    if (buttons_report_id != out->mouse_report_id) out->buttons_count = 0;
  }
  if (out->have_keyboard) {
    out->keyboard_report_bytes = report_bytes(sizes, nsizes, out->keyboard_report_id);
  }
  return out->have_mouse || out->have_keyboard;
}

static uint32_t extract_bits(const uint8_t *r, uint16_t off, uint8_t size) {
  uint32_t v = 0;
  for (uint8_t b = 0; b < size && b < 32; b++) {
    uint16_t bit = (uint16_t) (off + b);
    if (r[bit >> 3] & (1u << (bit & 7))) v |= 1u << b;
  }
  return v;
}

static int32_t extract_field(const uint8_t *r, const hid_field_t *f) {
  uint32_t v = extract_bits(r, f->bit_offset, f->bit_size);
  if (f->is_signed && f->bit_size < 32 && (v & (1u << (f->bit_size - 1)))) {
    v |= ~((1u << f->bit_size) - 1);
  }
  return (int32_t) v;
}

static int8_t clamp8(int32_t v) {
  if (v > 127) return 127;
  if (v < -127) return -127;
  return (int8_t) v;
}

bool hid_layout_decode_mouse(const hid_layout_t *l, const uint8_t *report, uint16_t len, hid_mouse_report_t *out) {
  if (!l->have_mouse || len != l->mouse_report_bytes) return false;
  if (l->mouse_report_id) {
    if (report[0] != l->mouse_report_id) return false;
    report++;
    len--;
  }
  uint8_t buttons = 0;
  for (uint8_t b = 0; b < l->buttons_count; b++) {
    if (extract_bits(report, (uint16_t) (l->buttons_bit_offset + b), 1)) buttons |= (uint8_t) (1u << b);
  }
  memset(out, 0, sizeof(*out));
  out->buttons = buttons;
  out->x = clamp8(extract_field(report, &l->x));
  out->y = clamp8(extract_field(report, &l->y));
  if (l->wheel.present) out->wheel = clamp8(extract_field(report, &l->wheel));
  return true;
}

bool hid_layout_decode_keyboard(const hid_layout_t *l, const uint8_t *report, uint16_t len, hid_keyboard_report_t *out) {
  if (!l->have_keyboard || !l->modifiers.present || l->keys_bit_size != 8) return false;
  if (len != l->keyboard_report_bytes) return false;
  if (l->keyboard_report_id) {
    if (report[0] != l->keyboard_report_id) return false;
    report++;
    len--;
  }
  memset(out, 0, sizeof(*out));
  out->modifier = (uint8_t) extract_bits(report, l->modifiers.bit_offset, 8);
  for (uint8_t k = 0; k < l->keys_count && k < 6; k++) {
    out->keycode[k] = (uint8_t) extract_bits(report, (uint16_t) (l->keys_bit_offset + 8 * k), 8);
  }
  return true;
}
