#undef TAPPING_TOGGLE
#define TAPPING_TOGGLE  2

#undef TAPPING_TERM
#define TAPPING_TERM  200

#define PERMISSIVE_HOLD

#undef BACKLIGHT_LEVELS
#define BACKLIGHT_LEVELS  5
#undef LED_MATRIX_VAL_STEP
#define LED_MATRIX_VAL_STEP 51

#ifdef SPLIT_KEYBOARD
#    define SPLIT_WPM_ENABLE
#    define SPLIT_TRANSACTION_IDS_USER  FT_MAX_WPM, FT_DISPLAY_STATE
#    define SPLIT_TRANSPORT_MIRROR
#    define LED_MATRIX_KEYREACTIVE_ENABLED
#endif

#ifdef ST7565_ENABLE
#    define ST7565_TIMEOUT 0
#endif

#ifdef KEYBOARD_ergodox_infinity
#    define EE_HANDS
#endif

#ifdef KEYBOARD_ergodox_ez
#    define QMK_KEYS_PER_SCAN  4
#endif

#define DEBUG_MATRIX_SCAN_RATE
