/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The title archive is a/0/2/6 (ARCID_TITLE). Member 0 is the version's
 * 8bpp tiles, member 2 its palette, member 1 the logo screen and member 3
 * the layer behind it. Bytes come from the staged NitroFS, never from git. */
#include "title_pic.h"

#ifndef TITLE_PIC_HOST
#include "nitro/fs.h"
#endif

#include <stdlib.h>
#include <string.h>

#define PIC_W 256
#define PIC_H 192
#define PIC_PIXELS (PIC_W * PIC_H)

static uint16_t s_top[PIC_PIXELS];
static uint16_t s_bottom[PIC_PIXELS];
static int s_ready;

bool TitlePic_Ready(void) {
    return s_ready != 0;
}

const uint16_t *TitlePic_Top(void) {
    return s_top;
}

const uint16_t *TitlePic_Bottom(void) {
    return s_bottom;
}

static uint16_t rgb565(uint16_t color) {
    uint32_t r = color & 31u;
    uint32_t g = (color >> 5) & 31u;
    uint32_t b = (color >> 10) & 31u;
    return (uint16_t)((r << 11) | (g << 6) | b);
}

static uint8_t *lz_decompress(const uint8_t *src, uint32_t src_len, uint32_t *out_len) {
    uint32_t size;
    uint32_t cursor;
    uint8_t kind;
    uint8_t *out;
    if (!src || src_len < 4 || (src[0] != 0x10 && src[0] != 0x11)) {
        return NULL;
    }
    kind = src[0];
    size = (uint32_t)src[1] | ((uint32_t)src[2] << 8) | ((uint32_t)src[3] << 16);
    cursor = 4;
    if (size == 0) {
        if (src_len < 8) {
            return NULL;
        }
        size = (uint32_t)src[4] | ((uint32_t)src[5] << 8) | ((uint32_t)src[6] << 16) | ((uint32_t)src[7] << 24);
        cursor = 8;
    }
    out = malloc(size ? size : 1);
    if (!out) {
        return NULL;
    }
    *out_len = 0;
    while (*out_len < size && cursor < src_len) {
        uint8_t flags = src[cursor++];
        int bit;
        for (bit = 7; bit >= 0 && *out_len < size; bit--) {
            if ((flags & (1u << bit)) == 0) {
                if (cursor >= src_len) {
                    free(out);
                    return NULL;
                }
                out[(*out_len)++] = src[cursor++];
                continue;
            }
            if (cursor + 1 >= src_len) {
                free(out);
                return NULL;
            }
            {
                uint8_t first = src[cursor++];
                uint8_t second = src[cursor++];
                uint32_t length;
                uint32_t distance;
                uint32_t i;
                if (kind == 0x10) {
                    length = (first >> 4) + 3;
                    distance = (((uint32_t)(first & 15) << 8) | second) + 1;
                } else if ((first >> 4) == 0) {
                    if (cursor >= src_len) {
                        free(out);
                        return NULL;
                    }
                    length = (((uint32_t)(first & 15) << 4) | (second >> 4)) + 0x11;
                    distance = (((uint32_t)(second & 15) << 8) | src[cursor++]) + 1;
                } else if ((first >> 4) == 1) {
                    if (cursor + 1 >= src_len) {
                        free(out);
                        return NULL;
                    }
                    length = (((uint32_t)(first & 15) << 12) | ((uint32_t)second << 4) | (src[cursor] >> 4)) + 0x111;
                    distance = (((uint32_t)(src[cursor] & 15) << 8) | src[cursor + 1]) + 1;
                    cursor += 2;
                } else {
                    length = (first >> 4) + 1;
                    distance = (((uint32_t)(first & 15) << 8) | second) + 1;
                }
                if (distance == 0 || distance > *out_len) {
                    free(out);
                    return NULL;
                }
                for (i = 0; i < length && *out_len < size; i++) {
                    out[*out_len] = out[*out_len - distance];
                    (*out_len)++;
                }
            }
        }
    }
    if (*out_len != size) {
        free(out);
        return NULL;
    }
    return out;
}

static uint8_t *inflate_member(const uint8_t *src, uint32_t len, uint32_t *out_len) {
    uint8_t *plain;
    if (src && len >= 4 && (src[0] == 0x10 || src[0] == 0x11)) {
        plain = lz_decompress(src, len, out_len);
        if (plain) {
            return plain;
        }
    }
    plain = malloc(len ? len : 1);
    if (!plain) {
        return NULL;
    }
    if (len) {
        memcpy(plain, src, len);
    }
    *out_len = len;
    return plain;
}

static int narc_member(const uint8_t *narc, uint32_t narc_len, int index, const uint8_t **data, uint32_t *len) {
    uint32_t header;
    uint32_t fat;
    uint32_t count;
    uint32_t start;
    uint32_t end;
    uint32_t fnt;
    uint32_t image;
    if (!narc || narc_len < 16 || memcmp(narc, "NARC", 4) != 0) {
        return 0;
    }
    header = (uint32_t)narc[12] | ((uint32_t)narc[13] << 8);
    if (header + 12 > narc_len || memcmp(narc + header, "BTAF", 4) != 0) {
        return 0;
    }
    fat = (uint32_t)narc[header + 4] | ((uint32_t)narc[header + 5] << 8) | ((uint32_t)narc[header + 6] << 16) |
          ((uint32_t)narc[header + 7] << 24);
    count = (uint32_t)narc[header + 8] | ((uint32_t)narc[header + 9] << 8);
    if ((uint32_t)index >= count || header + 12 + 8u * (uint32_t)index + 8 > narc_len) {
        return 0;
    }
    start = (uint32_t)narc[header + 12 + 8 * index] | ((uint32_t)narc[header + 13 + 8 * index] << 8) |
            ((uint32_t)narc[header + 14 + 8 * index] << 16) | ((uint32_t)narc[header + 15 + 8 * index] << 24);
    end = (uint32_t)narc[header + 16 + 8 * index] | ((uint32_t)narc[header + 17 + 8 * index] << 8) |
          ((uint32_t)narc[header + 18 + 8 * index] << 16) | ((uint32_t)narc[header + 19 + 8 * index] << 24);
    fnt = header + fat;
    if (fnt + 8 > narc_len || memcmp(narc + fnt, "BTNF", 4) != 0) {
        return 0;
    }
    image = fnt + ((uint32_t)narc[fnt + 4] | ((uint32_t)narc[fnt + 5] << 8) | ((uint32_t)narc[fnt + 6] << 16) |
                   ((uint32_t)narc[fnt + 7] << 24));
    if (image + 8 > narc_len || memcmp(narc + image, "GMIF", 4) != 0) {
        return 0;
    }
    image += 8;
    if (end < start || image + end > narc_len) {
        return 0;
    }
    *data = narc + image + start;
    *len = end - start;
    return 1;
}

static int read_palette(const uint8_t *raw, uint32_t len, uint16_t *colors, int max_colors) {
    uint32_t size;
    uint32_t i;
    if (len < 40 || memcmp(raw, "RLCN", 4) != 0 || memcmp(raw + 16, "TTLP", 4) != 0) {
        return 0;
    }
    size = (uint32_t)raw[32] | ((uint32_t)raw[33] << 8) | ((uint32_t)raw[34] << 16) | ((uint32_t)raw[35] << 24);
    if (size % 2 || 40 + size > len) {
        return 0;
    }
    if ((int)(size / 2) < max_colors) {
        max_colors = (int)(size / 2);
    }
    for (i = 0; i < (uint32_t)max_colors; i++) {
        colors[i] = (uint16_t)raw[40 + 2 * i] | ((uint16_t)raw[41 + 2 * i] << 8);
    }
    for (; i < 256; i++) {
        colors[i] = 0;
    }
    return max_colors;
}

static int read_chars(const uint8_t *raw, uint32_t len, int *bits, int *tile_w, int *tile_h, const uint8_t **chars,
                      uint32_t *chars_len) {
    uint32_t depth;
    uint32_t size;
    uint32_t offset;
    uint32_t height;
    uint32_t width;
    if (len < 48 || memcmp(raw, "RGCN", 4) != 0 || memcmp(raw + 16, "RAHC", 4) != 0) {
        return 0;
    }
    height = (uint32_t)raw[24] | ((uint32_t)raw[25] << 8);
    width = (uint32_t)raw[26] | ((uint32_t)raw[27] << 8);
    depth = (uint32_t)raw[28] | ((uint32_t)raw[29] << 8) | ((uint32_t)raw[30] << 16) | ((uint32_t)raw[31] << 24);
    size = (uint32_t)raw[40] | ((uint32_t)raw[41] << 8) | ((uint32_t)raw[42] << 16) | ((uint32_t)raw[43] << 24);
    offset = (uint32_t)raw[44] | ((uint32_t)raw[45] << 8) | ((uint32_t)raw[46] << 16) | ((uint32_t)raw[47] << 24);
    if (depth != 3 && depth != 4) {
        return 0;
    }
    if (24 + offset + size > len) {
        return 0;
    }
    *bits = depth == 3 ? 4 : 8;
    *tile_w = width == 0xffff ? 0 : (int)width;
    *tile_h = height == 0xffff ? 0 : (int)height;
    *chars = raw + 24 + offset;
    *chars_len = size;
    return 1;
}

static int read_screen(const uint8_t *raw, uint32_t len, int *width, int *height, int *eight, const uint8_t **map,
                       uint32_t *map_len) {
    uint32_t data_size;
    if (len < 36 || memcmp(raw, "RCSN", 4) != 0 || memcmp(raw + 16, "NRCS", 4) != 0) {
        return 0;
    }
    *width = (int)((uint32_t)raw[24] | ((uint32_t)raw[25] << 8));
    *height = (int)((uint32_t)raw[26] | ((uint32_t)raw[27] << 8));
    *eight = raw[28] != 0;
    data_size = (uint32_t)raw[32] | ((uint32_t)raw[33] << 8) | ((uint32_t)raw[34] << 16) | ((uint32_t)raw[35] << 24);
    if (36 + data_size > len || *width <= 0 || *height <= 0) {
        return 0;
    }
    *map = raw + 36;
    *map_len = data_size;
    return 1;
}

/* transparent: tile 0 and palette index 0 are holes, as on a DS background. */
static void paint(uint16_t *dst, const uint8_t *chars, uint32_t chars_len, int bits, const uint16_t *pal,
                  const uint8_t *map, uint32_t map_len, int map_w, int map_h, int clear, int transparent) {
    int y;
    int x;
    int tiles_across = map_w / 8;
    if (clear) {
        memset(dst, 0, PIC_PIXELS * sizeof *dst);
    }
    if (tiles_across <= 0 || map_h < 8) {
        return;
    }
    for (y = 0; y < PIC_H && y < map_h; y++) {
        for (x = 0; x < PIC_W && x < map_w; x++) {
            uint32_t entry_i = ((uint32_t)(y / 8) * (uint32_t)tiles_across) + (uint32_t)(x / 8);
            uint16_t entry;
            int tile;
            int hflip;
            int vflip;
            int bank;
            int px = x & 7;
            int py = y & 7;
            int index;
            if ((entry_i + 1) * 2 > map_len) {
                continue;
            }
            entry = (uint16_t)map[entry_i * 2] | ((uint16_t)map[entry_i * 2 + 1] << 8);
            tile = entry & 0x3ff;
            if (transparent && tile == 0) {
                continue;
            }
            hflip = (entry >> 10) & 1;
            vflip = (entry >> 11) & 1;
            bank = (entry >> 12) & 0xf;
            if (hflip) {
                px = 7 - px;
            }
            if (vflip) {
                py = 7 - py;
            }
            if (bits == 8) {
                uint32_t byte_i = (uint32_t)tile * 64u + (uint32_t)(py * 8 + px);
                if (byte_i >= chars_len) {
                    continue;
                }
                index = chars[byte_i];
            } else {
                uint32_t byte_i = (uint32_t)tile * 32u + (uint32_t)(py * 4 + (px / 2));
                if (byte_i >= chars_len) {
                    continue;
                }
                index = (px & 1) ? (chars[byte_i] >> 4) : (chars[byte_i] & 15);
                index += bank * 16;
            }
            if (transparent && (index & (bits == 8 ? 255 : 15)) == 0) {
                continue;
            }
            dst[(uint32_t)y * PIC_W + (uint32_t)x] = rgb565(pal[index & 255]);
        }
    }
}

static const char *s_note = "not loaded";

const char *TitlePic_Note(void) {
    return s_note;
}

static int compose(const uint8_t *narc, uint32_t narc_len, int char_i, int map_i, int pal_i, uint16_t *dst, int clear,
                   int transparent) {
    const uint8_t *raw_c;
    const uint8_t *raw_m;
    const uint8_t *raw_p;
    uint32_t len_c;
    uint32_t len_m;
    uint32_t len_p;
    uint8_t *cmem = NULL;
    uint8_t *mmem = NULL;
    uint8_t *pmem = NULL;
    uint32_t clen;
    uint32_t mlen;
    uint32_t plen;
    uint16_t pal[256];
    int bits;
    int tile_w;
    int tile_h;
    const uint8_t *chars;
    uint32_t chars_len;
    int map_w;
    int map_h;
    int eight;
    const uint8_t *map;
    uint32_t map_len;
    int ok = 0;
    if (!narc_member(narc, narc_len, char_i, &raw_c, &len_c) || !narc_member(narc, narc_len, map_i, &raw_m, &len_m) ||
        !narc_member(narc, narc_len, pal_i, &raw_p, &len_p)) {
        return 0;
    }
    cmem = inflate_member(raw_c, len_c, &clen);
    mmem = inflate_member(raw_m, len_m, &mlen);
    pmem = inflate_member(raw_p, len_p, &plen);
    if (cmem && mmem && pmem && read_palette(pmem, plen, pal, 256) &&
        read_chars(cmem, clen, &bits, &tile_w, &tile_h, &chars, &chars_len) &&
        read_screen(mmem, mlen, &map_w, &map_h, &eight, &map, &map_len)) {
        (void)tile_w;
        (void)tile_h;
        (void)eight;
        paint(dst, chars, chars_len, bits, pal, map, map_len, map_w, map_h, clear, transparent);
        ok = 1;
    }
    free(cmem);
    free(mmem);
    free(pmem);
    return ok;
}

/* Top is sub BG 2 (member 3, the title background) with sub BG 3 (member 1,
 * the version logo) over it. Bottom is main BG 3 (members 9, 10 and 8). */
bool TitlePic_FromArchive(const uint8_t *narc, uint32_t length) {
    int background;
    int logo;
    int lower;
    s_ready = 0;
    s_note = "a/0/2/6 did not decode";
    if (!narc || length < 16) {
        return false;
    }
    background = compose(narc, length, 0, 3, 2, s_top, 1, 0);
    logo = compose(narc, length, 0, 1, 2, s_top, background ? 0 : 1, 1);
    lower = compose(narc, length, 9, 10, 8, s_bottom, 1, 0);
    if (!logo) {
        return false;
    }
    s_ready = 1;
    if (background && lower) {
        s_note = "top is the logo over the title background; bottom is the lower title screen";
    } else if (background) {
        s_note = "top is the logo over the title background; lower title screen did not decode";
    } else if (lower) {
        s_note = "top is the logo; title background did not decode; bottom is the lower title screen";
    } else {
        s_note = "top is the logo; title background and lower title screen did not decode";
    }
    return true;
}

#ifndef TITLE_PIC_HOST
bool TitlePic_Load(void) {
    FSFile file;
    u32 length;
    uint8_t *narc;
    s32 got;
    int ok;
    s_ready = 0;
    s_note = "a/0/2/6 not open";
    if (!FS_OpenFile(&file, "a/0/2/6")) {
        return false;
    }
    length = FS_GetLength(&file);
    if (length < 16 || length > 8 * 1024 * 1024) {
        FS_CloseFile(&file);
        return false;
    }
    narc = malloc(length);
    if (!narc) {
        FS_CloseFile(&file);
        return false;
    }
    got = FS_ReadFile(&file, narc, (s32)length);
    FS_CloseFile(&file);
    if (got != (s32)length) {
        free(narc);
        s_note = "a/0/2/6 short read";
        return false;
    }
    ok = TitlePic_FromArchive(narc, length);
    free(narc);
    return ok;
}
#endif

#ifdef TITLE_PIC_HOST
#include <stdio.h>

static int expect_lz(void) {
    /* ABC, then a length-6 copy from three back, then X. */
    static const uint8_t stream[] = {0x10, 0x0a, 0x00, 0x00, 0x10, 'A', 'B', 'C', 0x30, 0x02, 'X'};
    uint32_t len = 0;
    uint8_t *out = lz_decompress(stream, sizeof stream, &len);
    int ok = out && len == 10 && memcmp(out, "ABCABCABCX", 10) == 0;
    free(out);
    return ok;
}

int main(int argc, char **argv) {
    FILE *fp;
    long length;
    uint8_t *narc;
    FILE *top;
    FILE *bottom;
    if (!expect_lz()) {
        fprintf(stderr, "lz10 back-reference failed\n");
        return 1;
    }
    if (argc != 4) {
        fprintf(stderr, "usage: title_pic_host ARCHIVE TOP.rgb565 BOTTOM.rgb565\n");
        return 2;
    }
    fp = fopen(argv[1], "rb");
    if (!fp) {
        perror(argv[1]);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    narc = malloc(length > 0 ? (size_t)length : 1);
    if (!narc || fread(narc, 1, (size_t)length, fp) != (size_t)length) {
        fprintf(stderr, "short read\n");
        return 1;
    }
    fclose(fp);
    if (!TitlePic_FromArchive(narc, (uint32_t)length)) {
        fprintf(stderr, "decode failed: %s\n", TitlePic_Note());
        return 1;
    }
    fprintf(stderr, "%s\n", TitlePic_Note());
    top = fopen(argv[2], "wb");
    bottom = fopen(argv[3], "wb");
    if (!top || !bottom) {
        fprintf(stderr, "open output failed\n");
        return 1;
    }
    fwrite(TitlePic_Top(), 2, 256 * 192, top);
    fwrite(TitlePic_Bottom(), 2, 256 * 192, bottom);
    fclose(top);
    fclose(bottom);
    free(narc);
    return 0;
}
#endif
