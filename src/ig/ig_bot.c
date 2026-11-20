/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ig_bot.c -- what the instagib fork tells the Quake III brain (IG-6).
//
// Pushed with the ruleset's own libvars (bl_main.c, BotRulesetLibVars), so at
// library init -- before the map, and so the AAS file, is loaded -- and every
// frame after, like `usehook`.  Only to the Quake III botlib: these are the
// fork's libvars and Q3's own bot_* switches, which Gladiator does not have.
//
//   ig_bothook       0   1: Q3 bots use the offhand hook on grapple routes,
//                        when the hook is on (OSP rulesets, hook_enable)
//   ig_bothook_cost  150 the fixed cost of a grapple route, in 1/100 s of
//                        route time (bspc bakes in 500); read at map load
//
// With ig_bothook 0 the brain gets its own defaults back (bot_grapple 0,
// offhandgrapple 0, no hook detection, the AAS file's costs): upstream.

#include "g_local.h"
#include "bot/bl_main.h"
#include "bot/bl_botlib.h"
#include "ig/ig.h"

void IG_BotLibVars(struct bot_library_s *lib)
{
    cvar_t *on = gi.cvar("ig_bothook", "0", 0);
    cvar_t *cost = gi.cvar("ig_bothook_cost", "150", 0);
    bool use;

    if (!lib || !lib->botlib || Q_stricmp(lib->botlib->id, "q3"))
        return;
    // The OSP hook is the one wired: its commands are `hook`/`unhook`.
    // Threewave's (ctf) and arena's are later work.
    use = on->value != 0 && BotTourneyHook();

    lib->funcs.BotLibVarSet("bot_grapple", use ? "1" : "0");
    lib->funcs.BotLibVarSet("offhandgrapple", use ? "1" : "0");
    lib->funcs.BotLibVarSet("ig_hookdetect", use ? "1" : "0");
    lib->funcs.BotLibVarSet("ig_rs_startgrapple", use ? cost->string : "0");
    if (use) {
        lib->funcs.BotLibVarSet("cmd_grappleon", "hook");
        lib->funcs.BotLibVarSet("cmd_grappleoff", "unhook");
    }
}
