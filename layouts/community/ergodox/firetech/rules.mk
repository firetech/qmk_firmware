# Configuration
#
COMMAND_ENABLE = no
CONSOLE_ENABLE = yes
WPM_ENABLE = yes
SWAP_HANDS_ENABLE = yes

DEBOUNCE_TYPE ?= sym_defer_pk

# Done here since ergodox_infinity/config.h is read before our config.h
OPT_DEFS += -DLED_MATRIX_KEYREACTIVE_ENABLED

# Extra source files
#
SRC += keymap_extra.c
ifeq ($(strip $(ST7565_ENABLE)), yes)
  SRC += st7565_display.c luna.c
else
  SRC += leds.c
endif
