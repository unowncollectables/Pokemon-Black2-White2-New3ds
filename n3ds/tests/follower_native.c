/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../source/follower.c"

#include <3ds.h>

static bool mirrors_correctly(void) {
    for (unsigned y = 0; y < s_frameSize; ++y) {
        unsigned row = y * s_frameSize * 4;
        unsigned bytes = s_frameSize / 2;
        const u8 *right = s_stripIndices + row + 4 * bytes;
        const u8 *left = s_stripIndices + row + 6 * bytes;
        for (unsigned x = 0; x < bytes; ++x) {
            u8 pair = left[bytes - 1 - x];
            if (right[x] != (u8)((pair >> 4) | (pair << 4))) return false;
        }
    }
    return true;
}

static const char *check_native_sprites(void) {
    if (!PbW2_FollowerLoadNative(1, 0, 0) || s_frameSize != 32) return "Bulbasaur 32";
    if (!PbW2_FollowerLoadNative(321, 0, 0) || s_frameSize != 64 || !mirrors_correctly()) return "Wailord 64";
    if (!PbW2_FollowerLoadNative(479, 2, 2) || s_frameSize != 32) return "Rotom form";
    FieldActorConfig male, female;
    if (!native_actor(208, 0, 0, &male) || !native_actor(208, 0, 1, &female) ||
        male.uid == female.uid) return "Steelix sexes";
    if (PbW2_FollowerCoverage(568, 0, 0) != TIER_FLOOR || PbW2_FollowerLoadNative(568, 0, 0) ||
        PbW2_FollowerLoadNative(479, 255, 2)) return "unsupported native entries";
    return NULL;
}

int main(void) {
    archiveMountSdmc();
    FILE *out = fopen("sdmc:/follower-native.txt", "w");
    if (out) {
        const char *failure = check_native_sprites();
        fprintf(out, "%s\n", failure ? failure : "PASS");
        fclose(out);
    }
    for (;;) svcSleepThread(1000000000);
}
