# Overview

![alt text](/img/img.png)

- This is a PlatformIO clock project based on the M5Paper device (https://github.com/m5stack/M5EPD).

- The base code used in this project is from M5-Clock (https://github.com/azw413/M5-Clock). Weather features using APIs were removed, leaving only the clock and built-in sensor information.

- Initially, the goal was to build a clock that could operate for several months on battery power alone. However, during development, current measurements showed that the time required to wake from sleep (~3100ms, average 101.6mA) was overwhelmingly longer than the time to read sensor values and refresh the screen (~800ms, average 165.4mA). This made optimization meaningless. To achieve one month of battery life, the screen refresh interval would need to be increased to over 5 minutes, which compromises practicality. As such, the project was finalized assuming continuous external power supply.

# Notes

- Since the final code assumes a constant power supply, it will not operate correctly on battery alone. If NTP is configured, the device works fine even without the internal battery. For battery operation, additional modifications are required—please refer to the comments in the code.

- After modifying the code for battery use, it is estimated that the device would last about 6 days when updating every minute with the default battery installed, though this has not been tested. To last for a year, a battery with a capacity of over 70,000mAh would be needed.

- To reduce screen flicker discomfort, the default update mode is set to UPDATE_MODE_DU. To remove ghosting, the screen refreshes in UPDATE_MODE_GC16 every hour.

- The original code’s TTF fonts were removed and replaced with large bitmap fonts. For optimization, only characters required for screen display are included.

# Fonts

- All fonts used in this project are free fonts.
  - Naver - Nanum Square Round, Extra Bold (https://hangeul.naver.com/font)
  - Hack (https://github.com/source-foundry/Hack)
  - GNU FreeFont - Mono Bold (https://github.com/opensourcedesign/fonts/tree/master/gnu-freefont_freemono)
