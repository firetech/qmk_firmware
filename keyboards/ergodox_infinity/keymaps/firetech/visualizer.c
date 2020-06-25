/* Copyright 2017 Fred Sundvik
 * Copyright 2020 Joakim Tufvegren
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
#error This visualizer needs that LCD backlight is enabled
#endif

#ifndef LCD_ENABLE
#error This visualizer needs that LCD is enabled
#endif

#ifndef BACKLIGHT_ENABLE
#error This visualizer needs that keyboard backlight is enabled
#endif

#include <stdio.h>
#include "visualizer.h"
#include "visualizer_keyframes.h"
#include "lcd_keyframes.h"
#include "lcd_backlight_keyframes.h"
#include "led_backlight_keyframes.h"
#include "system/serial_link.h"
#include "led.h"
#include "default_animations.h"
#include "wpm.h"
#include "keymap_wpm.h"

#ifdef EE_HANDS
#   include "eeconfig.h"
#endif

/* Configuration */
static const uint32_t default_color = LCD_COLOR(128, 0,    0xFF);
static const uint32_t fn_color      = LCD_COLOR(128, 0xFF, 0xFF);

#define STAT_BUF_SIZE 20


/* Keyframes */

static bool keyframe_enable(keyframe_animation_t* animation, visualizer_state_t* state) {
    bool ret = false;
    ret |= led_backlight_keyframe_enable(animation, state);
    ret |= lcd_keyframe_enable(animation, state);
    ret |= lcd_backlight_keyframe_enable(animation, state);
    return ret;
}

static bool keyframe_fade_in(keyframe_animation_t* animation, visualizer_state_t* state) {
    led_backlight_keyframe_fade_in_all(animation, state);

    // Fade in using brightness setting. That way, we end up at the correct value in the end.
    int frame_length = animation->frame_lengths[animation->current_frame];
    int current_pos  = frame_length - animation->time_left_in_frame;
    uint8_t brightness = state->status.backlight_level * 0xFF / BACKLIGHT_LEVELS;
    lcd_backlight_brightness(brightness * current_pos / frame_length);

    return true;
}

static bool keyframe_display_stats(keyframe_animation_t* animation, visualizer_state_t* state) {
    gdispClear(White);
    char stat_buf[STAT_BUF_SIZE];

    if (state->status.layer & 0x2) {
        snprintf(stat_buf, STAT_BUF_SIZE, "Backlight: %3u%%", state->status.backlight_level * 100 / BACKLIGHT_LEVELS);
        gdispDrawString(0, 0, stat_buf, state->font_fixed5x8, Black);
    }

    snprintf(stat_buf, STAT_BUF_SIZE, "WPM: %3u", get_current_wpm());
    gdispDrawString(0, 10, stat_buf, state->font_dejavusansbold12, Black);
    snprintf(stat_buf, STAT_BUF_SIZE, "Max: %3u", get_max_wpm());
    gdispDrawString(20, 20, stat_buf, state->font_fixed5x8, Black);

    return false;
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
    // Note that there's a 50 ms no-operation frame,
    // this prevents the color from changing when activating the layer
    // momentarily
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

void update_user_visualizer_state(visualizer_state_t* state, visualizer_keyboard_status_t* prev_status) {
    uint32_t prev_color = state->target_lcd_color;

    uint8_t brightness = state->status.backlight_level * 0xFF / BACKLIGHT_LEVELS;
    if (lcd_get_backlight_brightness() != brightness) {
        lcd_backlight_brightness(brightness);
    }

    if (state->status.layer & (1 << 2)) {
        state->target_lcd_color = fn_color;
        state->layer_text = "Fn";
    } else {
        state->target_lcd_color = default_color;
        state->layer_text = "Default";
    }

    if (prev_color != state->target_lcd_color) {
        start_keyframe_animation(&color_animation);
    }

    keyframe_animation_t* content_animation;
#ifdef EE_HANDS
    if (eeconfig_read_handedness()) {
#elif defined(MASTER_IS_ON_RIGHT)
    if (!is_keyboard_master()) {
#else
    if (is_keyboard_master()) {
#endif
        content_animation = &left_animation;
    } else {
        content_animation = &right_animation;
    }
    start_keyframe_animation(content_animation);
}

void user_visualizer_suspend(visualizer_state_t* state) {
    state->layer_text = "Suspending...";
    uint8_t h = LCD_HUE(state->current_lcd_color);
    uint8_t s = LCD_SAT(state->current_lcd_color);
    state->target_lcd_color = LCD_COLOR(h, s, 0);
    start_keyframe_animation(&default_suspend_animation);
}

void user_visualizer_resume(visualizer_state_t* state) {
    lcd_backlight_brightness(0);
    state->current_lcd_color = default_color;
    state->target_lcd_color = default_color;
    start_keyframe_animation(&startup_animation);
}
