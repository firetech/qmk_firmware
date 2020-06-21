/* Copyright 2020 Joakim Tufvegren
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "wpm.h"
#include "keymap_wpm.h"
#include <stdio.h>

#ifdef SERIAL_LINK_ENABLE
#    include "serial_link/protocol/transport.h"
#    include "serial_link/system/serial_link.h"
#endif

#define WPM_BUF_SIZE 4

enum custom_keycodes {
  WPM_RESET = SAFE_RANGE,
  WPM_MAX,
  WPM_CURR
};

uint8_t max_wpm = 0;

#ifdef SERIAL_LINK_ENABLE
MASTER_TO_ALL_SLAVES_OBJECT(max_wpm, uint8_t);

static remote_object_t* remote_objects[] = {
    REMOTE_OBJECT(max_wpm),
};

void matrix_init_user(void) {
    add_remote_objects(remote_objects, sizeof(remote_objects) / sizeof(remote_object_t*));
}

static void send_max_wpm(void) {
    *begin_write_max_wpm() = max_wpm;
    end_write_max_wpm();
}
#endif

void set_max_wpm(uint8_t new_max) {
  max_wpm = new_max;
#ifdef SERIAL_LINK_ENABLE
  send_max_wpm();
#endif
}

uint8_t get_max_wpm(void) {
#ifdef SERIAL_LINK_ENABLE
    if (is_serial_link_connected()) {
        uint8_t* new_max = read_max_wpm();
        if (new_max) {
            max_wpm = *new_max;
        }
    }
#endif
  return max_wpm;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  uint8_t wpm = get_current_wpm();
  if (wpm > max_wpm) {
    max_wpm = wpm;
#ifdef SERIAL_LINK_ENABLE
    send_max_wpm();
#endif
  }
  switch (keycode) {
    case WPM_RESET:
      if (record->event.pressed) {
        set_max_wpm(0);
        set_current_wpm(0);
      }
      break;
    case WPM_MAX:
    case WPM_CURR:
      if (record->event.pressed) {
        char wpm_buf[WPM_BUF_SIZE];
        snprintf(wpm_buf, WPM_BUF_SIZE, "%u", (keycode == WPM_MAX ? get_max_wpm() : wpm));
        send_string(wpm_buf);
      }
      break;
  }
  return true;
};
