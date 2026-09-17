#include <assert.h>

#include "bsp_display_rounding.h"

int main(void)
{
    const int width = 240;
    const int height = 320;
    const int radius = 30;

    assert(bsp_display_pixel_outside_rounded_rect(0, 0, width, height, radius));
    assert(bsp_display_pixel_outside_rounded_rect(29, 0, width, height, radius));
    assert(!bsp_display_pixel_outside_rounded_rect(30, 0, width, height, radius));
    assert(!bsp_display_pixel_outside_rounded_rect(9, 9, width, height, radius));
    assert(bsp_display_pixel_outside_rounded_rect(8, 8, width, height, radius));

    assert(bsp_display_pixel_outside_rounded_rect(239, 0, width, height, radius));
    assert(!bsp_display_pixel_outside_rounded_rect(209, 0, width, height, radius));
    assert(bsp_display_pixel_outside_rounded_rect(0, 319, width, height, radius));
    assert(!bsp_display_pixel_outside_rounded_rect(30, 319, width, height, radius));
    assert(!bsp_display_pixel_outside_rounded_rect(120, 160, width, height, radius));

    assert(!bsp_display_pixel_outside_rounded_rect(0, 0, width, height, 0));
    assert(bsp_display_pixel_outside_rounded_rect(-1, 0, width, height, radius));
    // Every pixel preserves its corner classification under a quarter-turn.
    for (int y = 0; y < width; ++y) {
        for (int x = 0; x < height; ++x) {
            assert(bsp_display_pixel_outside_rounded_rect(x, y, height, width, radius) ==
                   bsp_display_pixel_outside_rounded_rect(y, height - 1 - x, width, height, radius));
        }
    }
    return 0;
}
