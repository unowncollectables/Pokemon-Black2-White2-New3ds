/* SPDX-License-Identifier: GPL-3.0-or-later */
/* probabilityLoop for save init (entralink tree excluded from link closure). */
#include "field/event/survey.h"

void insideProbabilityLoop(SurveyProbabilityState *state, u32 selection) {
    (void)state;
    (void)selection;
}

void probabilityLoop(SurveyProbabilityState *state) {
    int i;

    for (i = 0; i < 6; i++) {
        state->probability[i] = 0;
        insideProbabilityLoop(state, (u32)i);
    }
}