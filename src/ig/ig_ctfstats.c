/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ig_ctfstats.c -- CTF counters for the `sv igstats` window, for
// tools/igbench.py (CTF-PLAN.md step 1, the CTF benchmark).
//
// It counts and never acts, like ig_stats.c: CTF rules only, and only while a
// window is open (`sv igstats reset` opens it here too).
//
// Events, as they happen:
//   igctf <frame> take|repickup|capture|return|lost|autoreturn <client> team <its team> flag <flag's team>
//         [carry <frames> dist <units>]   (capture and lost: the carry that ended)
// At `sv igstats`, per team (1 red, 2 blue), over the window:
//   home       frames its flag was at its base
//   unguarded  of those, frames no living teammate was within IG_CTF_GUARD of it
//   carried    frames an enemy carried it
//   dropped    frames it lay dropped
// and per client: takes, caps, returns, kills of carriers are botstats' business
// (frags), so here: carryframes, carrydist, helpframes (a teammate within
// IG_CTF_NEAR while it carried), chaseframes (an enemy within IG_CTF_NEAR while
// it carried), nearframes (frames it was within IG_CTF_NEAR of its own team's
// carrier: helping).

#include "g_local.h"
#include "ig/ig.h"

#define IG_CTF_GUARD    800     // units from the base flag that count as guarding it
#define IG_CTF_NEAR     512     // units that count as "with the carrier"

typedef struct {
    int     carryframes, helpframes, chaseframes, nearframes;
    float   carrydist;
    int     takes, caps, returns, lost;
    // the carry under way
    int     carry0;
    float   carryd;
    vec3_t  last;
} igc_cl_t;

static struct {
    bool        open;
    int         since;
    int         home[3], unguarded[3], carried[3], dropped[3];
    int         caps[3];
    igc_cl_t    cl[MAX_CLIENTS];
} igc;

static bool IG_CtfOn(void)
{
    return igc.open && G_Ruleset() == RULESET_CTF;
}

static int IG_FlagItem(int team)
{
    const gitem_t *it = FindItemByClassname(team == CTF_TEAM1 ? "item_flag_team1" : "item_flag_team2");

    return it ? ITEM_INDEX(it) : 0;
}

void IG_CtfStatsReset(void)
{
    memset(&igc, 0, sizeof(igc));
    igc.open = true;
    igc.since = level.framenum;
}

void IG_CtfEvent(const char *kind, edict_t *player, int flagteam)
{
    int i = -1, team = 0;
    igc_cl_t *c = NULL;

    if (!IG_CtfOn() || level.intermission_framenum)
        return;
    if (player && player->client) {
        i = player - g_edicts - 1;
        team = player->client->resp.ctf_team;
        if (i >= 0 && i < MAX_CLIENTS)
            c = &igc.cl[i];
    }
    if (c && (!strcmp(kind, "capture") || !strcmp(kind, "lost"))) {
        gi.dprintf("igctf %d %s %d team %d flag %d carry %d dist %.0f\n", level.framenum, kind, i, team, flagteam,
                   c->carry0 ? level.framenum - c->carry0 : 0, c->carryd);
        c->carry0 = 0;
    } else
        gi.dprintf("igctf %d %s %d team %d flag %d\n", level.framenum, kind, i, team, flagteam);
    if (!c)
        return;
    if (!strcmp(kind, "take") || !strcmp(kind, "repickup")) {
        c->takes++;
        c->carry0 = level.framenum;
        c->carryd = 0;
        VectorCopy(player->s.origin, c->last);
    } else if (!strcmp(kind, "capture")) {
        c->caps++;
        if (team == CTF_TEAM1 || team == CTF_TEAM2)
            igc.caps[team]++;
    } else if (!strcmp(kind, "return")) {
        c->returns++;
    } else if (!strcmp(kind, "lost")) {
        c->lost++;
    }
}

static bool IG_Alive(const edict_t *e)
{
    return e->inuse && e->client && e->health > 0 && !e->deadflag && e->client->resp.ctf_team;
}

void IG_CtfStatsFrame(void)
{
    int team, i, j;

    if (!IG_CtfOn() || level.intermission_framenum)
        return;
    for (team = CTF_TEAM1; team <= CTF_TEAM2; team++) {
        char *cls = team == CTF_TEAM1 ? "item_flag_team1" : "item_flag_team2";
        edict_t *e = NULL, *base = NULL;
        bool dropped = false, carried = false;
        int item = IG_FlagItem(team);

        while ((e = G_Find(e, FOFS(classname), cls)) != NULL) {
            if (e->spawnflags & DROPPED_ITEM)
                dropped = true;
            else
                base = e;
        }
        for (i = 0; i < game.maxclients && i < MAX_CLIENTS; i++) {
            edict_t *p = g_edicts + 1 + i;

            if (IG_Alive(p) && item && p->client->pers.inventory[item])
                carried = true;
        }
        if (carried)
            igc.carried[team]++;
        else if (dropped)
            igc.dropped[team]++;
        else if (base && !(base->svflags & SVF_NOCLIENT)) {
            bool guarded = false;

            igc.home[team]++;
            for (i = 0; i < game.maxclients && i < MAX_CLIENTS && !guarded; i++) {
                edict_t *p = g_edicts + 1 + i;
                vec3_t d;

                if (!IG_Alive(p) || p->client->resp.ctf_team != team)
                    continue;
                VectorSubtract(p->s.origin, base->s.origin, d);
                if (VectorLength(d) < IG_CTF_GUARD)
                    guarded = true;
            }
            if (!guarded)
                igc.unguarded[team]++;
        }
    }
    // the carriers: their carry, and who is with them
    for (i = 0; i < game.maxclients && i < MAX_CLIENTS; i++) {
        edict_t *p = g_edicts + 1 + i;
        igc_cl_t *c = &igc.cl[i];
        int enemyflag;
        vec3_t d;

        if (!IG_Alive(p))
            continue;
        enemyflag = IG_FlagItem(p->client->resp.ctf_team == CTF_TEAM1 ? CTF_TEAM2 : CTF_TEAM1);
        if (!enemyflag || !p->client->pers.inventory[enemyflag])
            continue;
        if (!c->carry0) {	// carrying since before the window
            c->carry0 = level.framenum;
            c->carryd = 0;
            VectorCopy(p->s.origin, c->last);
        }
        VectorSubtract(p->s.origin, c->last, d);
        d[2] = 0;
        if (VectorLength(d) < 400) {	// not a teleport
            c->carryd += VectorLength(d);
            c->carrydist += VectorLength(d);
        }
        VectorCopy(p->s.origin, c->last);
        c->carryframes++;
        {
            bool help = false, chase = false;	// any teammate / enemy near this frame

            for (j = 0; j < game.maxclients && j < MAX_CLIENTS; j++) {
                edict_t *q = g_edicts + 1 + j;

                if (j == i || !IG_Alive(q))
                    continue;
                VectorSubtract(q->s.origin, p->s.origin, d);
                if (VectorLength(d) >= IG_CTF_NEAR)
                    continue;
                if (q->client->resp.ctf_team == p->client->resp.ctf_team) {
                    igc.cl[j].nearframes++;
                    help = true;
                } else
                    chase = true;
            }
            if (help) c->helpframes++;
            if (chase) c->chaseframes++;
        }
    }
}

void IG_CtfStatsReport(void)
{
    int i;

    if (!igc.open || G_Ruleset() != RULESET_CTF)
        return;
    gi.dprintf("igctf summary frames %d caps1 %d caps2 %d home1 %d home2 %d unguarded1 %d unguarded2 %d "
               "carried1 %d carried2 %d dropped1 %d dropped2 %d\n",
               level.framenum - igc.since, igc.caps[1], igc.caps[2], igc.home[1], igc.home[2],
               igc.unguarded[1], igc.unguarded[2], igc.carried[1], igc.carried[2], igc.dropped[1], igc.dropped[2]);
    for (i = 0; i < game.maxclients && i < MAX_CLIENTS; i++) {
        edict_t *ent = g_edicts + 1 + i;
        igc_cl_t *c = &igc.cl[i];

        if (!ent->inuse || !ent->client)
            continue;
        gi.dprintf("igctf client %d team %d takes %d caps %d returns %d lost %d carryframes %d carrydist %d "
                   "helpframes %d chaseframes %d nearframes %d name %s\n",
                   i, ent->client->resp.ctf_team, c->takes, c->caps, c->returns, c->lost, c->carryframes,
                   (int)c->carrydist, c->helpframes, c->chaseframes, c->nearframes, ent->client->pers.netname);
    }
}
