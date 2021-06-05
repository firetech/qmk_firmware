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

#pragma once

typedef struct {
    bool is_on;
    uint8_t leds;
    uint32_t layer;
    bool swap_displays;
} ft_display_state_t;

extern ft_display_state_t ft_display_state;

void set_ft_display_state(ft_display_state_t *new_state);
bool same_ft_display_state(ft_display_state_t *a, ft_display_state_t *b);
