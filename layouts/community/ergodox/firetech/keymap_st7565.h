#pragma once

typedef struct {
    bool is_on;
    uint8_t leds;
    uint32_t layer;
    bool swap_displays;
    bool clear_display;
} ft_display_state_t;

extern ft_display_state_t ft_display_state;

void set_ft_display_state(ft_display_state_t *new_state);
bool same_ft_display_state(ft_display_state_t *a, ft_display_state_t *b);
