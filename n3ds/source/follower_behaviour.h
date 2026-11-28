/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

/* Followers Behaviour Stage 2 (followers-design.md SS5B): the
 * talk-to reaction system. Port-layer only: it reads the published
 * feed snapshot, picks a reaction, and stages a port-authored line
 * for the present-side overlay. It writes NOTHING to the party, the
 * event work, or the save -- the mood model is read-only reflection
 * of the bond the game itself built (SS5B.1). Lines are our own
 * words in HGSS's spirit; no game string is ripped or reproduced.
 *
 * The emote is returned as an animation kind: the follower's hop
 * language in follower.c is the emote channel (no new rendering
 * surface, no new assets). */

#include "pbw2_follower_feed.h"

enum PbW2FolEmote {
    PBW2_FOL_EMOTE_CALM = 0,   /* one small hop */
    PBW2_FOL_EMOTE_HAPPY,      /* two hops */
    PBW2_FOL_EMOTE_EXCITED,    /* two fast, higher hops */
    PBW2_FOL_EMOTE_WORRIED,    /* one slow, low hop */
    PBW2_FOL_EMOTE_FIND,       /* hop, pause, hop -- presenting a find */
};

/* Pick the reaction for a talk-to, stage its line for the overlay,
 * and return the emote kind. healedPending / findPending are the
 * caller's port-RAM event flags (SS5B.1: the Center heal reaction
 * and the grass find are reaction events, never bag writes); the
 * caller clears them once consumed here. */
int PbW2_FollowerBehaviourTalk(const PbW2FollowerFeed *feed, int healedPending, int findPending);

/* Present-side (host thread): draws the staged talk line at the
 * bottom of the top screen until its lifetime runs out. Called from
 * main.c's present loop beside the FPS overlay; a no-op when no
 * line is staged. */
void PbW2_FollowerTalkOverlay(void);
