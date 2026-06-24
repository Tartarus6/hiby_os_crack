#ifndef LV_CONF_H
#define LV_CONF_H

/* Set to 1 to enable configuration content */
#define LV_CONF_SKIP 0

/* Color depth: 16-bit (RGB565) is standard for many MIPS LCD screens */
#define LV_COLOR_DEPTH 16

/* Memory management: Use standard C library functions */
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* Enable built-in fonts we want to use in our UI */
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_28 1

/* Platform-specific driver settings */
#ifdef HOST_BUILD
  /* Host Simulation Settings (Arch Linux PC) */
  #define LV_USE_SDL 1
  #define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
  #define LV_USE_LINUX_FBDEV 0
  #define LV_USE_EVDEV 0
#else
  /* Target Build Settings (HiBy OS hardware) */
  #define LV_USE_SDL 0
  #define LV_USE_LINUX_FBDEV 1
  #define LV_USE_EVDEV 1
#endif

#endif /* LV_CONF_H */
