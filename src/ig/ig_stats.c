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
//   railhits     rail hits, same table (botstats' hits include hook damage)
//   nearest      units to the nearest other live player, summed over the
//                frames it was alive with someone else alive (spread)
//   crowd        frames with IG_CROWD_N or more others within IG_CROWD_DIST
//   fast, faster frames moving faster than 400 / 600 units/s (running is 300:
//                only hooks, pads and jumps get there)

#include "g_local.h"
#include "tourney/osp_types.h"
#include "ig/ig.h"

#define IG_UPPER        96      // units above the spawn-point mean
#define IG_PAD_DEBOUNCE 5       // frames between two counted launches

typedef struct {
    // counters
    int     alive, upper, pads, hooks, hookframes, climbs, lives, contacts, contactframes;
    float   hookz;              // feet height when the current hook was fired
    vec3_t  hookstart, hookanchor;  // the current hook: where it was fired, where it held
    int     hookpull, hookhang;     // ...and its frames pulling and hanging
    // a finished hook's line waits IG_LAND_FRAMES for where the bot came down
    bool    pend;
    int     pend_frame, pend_pull, pend_hang;
    // how a hook was used, for modelling a person's hooking: on the ground or
    // in the air when it went out, speed then and at the let-go, and a jump
    // pad within the landing second
    int     hookair, hookspeed, pend_air, pend_firespeed, pend_relspeed, pend_pad;
    vec3_t  pend_start, pend_anchor, pend_end;
    float   dist;
    float   nearest;
    int     nearframes, crowd, fast, faster;
    int     padflight, padhooked;   // frame a pad launched it (0: not flying one), a hook used since
    vec3_t  padstart;
    int     shots0;             // p_acc rail shots when the window opened
    int     railhits0;          // p_acc rail hits when the window opened
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

// Rail hits from the same table. botstats' "hits" counts any damage to
// another player, and in instagib the hook does damage (hook_initdamage):
// hooks that struck players read as rail accuracy (47% "rail" against 36%).
static int IG_RailHits(const edict_t *ent)
{
    int id = ent->client->resp.clientid;

    if (G_IsOspRuleset() && id >= 0 && id < (int)q_countof(p_acc))
        return p_acc[id].hits[ACC_RAILGUN];
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
            c->railhits0 = IG_RailHits(ent);
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

#define IG_LAND_FRAMES  10      // where a bot is a second after letting go
#define IG_CROWD_DIST   512
#define IG_CROWD_N      2

// Spread: for a live client, the nearest other live client and how many are
// within IG_CROWD_DIST.
static void IG_StatsSpread(int i, ig_cl_t *c, const edict_t *ent)
{
    int j, near = 0;
    float best = -1, d;
    vec3_t v;

    for (j = 0; j < game.maxclients && j < MAX_CLIENTS; j++) {
        const edict_t *o = g_edicts + 1 + j;

        if (j == i || !o->inuse || !o->client || o->deadflag || o->health <= 0 || o->solid == SOLID_NOT)
            continue;
        VectorSubtract(o->s.origin, ent->s.origin, v);
        d = VectorLength(v);
        if (best < 0 || d < best)
            best = d;
        if (d < IG_CROWD_DIST)
            near++;
    }
    if (best >= 0) {
        c->nearest += best;
        c->nearframes++;
    }
    if (near >= IG_CROWD_N)
        c->crowd++;
}

/*
One line per hook, for finding where hooks fail: fired at `start`, held at
`anchor` (0 0 0: never attached), let go at `end`, and `land` a second later
(`dead` if it died first).
*/
static void IG_StatsHookLine(int i, ig_cl_t *c, const edict_t *ent)
{
    char land[64];

    if (!c->pend)
        return;
    if (ent)
        Q_snprintf(land, sizeof(land), "%.0f %.0f %.0f", ent->s.origin[0], ent->s.origin[1], ent->s.origin[2]);
    else
        Q_strlcpy(land, "dead", sizeof(land));
    gi.dprintf("igstats hook %d frame %d pull %d hang %d start %.0f %.0f %.0f "
               "anchor %.0f %.0f %.0f end %.0f %.0f %.0f air %d firespeed %d relspeed %d pad %d "
               "human %d land %s\n", i, level.framenum,
               c->pend_pull, c->pend_hang, c->pend_start[0], c->pend_start[1], c->pend_start[2],
               c->pend_anchor[0], c->pend_anchor[1], c->pend_anchor[2],
               c->pend_end[0], c->pend_end[1], c->pend_end[2], c->pend_air, c->pend_firespeed,
               c->pend_relspeed, c->pend_pad, !(g_edicts[1 + i].flags & FL_BOT), land);
    c->pend = false;
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
            c->padflight = 0;
            c->have_origin = false;
            c->health = ent->health;
            c->shots_seen = shots;
        }
        if (alive) {
            c->alive++;
            IG_StatsSpread(i, c, ent);
            {
                float sp = VectorLength(ent->velocity);

                if (sp > 400) c->fast++;
                if (sp > 600) c->faster++;
            }
            if (ent->s.origin[2] + ent->mins[2] > igs.upperz)
                c->upper++;
            // a pad flight, launch to landing, and whether a hook was out on it
            // (the pad hook, IG-13): "igstats padflight" lines
            if (c->padflight) {
                if (ent->client->ctf_grapple)
                    c->padhooked = 1;
                if (ent->groundentity && level.framenum - c->padflight > 2) {
                    vec3_t d;

                    VectorSubtract(ent->s.origin, c->padstart, d);
                    d[2] = 0;
                    gi.dprintf("igstats padflight %d frames %d hooked %d dist %.0f rise %.0f human %d\n", i,
                               level.framenum - c->padflight, c->padhooked, VectorLength(d),
                               ent->s.origin[2] - c->padstart[2], !(ent->flags & FL_BOT));
                    c->padflight = 0;
                }
                else if (level.framenum - c->padflight > 100)
                    c->padflight = 0;
            }
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
                    VectorCopy(ent->s.origin, c->hookstart);
                    c->hookpull = c->hookhang = 0;
                    c->hookair = ent->groundentity == NULL;
                    c->hookspeed = (int)VectorLength(ent->velocity);
                }
                if (ent->client->ctf_grapplestate == CTF_GRAPPLE_STATE_PULL ||
                    ent->client->ctf_grapplestate == CTF_GRAPPLE_STATE_HANG) {
                    c->hookframes++;
                    VectorCopy(ent->client->ctf_grapple->s.origin, c->hookanchor);
                    if (ent->client->ctf_grapplestate == CTF_GRAPPLE_STATE_HANG)
                        c->hookhang++;
                    else
                        c->hookpull++;
                }
            }
            else if (c->had_hook) {
                if (ent->s.origin[2] - c->hookz > 32)
                    c->climbs++;
                IG_StatsHookLine(i, c, ent);    // a previous one still waiting
                c->pend = true;
                c->pend_frame = level.framenum + IG_LAND_FRAMES;
                c->pend_pull = c->hookpull;
                c->pend_air = c->hookair;
                c->pend_firespeed = c->hookspeed;
                c->pend_relspeed = (int)VectorLength(ent->velocity);
                c->pend_pad = 0;
                c->pend_hang = c->hookhang;
                VectorCopy(c->hookstart, c->pend_start);
                VectorCopy(c->hookanchor, c->pend_anchor);
                VectorCopy(ent->s.origin, c->pend_end);
                VectorClear(c->hookanchor);
            }
            if (c->pend && level.framenum >= c->pend_frame)
                IG_StatsHookLine(i, c, ent);
            c->had_hook = ent->client->ctf_grapple != NULL;

            if (!c->contacted && (shots > c->shots_seen || ent->health < c->health)) {
                c->contacted = true;
                c->contacts++;
                c->contactframes += level.framenum - c->spawn_frame;
            }
            c->health = ent->health;
        } else {
            if (c->pend)
                IG_StatsHookLine(i, c, NULL);
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
    if (!c->padflight) {
        c->padflight = level.framenum;
        c->padhooked = 0;
        VectorCopy(other->s.origin, c->padstart);
    }
    c->last_pad = level.framenum;
    if (c->pend)
        c->pend_pad = 1;        // the pad the hook was let go for
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
                   "railhits %d nearest %d nearframes %d crowd %d fast %d faster %d "
                   "name %s\n",
                   i, c->alive, c->upper, (int)c->dist, c->pads, c->hooks,
                   c->hookframes, c->climbs, c->lives, c->contacts, c->contactframes,
                   IG_RailShots(ent) - c->shots0, IG_RailHits(ent) - c->railhits0, (int)c->nearest, c->nearframes, c->crowd, c->fast, c->faster,
                   ent->client->pers.netname);
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

/*
IG-15: `ig_moverec 1`. Every movement command of a person (never a bot) as an
"igmove" line: what Billy does with the keys and the view while he slingshots
and strafe jumps, for the bots to copy. Per usercmd, so at the client's rate:
  igmove <slot> <frame> <msec> spd <horizontal> vz <vertical> g <on ground>
         hook <0 none, 1 flying, 2 pulling/hanging> f <forward> s <side> u <up>
         yaw <yaw> pitch <pitch> x <origin x> y <origin y> z <origin z>
*/
void IG_MoveRecord(edict_t *ent, const usercmd_t *ucmd, bool onground)
{
    static cvar_t *rec;
    int hook;

    if (!rec)
        rec = gi.cvar("ig_moverec", "0", 0);
    if (!rec->integer || !ent->client || (ent->flags & FL_BOT) || ent->health <= 0 ||
        ent->movetype == MOVETYPE_NOCLIP || G_IsObserver(ent))
        return;
    hook = !ent->client->ctf_grapple ? 0 :
           ent->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY ? 2 : 1;
    gi.dprintf("igmove %d %d %d spd %.0f vz %.0f g %d hook %d f %d s %d u %d yaw %.1f pitch %.1f x %.0f y %.0f z %.0f\n",
               (int)(ent - g_edicts) - 1, level.framenum, ucmd->msec,
               sqrtf(ent->velocity[0] * ent->velocity[0] + ent->velocity[1] * ent->velocity[1]),
               ent->velocity[2], onground, hook, ucmd->forwardmove, ucmd->sidemove, ucmd->upmove,
               ent->client->v_angle[YAW], ent->client->v_angle[PITCH],
               ent->s.origin[0], ent->s.origin[1], ent->s.origin[2]);
}
