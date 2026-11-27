/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_FOLLOWER_FEED_H
#define PBW2_FOLLOWER_FEED_H

/* Followers Stage 1 (followers-design.md SS6.1 + SS5B.2 "feed v2"):
 * the game side PUBLISHES field/party state once per field frame into
 * this struct in port memory; the port layer (n3ds/source/follower.c)
 * is the only reader. The game is never asked a second question, the
 * feed creates no actor, sets no flag, writes no work value and
 * touches no save data -- it is a publication, not a participation.
 * Everything here is read-only observation of game state.
 *
 * Feed v2 publishes the behaviour inputs from day one (SS5B.2), even
 * though the behaviour layer (Stages 6B/6C) lands later: terrain,
 * weather, time phase and the full party snapshot ride along now so
 * nobody has to retrofit the seam. */

#include "types.h"
#include "nitro/fx.h"

#define PBW2_FOLLOWER_PARTY_SLOTS 6

typedef struct {
    u16 species;
    u8 form;
    u8 sex;
    BOOL shiny;
    BOOL egg;
    u32 status;      /* PKM_PARAM_STATUS bitmask */
    u16 hp;
    u16 maxHp;       /* port derives the HP fraction; 0 = unknown */
    u8 happiness;    /* PKM_PARAM_HAPPINESS */
    u8 present;      /* slot occupied */
} PbW2FollowerPartySlot;

typedef struct {
    u32 frame;             /* increments once per published field frame */
    BOOL valid;            /* a field frame was published this frame */
    /* Player: tile, world position, facing, step progress. */
    s16 gridX, gridY, gridZ;
    VecFx32 worldPos;
    u16 faceDir;           /* DIR_UP/DOWN/LEFT/RIGHT (constants/directions.h) */
    fx32 stepProgress;     /* FX32 fraction of the current step, 0..1 */
    /* Place and conditions (behaviour inputs, SS5B.2). */
    u16 zoneId;
    u32 terrainAtFollower; /* raw tile type at the follower's tile */
    u32 weatherId;         /* GetNowWeather */
    u16 dayPeriod;         /* Field_GetDayPeriod */
    /* Avatar + script ownership: the hide rules' inputs. */
    u32 exState;           /* FLD_PLAYER_EXSTATE_*: walk/cycle/surf/dive */
    BOOL scriptOwnsField;  /* an event/task owns the field this frame */
    /* Party snapshot (read-only), refreshed on the SS5 cadence. */
    PbW2FollowerPartySlot party[PBW2_FOLLOWER_PARTY_SLOTS];
    u8 partyCount;
    u8 leadSlot;           /* first healthy non-egg slot, or 0xff */
} PbW2FollowerFeed;

/* The single feed instance, defined by the port layer. */
extern PbW2FollowerFeed g_PbW2FollowerFeed;

/* Port-layer entry points (n3ds/source/follower.c). Called from the
 * field render path only; both are no-ops unless the `follower`
 * config key is on. */
void PbW2_FollowerDraw(void);

#endif /* PBW2_FOLLOWER_FEED_H */
