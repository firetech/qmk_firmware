#undef TAPPING_TOGGLE
#define TAPPING_TOGGLE  2

#undef TAPPING_TERM
#define TAPPING_TERM  200

#define PERMISSIVE_HOLD

#ifdef KEYBOARD_ergodox_infinity
#    undef BACKLIGHT_LEVELS
#    define BACKLIGHT_LEVELS  10

#    define EE_HANDS
#endif

#ifdef KEYBOARD_ergodox_ez
#    define QMK_KEYS_PER_SCAN  4
#endif
