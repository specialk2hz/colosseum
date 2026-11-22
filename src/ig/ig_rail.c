/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ig_rail.c -- IG-25 `ig_railsplash`: rail jumping.
//
// Where a rail slug hits a wall or floor it pushes players away from the
// impact, like a rocket's blast without the damage:
//   ig_railsplash 1         only the shooter (rail jumps)
//   ig_railsplash 2         everyone near the impact
//   ig_railsplash_radius    120 units (a rocket's blast radius)
//   ig_railsplash_kick      speed added next to the impact, 900 u/s, falling off
//                           to nothing at the radius. A Q2 rocket jump adds about
//                           430 at the feet (T_RadiusDamage halves the self
//                           points, T_Damage gives itself 1600 x knockback / mass).
// Default 0: upstream's railgun. Never in a campaign.

#include "g_local.h"
#include "ig/ig.h"

static cvar_t *ig_railsplash, *ig_railsplash_radius, *ig_railsplash_kick;

/*
=================
IG_RailSplash

From fire_rail, with the trace that stopped the slug.
=================
*/
void IG_RailSplash(edict_t *self, const trace_t *tr)
{
    vec3_t  impact, center, dir;
    trace_t los;
    float   radius, kick, dist, speed;
    int     mode, i;
    edict_t *ent;

    if (!ig_railsplash) {
        ig_railsplash = gi.cvar("ig_railsplash", "0", 0);
        ig_railsplash_radius = gi.cvar("ig_railsplash_radius", "120", 0);
        ig_railsplash_kick = gi.cvar("ig_railsplash_kick", "900", 0);
    }
    mode = ig_railsplash->integer;
    if (mode <= 0 || G_IsCampaign() || !self || !self->client)
        return;
    // only a slug that stopped on something solid: not one that ran out of
    // range, and not the sky
    if (tr->fraction >= 1.0f || tr->allsolid || (tr->surface && (tr->surface->flags & SURF_SKY)))
        return;
    radius = ig_railsplash_radius->value;
    kick = ig_railsplash_kick->value;
    if (radius <= 0 || kick <= 0)
        return;

    // a unit off the surface, so the line-of-sight trace starts in the open
    VectorMA(tr->endpos, 1.0f, tr->plane.normal, impact);

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (!ent->inuse || !ent->client || ent->health <= 0 || ent->movetype == MOVETYPE_NOCLIP)
            continue;
        if (ent->solid == SOLID_NOT)        // observers
            continue;
        if (mode == 1 && ent != self)
            continue;

        VectorAdd(ent->absmin, ent->absmax, center);
        VectorScale(center, 0.5f, center);
        VectorSubtract(center, impact, dir);
        dist = VectorNormalize(dir);
        if (dist >= radius)
            continue;
        los = gi.trace(impact, NULL, NULL, center, NULL, MASK_SOLID);
        if (los.fraction < 1.0f)
            continue;

        speed = kick * (1.0f - dist / radius);
        VectorMA(ent->velocity, speed, dir, ent->velocity);
        if (dir[2] > 0)
            ent->groundentity = NULL;
    }
}
