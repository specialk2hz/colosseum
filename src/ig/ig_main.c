/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ig_main.c -- see ig.h.

#include "g_local.h"
#include "ig/ig.h"

/*
=================
IG_StartBlaster

IG-1.  OSP's loadout hardcodes the blaster (`start_weap[0] = 1`, rank 0 has no
`weapon_have` bit), so a railgun-only config still spawns everyone with a
second weapon.  `ig_noblaster 1` leaves it out.  Read where `start_weap` is
built (OSP_initWeapItem), i.e. at game start and on a config reload, like
`weapon_have` itself.

Safe with the blaster gone because the instagib config gives infinite slugs:
NoAmmoWeaponChange's last resort is the blaster and is never reached.  The
warmup arm of OSP_seedPlayer still hands the blaster out explicitly; `dm`
never runs it (sync_stat starts at 8).
=================
*/
int IG_StartBlaster(void)
{
    return gi.cvar("ig_noblaster", "0", 0)->value ? 0 : 1;
}

/*
=================
IG_NoFallFx

IG-24.  With falling damage off (not a campaign, dmflags DF_NO_FALLING),
`ig_nofallfx 1` also drops what a hard landing still does: the view dip
(`fall_value`) and the grunt (EV_FALL / EV_FALLFAR).  The landing makes the
short thud (EV_FALLSHORT) instead.  True when it has handled the landing and
P_FallingDamage should return.
=================
*/
bool IG_NoFallFx(edict_t *ent)
{
    static cvar_t *nofx;

    if (!nofx)
        nofx = gi.cvar("ig_nofallfx", "0", 0);
    if (!nofx->integer || G_IsCampaign() || !((int)dmflags->value & DF_NO_FALLING))
        return false;
    ent->s.event = EV_FALLSHORT;
    return true;
}
