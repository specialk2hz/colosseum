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
//   ig_botroam       0   1: goals with a reason (hunt) instead of the random
//                        fallback; item goals untouched (IG-7)
//   ig_botrevenge    0   1: a bot killed twice by one player within
//                        ig_botrevenge_window (60 s) goes after them for
//                        ig_botrevenge_time (20 s) or until it kills them (IG-8)
//   ig_botlibvars    ""  "name value name value ...": any further libvars for
//                        the Q3 brain, pushed after the above.  For tuning and
//                        A/B tests (igbench --set ig_botlibvars ...), not play
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
    // IG-9: slingshots ride on the same hook wiring
    lib->funcs.BotLibVarSet("ig_sling", use && gi.cvar("ig_botsling", "0", 0)->value ? "1" : "0");
    // IG-10: a hook mid-fight
    lib->funcs.BotLibVarSet("ig_fighthook", use && gi.cvar("ig_botfighthook", "0", 0)->value ? "1" : "0");
    lib->funcs.BotLibVarSet("ig_padhook", use && gi.cvar("ig_botpadhook", "0", 0)->value ? "1" : "0");   // IG-13
    lib->funcs.BotLibVarSet("ig_pullhop", use && gi.cvar("ig_botpullhop", "0", 0)->value ? "1" : "0");   // IG-17
    lib->funcs.BotLibVarSet("ig_stairhook", use && gi.cvar("ig_botstairhook", "0", 0)->value ? "1" : "0");   // IG-20
    lib->funcs.BotLibVarSet("ig_affinity", gi.cvar("ig_botaffinity", "0", 0)->value ? "1" : "0");   // IG-21
    lib->funcs.BotLibVarSet("ig_peace", gi.cvar("ig_botpeace", "0", 0)->value ? "1" : "0");   // IG-27
    lib->funcs.BotLibVarSet("ig_affinitychance", gi.cvar("ig_botaffinitychance", "0", 0)->value ? "1" : "0");   // IG-28
    lib->funcs.BotLibVarSet("ig_walkway", gi.cvar("ig_botwalkway", "0", 0)->value ? "1" : "0");   // IG-29
    // the server's hook_sky, for the lava escape (IG-9)
    lib->funcs.BotLibVarSet("ig_hooksky", gi.cvar("hook_sky", "0", 0)->value ? "1" : "0");
    if (use) {
        lib->funcs.BotLibVarSet("cmd_grappleon", IG_HOOKON);
        lib->funcs.BotLibVarSet("cmd_grappleoff", IG_HOOKOFF);
    }
    lib->funcs.BotLibVarSet("ig_roam", gi.cvar("ig_botroam", "0", 0)->value ? "1" : "0");
    lib->funcs.BotLibVarSet("ig_revenge", gi.cvar("ig_botrevenge", "0", 0)->value ? "1" : "0");
    lib->funcs.BotLibVarSet("ig_revenge_window", gi.cvar("ig_botrevenge_window", "60", 0)->string);
    lib->funcs.BotLibVarSet("ig_revenge_time", gi.cvar("ig_botrevenge_time", "20", 0)->string);
    // testing aid: bots say in chat which goal they chose and why
    lib->funcs.BotLibVarSet("ig_debug", gi.cvar("ig_botdebug", "0", 0)->value ? "1" : "0");
    {
        cvar_t *extra = gi.cvar("ig_botlibvars", "", 0);
        char buf[MAX_STRING_CHARS], *name, *value;
        const char *p = buf;

        Q_strlcpy(buf, extra->string, sizeof(buf));
        while ((name = COM_Parse(&p)) && *name) {
            char n[MAX_QPATH];

            Q_strlcpy(n, name, sizeof(n));
            value = COM_Parse(&p);
            if (!*value)
                break;
            lib->funcs.BotLibVarSet(n, value);
        }
    }
}

/*
IG-14: `ig_botstrafehack 1`. Quake II locks the jump for 144-200 ms after a
landing faster than 200 u/s (PMF_TIME_LAND), and ground friction takes ~60% of
the speed in that time. Q2PRO lifts the lock for its own and R1Q2 clients
(sv_strafejump_hack, pmoveParams_t.strafehack: the flag is never set) -- but
a bot is no client connection: its pmove runs with the server's default
parameters, lock on. A person on Q2PRO kept a slingshot's momentum through
the landing; a bot lost it (975 u/s let go, 330 at the next hop). This
clears the flag before a bot's pmove, which is what strafehack amounts to.
*/
void IG_BotPmove(edict_t *ent, pmove_state_t *s)
{
    static cvar_t *hack;

    if (!hack)
        hack = gi.cvar("ig_botstrafehack", "0", 0);
    if (!hack->integer || !(ent->flags & FL_BOT) || !(s->pm_flags & PMF_TIME_LAND))
        return;
    s->pm_flags &= ~PMF_TIME_LAND;
    if (!(s->pm_flags & (PMF_TIME_WATERJUMP | PMF_TIME_TELEPORT)))
        s->pm_time = 0;
}

/*
IG-26: true for the hook commands IG_BotLibVars gives the brain, so a route
that drives a bot can drop the brain's own (ig_route.c).
*/
bool IG_BotHookCommand(const char *cmd)
{
    return !Q_stricmp(cmd, IG_HOOKON) || !Q_stricmp(cmd, IG_HOOKOFF);
}
