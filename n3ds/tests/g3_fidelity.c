/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ds2d_compose.h"
#include "g3_intercept.h"
#include "g3_pica.h"
#include "g3_raster.h"
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
static void triangle_vertices(s16 depth) {
    vertex(-4096, -2048, depth);
    vertex(2048, -2048, depth);
    vertex(0, 2048, depth);
    cmd(G3OP_END, 0);
}

static void triangle(u16 color, s16 depth) {
    cmd(G3OP_COLOR, color);
    cmd(G3OP_BEGIN, GX_BEGIN_TRIANGLES);
    triangle_vertices(depth);
}

static void lit_triangle(u32 normal) {
    cmd(G3OP_BEGIN, GX_BEGIN_TRIANGLES);
    cmd(G3OP_NORMAL, normal);
    triangle_vertices(0);
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
    reg_GX_DISPCNT = (GX_DISPMODE_VRAM_D << REG_GX_DISPCNT_MODE_SHIFT) | 0x0108;
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

    /* Move the palette to a different VRAM page, then edit just its color. */
    for (unsigned i = 0; i < 4; ++i) descriptors[i] = (2u << 14) | 64;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_B, 8);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_COMP4x4 << 26);
    for (unsigned i = 0; i < 2; ++i) {
        palette[128 + 3] = i ? GX_RGB(0, 31, 0) : GX_RGB(31, 0, 0);
        PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_E + 262, 2);
        triangle(0x7fff, 0);
        cmd(0x50, 0);
        PbW2_G3RasterCapture(pixels);
        if (pixels[center] != (0x8000 | palette[128 + 3]))
            return "4x4 palette writes after a descriptor moves the palette span";
    }

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

    G3X_AlphaTest(TRUE, 15);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_A5I3 << 26);
    triangle(0x7fff, -2048);
    cmd(G3OP_TEXIMAGE_PARAM, 0);
    triangle(0x03e0, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x07c0)
        return "alpha test rejects equality";

    tiles[1] = (16 << 3) | 1;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 64);
    cmd(G3OP_TEXIMAGE_PARAM, GX_TEXFMT_A5I3 << 26);
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 16)
        return "alpha test accepts the next alpha value";

    G3X_AlphaTest(FALSE, 0);
    tiles[1] = (15 << 3) | 1;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 64);
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 15)
        return "disabled alpha test ignores reference";

    G3X_AlphaTest(TRUE, 0);
    tiles[1] = (2 << 3) | 1;
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 64);
    cmd(G3OP_POLYGON_ATTR, (8u << 16) | (3u << 6) | (1u << 11));
    triangle(0x7fff, -2048);
    cmd(G3OP_TEXIMAGE_PARAM, 0);
    cmd(G3OP_POLYGON_ATTR, opaque);
    triangle(0x03e0, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0x07c0)
        return "alpha test uses the GX modulation result";

    G3X_AlphaTest(TRUE, 31);
    triangle(0x7fff, 0);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 0)
        return "maximum alpha reference rejects even opaque polygons";
    G3X_AlphaTest(FALSE, 0);

    cmd(G3OP_POLYGON_ATTR, opaque | 1);
    cmd(G3OP_DIF_AMB, 31u | ((10u << 5) << 16));
    cmd(G3OP_LIGHT_COLOR, 0x7fff);
    cmd(G3OP_LIGHT_VECTOR, 0x200); /* light points along -X */
    lit_triangle(0x1ff);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != ((29u << 11) | (9u << 6)))
        return "diffuse and ambient light";

    lit_triangle(0x200);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != (9u << 6))
        return "normal facing away from the light";

    /* Change the vector matrix after loading the light, then restore
     * only the position matrix so the triangle stays in the same place. */
    const u32 reflectedNormal[16] = {
        (u32)-FX32_ONE, 0, 0, 0,
        0, FX32_ONE, 0, 0,
        0, 0, FX32_ONE, 0,
        0, 0, 0, FX32_ONE,
    };
    G3_MtxMode(GX_MTXMODE_POSITION_VECTOR);
    PbW2_G3Command(G3OP_MTX_LOAD_4x4, reflectedNormal);
    G3_MtxMode(GX_MTXMODE_POSITION);
    G3_Identity();
    lit_triangle(0x1ff);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != (9u << 6))
        return "normal uses the vector matrix and the light direction stays latched";

    G3_MtxMode(GX_MTXMODE_POSITION_VECTOR);
    G3_Identity();
    G3_MtxMode(GX_MTXMODE_POSITION);
    G3_Translate(FX32_ONE / 4, 0, 0);
    lit_triangle(0x1ff);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != ((29u << 11) | (9u << 6)))
        return "translation leaves the normal unchanged";
    G3_Identity();

    u32 shininess[32];
    for (unsigned i = 0; i < 32; ++i) shininess[i] = 0x80808080;
    PbW2_G3Command(0x34, shininess);
    cmd(G3OP_DIF_AMB, 0);
    cmd(G3OP_SPE_EMI, 0xfc00);
    lit_triangle(0x1ff);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 15)
        return "shininess table colors a specular highlight";

    cmd(G3OP_SPE_EMI, 10u << 26);
    cmd(G3OP_POLYGON_ATTR, opaque);
    lit_triangle(0x1ff);
    cmd(0x50, 0);
    PbW2_G3Compose(1, pixels);
    if (pixels[center] != 10)
        return "emission with no enabled lights";

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

    PBW2_VU16(0x04000304) = 0;
    bankD[0] = 0;
    reg_GX_DISPCAPCNT = capture;
    PbW2_ComposeScreens(pixels, NULL);
    if (bankD[0] != 0x801f || (reg_GX_DISPCAPCNT & (1u << 31)))
        return "capture continues when the main LCD is hidden";

    /* BG3 is red. An OBJ window with clear right-hand texels lets half
     * the sprite reveal BG3 and must never draw its green palette. */
    PBW2_VU16(0x04000304) = 0x8000;
    reg_GX_VRAMCNT_A = GX_VRAMCNT(1, 0);
    reg_GX_VRAMCNT_B = GX_VRAMCNT(2, 0);
    u8 *bgVram = PbW2_VramMap((void *)HW_BG_VRAM);
    u8 *objVram = PbW2_VramMap((void *)HW_OBJ_VRAM);
    u16 *bgPalette = PbW2_VramMap((void *)HW_BG_PLTT);
    u16 *objPalette = PbW2_VramMap((void *)HW_OBJ_PLTT);
    u16 *oam = PbW2_VramMap((void *)HW_OAM);
    for (unsigned i = 0; i < 128; ++i) oam[4 * i] = 0x0200;
    oam[0] = 40 | (GX_OAM_MODE_OBJWND << 10);
    oam[1] = 48;
    oam[2] = 0;
    memset(objVram, 0, 32);
    for (unsigned y = 0; y < 8; ++y) objVram[y * 4] = objVram[y * 4 + 1] = 0x11;
    memset(bgVram + 32, 0x11, 32);
    u16 *map = PbW2_VramMap((void *)(HW_BG_VRAM + 0x800));
    for (unsigned i = 0; i < 1024; ++i) map[i] = 1;
    bgPalette[0] = 0;
    bgPalette[1] = 0x001f;
    objPalette[1] = 0x03e0;
    PBW2_VU16(0x0400000e) = 1u << REG_G2_BG0CNT_SCREENBASE_SHIFT;
    reg_G2_WINOUT = GX_WND_PLANEMASK_BG3 << 8;
    reg_GX_DISPCNT = (1u << 16) | 0x1800 | (GX_WNDMASK_OW << 13);
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[39 * 256 + 48] || pixels[40 * 256 + 52] || pixels[40 * 256 + 48] != 0xf800)
        return "OBJ window texels restrict BG3 without drawing the sprite";

    oam[0] |= 0x0300;
    oam[3] = oam[15] = 0x100;
    oam[7] = oam[11] = 0;
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[40 * 256 + 48] || pixels[44 * 256 + 56] || pixels[44 * 256 + 52] != 0xf800)
        return "double-size affine OBJ window centers the mask";
    oam[0] &= ~0x0300;

    reg_GX_DISPCNT |= (GX_WNDMASK_W0 | GX_WNDMASK_W1) << 13;
    reg_G2_WIN0H = (50u << 8) | 54u;
    reg_G2_WIN0V = (40u << 8) | 48u;
    reg_G2_WIN1H = (51u << 8) | 56u;
    reg_G2_WIN1V = reg_G2_WIN0V;
    reg_G2_WININ = GX_WND_PLANEMASK_BG3 << 8;
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[40 * 256 + 49] != 0xf800 || pixels[40 * 256 + 51] || pixels[40 * 256 + 55] != 0xf800)
        return "rectangular window precedence over the OBJ window";

    reg_GX_DISPCNT = (1u << 16) | 0x108 | (GX_WNDMASK_W0 << 13);
    reg_G2_WIN0H = (250u << 8) | 4u;
    reg_G2_WIN0V = (190u << 8) | 5u;
    reg_G2_WININ = GX_WND_PLANEMASK_BG0;
    reg_G2_WINOUT = 0;
    reg_G3X_CLEAR_COLOR = 0x03e0 | (31u << 16);
    cmd(0x50, 0);
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[0] != 0x07c0 || pixels[191 * 256 + 255] != 0x07c0 || pixels[5 * 256] || pixels[10])
        return "wrapped window masks the 3D layer";

    reg_GX_DISPCNT = (1u << 16) | 0x1900;
    PBW2_VU16(0x04000008) = 2u << REG_G2_BG0CNT_SCREENBASE_SHIFT;
    u16 *bg0Map = PbW2_VramMap((void *)(HW_BG_VRAM + 0x1000));
    for (unsigned i = 0; i < 1024; ++i) bg0Map[i] = 2;
    memset(bgVram + 64, 0x22, 32);
    bgPalette[2] = 0x7c00;
    oam[0] = 40;
    oam[2] = 1u << 10;
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[0] != 0x001f || pixels[40 * 256 + 48] != 0x001f)
        return "BG0 wins a BG tie and obscures a lower-priority sprite";
    oam[2] = 0;
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[40 * 256 + 48] != 0x07c0)
        return "sprite appears over a BG at the same priority";

    reg_GX_DISPCNT &= ~0x0800u;
    oam[2] = 1u << 10;
    PBW2_VU16(0x04000050) = REG_G2_BLDCNT_EFFECT_ALPHA | GX_WND_PLANEMASK_BG0 | (GX_WND_PLANEMASK_OBJ << 8);
    PBW2_VU16(0x04000052) = 0x0808;
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[0] != 0x001f || pixels[40 * 256 + 48] != 0x03cf)
        return "BG alpha blend uses the sprite immediately behind it";
    oam[2] = 0;

    PBW2_VU16(0x04000050) = REG_G2_BLDCNT_EFFECT_INCREASE | GX_WND_PLANEMASK_BG0;
    PBW2_VU16(0x04000054) = 8;
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[0] != 0x841f || pixels[40 * 256 + 48] != 0x07c0)
        return "brightness targets the top BG and leaves an untargeted OBJ alone";

    reg_GX_DISPCNT |= 1u << 13;
    PBW2_VU16(0x04000040) = 8;
    PBW2_VU16(0x04000044) = 8;
    PBW2_VU16(0x04000048) = 0x1f;
    PBW2_VU16(0x0400004a) = 0x3f;
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[0] != 0x001f || pixels[8] != 0x841f)
        return "a window can disable its brightness effect";

    reg_GX_DISPCNT = 1u << 16;
    bgPalette[0] = GX_RGB(31, 0, 0);
    PBW2_VU16(0x04000050) = REG_G2_BLDCNT_EFFECT_DECREASE | (1u << 5);
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[0] != 0x7800) return "the backdrop can be darkened";

    reg_GX_DISPCNT |= 0x0108;
    PBW2_VU16(0x04000050) = REG_G2_BLDCNT_EFFECT_DECREASE | GX_WND_PLANEMASK_BG0;
    PbW2_G3InterceptInit();
    reg_G3X_CLEAR_COLOR = 0;
    reg_G3X_CLEAR_DEPTH = 0x7fff;
    triangle(GX_RGB(0, 31, 0), 0);
    cmd(0x50, 0);
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[center] != 0x03e0 || pixels[0] != 0xf800)
        return "brightness distinguishes visible BG0 geometry from the backdrop";
    PBW2_VU16(0x04000050) = 0;

    PbW2_G3InterceptInit();
    reg_G3X_CLEAR_COLOR = 0;
    reg_G3X_CLEAR_DEPTH = 0x7fff;
    reg_G3X_DISP3DCNT = 0;
    PBW2_VU16(0x04000380 + 2 * 10) = GX_RGB(0, 31, 2);
    u32 toon = (GX_POLYGONMODE_TOON << 4) | (3u << 6) | (31u << 16);
    cmd(G3OP_POLYGON_ATTR, toon);
    triangle(GX_RGB(10, 2, 22), 0);
    cmd(0x50, 0);
    PbW2_G3RasterCapture(pixels);
    if (pixels[center] != (0x8000 | GX_RGB(0, 31, 2))) return "toon table replaces vertex color";

    reg_G3X_DISP3DCNT = REG_G3X_DISP3DCNT_THS_MASK;
    triangle(GX_RGB(10, 2, 22), 0);
    cmd(0x50, 0);
    PbW2_G3RasterCapture(pixels);
    if (pixels[center] != (0x8000 | GX_RGB(10, 31, 13))) return "highlight adds and saturates";

    reg_G3X_FOG_COLOR = GX_RGB(0, 0, 31) | (31u << 16);
    reg_G3X_FOG_OFFSET = 0x39ff;
    reg_G3X_DISP3DCNT = REG_G3X_DISP3DCNT_FME_MASK;
    PBW2_VU8(0x04000360) = 0;
    PBW2_VU8(0x04000361) = 127;
    cmd(G3OP_POLYGON_ATTR, (3u << 6) | (31u << 16) | GX_POLYGON_ATTR_MISC_FOG);
    triangle(GX_RGB(31, 0, 0), 0);
    cmd(0x50, 0);
    PbW2_G3RasterCapture(pixels);
    if (pixels[center] != (0x8000 | GX_RGB(15, 0, 15)) || pixels[0] != 0)
        return "fog density interpolates by depth";

    reg_G3X_FOG_OFFSET = 0;
    reg_G3X_DISP3DCNT = REG_G3X_DISP3DCNT_FME_MASK | (10u << REG_G3X_DISP3DCNT_FOG_SHIFT_SHIFT);
    PBW2_VU8(0x04000361) = 0;
    PBW2_VU8(0x04000367) = 127;
    triangle(GX_RGB(31, 0, 0), 0);
    cmd(0x50, GX_BUFFERMODE_W << 1);
    PbW2_G3RasterCapture(pixels);
    if (pixels[center] != (0x8000 | GX_RGB(0, 0, 31))) return "W-buffer fog uses clip-space W";

    reg_G3X_DISP3DCNT |= REG_G3X_DISP3DCNT_FMOD_MASK;
    reg_G3X_FOG_COLOR = GX_RGB(0, 0, 31);
    for (unsigned i = 0; i < 32; ++i) PBW2_VU8(0x04000360 + i) = 127;
    triangle(GX_RGB(31, 0, 0), 0);
    cmd(0x50, 0);
    PbW2_G3RasterCapture(pixels);
    if (pixels[center] != GX_RGB(31, 0, 0)) return "alpha-only fog preserves color and clears visibility";

    reg_G3X_DISP3DCNT &= ~REG_G3X_DISP3DCNT_FMOD_MASK;
    reg_G3X_CLEAR_COLOR = REG_G3X_CLEAR_COLOR_F_MASK;
    reg_G3X_FOG_COLOR |= 31u << 16;
    cmd(G3OP_POLYGON_ATTR, (3u << 6) | (31u << 16));
    triangle(GX_RGB(31, 0, 0), 0);
    cmd(0x50, 0);
    PbW2_G3RasterCapture(pixels);
    if (pixels[center] != (0x8000 | GX_RGB(31, 0, 0)) || pixels[0] != (0x8000 | GX_RGB(0, 0, 31)))
        return "fog applies only to marked geometry and clear pixels";

    reg_G3X_CLEAR_COLOR = 0;
    reg_G3X_DISP3DCNT = REG_G3X_DISP3DCNT_ABE_MASK;
    u32 mask = opaque | (GX_POLYGONMODE_SHADOW << 4);
    u32 shadow = (3u << 6) | (GX_POLYGONMODE_SHADOW << 4) | (15u << 16);
    for (unsigned id = 0; id <= 2; ++id) {
        cmd(G3OP_POLYGON_ATTR, opaque | (1u << 24));
        triangle(GX_RGB(31, 0, 0), 0);
        cmd(G3OP_POLYGON_ATTR, mask);
        triangle(GX_RGB(0, 31, 0), 2048);
        if (id != 0) {
            cmd(G3OP_POLYGON_ATTR, shadow | (id << 24));
            triangle(0, -2048);
        }
        cmd(0x50, 0);
        PbW2_G3RasterCapture(pixels);
        if (pixels[center] != (0x8000 | GX_RGB(id == 2 ? 15 : 31, 0, 0)) || pixels[0] != 0)
            return "shadow mask is invisible and rejects the occluder's polygon ID";
    }

    cmd(G3OP_POLYGON_ATTR, opaque | (1u << 24));
    triangle(GX_RGB(31, 0, 0), 0);
    cmd(G3OP_POLYGON_ATTR, mask);
    triangle(0, -2048);
    cmd(G3OP_POLYGON_ATTR, shadow | (2u << 24));
    triangle(0, -2048);
    cmd(0x50, 0);
    PbW2_G3RasterCapture(pixels);
    if (pixels[center] != (0x8000 | GX_RGB(31, 0, 0))) return "shadow mask requires depth failure";

    reg_GX_VRAMCNT_A = GX_VRAMCNT(3, 0);
    memset(tiles, 0, 32);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 32);
    cmd(G3OP_POLYGON_ATTR, opaque | (1u << 24));
    triangle(GX_RGB(31, 0, 0), 0);
    cmd(G3OP_POLYGON_ATTR, mask);
    triangle(0, 2048);
    cmd(G3OP_TEXIMAGE_PARAM, (GX_TEXFMT_PLTT16 << 26) | (1u << 29));
    cmd(G3OP_POLYGON_ATTR, shadow | (2u << 24));
    triangle(0, -2048);
    cmd(0x50, 0);
    PbW2_G3RasterCapture(pixels);
    if (pixels[center] != (0x8000 | GX_RGB(31, 0, 0))) return "shadow texture index zero remains transparent";
    cmd(G3OP_TEXIMAGE_PARAM, 0);

    reg_G3X_DISP3DCNT = REG_G3X_DISP3DCNT_EME_MASK;
    PBW2_VU16(0x04000332) = GX_RGB(0, 31, 0);
    cmd(G3OP_POLYGON_ATTR, opaque | (8u << 24));
    for (unsigned clearId = 0; clearId <= 8; clearId += 8) {
        reg_G3X_CLEAR_COLOR = clearId << 24;
        triangle(GX_RGB(31, 0, 0), 0);
        cmd(0x50, 0);
        PbW2_G3RasterCapture(pixels);
        unsigned marked = 0;
        for (unsigned i = 0; i < 256 * 192; ++i) {
            if (pixels[i] == (0x8000 | GX_RGB(0, 31, 0))) ++marked;
        }
        if (pixels[center] != (0x8000 | GX_RGB(31, 0, 0)) ||
            (clearId == 0 ? marked == 0 : marked != 0))
            return "edge marking uses the nearer polygon ID and leaves the interior untouched";
    }

    /* A partially visible tile crosses both screen-block boundaries. */
    reg_GX_DISPCNT = (1u << 16) | 0x100;
    PBW2_VU16(0x04000050) = 0;
    PBW2_VU16(0x04000008) = (3u << 14) | (2u << REG_G2_BG0CNT_SCREENBASE_SHIFT);
    PBW2_VU16(0x04000010) = 254;
    PBW2_VU16(0x04000012) = 255;
    memset(bg0Map, 0, 4 * 1024 * sizeof(*bg0Map));
    bg0Map[31 * 32 + 31] = 4 | (2u << 12) | (1u << 10) | (1u << 11);
    bg0Map[1024 + 31 * 32] = bg0Map[31 * 32 + 31];
    bg0Map[3072] = 4 | (2u << 12);
    bgPalette[0] = 0;
    bgPalette[33] = 0x03e0;
    memset(bgVram + 4 * 32, 0, 32);
    bgVram[4 * 32 + 3] = 0x10;
    bgVram[4 * 32] = 0x01;
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[0] || pixels[1] != 0x07c0 || pixels[2] != 0x07c0 || pixels[3] || pixels[258] != 0x07c0)
        return "scrolled 4bpp text tiles preserve flips, palette bank and transparency";

    PBW2_VU16(0x04000008) |= 0x80;
    bgPalette[200] = 0x7c00;
    memset(bgVram + 4 * 64, 0, 64);
    bgVram[4 * 64] = bgVram[4 * 64 + 7] = 200;
    PbW2_ComposeScreens(pixels, NULL);
    PBW2_VU16(0x04000010) = PBW2_VU16(0x04000012) = 0;
    if (pixels[0] || pixels[1] != 0x001f || pixels[2] != 0x001f || pixels[3] || pixels[258] != 0x001f)
        return "scrolled 8bpp text tiles use the full palette across screen blocks";

    /* Sample a flipped bitmap, then cross the left edge with and without wrap. */
    reg_GX_DISPCNT = (1u << 16) | 0x400 | GX_BGMODE_5;
    PBW2_VU16(0x0400000c) = 0x84;
    PBW2_VU16(0x04000020) = (u16)-256;
    PBW2_VU16(0x04000022) = 1280;
    PBW2_VU16(0x04000024) = 0;
    PBW2_VU16(0x04000026) = 256;
    PBW2_VU32(0x04000028) = 384;
    PBW2_VU32(0x0400002c) = 0;
    PbW2_NoteBgAffineWrite((u32)(uintptr_t)(pbw2_io + 0x20));
    u16 *direct = (u16 *)(void *)bgVram;
    bgPalette[0] = GX_RGB(0, 31, 0);
    direct[0] = 0x8000;
    direct[1] = 0x801f;
    direct[127] = direct[128 + 6] = 0xfc00;
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[0] != 0xf800 || pixels[1] || pixels[2] != 0x07c0 || pixels[256] != 0x001f)
        return "signed bitmap transform, opaque black and clipped left edge";
    PBW2_VU16(0x0400000c) = 0x80 | (1u << REG_G2_BG2CNT_AREAOVER_SHIFT);
    bgVram[0] = 0;
    bgVram[1] = bgVram[127] = bgVram[128 + 6] = 200;
    PbW2_ComposeScreens(pixels, NULL);
    if (pixels[0] != 0x001f || pixels[1] != 0x07c0 || pixels[2] != 0x001f || pixels[256] != 0x001f)
        return "indexed bitmap transform preserves palette zero and wraps at bitmap width";

    PbW2_G3InterceptInit();
    reg_GX_VRAMCNT_A = GX_VRAMCNT(3, 0);
    reg_GX_VRAMCNT_E = GX_VRAMCNT(3, 0);
    reg_G3X_DISP3DCNT = 0;
    reg_G3X_CLEAR_COLOR = 0;
    reg_G3X_CLEAR_DEPTH = 0x7fff;
    palette[1] = GX_RGB(0, 31, 0);
    for (unsigned y = 0; y < 8; ++y) {
        memset(tiles + y * 4, 0, 2);
        memset(tiles + y * 4 + 2, 0x11, 2);
    }
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, 32);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_E, 4);
    G3_TexImageParam(GX_TEXFMT_PLTT16, GX_TEXGEN_TEXCOORD, 0, 0, 0, 0, 1, 0);
    G3_Begin(GX_BEGIN_QUADS);
    G3_WRITE_COMMAND(reg_G3_TEXCOORD, G3OP_TEXCOORD, GX_ST(0, 0));
    G3_Vtx(-FX32_ONE, FX32_ONE, 0);
    G3_WRITE_COMMAND(reg_G3_TEXCOORD, G3OP_TEXCOORD, GX_ST(0, 8 * FX32_ONE));
    G3_Vtx(-FX32_ONE, -FX32_ONE, 0);
    G3_WRITE_COMMAND(reg_G3_TEXCOORD, G3OP_TEXCOORD, GX_ST(8 * FX32_ONE, 8 * FX32_ONE));
    G3_Vtx(FX32_ONE, -FX32_ONE, 0);
    G3_WRITE_COMMAND(reg_G3_TEXCOORD, G3OP_TEXCOORD, GX_ST(8 * FX32_ONE, 0));
    G3_Vtx(FX32_ONE, FX32_ONE, 0);
    G3_End();
    cmd(0x50, 0);
    PbW2_G3RasterCapture(pixels);
    if (pixels[96 * 256 + 64] || pixels[96 * 256 + 192] != (0x8000 | GX_RGB(0, 31, 0)))
        return "packed sprite coordinates sample outside transparent texel zero";
    return NULL;
}
/* The precision and geometry effect cases also exercise software fallback. */
extern bool __real_PbW2_PicaFinish(uint32_t clear, float depth, uint16_t *pixels);
static unsigned s_gpuFrames;

bool __wrap_PbW2_PicaFinish(uint32_t clear, float depth, uint16_t *pixels) {
    bool complete = __real_PbW2_PicaFinish(clear, depth, pixels);
    if (complete)
        ++s_gpuFrames;
    return complete;
}

static bool check_optional_edges(void) {
    PbW2_PicaSetOptions(true, true);
    PbW2_G3InterceptInit();
    reg_G3X_DISP3DCNT = REG_G3X_DISP3DCNT_EME_MASK;
    reg_G3X_CLEAR_COLOR = 0;
    reg_G3X_CLEAR_DEPTH = 0x7fff;
    PBW2_VU16(0x04000332) = GX_RGB(0, 31, 0);
    u32 opaque = (31u << 16) | (3u << 6) | (8u << 24);
    unsigned before = s_gpuFrames;
    cmd(G3OP_POLYGON_ATTR, opaque);
    triangle(GX_RGB(31, 0, 0), 0);
    cmd(0x50, 0);
    if (s_gpuFrames != before + 1) return false;
    PbW2_G3RasterCapture(pixels);
    if (pixels[96 * 256 + 128] != (0x8000 | GX_RGB(31, 0, 0))) return false;
    for (unsigned i = 0; i < 256 * 192; ++i) {
        if (pixels[i] == (0x8000 | GX_RGB(0, 31, 0))) return false;
    }

    /* A feature that needs software rendering still gets its outlines. */
    cmd(G3OP_POLYGON_ATTR, opaque | (GX_POLYGONMODE_TOON << 4));
    triangle(GX_RGB(31, 0, 0), 0);
    cmd(0x50, 0);
    if (s_gpuFrames != before + 1) return false;
    PbW2_G3RasterCapture(pixels);
    for (unsigned i = 0; i < 256 * 192; ++i) {
        if (pixels[i] == (0x8000 | GX_RGB(0, 31, 0))) return true;
    }
    return false;
}

static bool check_wide_frame(bool gpu) {
    static u16 sides[64 * 192];
    static u16 native[PBW2_NATIVE_TOP_WIDTH * PBW2_NATIVE_TOP_HEIGHT];
    memset(pbw2_io, 0, sizeof pbw2_io);
    PbW2_G3InterceptInit();
    PBW2_VU16(0x04000304) = 0x8000;
    reg_G3X_CLEAR_DEPTH = 0x7fff;
    PbW2_G3SetWide(true);
    PbW2_PicaSetOptions(gpu, false);
    unsigned before = s_gpuFrames;
    cmd(G3OP_POLYGON_ATTR, (31u << 16) | (3u << 6));
    cmd(G3OP_COLOR, GX_RGB(31, 0, 0));
    cmd(G3OP_BEGIN, GX_BEGIN_TRIANGLES);
    vertex(-5080, -2048, 0);
    vertex(-4120, -2048, 0);
    vertex(-4608, 2048, 0);
    cmd(G3OP_END, 0);
    cmd(0x50, 0);
    memset(pixels, 0, sizeof pixels);
    bool haveNative;
    bool wide = PbW2_G3RasterComposite(pixels, NULL, NULL, NULL, sides, 0, native, &haveNative);
    if (haveNative != gpu || (gpu && native[120 * 400 + 20] != (0x8000 | GX_RGB(31, 0, 0))))
        return false;
    PbW2_G3RasterCapture(pixels);
    for (unsigned i = 0; i < 256 * 192; ++i)
        if (pixels[i] & 0x8000) return false;

    reg_GX_DISPCNT = (1u << REG_GX_DISPCNT_MODE_SHIFT) | REG_GX_DISPCNT_BG02D3D_MASK | 0x100u;
    u16 *palette = PbW2_VramMap((void *)HW_BG_PLTT);
    palette[0] = GX_RGB(0, 0, 31);
    PbW2_ComposeScreens(pixels, NULL);
    const u16 *top = PbW2_NativeTop();
    if (gpu && (!top || top[120 * 400 + 20] != 0xf800 || top[120 * 400 + 200] != 0x001f))
        return false;
    if (!gpu && top) return false;

    /* Master brightness affects both the native polygon and its backdrop. */
    PBW2_VU16(0x0400006c) = REG_GX_MASTER_BRIGHT_MODE_DOWN | 16;
    PbW2_ComposeScreens(pixels, NULL);
    top = PbW2_NativeTop();
    if (gpu && (!top || top[120 * 400 + 20] || top[120 * 400 + 200])) return false;

    if (!wide || sides[96 * 64 + 16] != 0xf800 || sides[96 * 64 + 48] || s_gpuFrames != before + gpu)
        return false;

    if (gpu) {
        reg_GX_DISPCNT |= 0x200u;
        PBW2_VU16(0x0400006c) = 0;
        PBW2_VU16(0x04000008) = 1;
        PBW2_VU16(0x0400000a) = 2u << REG_G2_BG0CNT_SCREENBASE_SHIFT;
        reg_GX_VRAMCNT_A = GX_VRAMCNT(1, 0);
        u8 *vram = PbW2_VramMap((void *)HW_BG_VRAM);
        u16 *map = (u16 *)(vram + 0x1000);
        memset(vram, 0, 64);
        memset(map, 0, 0x800);
        map[12 * 32 + 16] = 1;
        memset(vram + 32, 0x11, 32);
        palette[1] = GX_RGB(0, 31, 0);
        triangle(GX_RGB(31, 0, 0), 0);
        cmd(0x50, 0);
        PbW2_ComposeScreens(pixels, NULL);
        top = PbW2_NativeTop();
        if (!top || top[121 * 400 + 201] != 0x07c0 || top[121 * 400 + 197] != 0xf800)
            return false;
    }
    PbW2_G3SetWide(false);
    return true;
}

extern void gfxInitDefault(void);
int main(void) {
    gfxInitDefault();
    archiveMountSdmc();
    FILE *f = fopen("sdmc:/g3-fidelity.txt", "w");
    if (f) {
        PbW2_PicaSetOptions(false, false);
        const char *failure = checks();
        if (!failure && s_gpuFrames)
            failure = "unexpected GPU work";
        fprintf(f, "CPU %s%s\n", failure ? "FAIL " : "PASS", failure ? failure : "");

        PbW2_PicaSetOptions(true, false);
        failure = PbW2_PicaEnabled() ? checks() : "GPU initialization";
        if (!failure && s_gpuFrames != 34)
            failure = "unexpected GPU frame count";
        fprintf(f, "PICA200 %s%s (%u GPU frames)\n", failure ? "FAIL " : "PASS", failure ? failure : "", s_gpuFrames);
        fprintf(f, "Optional GPU outlines %s\n", check_optional_edges() ? "PASS" : "FAIL");
        fprintf(f, "Wide camera CPU %s\n", check_wide_frame(false) ? "PASS" : "FAIL");
        fprintf(f, "Wide camera PICA200 %s\n", check_wide_frame(true) ? "PASS" : "FAIL");
        fclose(f);
    }
    for (;;)
        svcSleepThread(1000000000);
}
