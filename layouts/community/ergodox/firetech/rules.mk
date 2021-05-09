# Configuration
#
COMMAND_ENABLE = no
CONSOLE_ENABLE = yes
WPM_ENABLE = yes


# Extra source files
#
SRC += keymap_extra.c
ifneq ($(strip $(VISUALIZER_ENABLE)), yes)
  SRC += keymap_leds.c
endif
