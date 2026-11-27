#ifndef POKEBW2_FIELD_ACTOR_FIELD_ACTOR_CONFIG_H
#define POKEBW2_FIELD_ACTOR_FIELD_ACTOR_CONFIG_H

#include "types.h"

// The resources of an object code's model
typedef struct {
    u16 res1;
    u16 res2;
    u16 animations[3];
} FieldActorResGroup;

// An object code's record in ARCID_MMODEL_TBL, from 4 bytes into the file, at the index GetIndexOfObjID returns. Names
// from swan's FieldActorConfig
typedef struct {
    u16 uid;
    u8 entityType;
    u8 sceneNodeType;
    u8 enableShadow;
    u8 footprintType;
    u8 enableReflections;
    u8 billboardSize;
    u8 spriteAtlasSize;
    u8 spriteControllerType;
    u8 gender;
    u8 collWidth;
    u8 collHeight;
    s8 wPosOffsetX;
    s8 wPosOffsetY;
    s8 wPosOffsetZ;
    FieldActorResGroup rscIndices;
    u16 padding;
} FieldActorConfig;

// A config's billboardSize: the size of the actor's billboard in pixels, by field_actor_drawdata.c's
// ACTOR_BILLBOARD_SIZES_INT
enum {
    ACTOR_BILLBOARD_SIZE_32,
    ACTOR_BILLBOARD_SIZE_16,
    ACTOR_BILLBOARD_SIZE_64,
};

#endif // POKEBW2_FIELD_ACTOR_FIELD_ACTOR_CONFIG_H
