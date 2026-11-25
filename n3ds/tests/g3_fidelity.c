/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ds2d_compose.h"
#include "g3_intercept.h"
#include "g3_pica.h"
#include "nitro/gx.h"
#include "vram_dma.h"
#include "vram_host.h"
#include <stdio.h>
#include <string.h>

extern int archiveMountSdmc(void);
extern void svcSleepThread(s64 ns);
u8 pbw2_io[0x110000] ATTRIBUTE_ALIGN(32);
static u16 pixels[256 * 192];
void Log_Line(const char *tag, const char *fmt, ...) {
    (void)tag;
    (void)fmt;
}

static void cmd(u32 op, u32 param) {
    PbW2_G3Command(op, &param);
}
static void vertex(s16 x, s16 y, s16 z) {
    const u32 p[] = { (u16)x | ((u32)(u16)y << 16), (u16)z };
    PbW2_G3Command(0x23, p);
}
static void triangle(u16 color, s16 depth) {
    cmd(0x20, color);
    cmd(G3OP_BEGIN, GX_BEGIN_TRIANGLES);
    vertex(-4096, -2048, depth);
    vertex(2048, -2048, depth);
    vertex(0, 2048, depth);
    cmd(G3OP_END, 0);
}

static const char *checks(void) {
    memset(pbw2_io, 0, sizeof pbw2_io);
    memset(pixels, 0, sizeof pixels);
    unsigned center = 96 * 256 + 128;
    PbW2_VramHostInit();
    PbW2_G3InterceptInit();
    const u32 box[] = { 0xFC00FC00u, 0x0800FC00u, 0x08000800u };
    const u32 outside[] = { 0xFC002000u, 0x0800FC00u, 0x08000800u };
    const u32 intersects[] = { 0xFC000C00u, 0x0800FC00u, 0x08000800u };
    PbW2_G3Command(G3OP_BOX_TEST, box);
    if (!(reg_G3X_GXSTAT & REG_G3X_GXSTAT_TR_MASK))
        return "visible box";
    PbW2_G3Command(G3OP_BOX_TEST, outside);
    if (reg_G3X_GXSTAT & REG_G3X_GXSTAT_TR_MASK)
        return "culled box";
    G3_MtxMode(GX_MTXMODE_POSITION);
    G3_Translate(FX32_ONE, 0, 0);
    PbW2_G3Command(G3OP_BOX_TEST, intersects);
    if (reg_G3X_GXSTAT & REG_G3X_GXSTAT_TR_MASK)
        return "translated box";
    G3_Identity();
    PbW2_G3Command(G3OP_BOX_TEST, intersects);
    if (!(reg_G3X_GXSTAT & REG_G3X_GXSTAT_TR_MASK))
        return "restored matrix";
    reg_G3X_CLEAR_DEPTH = 0x7fff;
    triangle(0x03e0, -2048);
    triangle(0x001f, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x07c0 || pixels[0])
        return "clipping or depth";
    reg_GX_VRAMCNT_A = GX_VRAMCNT(3, 0);
    reg_GX_VRAMCNT_E = GX_VRAMCNT(3, 0);
    memset(PbW2_VramMap((void *)HW_LCDC_VRAM_A), 0x11, 32);
    u16 *palette = PbW2_VramMap((void *)HW_LCDC_VRAM_E);
    palette[1] = 0x7c00;
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_PLTT16 << 26);
    cmd(0x2b, 0);
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    reg_GX_VRAMCNT_D = GX_VRAMCNT_LCDC;
    reg_GX_DISPCNT = (GX_DISPMODE_VRAM_D << REG_GX_DISPCNT_MODE_SHIFT) | 0x8108;
    PBW2_VU16(0x04000304) = 0x8000;
    reg_GX_DISPCAPCNT = (1u << 31) | (3u << 20) | (3u << 16);
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[center] != 0x001f || (reg_GX_DISPCAPCNT & (1u << 31)))
        return "composed capture";
    u16 *bankD = PbW2_VramMap((void *)HW_LCDC_VRAM_D);
    if (bankD[center] != 0xfc00)
        return "VRAM capture color";
    const u32 capture = (1u << 31) | (1u << 24) | (3u << 20) | (3u << 16);
    reg_GX_DISPCAPCNT = capture;
    PbW2_ComposeScreens(pixels, NULL);
    if (bankD[0] != 0 || bankD[center] != 0xfc00)
        return "3D capture validity";
    bankD[0] = 0x83e0;
    bankD[center] = 0x001f;
    reg_GX_DISPCAPCNT = capture | (2u << 29) | (16u << 8) | 16u;
    PbW2_ComposeScreens(pixels, NULL);
    if (bankD[0] != 0x83e0 || bankD[center] != 0xfc00)
        return "capture blend validity";

    u8 *tiles = PbW2_VramMap((void *)HW_LCDC_VRAM_A);
    palette[1] = 0x001f;
    memset(tiles, (31 << 3) | 1, 64);
    tiles[7] = (15 << 3) | 1;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 64);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_E, 4);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_A5I3 << 26);
    triangle(0x7fff, -2048);
    cmd(G3OP_TEXIMAGE_PARAM, 0);
    triangle(0x03e0, 0);
    cmd(0x50, 0);
    memset(pixels, 0, sizeof pixels);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0xf800)
        return "opaque texel depth";

    memset(tiles, (15 << 3) | 1, 64);
    tiles[7] = (31 << 3) | 1;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 64);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_A5I3 << 26);
    triangle(0x7fff, -2048);
    cmd(G3OP_TEXIMAGE_PARAM, 0);
    triangle(0x03e0, 0);
    cmd(0x50, 0);
    memset(pixels, 0, sizeof pixels);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x07c0)
        return "fractional texel depth";

    memset(tiles, 0x11, 32);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 32);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_PLTT16 << 26);
    triangle(0x7fff, -2048);
    const u16 newPalette[] = { 0, 0x03e0 };
    if (!PbW2_TryDmaCopy32(newPalette, palette, sizeof newPalette))
        return "LCDC DMA";
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    memset(pixels, 0, sizeof pixels);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0xf800)
        return "palette rewrite during submission";

    /* Sample past the right edge of an asymmetric texture with mirror on. */
    memset(tiles, 0x11, 32);
    tiles[3] = 0x21;
    palette[2] = 0x7c00;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 32);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_E, 6);
    const u32 pltt16 = GX_TEXFMT_PLTT16 << 26;
    cmd(0x22, 8 * 16 + 8);
    cmd(G3OP_TEXIMAGE_PARAM, pltt16 | (1u << 16) | (1u << 18));
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    memset(pixels, 0, sizeof pixels);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x001f)
        return "S mirror";

    cmd(G3OP_TEXIMAGE_PARAM, pltt16 | (1u << 16));
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    memset(pixels, 0, sizeof pixels);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x07c0)
        return "S repeat";

    /* Check both axes and a negative coordinate against the same VRAM data. */
    tiles[28] = 0x12;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 32);
    cmd(0x22, (136u << 16) | 8u);
    cmd(G3OP_TEXIMAGE_PARAM, pltt16 | (1u << 17) | (1u << 19));
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x001f)
        return "T mirror";

    cmd(0x22, 0xfff8);
    cmd(G3OP_TEXIMAGE_PARAM, pltt16 | (1u << 16));
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x001f)
        return "negative S repeat";

    cmd(G3OP_TEXIMAGE_PARAM, pltt16 | (1u << 16) | (1u << 18));
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x07c0)
        return "negative S mirror";

    /* Only S=1, T=1 samples blue. Change the texture matrix between
     * issuing TEXCOORD or NORMAL and drawing the triangle. */
    memset(tiles, 0x11, 32);
    tiles[4] = 0x21;
    palette[1] = 0x03e0;
    palette[2] = 0x7c00;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 32);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_E, 6);
    G3_MtxMode(GX_MTXMODE_TEXTURE);
    G3_Identity();
    G3_Translate(16 * FX32_ONE, 16 * FX32_ONE, 0);
    cmd(G3OP_TEXIMAGE_PARAM, pltt16 | ((u32)GX_TEXGEN_TEXCOORD << 30));
    cmd(G3OP_TEXCOORD, 0);
    G3_Identity();
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x001f)
        return "TEXCOORD latches texture matrix";

    u32 matrix[16] = {0};
    matrix[0] = matrix[1] = (u32)(-32 * FX32_ONE);
    PbW2_G3Command(G3OP_MTX_LOAD_4x4, matrix);
    cmd(G3OP_TEXIMAGE_PARAM, pltt16 | ((u32)GX_TEXGEN_NORMAL << 30));
    cmd(G3OP_TEXCOORD, 0);
    cmd(G3OP_NORMAL, 0x300); /* x = -0.5 */
    G3_Identity();
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x001f)
        return "NORMAL latches generated coordinates";

    cmd(G3OP_TEXIMAGE_PARAM, pltt16 | ((u32)GX_TEXGEN_VERTEX << 30));
    cmd(G3OP_TEXCOORD, 0);
    matrix[0] = matrix[1] = 0;
    matrix[8] = matrix[9] = (u32)(-32 * FX32_ONE);
    PbW2_G3Command(G3OP_MTX_LOAD_4x4, matrix);
    triangle(0x7fff, -2048);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x001f)
        return "vertex generates texture coordinates at submission";
    G3_Identity();
    G3_MtxMode(GX_MTXMODE_POSITION);

    /* Change a 4x4 block descriptor while reusing its texel indices. */
    reg_GX_VRAMCNT_B = GX_VRAMCNT(3, 1);
    u16 *descriptors = PbW2_VramMap((void *)HW_LCDC_VRAM_B);
    memset(tiles, 0xe4, 16);
    palette[3] = 0x7c00;
    descriptors[0] = 0;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 16);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_B, 2);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_E, 8);
    cmd(0x22, 56);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_COMP4x4 << 26);
    triangle(0x7fff, -2048);
    cmd(G3OP_TEXIMAGE_PARAM, 0);
    triangle(0x03e0, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x07c0)
        return "4x4 transparent descriptor";

    descriptors[0] = 2u << 14;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_B, 2);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_COMP4x4 << 26);
    triangle(0x7fff, -2048);
    cmd(G3OP_TEXIMAGE_PARAM, 0);
    triangle(0x03e0, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x001f)
        return "4x4 opaque descriptor";

    const u32 opaque = (31u << 16) | (3u << 6);
    const u32 decal = opaque | (GX_POLYGONMODE_DECAL << 4);
    cmd(0x22, 0);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_A5I3 << 26);
    palette[1] = 0x7c00;
    memset(tiles, 1, 64);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_E, 4);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 64);
    cmd(G3OP_POLYGON_ATTR, decal);
    triangle(0x001f, -2048);
    cmd(G3OP_POLYGON_ATTR, opaque);
    cmd(G3OP_TEXIMAGE_PARAM, 0);
    triangle(0x03e0, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0xf800)
        return "decal zero texel alpha";

    memset(tiles, (31 << 3) | 1, 64);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 64);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_A5I3 << 26);
    triangle(0x001f, 0);
    cmd(G3OP_POLYGON_ATTR, decal);
    triangle(0x001f, -2048);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x001f)
        return "decal full texel alpha";

    memset(tiles, (15 << 3) | 1, 64);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 64);
    triangle(0x001f, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != ((15u << 11) | 14u))
        return "decal fractional texel alpha";

    const u32 translucent = (15u << 16) | (3u << 6);
    reg_G3X_CLEAR_COLOR = 31u << 16;
    cmd(G3OP_TEXIMAGE_PARAM, 0);
    cmd(G3OP_POLYGON_ATTR, translucent);
    triangle(0x001f, 0);
    triangle(0x03e0, -2048);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != (15u << 11))
        return "translucent polygons with the same ID";

    triangle(0x001f, 0);
    cmd(G3OP_POLYGON_ATTR, translucent | (63u << 24));
    triangle(0x03e0, -2048);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if ((pixels[center] & 0x07ff) != (15u << 6) || !(pixels[center] & 0xf800))
        return "translucent polygons with different IDs";

    cmd(G3OP_POLYGON_ATTR, translucent);
    triangle(0x001f, 0);
    cmd(G3OP_POLYGON_ATTR, opaque);
    triangle(0x7c00, -1024);
    cmd(G3OP_POLYGON_ATTR, translucent);
    triangle(0x001f, -2048);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if ((pixels[center] & 0xffe0) != (15u << 11) || !(pixels[center] & 31))
        return "opaque pixel resets translucent ID";

    memset(tiles, 1, 64);
    tiles[1] = (15 << 3) | 1;
    tiles[2] = (31 << 3) | 1;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 64);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_A5I3 << 26);
    cmd(G3OP_POLYGON_ATTR, opaque);
    triangle(0x7fff, 0);
    cmd(G3OP_TEXIMAGE_PARAM, 0);
    cmd(G3OP_POLYGON_ATTR, translucent);
    triangle(0x001f, -2048);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != (15u << 11))
        return "zero-alpha texel leaves translucent ID alone";

    cmd(G3OP_TEXCOORD, 16);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_A5I3 << 26);
    cmd(G3OP_POLYGON_ATTR, opaque | (1u << 11));
    triangle(0x7fff, -2048);
    cmd(G3OP_TEXIMAGE_PARAM, 0);
    cmd(G3OP_POLYGON_ATTR, opaque);
    triangle(0x03e0, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 15)
        return "fractional texel writes depth when requested";

    /* A 256x128 capture at 0x18000 wraps inside its 128 KiB bank. */
    reg_G3X_CLEAR_COLOR = 0x001f | (31u << 16);
    cmd(0x50, 0);
    bankD[0x4000] = 0x83e0;
    bankD[0xbfff] = 0x83e0;
    reg_GX_DISPCAPCNT = (1u << 31) | (1u << 24) | (2u << 20) | (3u << 18) | (3u << 16);
    PbW2_ComposeScreens(pixels, NULL);
    if (bankD[0] != 0x801f || bankD[0x3fff] != 0x801f || bankD[0xc000] != 0x801f ||
        bankD[0xffff] != 0x801f || bankD[0x4000] != 0x83e0 || bankD[0xbfff] != 0x83e0)
        return "capture write wrap";
    return NULL;
}
/* The mid-frame palette write and fractional decal use the CPU. */
extern bool __real_PbW2_PicaFinish(uint32_t clear, float depth, uint16_t *pixels);
static unsigned s_gpuFrames;

bool __wrap_PbW2_PicaFinish(uint32_t clear, float depth, uint16_t *pixels) {
    bool complete = __real_PbW2_PicaFinish(clear, depth, pixels);
    if (complete)
        ++s_gpuFrames;
    return complete;
}

extern void gfxInitDefault(void);
int main(void) {
    gfxInitDefault();
    archiveMountSdmc();
    FILE *f = fopen("sdmc:/g3-fidelity.txt", "w");
    if (f) {
        PbW2_PicaSetEnabled(false);
        const char *failure = checks();
        if (!failure && s_gpuFrames)
            failure = "unexpected GPU work";
        fprintf(f, "CPU %s%s\n", failure ? "FAIL " : "PASS", failure ? failure : "");

        PbW2_PicaSetEnabled(true);
        failure = PbW2_PicaEnabled() ? checks() : "GPU initialization";
        if (!failure && s_gpuFrames != 21)
            failure = "unexpected GPU frame count";
        fprintf(f, "PICA200 %s%s (%u GPU frames)\n", failure ? "FAIL " : "PASS", failure ? failure : "", s_gpuFrames);
        fclose(f);
    }
    for (;;)
        svcSleepThread(1000000000);
}
