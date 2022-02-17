/* Copyright 2022 Joakim Tufvegren
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

void render_luna(int x, int y);
void process_record_luna(uint16_t keycode, keyrecord_t *record);

#if defined(SPLIT_KEYBOARD) && defined(SPLIT_LUNA_RPC_ENABLE)
#    include "transactions.h"

#    ifndef FORCED_SYNC_THROTTLE_MS
#        define FORCED_SYNC_THROTTLE_MS 100
#    endif

void register_luna_rpc(void);
void luna_rpc(void);
#endif
