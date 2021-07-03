#undef TAPPING_TOGGLE
#define TAPPING_TOGGLE  2

#undef TAPPING_TERM
#define TAPPING_TERM  200

#define PERMISSIVE_HOLD

#undef BACKLIGHT_LEVELS
#define BACKLIGHT_LEVELS  5
#undef LED_MATRIX_VAL_STEP
#define LED_MATRIX_VAL_STEP  51

#ifdef SPLIT_KEYBOARD
#    ifdef ST7565_ENABLE
#        define SPLIT_LED_STATE_ENABLE
#        define SPLIT_LAYER_STATE_ENABLE
#    endif
#    ifdef LED_MATRIX_ENABLE
#        define SPLIT_TRANSPORT_MIRROR
#        define LED_MATRIX_KEYREACTIVE_ENABLED
#    endif
#    ifdef WPM_ENABLE
#        define SPLIT_WPM_ENABLE
#    endif

// It's easier to just define all of these and not use them if irrelevant
#    define SPLIT_TRANSACTION_IDS_USER  FT_MAX_WPM, FT_DISPLAY_STATE, FT_DISPLAY_FADE_STATE
#endif

#ifdef ST7565_ENABLE
#    define ST7565_TIMEOUT  3600000 // 60 minutes
#    if defined(LED_MATRIX_ENABLE) && defined(LED_DISABLE_WHEN_USB_SUSPENDED)
#        undef LED_DISABLE_WHEN_USB_SUSPENDED  // Controlled via ST7565 callbacks instead
#    endif
#endif

#ifdef KEYBOARD_ergodox_infinity
#    define EE_HANDS
#endif

#ifdef KEYBOARD_ergodox_ez
#    define QMK_KEYS_PER_SCAN  4
#endif
