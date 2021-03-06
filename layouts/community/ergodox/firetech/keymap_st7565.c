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
#include <math.h>
#include <string.h>

#include "keymap_st7565.h"
#include "keymap_extra.h"
#include "bongocat.h"

#define STAT_BUF_SIZE  10

#define WPM_ANIM_START  20  // Animation idle below this WPM value
#define WPM_TO_FRAME_TIME(wpm)  (2565 - 537 * log(wpm))  // Formula to convert WPM to frame time

typedef struct {
    bool is_on;
    bool swap_displays;
    bool suspended;
} ft_display_state_t;

ft_display_state_t ft_display_state = {
    .is_on = true,
    .swap_displays = false,
    .suspended = false,
};
static bool clear_display = true;

#ifdef SPLIT_KEYBOARD
static bool same_ft_display_state(ft_display_state_t *a, ft_display_state_t *b) {
    if (a->is_on == b->is_on &&
            a->swap_displays == b->swap_displays &&
            a->suspended == b->suspended) {
        return true;
    }
    return false;
}

static void display_state_slave_handler(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    ft_display_state_t *new_state = (ft_display_state_t *)in_data;
    if (!same_ft_display_state(&ft_display_state, new_state)) {
        memcpy(&ft_display_state, new_state, sizeof(ft_display_state));
        if (ft_display_state.is_on != st7565_is_on()) {
            if (ft_display_state.is_on) {
                st7565_on();
            } else {
                st7565_off();
            }
        }
        clear_display = true;
    }
}

void ft_display_register_rpc(void) {
    transaction_register_rpc(FT_DISPLAY_STATE, display_state_slave_handler);
}

void ft_display_rpc(bool force) {
    if (!is_keyboard_master()) {
        return;
    }

    static uint16_t last_display_state_update = 0;
    static ft_display_state_t last_display_state;
    if (force || !same_ft_display_state(&last_display_state, &ft_display_state) || timer_elapsed(last_display_state_update) > FORCED_SYNC_THROTTLE_MS) {
        if (transaction_rpc_send(FT_DISPLAY_STATE, sizeof(ft_display_state), &ft_display_state)) {
            memcpy(&last_display_state, &ft_display_state, sizeof(last_display_state));
            last_display_state_update = timer_read();
        }
    }
}
#endif

void set_ft_display_suspended(bool suspended) {
    if (!is_keyboard_master()) {
        return;
    }

    ft_display_state.suspended = suspended;
    if (suspended) {
        st7565_off();
    } else {
        st7565_on();
    }
}

/* LCD backlight code copied from quantum/visualizer/lcd_backlight.c */
static uint8_t current_hue        = 0;
static uint8_t current_saturation = 0;
static uint8_t current_intensity  = 0;
static uint8_t current_brightness = 255;

// This code is based on Brian Neltner's blogpost and example code
// "Why every LED light should be using HSI colorspace".
// http://blog.saikoled.com/post/43693602826/why-every-led-light-should-be-using-hsi
static void hsi_to_rgb(float h, float s, float i, uint16_t* r_out, uint16_t* g_out, uint16_t* b_out) {
    unsigned int r, g, b;
    h = fmodf(h, 360.0f);                         // cycle h around to 0-360 degrees
    h = 3.14159f * h / 180.0f;                    // Convert to radians.
    s = s > 0.0f ? (s < 1.0f ? s : 1.0f) : 0.0f;  // clamp s and i to interval [0,1]
    i = i > 0.0f ? (i < 1.0f ? i : 1.0f) : 0.0f;

    // Math! Thanks in part to Kyle Miller.
    if (h < 2.09439f) {
        r = 65535.0f * i / 3.0f * (1.0f + s * cos(h) / cosf(1.047196667f - h));
        g = 65535.0f * i / 3.0f * (1.0f + s * (1.0f - cosf(h) / cos(1.047196667f - h)));
        b = 65535.0f * i / 3.0f * (1.0f - s);
    } else if (h < 4.188787) {
        h = h - 2.09439;
        g = 65535.0f * i / 3.0f * (1.0f + s * cosf(h) / cosf(1.047196667f - h));
        b = 65535.0f * i / 3.0f * (1.0f + s * (1.0f - cosf(h) / cosf(1.047196667f - h)));
        r = 65535.0f * i / 3.0f * (1.0f - s);
    } else {
        h = h - 4.188787;
        b = 65535.0f * i / 3.0f * (1.0f + s * cosf(h) / cosf(1.047196667f - h));
        r = 65535.0f * i / 3.0f * (1.0f + s * (1.0f - cosf(h) / cosf(1.047196667f - h)));
        g = 65535.0f * i / 3.0f * (1.0f - s);
    }
    *r_out = r > 65535 ? 65535 : r;
    *g_out = g > 65535 ? 65535 : g;
    *b_out = b > 65535 ? 65535 : b;
}

static void lcd_backlight_color(uint8_t hue, uint8_t saturation, uint8_t intensity) {
    uint16_t r, g, b;
    float    hue_f        = 360.0f * (float)hue / 255.0f;
    float    saturation_f = (float)saturation / 255.0f;
    float    intensity_f  = (float)intensity / 255.0f;
    intensity_f *= (float)current_brightness / 255.0f;
    hsi_to_rgb(hue_f, saturation_f, intensity_f, &r, &g, &b);
    current_hue        = hue;
    current_saturation = saturation;
    current_intensity  = intensity;
    ergodox_infinity_lcd_color(r, g, b);
}

static void lcd_backlight_brightness(uint8_t b) {
    current_brightness = b;
    lcd_backlight_color(current_hue, current_saturation, current_intensity);
}
/* End of copied LCD backlight code */

static uint8_t prev_brightness = 0;

void st7565_on_user(void) {
    if (ft_display_state.suspended) {
        uint8_t stored_brightness = prev_brightness;
        st7565_off();
        prev_brightness = stored_brightness;
        return;
    }
    ft_display_state.is_on = true;
#ifdef LED_MATRIX_ENABLE
    led_matrix_set_val_noeeprom(prev_brightness);
#endif
    lcd_backlight_brightness(prev_brightness);
}

void st7565_off_user(void) {
    ft_display_state.is_on = false;
    prev_brightness = current_brightness;
#ifdef LED_MATRIX_ENABLE
    led_matrix_set_val_noeeprom(0);
#endif
    lcd_backlight_brightness(0);
}

static inline void write_led_state(uint8_t *row, uint8_t *col, const char *text_P) {
    const size_t length = strlen_P(text_P);
    if (*col + length > st7565_max_chars()) {
        st7565_advance_page(true);
        st7565_write_P(PSTR("  "), false);
        *col = 2;
        *row += 1;
    }
    st7565_write_P(text_P, false);
    st7565_write_char(' ', false);
    *col += length + 1;
}

static void draw_right(const char *layer_text_P) {
    st7565_write_ln_P(layer_text_P, false);
    if (strlen_P(layer_text_P) < st7565_max_chars()) {
        st7565_advance_page(true);
    }

    st7565_write_P("\x0F ", false);
    uint8_t row = 2;
    uint8_t col = 2;
    led_t leds = host_keyboard_led_state();
    if (leds.num_lock) { write_led_state(&row, &col, PSTR("Num")); }
    if (leds.caps_lock) { write_led_state(&row, &col, PSTR("Caps")); }
    if (leds.scroll_lock) { write_led_state(&row, &col, PSTR("Scroll")); }
    if (leds.compose) { write_led_state(&row, &col, PSTR("Compose")); }
    if (leds.kana) { write_led_state(&row, &col, PSTR("Kana")); }
    while (row++ <= 3) {
        st7565_advance_page(true);
    }
}

static void draw_left(void) {
    char stat_buf[STAT_BUF_SIZE];

    const uint8_t* wpm_frame = bongocat[0];
#ifdef WPM_ENABLE
    uint8_t wpm = get_current_wpm();

    st7565_advance_page(true);
    st7565_advance_page(true);
    st7565_write_P(PSTR("WPM: "), false);
    snprintf(stat_buf, STAT_BUF_SIZE, "%u", wpm);
    st7565_write_ln(stat_buf, false);

    st7565_write_P(PSTR("    ("), false);
    snprintf(stat_buf, STAT_BUF_SIZE, "%u", get_max_wpm());
    st7565_write(stat_buf, false);
    st7565_write_ln_P(PSTR(")"), false);

    static uint16_t wpm_anim_timer = 0;
    static uint8_t wpm_anim_at_frame = 1;
    if (wpm >= WPM_ANIM_START) {
        wpm_frame = bongocat[wpm_anim_at_frame];

        if (timer_elapsed(wpm_anim_timer) >= WPM_TO_FRAME_TIME(wpm)) {
            wpm_anim_at_frame = 3 - wpm_anim_at_frame;
            wpm_anim_timer = timer_read();
        }
    }
#endif

    /* My bongocat.h was formatted for uGFX, can't use st7565_write_raw_P. */
    uint8_t x = ST7565_DISPLAY_WIDTH - BONGOCAT_WIDTH;
    uint8_t y = 0;
    for (uint8_t byte = 0; byte < BONGOCAT_LENGTH; byte++) {
        uint8_t pixels = pgm_read_byte(wpm_frame + byte);
        for (uint8_t mask = (1u << 7); mask > 0; mask >>= 1) {
            st7565_write_pixel(x, y, (pixels & mask) == 0);
            if (++x > ST7565_DISPLAY_WIDTH) {
                x = ST7565_DISPLAY_WIDTH - BONGOCAT_WIDTH;
                y++;
                break;
            }
        }
    }

#ifdef LED_MATRIX_ENABLE
    if (layer_state & (1 << _FN)) {
        st7565_set_cursor(0, 0);
        uint8_t backlight_level = 0;
        if (led_matrix_is_enabled()) {
            backlight_level = led_matrix_get_val() * 100.0 / UINT8_MAX;
        }
        st7565_write_P(PSTR("Backlight: "), false);
        snprintf(stat_buf, STAT_BUF_SIZE, "%u%%", backlight_level);
        st7565_write(stat_buf, false);

        if (led_matrix_is_enabled()) {
            st7565_set_cursor(3, 1);
            st7565_write_P(PSTR("\x10 Mode: "), false);
            snprintf(stat_buf, STAT_BUF_SIZE, "%u", led_matrix_get_mode());
            st7565_write(stat_buf, false);
        }
    }
#endif
}

void st7565_task_user(void) {
    if (is_keyboard_master()) {
        if (swap_hands != ft_display_state.swap_displays) {
            ft_display_state.swap_displays = swap_hands;
            clear_display = true;
        }
    }

    if (ft_display_state.is_on) {
#ifdef LED_MATRIX_ENABLE
        lcd_backlight_brightness(led_matrix_get_val());
#endif

        const char *layer_text_P;
        if (layer_state & (1 << _FN)) {
            lcd_backlight_color(170, 255, 255);
            if (layer_state & (1 << _SWAP)) {
                layer_text_P = PSTR("Fn   Swap On Space");
            } else {
                layer_text_P = PSTR("Fn");
            }
        } else if (layer_state & (1 << _SWAP)) {
            lcd_backlight_color(128, 255, 255);
            layer_text_P = PSTR("Swap On Space");
        } else {
            lcd_backlight_color(0, 0, 255);
            layer_text_P = PSTR("Default");
        }

        bool is_left = is_keyboard_left();
        if (ft_display_state.swap_displays) { is_left = !is_left; }

        if (clear_display) {
            st7565_clear();
            clear_display = false;
        }

        if (is_left) {
            draw_left();
        } else {
            draw_right(layer_text_P);
        }
    }
}
