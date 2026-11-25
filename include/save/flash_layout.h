#ifndef POKEBW2_SAVE_FLASH_LAYOUT_H
#define POKEBW2_SAVE_FLASH_LAYOUT_H

#include "types.h"

// The DS compiler stores 64-bit flash fields at four-byte alignment.
// Keep that alignment for serialized fields when the native ABI uses eight.
#ifdef PBW2_N3DS
typedef s64 SaveS64 __attribute__((aligned(4)));
typedef u64 SaveU64 __attribute__((aligned(4)));
#else
typedef s64 SaveS64;
typedef u64 SaveU64;
#endif

#endif // POKEBW2_SAVE_FLASH_LAYOUT_H
