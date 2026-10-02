#ifndef MEMORIES_OVERLAY_PIXELS_H
#define MEMORIES_OVERLAY_PIXELS_H
#include "modapi.h"
/* Named extension, not a MemoriesModHost ABI member. Coordinates are canvas
 * pixels; only usable inside the owner's overlay callback. ARGB nearest
 * sampling, brightness/alpha 0..255, clipped to the active canvas. */
void ModMenu_DrawPixelsV1(const MemoriesModHost *, int x, int y, int w, int h,
                          const uint32_t *argb, int source_w, int source_h,
                          unsigned brightness, unsigned alpha);
#endif
