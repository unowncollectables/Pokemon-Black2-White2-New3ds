/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Followers Behaviour Stage 2: talk-to reaction selection + the
 * present-side talk line overlay. See follower_behaviour.h for the
 * rules this file lives by (read-only mood model, port-authored
 * lines, reaction events instead of gameplay writes). */
#include "follower_behaviour.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

/* Tile classes at the follower's tile (field/map/fieldmap.h). */
#define TCLASS_GRASS 0x04
#define TCLASS_GRASS_B 0x05
#define TCLASS_GRASS_DOUBLE 0x06
#define TCLASS_GRASS_DOUBLE_B 0x07
#define TCLASS_REALLY_TALL_GRASS 0x08
#define TCLASS_REALLY_TALL_GRASS_DOUBLE 0x09
#define TCLASS_CAVE 0x0a
#define TCLASS_SAND 0x0b
#define TCLASS_DEEP_SAND 0x0c
#define TCLASS_SNOW 0x0e
#define TCLASS_ELECTRIC_FIELD 0x30
#define TCLASS_ELECTRIC_ROCK 0x32
#define TCLASS_HIDDEN_GROTTO_GRASS 0x24
#define TCLASS_GRASS_C 0x21
#define TCLASS_GRASS_DOUBLE_C 0x22

/* Field weather ids (system/zone_weather.h). */
#define WEATHER_FLAKE 1
#define WEATHER_RAIN 2
#define WEATHER_SANDSTORM 3
#define WEATHER_BLIZZARD 4
#define WEATHER_THUNDERSTORM 6
#define WEATHER_GUSTY_RAIN 7
#define WEATHER_DIAMOND_DUST 8
#define WEATHER_SANDSTORM_2 12

#define SPECIES_TRUBBISH 568

/* ---------------- staged talk line (game thread -> host) ---------------- */
/* The game thread stages a line; the host thread's present loop
 * draws it and counts its lifetime down. A sequence counter guards
 * the hand-off: odd while the writer is copying, even when stable.
 * A torn read is cosmetic at worst -- the reader just skips a
 * frame whose sequence moved under it. */
static char s_line[64];
static volatile unsigned s_lineSeq;
static volatile int s_lineFramesLeft;

static void stage_line(const char *text) {
    ++s_lineSeq;
    snprintf(s_line, sizeof s_line, "%s", text);
    s_lineFramesLeft = 300; /* ~5 s of presented frames */
    ++s_lineSeq;
}

/* ------------------------- reaction selection ------------------------- */

int PbW2_FollowerBehaviourTalk(const PbW2FollowerFeed *feed, int healedPending, int findPending) {
    if (feed->leadSlot >= PBW2_FOLLOWER_PARTY_SLOTS) {
        stage_line("... ... ...?");
        return PBW2_FOL_EMOTE_CALM;
    }
    const PbW2FollowerPartySlot *lead = &feed->party[feed->leadSlot];

    /* One-shot event reactions first (SS5B.1): the heal and the
     * find outrank every ambient read. */
    if (healedPending) {
        stage_line("...looks refreshed and full of energy!");
        return PBW2_FOL_EMOTE_EXCITED;
    }
    if (findPending) {
        stage_line("...found something shiny in the grass!");
        return PBW2_FOL_EMOTE_FIND;
    }
    /* The mood model: the lead's real status and HP, read-only. */
    if (lead->status != 0) {
        stage_line("...looks unwell. It needs care.");
        return PBW2_FOL_EMOTE_WORRIED;
    }
    if (lead->maxHp > 0 && lead->hp * 4 <= lead->maxHp) {
        stage_line("...looks worn out, but it's hanging in there.");
        return PBW2_FOL_EMOTE_WORRIED;
    }
    if (lead->maxHp > 0 && lead->hp * 2 <= lead->maxHp) {
        stage_line("...looks a little tired.");
        return PBW2_FOL_EMOTE_CALM;
    }
    /* Species-specific moment #1 (SS5B.1): Riley's boy. The design's
     * Castelia-alley refinement keys on a zone class the feed does
     * not publish yet, so Trubbish's line is species-wide for now. */
    if (lead->species == SPECIES_TRUBBISH) {
        stage_line("...looks right at home, rummaging happily!");
        return PBW2_FOL_EMOTE_HAPPY;
    }
    /* Weather (Gen 5 field weather, from the feed). */
    switch (feed->weatherId) {
    case WEATHER_RAIN:
    case WEATHER_GUSTY_RAIN:
    case WEATHER_THUNDERSTORM:
        stage_line("...is shaking off the rain, unbothered.");
        return PBW2_FOL_EMOTE_CALM;
    case WEATHER_SANDSTORM:
    case WEATHER_SANDSTORM_2:
        stage_line("...is squinting against the blowing sand.");
        return PBW2_FOL_EMOTE_WORRIED;
    case WEATHER_FLAKE:
    case WEATHER_BLIZZARD:
    case WEATHER_DIAMOND_DUST:
        stage_line("...is catching snowflakes as they fall!");
        return PBW2_FOL_EMOTE_HAPPY;
    default:
        break;
    }
    /* Terrain at the follower's own tile (feed v2's class). */
    switch (feed->terrainClassAtFollower) {
    case TCLASS_GRASS:
    case TCLASS_GRASS_B:
    case TCLASS_GRASS_DOUBLE:
    case TCLASS_GRASS_DOUBLE_B:
    case TCLASS_REALLY_TALL_GRASS:
    case TCLASS_REALLY_TALL_GRASS_DOUBLE:
    case TCLASS_HIDDEN_GROTTO_GRASS:
    case TCLASS_GRASS_C:
    case TCLASS_GRASS_DOUBLE_C:
        stage_line("...is rustling through the grass beside you.");
        return PBW2_FOL_EMOTE_HAPPY;
    case TCLASS_CAVE:
        stage_line("...is peering into the dark, staying close.");
        return PBW2_FOL_EMOTE_CALM;
    case TCLASS_ELECTRIC_FIELD:
    case TCLASS_ELECTRIC_ROCK:
        stage_line("...is buzzing with the charge in the air!");
        return PBW2_FOL_EMOTE_EXCITED;
    case TCLASS_SAND:
    case TCLASS_DEEP_SAND:
        stage_line("...is plodding happily through the sand.");
        return PBW2_FOL_EMOTE_CALM;
    case TCLASS_SNOW:
        stage_line("...is leaving a trail of little footprints.");
        return PBW2_FOL_EMOTE_HAPPY;
    default:
        break;
    }
    /* Happiness tiers: the bond the game itself built. */
    if (lead->happiness >= 200) {
        stage_line("...is beaming! It adores travelling with you.");
        return PBW2_FOL_EMOTE_EXCITED;
    }
    if (lead->happiness >= 150) {
        stage_line("...is smiling happily!");
        return PBW2_FOL_EMOTE_HAPPY;
    }
    if (lead->happiness >= 70) {
        stage_line("...looks content walking with you.");
        return PBW2_FOL_EMOTE_CALM;
    }
    if (lead->happiness >= 35) {
        stage_line("...is keeping pace, watching you.");
        return PBW2_FOL_EMOTE_CALM;
    }
    stage_line("...keeps its distance, still unsure.");
    return PBW2_FOL_EMOTE_WORRIED;
}

/* ------------------------- present-side overlay ------------------------- */
/* Same glyph painter and column-major addressing as the FPS
 * overlay (fps_overlay.c), aimed at a bar along the bottom of the
 * top screen -- the port's text box for talk reactions. */
static void draw_text(u16 *fb, unsigned stride, unsigned x, unsigned y, const char *text, u16 color) {
    const ConsoleFont *font = &consoleGetDefault()->font;
    for (; *text; ++text, x += 8) {
        const u8 *glyph = font->gfx + 8 * ((unsigned char)*text - font->asciiOffset);
        for (unsigned dx = 0; dx < 8; ++dx) {
            for (unsigned dy = 0; dy < 8; ++dy) {
                if (glyph[dy] & (0x80u >> dx)) fb[(x + dx) * stride + stride - 1 - y - dy] = color;
            }
        }
    }
}

void PbW2_FollowerTalkOverlay(void) {
    if (s_lineFramesLeft <= 0) return;
    unsigned seq = s_lineSeq;
    if (seq & 1) return; /* writer mid-copy; next frame */
    char line[64];
    snprintf(line, sizeof line, "%s", s_line);
    if (s_lineSeq != seq) return;
    --s_lineFramesLeft;

    u16 stride, width;
    u16 *fb = (u16 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &stride, &width);
    if (!fb || stride != 240 || width != 400) return;
    /* Bar: x 6..393, v 206..230 in the painter's coordinates. */
    for (unsigned x = 6; x < 394; ++x) {
        for (unsigned v = 206; v < 231; ++v) {
            fb[x * stride + stride - 1 - v] = 0x0000;
        }
    }
    draw_text(fb, stride, 13, 215, line, 0x0000);
    draw_text(fb, stride, 12, 214, line, 0xffff);
}
