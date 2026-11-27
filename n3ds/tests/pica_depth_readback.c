/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "g3_pica.h"

#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <string.h>

extern const u8 _binary_g3_pass_shbin_start[], _binary_g3_pass_shbin_end[];

enum { SIZE = 64 };

static void rectangle(PicaVertex *vertices, float left, float right, float depth) {
    const PicaVertex corners[] = {
        {.x = left,  .y = -0.5f, .z = -depth, .w = 1.0f},
        {.x = right, .y = -0.5f, .z = -depth, .w = 1.0f},
        {.x = left,  .y =  0.5f, .z = -depth, .w = 1.0f},
        {.x = right, .y =  0.5f, .z = -depth, .w = 1.0f},
    };
    const unsigned triangles[] = {0, 1, 2, 2, 1, 3};
    for (unsigned i = 0; i < 6; ++i) vertices[i] = corners[triangles[i]];
}

static bool probe(FILE *out) {
    C3D_RenderTarget *target = C3D_RenderTargetCreate(SIZE, SIZE, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    PicaVertex *vertices = linearAlloc(12 * sizeof(*vertices));
    u32 *pixels = linearAlloc(SIZE * SIZE * sizeof(*pixels));
    DVLB_s *shader = DVLB_ParseFile((u32 *)_binary_g3_pass_shbin_start,
                                    _binary_g3_pass_shbin_end - _binary_g3_pass_shbin_start);
    if (!target || !vertices || !pixels || !shader) return false;

    shaderProgram_s program;
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &shader->DVLE[0]);
    rectangle(vertices, -1.0f, 0.0f, 0.25f);
    rectangle(vertices + 6, 0.0f, 1.0f, 0.75f);
    memset(pixels, 0x5a, SIZE * SIZE * sizeof(*pixels));
    GSPGPU_FlushDataCache(pixels, SIZE * SIZE * sizeof(*pixels));
    GSPGPU_FlushDataCache(vertices, 12 * sizeof(*vertices));

    if (!C3D_FrameBegin(0)) return false;
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0, 0xffffff);
    C3D_FrameDrawOn(target);
    C3D_SetViewport(0, 0, SIZE, SIZE);
    C3D_BindProgram(&program);
    C3D_AttrInfo *attrs = C3D_GetAttrInfo();
    AttrInfo_Init(attrs);
    AttrInfo_AddLoader(attrs, 0, GPU_FLOAT, 4);
    AttrInfo_AddLoader(attrs, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attrs, 2, GPU_FLOAT, 4);
    C3D_BufInfo *buffers = C3D_GetBufInfo();
    BufInfo_Init(buffers);
    BufInfo_Add(buffers, vertices, sizeof(*vertices), 3, 0x210);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthMap(true, -1.0f, 0.0f);
    C3D_DepthTest(true, GPU_LEQUAL, GPU_WRITE_ALL);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_REPLACE);
    C3D_StencilTest(true, GPU_ALWAYS, 0x12, 0xff, 0xff);
    C3D_DrawArrays(GPU_TRIANGLES, 0, 6);
    C3D_StencilTest(true, GPU_ALWAYS, 0x87, 0xff, 0xff);
    C3D_DrawArrays(GPU_TRIANGLES, 6, 6);
    C3D_FrameSplit(GX_CMDLIST_FLUSH);
    unsigned dim = GX_BUFFER_DIM(SIZE, SIZE);
    Result transfer = GX_DisplayTransfer(target->frameBuf.depthBuf, dim, pixels, dim,
                                        GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                                            GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8));
    C3D_FrameEnd(GX_CMDLIST_FLUSH);
    if (!C3D_FrameBegin(0)) return false;
    C3D_FrameEnd(GX_CMDLIST_FLUSH);
    GSPGPU_InvalidateDataCache(pixels, SIZE * SIZE * sizeof(*pixels));

    u32 left = pixels[SIZE * 32 + 16];
    u32 right = pixels[SIZE * 32 + 48];
    u32 clear = pixels[SIZE * 4 + 32];
    fprintf(out, "GX result=%08lx left=%08lx right=%08lx clear=%08lx\n",
            (unsigned long)transfer, (unsigned long)left, (unsigned long)right, (unsigned long)clear);
    /* D24S8 stores the 24-bit depth below the stencil byte. */
    bool pass = R_SUCCEEDED(transfer) && left >> 24 == 0x12 && right >> 24 == 0x87 &&
                (left & 0xffffff) >= 0x3ffffe && (left & 0xffffff) <= 0x400001 &&
                (right & 0xffffff) >= 0xbffffe && (right & 0xffffff) <= 0xc00001 && clear == 0xffffff;
    shaderProgramFree(&program);
    DVLB_Free(shader);
    linearFree(pixels);
    linearFree(vertices);
    C3D_RenderTargetDelete(target);
    return pass;
}

int main(void) {
    gfxInitDefault();
    archiveMountSdmc();
    FILE *out = fopen("sdmc:/pica-depth-readback.txt", "w");
    if (out) {
        bool ready = C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
        bool pass = ready && probe(out);
        fprintf(out, "%s\n", pass ? "PASS" : "FAIL");
        fclose(out);
        if (ready) C3D_Fini();
    }
    for (;;) svcSleepThread(1000000000);
}
