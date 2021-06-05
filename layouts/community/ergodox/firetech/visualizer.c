/* Copyright 2017 Fred Sundvik
 * Copyright 2021 Joakim Tufvegren
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

#ifndef LCD_BACKLIGHT_ENABLE
#    error This visualizer needs that LCD backlight is enabled
#endif

#ifndef LCD_ENABLE
#    error This visualizer needs that LCD is enabled
#endif

#if !defined(LED_MATRIX_ENABLE) && !defined(BACKLIGHT_ENABLE) && !defined(LCD_BACKLIGHT_LEVEL)
#    define LCD_BACKLIGHT_LEVEL 0x99
#endif

#include <stdio.h>
#include <math.h>

#include "visualizer.h"
#include "visualizer_keyframes.h"
#include "lcd_keyframes.h"
#include "lcd_backlight_keyframes.h"
#ifdef BACKLIGHT_ENABLE
#    include "led_backlight_keyframes.h"
#endif
#include "led.h"
#include "default_animations.h"
#include "wpm.h"
#include "keymap_extra.h"
#include "bongocat.h"


/* Configuration */
static const uint32_t default_color = LCD_COLOR(128, 0,    0xFF);
static const uint32_t swap_color    = LCD_COLOR(128, 0xFF, 0xFF);
static const uint32_t fn_color      = LCD_COLOR(170, 0xFF, 0xFF);

#define STAT_BUF_SIZE  20

#define WPM_ANIM_START  20  // Animation idle below this WPM value
#define WPM_TO_FRAME_TIME(wpm)  (2565 - 537 * log(wpm))  // Formula to convert WPM to frame time

typedef struct {
    uint8_t swap_hands;
    uint8_t last_backlight;
} visualizer_user_data_t;

// Don't access from visualization function, use the visualizer state instead
static visualizer_user_data_t user_data_keyboard = {
    .swap_hands = 0,
    .last_backlight = 0,
};

_Static_assert(sizeof(visualizer_user_data_t) <= VISUALIZER_USER_DATA_SIZE,
    "Please increase the VISUALIZER_USER_DATA_SIZE");


/* Keyframes */

static bool keyframe_enable(keyframe_animation_t* animation, visualizer_state_t* state) {
    bool ret = false;
#ifdef BACKLIGHT_ENABLE
    ret |= led_backlight_keyframe_enable(animation, state);
#endif
    ret |= lcd_keyframe_enable(animation, state);
    ret |= lcd_backlight_keyframe_enable(animation, state);
    return ret;
}

static bool keyframe_fade_in(keyframe_animation_t* animation, visualizer_state_t* state) {
#ifdef BACKLIGHT_ENABLE
    led_backlight_keyframe_fade_in_all(animation, state);
#endif

    // Fade in using brightness setting. That way, we end up at the correct value in the end.
    int frame_length = animation->frame_lengths[animation->current_frame];
    int current_pos  = frame_length - animation->time_left_in_frame;
#if defined(LED_MATRIX_ENABLE)
    uint8_t brightness = led_matrix_get_val();
#elif defined(BACKLIGHT_ENABLE)
    uint8_t brightness = state->status.backlight_level * 0xFF / BACKLIGHT_LEVELS;
#else
    uint8_t brightness = LCD_BACKLIGHT_LEVEL;
#endif
    lcd_backlight_brightness(brightness * current_pos / frame_length);

    return true;
}

static bool keyframe_display_stats(keyframe_animation_t* animation, visualizer_state_t* state) {
    char stat_buf[STAT_BUF_SIZE];
    uint8_t wpm = get_current_wpm();
    bool quick_update = false;

    gdispClear(White);

    static uint8_t wpm_anim_at_frame = 1;
    static uint16_t wpm_anim_timer = 0;
    const uint8_t *wpm_frame_P = bongocat[0];
    if (wpm >= WPM_ANIM_START) {
        wpm_frame_P = bongocat[wpm_anim_at_frame];

        if (timer_elapsed(wpm_anim_timer) >= WPM_TO_FRAME_TIME(wpm)) {
            wpm_anim_at_frame = 3 - wpm_anim_at_frame;
            wpm_anim_timer = timer_read();
        }
        quick_update = true;
    }
#if defined(__AVR__)
    const uint8_t wpm_frame[BONGOCAT_LENGTH];
    for (uint8_t i = 0; i < BONGOCAT_LENGTH; i ++) {
        wpm_frame[i] = pgm_read_byte(wpm_frame_P + i);
    }
#else
    const uint8_t *wpm_frame = wpm_frame_P;
#endif
    gdispGBlitArea(GDISP, LCD_WIDTH - BONGOCAT_WIDTH, 0, BONGOCAT_WIDTH, BONGOCAT_HEIGHT, 0, 0, BONGOCAT_LINEWIDTH, (pixel_t*)wpm_frame);

#if defined(LED_MATRIX_ENABLE) || defined(BACKLIGHT_ENABLE)
    if (state->status.layer & (1 << FN_LAYER)) {
#    if defined(LED_MATRIX_ENABLE)
        quick_update = true;
        uint8_t brightness = led_matrix_get_val();
        uint8_t backlight_level = 0;
        if (led_matrix_is_enabled()) {
            backlight_level = brightness * 100.0 / UINT8_MAX;
        }
#    elif defined(BACKLIGHT_ENABLE)
        uint8_t backlight_level = state->status.backlight_level * 100.0 / BACKLIGHT_LEVELS;
#    endif
        snprintf(stat_buf, STAT_BUF_SIZE, "Backlight: %3u%%", backlight_level);
        gdispDrawString(0, 0, stat_buf, state->font_fixed5x8, Black);
    }
#endif

    snprintf(stat_buf, STAT_BUF_SIZE, "WPM: %3u", wpm);
    gdispDrawString(0, 10, stat_buf, state->font_dejavusansbold12, Black);
    snprintf(stat_buf, STAT_BUF_SIZE, "Max: %3u", get_max_wpm());
    gdispDrawString(20, 20, stat_buf, state->font_fixed5x8, Black);

    return quick_update;
}


/* Animations */

// Basically a faster version of default_startup_animation
static keyframe_animation_t startup_animation = {
    .num_frames    = 3,
    .loop          = false,
    .frame_lengths = {0, 0, gfxMillisecondsToTicks(1000)},
    .frame_functions = {
        keyframe_enable,
        lcd_keyframe_draw_logo,
        keyframe_fade_in
    },
};

static keyframe_animation_t* current_content_animation = NULL;

static keyframe_animation_t left_animation = {
    .num_frames = 1,
    .loop = true,
    .frame_lengths = {gfxMillisecondsToTicks(1000)}, // Keep updated
    .frame_functions = {keyframe_display_stats}
};

static keyframe_animation_t right_animation = {
    .num_frames = 1,
    .loop = false,
    .frame_lengths = {0},
    .frame_functions = {lcd_keyframe_display_layer_and_led_states}
};

// The color animation animates the LCD color when you change layers
static keyframe_animation_t color_animation = {
    .num_frames = 1,
    .loop = false,
    .frame_lengths = {gfxMillisecondsToTicks(150)},
    .frame_functions = {lcd_backlight_keyframe_animate_color},
};


/* User Visualizer API functions */

void initialize_user_visualizer(visualizer_state_t* state) {
    lcd_backlight_brightness(0);
    state->current_lcd_color = default_color;
    state->target_lcd_color = default_color;
    start_keyframe_animation(&startup_animation);
}

void set_hand_swap(bool do_swap) {
    user_data_keyboard.swap_hands = do_swap;
    visualizer_set_user_data(&user_data_keyboard);
}

#ifdef LED_MATRIX_ENABLE
void on_backlight_change(void) {
    uint8_t brightness = led_matrix_is_enabled() ? led_matrix_get_val() : 0;
    if (user_data_keyboard.last_backlight != brightness) {
        // Trigger a visualizer state update
        user_data_keyboard.last_backlight = brightness;
        visualizer_set_user_data(&user_data_keyboard);
    }
}
#endif

void update_user_visualizer_state(visualizer_state_t* state, visualizer_keyboard_status_t* prev_status) {
    uint32_t prev_color = state->target_lcd_color;

#if defined(LED_MATRIX_ENABLE)
    uint8_t brightness = led_matrix_is_enabled() ? led_matrix_get_val() : 0;
#elif defined(BACKLIGHT_ENABLE)
    user_data_keyboard.last_backlight = state->status.backlight_level;
    // Deliberately not calling set_user_data here, not needed for this value.
    uint8_t brightness = user_data_keyboard.last_backlight;
    brightness *= 0xFF / BACKLIGHT_LEVELS;
#else
    uint8_t brightness = LCD_BACKLIGHT_LEVEL;
#endif
    if (lcd_get_backlight_brightness() != brightness) {
        lcd_backlight_brightness(brightness);
    }

    if (state->status.layer & (1 << FN_LAYER)) {
        state->target_lcd_color = fn_color;
        if (state->status.layer & (1 << SWAP_LAYER)) {
            state->layer_text = "Fn    (+ Swap)";
        } else {
            state->layer_text = "Fn";
        }
    } else if (state->status.layer & (1 << SWAP_LAYER)) {
        state->target_lcd_color = swap_color;
        state->layer_text = "Swap On Space";
    } else {
        state->target_lcd_color = default_color;
        state->layer_text = "Default";
    }

    if (prev_color != state->target_lcd_color) {
        start_keyframe_animation(&color_animation);
    }

    bool is_left = is_keyboard_left();
    visualizer_user_data_t *user_data = (visualizer_user_data_t *)state->status.user_data;
    if (user_data->swap_hands) { is_left = !is_left; }

    keyframe_animation_t* content_animation;
    if (is_left) {
        content_animation = &left_animation;
    } else {
        content_animation = &right_animation;
    }
    if (current_content_animation != content_animation || !content_animation->loop) {
        if (current_content_animation) {
            stop_keyframe_animation(current_content_animation);
        }
        start_keyframe_animation(content_animation);
        current_content_animation = content_animation;
    } else {
        content_animation->need_update = true;
    }
}

void user_visualizer_suspend(visualizer_state_t* state) {
    state->layer_text = "Suspending...";
    uint8_t h = LCD_HUE(state->current_lcd_color);
    uint8_t s = LCD_SAT(state->current_lcd_color);
    state->target_lcd_color = LCD_COLOR(h, s, 0);
    start_keyframe_animation(&default_suspend_animation);
    current_content_animation = NULL;
}

void user_visualizer_resume(visualizer_state_t* state) {
#ifdef BACKLIGHT_ENABLE
    backlight_set(user_data_keyboard.last_backlight); // Chibios suspend sets this to 0, so we need to reset the value.
#endif
    lcd_backlight_brightness(0);
    state->current_lcd_color = default_color;
    state->target_lcd_color = default_color;
    start_keyframe_animation(&startup_animation);
    current_content_animation = NULL;
}
