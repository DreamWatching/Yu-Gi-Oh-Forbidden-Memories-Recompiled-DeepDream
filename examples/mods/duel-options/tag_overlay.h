#ifndef TAG_OVERLAY_H
#define TAG_OVERLAY_H
#include "pc/mods/modapi.h"
/* Optional host extension resolved by name; never take another project's
 * numbered ABI slot. Required before any overlay callbacks are installed. */
typedef void (*TagDrawPixels)(const MemoriesModHost *, int, int, int, int,
                              const uint32_t *, int, int, unsigned, unsigned);
extern TagDrawPixels TagOverlay_DrawPixels;
#endif
