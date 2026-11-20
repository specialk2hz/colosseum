/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ig_stats.c -- `sv igstats`: movement and engagement counters per
// client, over a window, for tools/igbench.py (IG-4).
//
// It counts and never acts: no game state is written, so it has no switch --
// with no window open every entry point returns at its first line.
// It complements `sv botstats` (src/bot/bl_main.c), which counts kills,
// damage, hits and pickups; igbench joins the two reports by client slot.
//
//   sv igstats reset   open a window (counters zeroed)
//   sv igstats         print the window so far
//   sv igstats off     close it
//
// Per client, over the frames it was alive in the window:
//   alive        frames alive
//   upper        frames with its feet more than IG_UPPER units above the mean
//                height of the map's deathmatch spawn points ("upper levels":
//                a proxy, the same for every run of a map)
//   dist         units travelled
//   pads         jump pad launches (trigger_push, debounced: a pad touches
//                every frame the player is inside it)
//   hooks        hook shots (the client's hook entity appearing)
//   hookframes   frames pulling or hanging on the hook
//   climbs       hooks that ended with the bot more than 32 units higher than
//                where it fired (a hook that got it somewhere up)
//   lives        spawns inside the window
//   contacts     lives (spawned inside the window) that reached first contact:
//                the first rail shot it fires, the first damage it takes, or
//                its death, whichever comes first
//   contactframes  frames from spawn to first contact, summed over those lives
//   shots        rail shots (OSP's accuracy table, p_acc)

#include "g_local.h"
#include "tourney/osp_types.h"
#include "ig/ig.h"

#define IG_UPPER        96      // units above the spawn-point mean
#define IG_PAD_DEBOUNCE 5       // frames between two counted launches

typedef struct {
    // counters
    int     alive, upper, pads, hooks, hookframes, climbs, lives, contacts, contactframes;
    float   hookz;              // feet height when the current hook was fired
    float   dist;
    int     shots0;             // p_acc rail shots when the window opened
    // per-frame state
    bool    was_alive, contacted, have_origin;
    vec3_t  origin;
    bool    had_hook;
    int     spawn_frame, last_pad, shots_seen, health;
} ig_cl_t;

static struct {
    bool    open;
    int     since;
    float   upperz;
    ig_cl_t cl[MAX_CLIENTS];
} igs;

static int IG_RailShots(const edict_t *ent)
{
    int id = ent->client->resp.clientid;

    // OSP's accuracy table only exists under the four OSP rulesets; outside
    // them there is no shot count to read, and the report says 0.
    if (G_IsOspRuleset()) {
        if (id >= 0 && id < (int)q_countof(p_acc))
            return p_acc[id].shots[ACC_RAILGUN];
    }
    return 0;
}

static float IG_SpawnMeanZ(void)
{
    edict_t *e = NULL;
    float   sum = 0;
    int     n = 0;

    while ((e = G_Find(e, FOFS(classname), "info_player_deathmatch")) != NULL) {
        sum += e->s.origin[2];
        n++;
    }
    return n ? sum / n : 0;
}

static void IG_StatsReset(void)
{
    int i;

    memset(&igs, 0, sizeof(igs));
    igs.open = true;
    igs.since = level.framenum;
    igs.upperz = IG_SpawnMeanZ() + IG_UPPER;
    for (i = 0; i < game.maxclients && i < MAX_CLIENTS; i++) {
        edict_t *ent = g_edicts + 1 + i;

        if (ent->inuse && ent->client) {
            ig_cl_t *c = &igs.cl[i];

            c->shots0 = IG_RailShots(ent);
            c->shots_seen = c->shots0;
            // A life already under way is not timed to contact: its spawn
            // is before the window.  It still counts alive/upper/dist frames.
            if (!ent->deadflag && ent->health > 0 && ent->solid != SOLID_NOT) {
                c->was_alive = true;
                c->contacted = true;
                c->health = ent->health;
            }
        }
    }
}

/*
=================
IG_StatsFrame

Once per server frame, from the end of G_RunFrame.
=================
*/
void IG_StatsFrame(void)
{
    int i;

    if (!igs.open || level.intermission_framenum)
        return;
    for (i = 0; i < game.maxclients && i < MAX_CLIENTS; i++) {
        edict_t *ent = g_edicts + 1 + i;
        ig_cl_t *c = &igs.cl[i];
        bool    alive;
        int     shots;

        if (!ent->inuse || !ent->client) {
            c->was_alive = false;
            continue;
        }
        alive = !ent->deadflag && ent->health > 0 && ent->solid != SOLID_NOT;
        shots = IG_RailShots(ent);
        if (alive && !c->was_alive) {
            // a spawn inside the window
            c->lives++;
            c->spawn_frame = level.framenum;
            c->contacted = false;
            c->have_origin = false;
            c->health = ent->health;
            c->shots_seen = shots;
        }
        if (alive) {
            c->alive++;
            if (ent->s.origin[2] + ent->mins[2] > igs.upperz)
                c->upper++;
            if (c->have_origin) {
                vec3_t d;

                VectorSubtract(ent->s.origin, c->origin, d);
                // a teleport is a jump, not travel
                if (VectorLength(d) < 512)
                    c->dist += VectorLength(d);
            }
            VectorCopy(ent->s.origin, c->origin);
            c->have_origin = true;

            // A hook is out while ctf_grapple is set (OSP and Threewave alike);
            // FLY is state 0, so the state alone cannot say "no hook".
            if (ent->client->ctf_grapple) {
                if (!c->had_hook) {
                    c->hooks++;
                    c->hookz = ent->s.origin[2];
                }
                if (ent->client->ctf_grapplestate == CTF_GRAPPLE_STATE_PULL ||
                    ent->client->ctf_grapplestate == CTF_GRAPPLE_STATE_HANG)
                    c->hookframes++;
            }
            else if (c->had_hook && ent->s.origin[2] - c->hookz > 32)
                c->climbs++;
            c->had_hook = ent->client->ctf_grapple != NULL;

            if (!c->contacted && (shots > c->shots_seen || ent->health < c->health)) {
                c->contacted = true;
                c->contacts++;
                c->contactframes += level.framenum - c->spawn_frame;
            }
            c->health = ent->health;
        } else {
            // killed before it fired or was hurt: the death is the contact
            if (c->was_alive && !c->contacted) {
                c->contacted = true;
                c->contacts++;
                c->contactframes += level.framenum - c->spawn_frame;
            }
            c->had_hook = false;
        }
        c->shots_seen = shots;
        c->was_alive = alive;
    }
}

/*
=================
IG_StatsPush

A trigger_push launched `other` (g_trigger.c, trigger_push_touch).
=================
*/
void IG_StatsPush(edict_t *other)
{
    ig_cl_t *c;
    int     i;

    if (!igs.open || !other || !other->client)
        return;
    i = other - g_edicts - 1;
    if (i < 0 || i >= MAX_CLIENTS)
        return;
    c = &igs.cl[i];
    if (level.framenum - c->last_pad > IG_PAD_DEBOUNCE)
        c->pads++;
    c->last_pad = level.framenum;
}

static void IG_StatsReport(void)
{
    int i;

    gi.dprintf("igstats frame %d since %d frames %d upperz %.0f %s\n",
               level.framenum, igs.since, level.framenum - igs.since,
               igs.upperz, igs.open ? "open" : "closed");
    for (i = 0; i < game.maxclients && i < MAX_CLIENTS; i++) {
        edict_t *ent = g_edicts + 1 + i;
        ig_cl_t *c = &igs.cl[i];

        if (!ent->inuse || !ent->client)
            continue;
        gi.dprintf("igstats client %d alive %d upper %d dist %d pads %d hooks %d "
                   "hookframes %d climbs %d lives %d contacts %d contactframes %d shots %d "
                   "name %s\n",
                   i, c->alive, c->upper, (int)c->dist, c->pads, c->hooks,
                   c->hookframes, c->climbs, c->lives, c->contacts, c->contactframes,
                   IG_RailShots(ent) - c->shots0, ent->client->pers.netname);
    }
}

/*
=================
IG_ServerCommand

`sv igstats [reset|off]`.  Returns true if it recognised the command.
=================
*/
bool IG_ServerCommand(const char *cmd)
{
    const char *arg;

    if (Q_stricmp(cmd, "igstats"))
        return false;
    arg = gi.argv(2);
    if (!Q_stricmp(arg, "reset")) {
        IG_StatsReset();
        gi.dprintf("igstats reset %d\n", level.framenum);
    } else if (!Q_stricmp(arg, "off")) {
        igs.open = false;
        gi.dprintf("igstats off\n");
    } else {
        IG_StatsReport();
    }
    return true;
}
