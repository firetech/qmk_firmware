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

#include QMK_KEYBOARD_H

#include "keymap_extra.h"

uint16_t blink_timer = 0;
uint16_t blink_led_3[] = { 0, 0 };

void matrix_scan_user(void){
    if (blink_led_3[0]){
        ergodox_right_led_3_off();
        uint16_t blink_elapsed = timer_elapsed(blink_timer);
        if (blink_elapsed > blink_led_3[1]) { blink_timer = timer_read(); }
        if (blink_elapsed < blink_led_3[0]) { ergodox_right_led_3_on(); }
    }
};

layer_state_t layer_state_set_user(layer_state_t state) {
    ergodox_right_led_3_off();
    blink_led_3[0] = 0;

    if (state & (1 << FN_LAYER)) {
        blink_led_3[0] = 300;
        blink_led_3[1] = 500;
        if (state & (1 << SWAP_LAYER)) {
            blink_led_3[0] = 800;
            blink_led_3[1] = 1000;
        }
    } else if (state & (1 << SWAP_LAYER)) {
        ergodox_right_led_3_on();
    }
    return state;
};

void led_set_user(uint8_t usb_led) {
    if (usb_led & (1<<USB_LED_NUM_LOCK)) {
        ergodox_right_led_1_on();
    } else {
        ergodox_right_led_1_off();
    }
    if (usb_led & (1<<USB_LED_CAPS_LOCK)) {
        ergodox_right_led_2_on();
    } else {
        ergodox_right_led_2_off();
    }
};
