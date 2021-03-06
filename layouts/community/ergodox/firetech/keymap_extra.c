/* Copyright 2021 Joakim Tufvegren
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

#include QMK_KEYBOARD_H
#include <string.h>
#include <stdio.h>

#include "keymap_extra.h"
#ifdef ST7565_ENABLE
#    include "keymap_st7565.h"
#endif

#define WPM_BUF_SIZE 4

enum custom_keycodes {
    WPM_RESET = SAFE_RANGE,
    WPM_MAX,
    WPM_CURR
};

static uint8_t max_wpm = 0;

#if defined(SPLIT_KEYBOARD) && defined(WPM_ENABLE)
void max_wpm_slave_handler(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    set_max_wpm(*((uint8_t *)in_data));
}

static bool max_wpm_changed = false;
#endif

void housekeeping_task_user(void) {
#ifdef SPLIT_KEYBOARD
    if (is_keyboard_master()) {
#    ifdef WPM_ENABLE
        static uint16_t last_max_wpm_update = 0;
        if (max_wpm_changed || timer_elapsed(last_max_wpm_update) > FORCED_SYNC_THROTTLE_MS) {
            if (transaction_rpc_send(FT_MAX_WPM, sizeof(max_wpm), &max_wpm)) {
                max_wpm_changed = false;
                last_max_wpm_update = timer_read();
            }
        }
#    endif
#    ifdef ST7565_ENABLE
        ft_display_rpc(false);
#    endif
    }
#endif
}

void keyboard_post_init_user(void) {
#ifdef SPLIT_KEYBOARD
#    ifdef WPM_ENABLE
    transaction_register_rpc(FT_MAX_WPM, max_wpm_slave_handler);
#    endif
#    ifdef ST7565_ENABLE
    ft_display_register_rpc();
#    endif
#endif
}

void suspend_power_down_user(void) {
#ifdef ST7565_ENABLE
    if (is_keyboard_master()) {
        set_ft_display_suspended(true);
#    ifdef SPLIT_KEYBOARD
        ft_display_rpc(true);
#    endif
    }
#endif
}

void suspend_wakeup_init_user(void) {
#ifdef ST7565_ENABLE
    if (is_keyboard_master()) {
        set_ft_display_suspended(false);
#    ifdef SPLIT_KEYBOARD
        ft_display_rpc(true);
#    endif
    }
#endif
}

void set_max_wpm(uint8_t new_max) {
    max_wpm = new_max;
#ifdef SPLIT_KEYBOARD
    max_wpm_changed = true;
#endif
}

uint8_t get_max_wpm(void) {
    return max_wpm;
}


bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    uint8_t wpm = get_current_wpm();
    if (wpm > max_wpm) {
        set_max_wpm(wpm);
    }
    switch (keycode) {
        case WPM_RESET:
            if (record->event.pressed) {
                set_max_wpm(0);
                set_current_wpm(0);
            }
            return false;
        case WPM_MAX:
        case WPM_CURR:
            if (record->event.pressed) {
                char wpm_buf[WPM_BUF_SIZE];
                snprintf(wpm_buf, WPM_BUF_SIZE, "%u", (keycode == WPM_MAX ? get_max_wpm() : wpm));
                send_string(wpm_buf);
            }
            return false;
    }
    return true;
}
