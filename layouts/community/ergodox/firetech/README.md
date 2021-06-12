Overview
========

This is my daily driver Ergodox configuration, which I use both at home (on an
Infinity Ergodox) and at work (on an Ergodox EZ).

Includes some extra fanciness:
* Keeps track of max measured WPM.
* Keys to output current and max WPM (and one to reset both).
* A visualizer.c for Ergodox Infinity built with `VISUALIZER_ENABLE` (the old
  default).
* An ST7565 renderer for Ergodox Infinity built with `ST7565_ENABLE` (current
  default).
* Custom indicator LED handling on other Ergodox variants than Infinity.

How to build
------------
`make ergodox_infinity:firetech`
or
`make ergodox_ez:firetech`

(Will probably work on other Ergodox variants, but I've only ever tested it on
Infinity and EZ.)

Layers
------
* Base: Quite basic QWERTY layout.
* Swap: All keys transparent except for Space (x2), which mirrors the keyboard
  while held (`SH_T(KC_SPC)`).
* Fn: Function layer. Contains all F keys, media keys, backlight control,
  numpad, and some WPM features (reset, type current, type max) and some other
  less commonly used keys.

LEDs
----
When built for an Ergodox other than Infinity, the indicator LEDs have the
following meaning:
1. (Red) Num Lock
2. (Green) Caps Lock
3. (Blue) Layer status
   * Off: Base layer only
   * Blinking slow: Fn
   * Blinking fast: Fn + Swap
   * On: Swap
