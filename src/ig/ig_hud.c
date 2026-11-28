/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ig_hud.c -- IG-41 `ig_chasespeed 1`: a speedometer for chase cameras.
//
// A spectator chasing a player sees the player's horizontal speed, in small
// text under the "Chasing" line: `612 ups`.  A statusbar `num` would be the
// big HUD digits, so it rides on the chase layout instead, which UpdateChaseCam
// (g_chase.c) then sends every frame rather than every 32 -- about 60 bytes a
// frame to each spectator, and only while no scoreboard or menu has the layout.
// 0 (default): upstream, the layout every 32 frames and no speed.

#include "g_local.h"
#include "ig/ig.h"

bool IG_ChaseSpeedOn(void)
{
    static cvar_t *on;

    if (!on)
        on = gi.cvar("ig_chasespeed", "1", 0);
    return on->integer != 0;
}

// Append the speed line to the chase layout `s`, placed under the chase text:
// OSP's plate sits at xv 44 yb -59, the others' at xv 0 yb -68.
void IG_ChaseSpeed(const edict_t *targ, char *s, size_t size, bool osp)
{
    float speed;
    size_t len;

    if (!IG_ChaseSpeedOn() || !targ)
        return;
    speed = sqrtf(targ->velocity[0] * targ->velocity[0] + targ->velocity[1] * targ->velocity[1]);
    len = strlen(s);
    if (len >= size)
        return;
    Q_snprintf(s + len, size - len, osp ? " xv 44 yb -51 string2 \"%d ups\"" : " xv 0 yb -60 string2 \"%d ups\"",
               (int)(speed + 0.5f));
}
