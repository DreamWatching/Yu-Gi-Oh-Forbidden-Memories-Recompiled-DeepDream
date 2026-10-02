/* Reconstruct private opening-contest artwork from the player's USA disc.
 * No pixels are compiled into the distributable. Archive coordinates follow
 * extract_images.py's replay of the retail GPU loaders. Built once per run. */
#include "clash_art.h"
#include <stdlib.h>
#include <string.h>

static unsigned int backs[10][6400], fronts[10][6400];
unsigned int clash_crossed_swords[6400];
const unsigned int *clash_backs[10], *clash_fronts[10];
static int ready;

static unsigned colour(unsigned w)
{
    unsigned r = w & 31, g = w >> 5 & 31, b = w >> 10 & 31;
    return w ? 0xff000000u | ((r << 3 | r >> 2) << 16) |
               ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2) : 0;
}

static int read_bytes(const MemoriesModHost *host, int wa, unsigned offset,
                      unsigned length, unsigned char *out)
{
    unsigned skip = offset & 2047, sectors = (skip + length + 2047) / 2048;
    unsigned char *raw = malloc(sectors * 2048);
    int ok;
    if (!raw) return 0;
    ok = host->disc_read(host, wa + offset / 2048, sectors, raw) == (int)sectors;
    if (ok) memcpy(out, raw + skip, length);
    free(raw);
    return ok;
}

/* Premultiplied bilinear sampling prevents dark fringes around rotated art. */
static unsigned sample(const unsigned *pixels, int width, int height, double x, double y)
{
    int ix = (int)x, iy = (int)y, dx, dy, c;
    double a = 0, channels[3] = {0, 0, 0};
    if (x < ix) ix--;
    if (y < iy) iy--;
    for (dy = 0; dy < 2; dy++) for (dx = 0; dx < 2; dx++) {
        unsigned pixel, alpha;
        double weight;
        int sx = ix + dx, sy = iy + dy;
        if (sx < 0 || sy < 0 || sx >= width || sy >= height) continue;
        pixel = pixels[sy * width + sx]; alpha = pixel >> 24;
        weight = (dx ? x - ix : 1 - (x - ix)) * (dy ? y - iy : 1 - (y - iy));
        a += alpha * weight;
        for (c = 0; c < 3; c++) channels[c] += (pixel >> (c * 8) & 255) * alpha * weight;
    }
    if (a < .5) return 0;
    return (unsigned)(a + .5) << 24 | (unsigned)(channels[2] / a + .5) << 16 |
           (unsigned)(channels[1] / a + .5) << 8 | (unsigned)(channels[0] / a + .5);
}

static unsigned over(unsigned bottom, unsigned top)
{
    unsigned a = top >> 24, b = bottom >> 24, out = a * 255 + b * (255 - a), rgb = 0;
    int c;
    if (!out) return 0;
    for (c = 0; c < 3; c++) {
        unsigned v = ((top >> (c * 8) & 255) * a * 255 +
                      (bottom >> (c * 8) & 255) * b * (255 - a) + out / 2) / out;
        rgb |= v << (c * 8);
    }
    return ((out + 127) / 255) << 24 | rgb;
}

int ClashArt_Load(const MemoriesModHost *host)
{
    static const double cosine[10] = {1,.809016994,.309016994,-.309016994,-.809016994,-1,-.809016994,-.309016994,.309016994,.809016994};
    static const double sine[10] = {0,.587785252,.951056516,.951056516,.587785252,0,-.587785252,-.951056516,-.951056516,-.587785252};
    static const unsigned char star_xy[10][2] = {{128,48},{144,48},{160,48},{176,48},{192,48},{208,48},{224,48},{240,48},{128,64},{144,64}};
    unsigned char *raw;
    unsigned back[6400] = {0}, sword[32 * 128];
    int wa, i, x, y, blade;
    if (ready) return 1;
    wa = host->disc_file_start(host, "\\DATA\\WA_MRG.MRG;1");
    if (wa <= 0) return 0;
    /* Covers the guardian sheet, card column, both CLUTs and full sword. */
    raw = malloc(0x51000);
    if (!raw) return 0;
    if (!read_bytes(host, wa, 0xb50000, 0x51000, raw)) { free(raw); return 0; }
    for (y = 0; y < 60; y++) for (x = 0; x < 48; x++) {
        unsigned index = raw[0x23000 + (y + 130) * 128 + x + 58];
        unsigned at = 0x33600 + index * 2;
        back[(y + 10) * 80 + x + 16] = colour(raw[at] | raw[at + 1] << 8);
    }
    for (i = 0; i < 10; i++) {
        for (y = 0; y < 80; y++) for (x = 0; x < 80; x++) {
            double px = x - 39.5, py = y - 39.5;
            backs[i][y * 80 + x] = sample(back, 80, 80,
                cosine[i] * px + sine[i] * py + 39.5,
                -sine[i] * px + cosine[i] * py + 39.5);
        }
        memset(fronts[i], 0, sizeof(fronts[i]));
        for (y = 0; y < 60; y++) for (x = 0; x < 48; x++) {
            unsigned index = raw[0x23000 + (y + 130) * 128 + x + 2];
            unsigned at = 0x33c00 + index * 2;
            fronts[i][(y + 10) * 80 + x + 16] = colour(raw[at] | raw[at + 1] << 8);
        }
        for (y = 0; y < 30; y++) for (x = 0; x < 30; x++) {
            unsigned sx = star_xy[i][0] + x * 16 / 30, sy = star_xy[i][1] + y * 16 / 30;
            unsigned index = raw[sy * 128 + sx / 2] >> ((sx & 1) * 4) & 15;
            unsigned at = 0x10500 + index * 2, pos = (y + 14) * 80 + x + 25;
            fronts[i][pos] = over(fronts[i][pos], colour(raw[at] | raw[at + 1] << 8));
        }
        clash_backs[i] = backs[i]; clash_fronts[i] = fronts[i];
    }
    /* Whole first blade including its tip; both crossed blades use this art. */
    for (y = 0; y < 128; y++) for (x = 0; x < 32; x++) {
        unsigned index = raw[0x49000 + (y + 128) * 128 + x];
        unsigned at = 0x48000 + index * 2;
        sword[y * 32 + x] = colour(raw[at] | raw[at + 1] << 8);
    }
    memset(clash_crossed_swords, 0, sizeof(clash_crossed_swords));
    for (blade = 0; blade < 2; blade++) for (y = 0; y < 80; y++) for (x = 0; x < 80; x++) {
        double s = blade ? .743144825 : -.743144825, c = .669130606;
        double px = x - 39.5, py = y - 39.5;
        double sx = (c * px + s * py + 8) * 32 / 17.0;
        double sy = (-s * px + c * py + 37.5) * 128 / 76.0;
        unsigned *pixel = &clash_crossed_swords[y * 80 + x];
        *pixel = over(*pixel, sample(sword, 32, 128, sx, sy));
    }
    free(raw); ready = 1;
    return 1;
}
