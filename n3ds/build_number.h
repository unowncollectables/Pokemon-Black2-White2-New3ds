/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_BUILD_NUMBER_H
#define PBW2_BUILD_NUMBER_H

/* The port's internal build counter (Nate's tracking): bumped with
 * every push to main and stamped into the log header and the settings
 * card, so any hardware log identifies the build it came from. This
 * define is the single source - nothing else hardcodes the number. */
#define PBW2_BUILD_NUMBER 23

#define PBW2_BUILD_NUMBER_TEXT_(x) #x
#define PBW2_BUILD_NUMBER_TEXT(x) PBW2_BUILD_NUMBER_TEXT_(x)

#endif /* PBW2_BUILD_NUMBER_H */
