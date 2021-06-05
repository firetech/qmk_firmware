#include QMK_KEYBOARD_H
#include <math.h>
#include <string.h>

#include "keymap_st7565.h"
#include "keymap_extra.h"
#include "bongocat.h"

#define STAT_BUF_SIZE  10

#define WPM_ANIM_START  20  // Animation idle below this WPM value
#define WPM_TO_FRAME_TIME(wpm)  (2565 - 537 * log(wpm))  // Formula to convert WPM to frame time

ft_display_state_t ft_display_state = {
    .is_on = true,
    .leds  = 0xFF,
    .layer = 0xFFFFFFFF,
    .swap_displays = false,
};
static bool clear_display = true;

void set_ft_display_state(ft_display_state_t* new_state) {
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

bool same_ft_display_state(ft_display_state_t *a, ft_display_state_t *b) {
    if (a->is_on == b->is_on &&
            a->leds == b->leds &&
            a->layer == b->layer &&
            a->layer == b->layer &&
            a->swap_displays == b->swap_displays) {
        return true;
    }
    return false;
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
    ft_display_state.is_on = true;
    lcd_backlight_brightness(prev_brightness);
}

void st7565_off_user(void) {
    ft_display_state.is_on = false;
    prev_brightness = current_brightness;
    lcd_backlight_brightness(0);
}

void set_hand_swap(bool do_swap) {
    ft_display_state.swap_displays = do_swap;
    clear_display = true;
}

static inline void write_led_state(uint8_t *row, uint8_t *col, uint8_t led, const char *text_P) {
    if (ft_display_state.leds & (1u << led)) {
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
}

void draw_right(const char *layer_text_P) {
    st7565_write_ln_P(layer_text_P, false);
    if (strlen_P(layer_text_P) < st7565_max_chars()) {
        st7565_advance_page(true);
    }

    st7565_write_char(0x2A, false);
    st7565_write_char(' ', false);
    uint8_t row = 2;
    uint8_t col = 2;
    write_led_state(&row, &col, USB_LED_NUM_LOCK, PSTR("Num"));
    write_led_state(&row, &col, USB_LED_CAPS_LOCK, PSTR("Caps"));
    write_led_state(&row, &col, USB_LED_SCROLL_LOCK, PSTR("Scroll"));
    write_led_state(&row, &col, USB_LED_COMPOSE, PSTR("Compose"));
    write_led_state(&row, &col, USB_LED_KANA, PSTR("Kana"));
    while (row++ <= 3) {
        st7565_advance_page(true);
    }
}

void draw_left(void) {
    char stat_buf[STAT_BUF_SIZE];

    const uint8_t* wpm_frame = bongocat[0];
#ifdef WPM_ENABLE
    uint8_t wpm = get_current_wpm();

    st7565_advance_page(true);
    st7565_advance_page(true);
    st7565_write_P(PSTR("WPM: "), false);
    snprintf(stat_buf, STAT_BUF_SIZE, "%3u", wpm);
    st7565_write_ln(stat_buf, false);

    st7565_write_P(PSTR("Max: "), false);
    snprintf(stat_buf, STAT_BUF_SIZE, "%3u", get_max_wpm());
    st7565_write_ln(stat_buf, false);

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

debug_enable = true;
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
    if (ft_display_state.layer & (1 << FN_LAYER)) {
        st7565_set_cursor(0, 0);
        uint8_t brightness = led_matrix_get_val();
        uint8_t backlight_level = 0;
        if (led_matrix_is_enabled()) {
            backlight_level = brightness * 100.0 / UINT8_MAX;
        }
        st7565_write_P(PSTR("Backlight: "), false);
        snprintf(stat_buf, STAT_BUF_SIZE, "%3u%%", backlight_level);
        st7565_write(stat_buf, false);
    }
#endif
}

void st7565_task_user(void) {
    if (is_keyboard_master()) {
        ft_display_state.is_on = st7565_is_on();
        ft_display_state.leds = host_keyboard_leds();
        ft_display_state.layer = layer_state;
    }

    if (ft_display_state.is_on) {
#ifdef LED_MATRIX_ENABLE
        lcd_backlight_brightness(led_matrix_get_val());
#endif

        const char *layer_text_P;
        if (ft_display_state.layer & (1 << FN_LAYER)) {
            lcd_backlight_color(170, 255, 255);
            if (ft_display_state.layer & (1 << SWAP_LAYER)) {
                layer_text_P = PSTR("Fn   Swap On Space");
            } else {
                layer_text_P = PSTR("Fn");
            }
        } else if (ft_display_state.layer & (1 << SWAP_LAYER)) {
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
