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

#define FADE_LENGTH 750

#define STAT_BUF_SIZE  10

#define WPM_ANIM_START  20  // Animation idle below this WPM value
#define WPM_TO_FRAME_TIME(wpm)  (2565 - 537 * log(wpm))  // Formula to convert WPM to frame time

enum ft_display_mode {
    OFF,
    FADE_IN,
    ON,
    FADE_OUT,
};

typedef struct {
    enum ft_display_mode mode;
    bool swap_displays;
    bool suspended;
} ft_display_state_t;

typedef struct {
    uint16_t timer;
    uint8_t brightness;
} ft_display_fade_state_t;

ft_display_state_t ft_display_state = {
    .swap_displays = false,
    .mode = OFF,
    .suspended = false,
};
ft_display_fade_state_t ft_display_fade_state = {
    .timer = 0,
    .brightness = 0,
};
static bool clear_display = true;

#ifdef SPLIT_KEYBOARD
static bool same_ft_display_state(ft_display_state_t *a, ft_display_state_t *b) {
    return (a->mode == b->mode &&
        a->swap_displays == b->swap_displays &&
        a->suspended == b->suspended);
}

static bool same_ft_display_fade_state(ft_display_fade_state_t *a, ft_display_fade_state_t *b) {
    return (a->timer == b->timer &&
        a->brightness == b->brightness);
}

static void ft_display_state_slave_handler(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    ft_display_state_t *new_state = (ft_display_state_t *)in_data;
    if (!same_ft_display_state(&ft_display_state, new_state)) {
        memcpy(&ft_display_state, new_state, sizeof(ft_display_state));
        bool should_be_on = (ft_display_state.mode == FADE_IN || ft_display_state.mode == ON);
        if (should_be_on && !st7565_is_on()) {
            st7565_on();
        } else if (!should_be_on && st7565_is_on()) {
            st7565_off();
        }
        clear_display = true;
    }
}

static void ft_display_fade_state_slave_handler(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    memcpy(&ft_display_fade_state, in_data, sizeof(ft_display_fade_state));
}

void ft_display_register_rpc(void) {
    transaction_register_rpc(FT_DISPLAY_STATE, ft_display_state_slave_handler);
    transaction_register_rpc(FT_DISPLAY_FADE_STATE, ft_display_fade_state_slave_handler);
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

    if (ft_display_state.mode == FADE_IN || ft_display_state.mode == FADE_OUT) {
        static uint16_t last_display_fade_state_update = 0;
        static ft_display_fade_state_t last_display_fade_state;
        if (force || !same_ft_display_fade_state(&last_display_fade_state, &ft_display_fade_state) || timer_elapsed(last_display_fade_state_update) > FORCED_SYNC_THROTTLE_MS) {
            if (transaction_rpc_send(FT_DISPLAY_FADE_STATE, sizeof(ft_display_fade_state), &ft_display_fade_state)) {
                memcpy(&last_display_fade_state, &ft_display_fade_state, sizeof(last_display_fade_state));
                last_display_fade_state_update = timer_read();
            }
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

void st7565_on_user(void) {
    if (ft_display_state.suspended) {
        // Stay suspended
        st7565_off();
        return;
    }

    if (is_keyboard_master()) {
        if (ft_display_state.mode == OFF) {
            ft_display_state.mode = FADE_IN;
        }
        if (ft_display_state.mode == FADE_IN) {
            ft_display_fade_state.timer = sync_timer_read();
            clear_display = true;
        }
    } else {
        ft_display_state.mode = ON;
    }
}

void st7565_off_user(void) {
    if (is_keyboard_master()) {
        if (ft_display_state.mode == FADE_IN || ft_display_state.mode == ON) {
            ft_display_fade_state.brightness = current_brightness;
            ft_display_fade_state.timer = sync_timer_read();
            ft_display_state.mode = FADE_OUT;
            clear_display = true;
        }
    } else {
        ft_display_state.mode = OFF;
    }
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

display_rotation_t st7565_init_user(display_rotation_t rotation) {
    // Prepare FADE_IN
    lcd_backlight_brightness(0);
    lcd_backlight_color(0, 0, 255);
    if (is_keyboard_master()) {
#ifdef LED_MATRIX_ENABLE
        ft_display_fade_state.brightness = led_matrix_get_val();
#else
        ft_display_fade_state.brightness = current_brightness;
#endif
        ft_display_fade_state.timer = sync_timer_read();
        led_matrix_set_val_noeeprom(0);
        ft_display_state.mode = FADE_IN;
    }
    return rotation;
}

void st7565_task_user(void) {
    if (is_keyboard_master()) {
        if (swap_hands != ft_display_state.swap_displays) {
            ft_display_state.swap_displays = swap_hands;
            clear_display = true;
        }
    }

    if (clear_display) {
        st7565_clear();
        clear_display = false;
    }

    if (ft_display_state.mode == FADE_IN || ft_display_state.mode == FADE_OUT) {
        uint16_t fade_elapsed = sync_timer_elapsed(ft_display_fade_state.timer);
        if (fade_elapsed < FADE_LENGTH) {
            uint8_t fade_brightness = ((float)fade_elapsed / FADE_LENGTH) * ft_display_fade_state.brightness;
            if (ft_display_state.mode == FADE_IN) {
                // Draw logo
                static const char qmk_logo[] = {
                    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94,
                    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4,
                    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0x00
                };

                st7565_write(qmk_logo, false);
                st7565_set_cursor(3, 3);
                st7565_write("ergodox/firetech", false);
            } else {
                fade_brightness = ft_display_fade_state.brightness - fade_brightness;
            }
#ifdef LED_MATRIX_ENABLE
            if (is_keyboard_master()) {
                led_matrix_set_val_noeeprom(fade_brightness);
            }
#endif
            lcd_backlight_brightness(fade_brightness);
            return;
        } else {
            uint8_t final_brightness;
            if (ft_display_state.mode == FADE_IN) {
                ft_display_state.mode = ON;
                final_brightness = ft_display_fade_state.brightness;
            } else {
                st7565_off();
                ft_display_state.mode = OFF;
                final_brightness = 0;
            }
#ifdef LED_MATRIX_ENABLE
            if (is_keyboard_master()) {
                led_matrix_set_val_noeeprom(final_brightness);
            }
#endif
            lcd_backlight_brightness(final_brightness);
        }
    }

    if (ft_display_state.mode == ON) {
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

        if (is_left) {
            draw_left();
        } else {
            draw_right(layer_text_P);
        }
    }
}
