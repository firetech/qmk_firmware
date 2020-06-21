#undef TAPPING_TOGGLE
#define TAPPING_TOGGLE  2

#undef TAPPING_TERM
#define TAPPING_TERM  200

#define PERMISSIVE_HOLD

#ifdef KEYBOARD_ergodox_infinity
#    undef BACKLIGHT_LEVELS
#    define BACKLIGHT_LEVELS  5
#    undef LED_MATRIX_VAL_STEP
#    define LED_MATRIX_VAL_STEP 51

#    define EE_HANDS

#    define SPLIT_WPM_ENABLE
#    define SPLIT_TRANSACTION_IDS_USER  MAX_WPM
#    define SPLIT_TRANSPORT_MIRROR
#    define LED_MATRIX_KEYREACTIVE_ENABLED
#endif

#ifdef KEYBOARD_ergodox_ez
#    define QMK_KEYS_PER_SCAN  4
#endif
