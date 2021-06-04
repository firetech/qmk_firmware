# MCU name
MCU = MK20DX256

# Bootloader selection
BOOTLOADER = kiibohd

# Build Options
#   comment out to disable the options.
#
BOOTMAGIC_ENABLE = no  # Virtual DIP switch configuration
MOUSEKEY_ENABLE  = yes # Mouse keys
EXTRAKEY_ENABLE  = yes # Audio control and System control
CONSOLE_ENABLE   = no  # Console for debug
COMMAND_ENABLE   = yes # Commands for debug and configuration
SLEEP_LED_ENABLE = yes # Breathing sleep LED during USB suspend
NKRO_ENABLE      = yes # USB Nkey Rollover - if this doesn't work, see here: https://github.com/tmk/tmk_keyboard/wiki/FAQ#nkro-doesnt-work
UNICODE_ENABLE   = yes # Unicode
SWAP_HANDS_ENABLE= yes # Allow swapping hands of keyboard

VISUALIZER_ENABLE = yes
LCD_ENABLE = yes
LCD_BACKLIGHT_ENABLE = yes
MIDI_ENABLE = no
RGBLIGHT_ENABLE = no

LCD_DRIVER = st7565
LCD_WIDTH = 128
LCD_HEIGHT = 32

LED_MATRIX_ENABLE = yes
LED_MATRIX_DRIVER = IS31FL3731

SPLIT_KEYBOARD = yes
SERIAL_DRIVER = usart

# project specific files
SRC = led.c

LAYOUTS = ergodox
