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
//   ig_botstrafe     0   brain ig_strafe: 2 strafe-jump chains (IG-31)
//   ig_botclimbrun   0   brain ig_climbrun, bits: 2 let go over the ledge,
//                        1 fire on the run (IG-32); needs ig_bothook
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
    // IG-31: 2 = strafe-jump chains (1 = IG-11's single hops); the game half is ig_botjumpland
    lib->funcs.BotLibVarSet("ig_strafe", gi.cvar("ig_botstrafe", "0", 0)->string);
    // IG-32: climb grapples, bits: 2 let go over the ledge, 1 fire on the run (experimental)
    lib->funcs.BotLibVarSet("ig_climbrun", use ? gi.cvar("ig_botclimbrun", "0", 0)->string : "0");
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
/*
IG-31: `ig_botjumpland 1`. Billy presses jump just before he lands while strafe
jumping, so he takes off again on the first ground frame and friction never
touches him. A bot cannot hold jump: the botlib drops it every other frame
(be_ea.c ACTION_JUMPEDLASTFRAME), so a jump asked for in the air may be off on
the landing frame. A bot that asked for jump in the air in the last two frames
keeps it held until it lands. Airborne jumps do nothing in Quake II (PM_CheckJump
needs ground), and PMF_JUMP_HELD is only set by a jump, so the held jump fires on
the landing.
*/
static int ig_airjump[MAX_CLIENTS];

void IG_BotPmove(edict_t *ent, pmove_state_t *s, usercmd_t *cmd)
{
    static cvar_t *hack, *jumpland;
    int n = (int)(ent - g_edicts) - 1;

    if (!hack) {
        hack = gi.cvar("ig_botstrafehack", "0", 0);
        jumpland = gi.cvar("ig_botjumpland", "0", 0);
    }
    if (jumpland->integer && (ent->flags & FL_BOT) && n >= 0 && n < MAX_CLIENTS) {
        if (ent->groundentity || ent->client->ctf_grapple)
            ig_airjump[n] = 0;
        else if (cmd->upmove > 0)
            ig_airjump[n] = level.framenum;
        else if (ig_airjump[n] && level.framenum - ig_airjump[n] <= 2 && ig_airjump[n] <= level.framenum)
            cmd->upmove = 400;
    }
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

/*
=================
IG_BotUnstuck  (IG-30, `ig_botunstuck`)

A bot that is stuck for good is put back at a spawn point, frags untouched:
  - inside something solid for a second: q2dm1pro's lift rises into a
    player-clip lid that covers all of its top but a strip at the south edge,
    so a bot riding in the middle ends up in the clip, where pmove cannot move
    it, and rides up and down with the lift for the rest of the map;
  - or not moved more than 16 units across the floor in ig_botunstuck_time
    seconds (10) while asking to walk, and not on the hook.
From ClientThink after the pmove, for bots only.  0 = upstream.
=================
*/
#define IG_UNSTUCK_RANGE    16.0f
#define IG_UNSTUCK_SOLID    (1 * BASE_FRAMERATE)

static struct {
    int     frame;      // the frame of the last check, to spot respawns and new maps
    int     since;      // when it settled at `at`
    int     solid;      // when it was first found in solid, 0 if it is not
    vec3_t  at;
} ig_stuck[MAX_CLIENTS];

/*
IG_Respot: puts a live player at a spawn point the ruleset picks, like a
teleporter (flash, hook let go, velocity cleared), frags untouched. Never onto
someone: up to three picks, false if all were taken. For IG-30 (stuck bots) and
IG-33 (lava and pit rescue).
*/
bool IG_Respot(edict_t *ent, const char *why)
{
    vec3_t  origin, angles;
    trace_t tr;
    int     i, tries;

    for (tries = 0; tries < 3; tries++) {
        if (!G_SelectSpawnPoint(ent, origin, angles))
            return false;
        origin[2] += 10;    // as PutClientInServer places a body ("make sure off ground")
        tr = gi.trace(origin, ent->mins, ent->maxs, origin, ent, MASK_PLAYERSOLID);
        if (!tr.startsolid)
            break;
    }
    if (tries == 3)
        return false;
    gi.dprintf("ig: respot %d %s at %.0f %.0f %.0f\n", (int)(ent - g_edicts) - 1, why,
               ent->s.origin[0], ent->s.origin[1], ent->s.origin[2]);

    G_PlayerResetGrapple(ent);
    gi.unlinkentity(ent);
    VectorCopy(origin, ent->s.origin);
    VectorCopy(origin, ent->s.old_origin);
    VectorClear(ent->velocity);
    VectorClear(ent->client->oldvelocity);  // or the landing check turns the teleport event into a footstep
    ent->groundentity = NULL;
    ent->client->ps.pmove.pm_time = 160 >> PM_TIME_SHIFT;
    ent->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;
    ent->s.event = EV_PLAYER_TELEPORT;
    for (i = 0; i < 3; i++)
        ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(angles[i] - ent->client->resp.cmd_angles[i]);
    VectorCopy(angles, ent->s.angles);
    VectorCopy(angles, ent->client->ps.viewangles);
    VectorCopy(angles, ent->client->v_angle);
    gi.linkentity(ent);
    return true;
}

void IG_BotUnstuck(edict_t *ent, const usercmd_t *ucmd)
{
    static cvar_t *on, *still;
    trace_t tr;
    float   dx, dy;
    int     n = (int)(ent - g_edicts) - 1;

    if (!on) {
        on = gi.cvar("ig_botunstuck", "0", 0);
        still = gi.cvar("ig_botunstuck_time", "10", 0);
    }
    if (!on->integer || !(ent->flags & FL_BOT) || n < 0 || n >= MAX_CLIENTS ||
        ent->health <= 0 || ent->deadflag || ent->movetype == MOVETYPE_NOCLIP || G_IsObserver(ent))
        return;

    // a new body or a new map: start over
    if (ig_stuck[n].frame > level.framenum || ig_stuck[n].frame < level.framenum - 2) {
        ig_stuck[n].since = level.framenum;
        ig_stuck[n].solid = 0;
        VectorCopy(ent->s.origin, ig_stuck[n].at);
    }
    ig_stuck[n].frame = level.framenum;

    tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, MASK_PLAYERSOLID);
    if (!tr.startsolid)
        ig_stuck[n].solid = 0;
    else if (!ig_stuck[n].solid)
        ig_stuck[n].solid = level.framenum;
    else if (level.framenum - ig_stuck[n].solid >= IG_UNSTUCK_SOLID) {
        IG_Respot(ent, "solid");
        ig_stuck[n].frame = -1;
        return;
    }

    dx = ent->s.origin[0] - ig_stuck[n].at[0];
    dy = ent->s.origin[1] - ig_stuck[n].at[1];
    if (dx * dx + dy * dy > IG_UNSTUCK_RANGE * IG_UNSTUCK_RANGE || ent->client->ctf_grapple ||
        (!ucmd->forwardmove && !ucmd->sidemove)) {
        ig_stuck[n].since = level.framenum;
        VectorCopy(ent->s.origin, ig_stuck[n].at);
    } else if (still->value > 0 && level.framenum - ig_stuck[n].since >= still->value * BASE_FRAMERATE) {
        IG_Respot(ent, "still");
        ig_stuck[n].frame = -1;
    }
}

/*
=================
IG_HazardRescue  (IG-33, `ig_lavarescue`)

Corkscrew's pit-fall recovery (Billy): a player who touches lava or slime, or
a trigger_hurt (pits, the void), is put back at a spawn point instead of
dying -- no death, no suicide, no frag lost. From T_Damage, before anything
is taken; true when the damage is cancelled. Any ruleset but the campaign;
people and bots alike.
=================
*/
bool IG_HazardRescue(edict_t *targ, int mod)
{
    static cvar_t *on;

    if (!on)
        on = gi.cvar("ig_lavarescue", "0", 0);
    if (!on->integer || G_IsCampaign() || !targ->client || targ->health <= 0 || targ->deadflag ||
        targ->movetype == MOVETYPE_NOCLIP || G_IsObserver(targ))
        return false;
    if (mod != MOD_LAVA && mod != MOD_SLIME && mod != MOD_TRIGGER_HURT)
        return false;
    return IG_Respot(targ, mod == MOD_LAVA ? "lava" : mod == MOD_SLIME ? "slime" : "pit");
}

/*
IG_HazardRescueEnter: from P_WorldEffects as a player first touches lava or
slime, before the splash sound: rescued there, only the teleport is heard.
*/
bool IG_HazardRescueEnter(edict_t *ent)
{
    if (!(ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME)))
        return false;
    if (!IG_HazardRescue(ent, (ent->watertype & CONTENTS_LAVA) ? MOD_LAVA : MOD_SLIME))
        return false;
    ent->client->old_waterlevel = 0;    // no splash-out sound on the next frame
    ent->flags &= ~FL_INWATER;
    return true;
}
