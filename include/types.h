#ifndef POKEBW2_TYPES_H
#define POKEBW2_TYPES_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed long s32;
typedef signed long long s64;

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;

typedef float f32;
typedef double f64;

// An address held as an integer, which a native port widens to the size of a pointer
typedef u32 uptr;

typedef int BOOL;
#define TRUE 1
#define FALSE 0

#define NULL ((void *)0)

#define NELEMS(array) (sizeof(array) / sizeof((array)[0]))

// NitroSDK's alignment attribute, as in `static u8 buffer[64] ATTRIBUTE_ALIGN(32);`
#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))

#ifdef PBW2_N3DS
extern u8 pbw2_io[];
#define PBW2_IO(addr) (pbw2_io + ((u32)(addr) - 0x04000000u))
#define PBW2_VU8(addr) (*(vu8 *)PBW2_IO(addr))
#define PBW2_VU16(addr) (*(vu16 *)PBW2_IO(addr))
#define PBW2_VU32(addr) (*(vu32 *)PBW2_IO(addr))
#endif

#endif // POKEBW2_TYPES_H
