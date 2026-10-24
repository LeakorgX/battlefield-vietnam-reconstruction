/* State shared by world_render.c (materials, reflection, shadows) and
 * renderer_upgrade.c (scene target and post chain). Defined in
 * world_render.c; renderer_upgrade therefore needs world_render in the
 * same build. See docs/RENDERER-PLAN.md. */
#ifndef WORLD_SHARED_H
#define WORLD_SHARED_H
#include <stdint.h>

struct world_shared {
    /* world_render: the main camera of the current frame, read from the
     * shader manager at the first world draw (view +0x14, projection +0x94). */
    uint32_t frame, camera_valid;
    float view[16], proj[16], viewproj[16];
    float camera[3], right[3], up[3], forward[3];
    float sun[3], sun_color[3];          /* toward the sun; linear colour */
    float water_height;
    uint32_t water_seen;
    /* renderer_upgrade: post chain state for the next frame's draws */
    uint32_t scene_active;               /* the FP16 world target is bound */
    uint32_t world_drawn;                /* a world draw happened since world begin */
    uint32_t post_on;                    /* exposure, tone mapping, bloom, fog, AO */
    /* the projection the engine set last */
    float raw_proj[16];
};
extern struct world_shared world_shared;
#endif
