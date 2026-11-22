/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ig_route.c -- IG-26 `ig_botroutes`: bots replay recorded routes.
//
// A route is a person's run recorded with `ig_moverec` (IG-15) and cut out of
// the log by server/tools/route-extract.py into q3bot/maps/<map>.route, next
// to the map's navigation file. A bot that
// stands near the first part of a route, with no enemy in sight, takes it with
// chance `ig_routechance` (checked at most once a second). While on it the bot:
//   * moves and looks as the recording did at the matching point of the path
//     (the point nearest the bot, never going back), steering toward the path
//     on the ground when it has drifted;
//   * fires the hook where the recording fired it, aimed at the same anchor
//     (traced from the recorded eye when the file is loaded), and lets go
//     where the recording let go;
//   * gives control back at the end, or when it falls more than 128 units off
//     the path, a hook fails to latch, an enemy comes into sight while it is
//     on the ground, or it takes twice the recorded time.
// The bot brain's own hook commands are ignored while a route drives the bot.
// Default 0: no routes. OSP rulesets with the hook enabled only.
//
// File format (q3bot/maps/<map>.route), one route per block:
//   route <name>
//   p <msec> <forward> <side> <up> <yaw> <pitch> <x> <y> <z> <hook 0/1/2> <buttons>
//     [<onground 0/1> <zvel>]   (optional; without them the bot copies the jump key)
//   ...
//   end

#include "g_local.h"
#include "tourney/osp_types.h"
#include "ig/ig.h"

#define RT_MAXROUTES    16
#define RT_MAXPOINTS    4096
#define RT_MAXHOOKS     32
#define RT_STARTFRAC    0.25f   // a bot may join anywhere in the first quarter
#define RT_STARTDIST    64.0f
#define RT_OFFPATH      128.0f  // on the ground
#define RT_OFFPATHAIR   256.0f  // in the air or hooked: momentum differs from the recording's
#define RT_LOOKAHEAD    150     // ms of recording to steer toward on the ground

typedef struct {
    short   msec;
    short   f, s, u;
    float   yaw, pitch;
    vec3_t  org;
    byte    hook;
    byte    buttons;
    signed char ground;         // -1 unknown (old files)
    short   zvel;
} rt_point_t;

typedef struct {
    int     fire, release;      // point indices; release -1 = held to the end
    vec3_t  anchor;
    bool    ok;                 // the anchor trace hit something
} rt_hook_t;

typedef struct {
    char        name[32];
    int         npoints;
    rt_point_t  *p;
    int         nhooks;
    rt_hook_t   hooks[RT_MAXHOOKS];
    int         totalms;
} rt_route_t;

static struct {
    char        map[MAX_QPATH];
    int         nroutes;
    rt_route_t  routes[RT_MAXROUTES];
} rt;

typedef struct {
    int     route;              // -1 none
    int     idx;
    int     hook;               // next hook event
    bool    hooked;             // our hook is out
    int     firedframe;
    int     startframe;
    int     nextcheck;
    bool    fireangle;          // aim set this frame for a pending fire
} rt_bot_t;

static rt_bot_t rb[MAX_CLIENTS];
static cvar_t *ig_botroutes, *ig_routechance, *ig_routedebug;

static void RT_Cvars(void)
{
    int i;

    if (!ig_botroutes) {
        for (i = 0; i < MAX_CLIENTS; i++)
            rb[i].route = -1;
        ig_botroutes = gi.cvar("ig_botroutes", "0", 0);
        ig_routechance = gi.cvar("ig_routechance", "0.5", 0);
        ig_routedebug = gi.cvar("ig_routedebug", "0", 0);
    }
}

static void RT_Free(void)
{
    int i;

    for (i = 0; i < rt.nroutes; i++)
        if (rt.routes[i].p)
            gi.TagFree(rt.routes[i].p);
    memset(&rt, 0, sizeof(rt));
    for (i = 0; i < MAX_CLIENTS; i++)
        rb[i].route = -1;
}

// The anchor: what the recorded view hit when the hook was fired.
static void RT_Anchors(rt_route_t *r)
{
    int i, h;
    vec3_t eye, ang, fwd, end;
    trace_t tr;

    r->nhooks = 0;
    for (i = 1; i < r->npoints && r->nhooks < RT_MAXHOOKS; i++) {
        if (r->p[i - 1].hook != 0 || r->p[i].hook == 0)
            continue;
        h = r->nhooks++;
        r->hooks[h].fire = i - 1;
        r->hooks[h].release = -1;
        VectorCopy(r->p[i - 1].org, eye);
        eye[2] += 22;
        VectorSet(ang, r->p[i - 1].pitch, r->p[i - 1].yaw, 0);
        AngleVectors(ang, fwd, NULL, NULL);
        VectorMA(eye, 8192, fwd, end);
        tr = gi.trace(eye, NULL, NULL, end, NULL, MASK_SHOT);
        VectorCopy(tr.endpos, r->hooks[h].anchor);
        r->hooks[h].ok = tr.fraction < 1.0f && !tr.startsolid;
    }
    for (h = 0; h < r->nhooks; h++)
        for (i = r->hooks[h].fire + 1; i < r->npoints; i++)
            if (r->p[i].hook == 0) {
                r->hooks[h].release = i;
                break;
            }
}

static void RT_Load(void)
{
    char path[MAX_OSPATH], rel[MAX_QPATH], line[256], name[32];
    FILE *f;
    rt_route_t *r = NULL;
    rt_point_t pt;
    int f_, s_, u_, hook, buttons, msec, ground, zvel, got, n = 0;

    RT_Free();
    Q_strlcpy(rt.map, level.mapname, sizeof(rt.map));
    Q_snprintf(rel, sizeof(rel), "q3bot/maps/%s.route", level.mapname);
    if (!G_FsReadPath(path, sizeof(path), rel))
        return;
    f = fopen(path, "r");
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "route %31s", name) == 1) {
            if (rt.nroutes >= RT_MAXROUTES)
                break;
            r = &rt.routes[rt.nroutes++];
            memset(r, 0, sizeof(*r));
            Q_strlcpy(r->name, name, sizeof(r->name));
            r->p = gi.TagMalloc(sizeof(rt_point_t) * RT_MAXPOINTS, TAG_GAME);
            continue;
        }
        if (!strncmp(line, "end", 3)) {
            if (r && r->npoints < 20) {         // too short to be a route
                gi.TagFree(r->p);
                rt.nroutes--;
            } else if (r) {
                RT_Anchors(r);
                n++;
            }
            r = NULL;
            continue;
        }
        if (!r || r->npoints >= RT_MAXPOINTS || line[0] != 'p')
            continue;
        memset(&pt, 0, sizeof(pt));
        ground = -1;
        zvel = 0;
        got = sscanf(line, "p %d %d %d %d %f %f %f %f %f %d %d %d %d", &msec, &f_, &s_, &u_,
                     &pt.yaw, &pt.pitch, &pt.org[0], &pt.org[1], &pt.org[2], &hook, &buttons,
                     &ground, &zvel);
        if (got < 11)
            continue;
        pt.ground = got >= 13 ? ground : -1;
        pt.zvel = got >= 13 ? zvel : 0;
        pt.msec = msec;
        pt.f = f_;
        pt.s = s_;
        pt.u = u_;
        pt.hook = hook;
        pt.buttons = buttons;
        r->p[r->npoints++] = pt;
        r->totalms += msec;
    }
    if (r) {                                    // no "end": drop it
        gi.TagFree(r->p);
        rt.nroutes--;
    }
    fclose(f);
    gi.dprintf("ig_botroutes: %d route(s) for %s\n", n, level.mapname);
}

static void RT_Stop(edict_t *bot, rt_bot_t *b, const char *why)
{
    if (ig_routedebug->integer)
        gi.dprintf("ig_route: %s leaves %s at %d/%d (%s)\n", bot->client->pers.netname,
                   rt.routes[b->route].name, b->idx, rt.routes[b->route].npoints, why);
    if (b->hooked && G_IsOspRuleset())
        OSP_hookoff_cmd(bot);
    b->route = -1;
    b->hooked = false;
    b->nextcheck = level.framenum + 3 * BASE_FRAMERATE;
}

static bool RT_EnemyInSight(edict_t *bot)
{
    int i;
    edict_t *e;

    for (i = 1; i <= game.maxclients; i++) {
        e = g_edicts + i;
        if (e == bot || !e->inuse || !e->client || e->health <= 0 || e->solid == SOLID_NOT)
            continue;
        if (visible(bot, e) && infront(bot, e))
            return true;
    }
    return false;
}

static void RT_TryStart(edict_t *bot, rt_bot_t *b)
{
    int i, j, best = -1, bestj = 0, last;
    float d, bestd = RT_STARTDIST;
    vec3_t v;

    b->nextcheck = level.framenum + BASE_FRAMERATE;
    if (!bot->groundentity || RT_EnemyInSight(bot))
        return;
    for (i = 0; i < rt.nroutes; i++) {
        last = (int)(rt.routes[i].npoints * RT_STARTFRAC);
        for (j = 0; j < last; j++) {
            VectorSubtract(rt.routes[i].p[j].org, bot->s.origin, v);
            if (fabsf(v[2]) > 32)
                continue;
            v[2] = 0;
            d = VectorLength(v);
            if (d < bestd) {
                bestd = d;
                best = i;
                bestj = j;
            }
        }
    }
    if (best < 0 || frand() >= ig_routechance->value)
        return;
    b->route = best;
    b->idx = bestj;
    b->hook = 0;
    while (b->hook < rt.routes[best].nhooks && rt.routes[best].hooks[b->hook].fire < bestj)
        b->hook++;
    b->hooked = false;
    b->startframe = level.framenum;
    if (ig_routedebug->integer)
        gi.dprintf("ig_route: %s takes %s at %d\n", bot->client->pers.netname, rt.routes[best].name, bestj);
}

static void RT_SetAngles(edict_t *bot, usercmd_t *ucmd, float pitch, float yaw)
{
    ucmd->angles[PITCH] = ANGLE2SHORT(pitch) - bot->client->ps.pmove.delta_angles[PITCH];
    ucmd->angles[YAW] = ANGLE2SHORT(yaw) - bot->client->ps.pmove.delta_angles[YAW];
    ucmd->angles[ROLL] = 0;
}

/*
=================
IG_RouteInput

From BotExecuteInput, with the command built from the brain's input: true when
a route rewrote it.
=================
*/
bool IG_RouteInput(edict_t *bot, usercmd_t *ucmd)
{
    int c = bot - g_edicts - 1, j, k, ms, end;
    float d, bestd;
    rt_bot_t *b;
    rt_route_t *r;
    rt_point_t *p;
    vec3_t v, dir, ang, fwd, right;

    RT_Cvars();
    if (c < 0 || c >= MAX_CLIENTS)
        return false;
    b = &rb[c];
    if (!ig_botroutes->integer || !G_IsOspRuleset() || bot->health <= 0) {
        if (b->route >= 0 && rt.nroutes)
            RT_Stop(bot, b, "off or dead");
        b->route = -1;
        return false;
    }
    if (strcmp(rt.map, level.mapname))
        RT_Load();
    if (!rt.nroutes)
        return false;
    if (b->route < 0 || b->route >= rt.nroutes) {
        b->route = -1;
        if (level.framenum >= b->nextcheck)
            RT_TryStart(bot, b);
        if (b->route < 0)
            return false;
    }
    r = &rt.routes[b->route];

    // progress: the nearest point ahead, within the next second or so
    bestd = 1e9f;
    end = b->idx + 160;
    if (end > r->npoints)
        end = r->npoints;
    k = b->idx;
    for (j = b->idx; j < end; j++) {
        VectorSubtract(r->p[j].org, bot->s.origin, v);
        d = VectorLength(v);
        if (d < bestd) {
            bestd = d;
            k = j;
        }
    }
    b->idx = k;
    // ig_routedebug 2: where the bot is against the recording, twice a second
    if (ig_routedebug->integer >= 2 && !(level.framenum % 5)) {
        float rs = 0;
        if (k + 1 < r->npoints && r->p[k].msec > 0) {
            VectorSubtract(r->p[k + 1].org, r->p[k].org, v);
            v[2] = 0;
            rs = VectorLength(v) * 1000.0f / r->p[k + 1].msec;
        }
        gi.dprintf("ig_route2: %s %s %d off %.0f spd %.0f rec %.0f vz %.0f g %d hook %d rec %d\n",
                   bot->client->pers.netname, r->name, k, bestd,
                   sqrtf(bot->velocity[0] * bot->velocity[0] + bot->velocity[1] * bot->velocity[1]),
                   rs, bot->velocity[2], bot->groundentity != NULL,
                   !bot->client->ctf_grapple ? 0 : bot->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY ? 2 : 1,
                   r->p[k].hook);
    }
    if (b->idx >= r->npoints - 3) {
        RT_Stop(bot, b, "done");
        return false;
    }
    if (bestd > ((bot->groundentity && !b->hooked) ? RT_OFFPATH : RT_OFFPATHAIR)) {
        RT_Stop(bot, b, "off the path");
        return false;
    }
    if ((level.framenum - b->startframe) * 100 > 2 * r->totalms + 2000) {
        RT_Stop(bot, b, "too slow");
        return false;
    }
    if (bot->groundentity && !b->hooked && level.framenum - b->startframe > 5 &&
        RT_EnemyInSight(bot)) {
        RT_Stop(bot, b, "enemy");
        return false;
    }
    // a hook that never latched
    if (b->hooked && level.framenum - b->firedframe > 8 && bot->client->ctf_grapple &&
        bot->client->ctf_grapplestate <= CTF_GRAPPLE_STATE_FLY) {
        RT_Stop(bot, b, "hook missed");
        return false;
    }
    if (b->hooked && !bot->client->ctf_grapple && level.framenum - b->firedframe > 3) {
        if (ig_routedebug->integer)
            gi.dprintf("ig_route: %s hook %d gone after %d frames at %d\n", bot->client->pers.netname,
                       b->hook, level.framenum - b->firedframe, b->idx);
        b->hooked = false;              // released by the game (hit, timeout)
    }

    p = &r->p[b->idx];
    ucmd->forwardmove = p->f;
    ucmd->sidemove = p->s;
    ucmd->upmove = p->u;
    ucmd->buttons &= ~BUTTON_ATTACK;    // the view is ours: no stray shots
    RT_SetAngles(bot, ucmd, p->pitch, p->yaw);
    b->fireangle = false;

    // on the ground and off the line: steer toward a point a little ahead
    if (bot->groundentity && bestd > 24) {
        for (j = b->idx, ms = 0; j < r->npoints - 1 && ms < RT_LOOKAHEAD; j++)
            ms += r->p[j].msec;
        VectorSubtract(r->p[j].org, bot->s.origin, dir);
        dir[2] = 0;
        VectorNormalize(dir);
        VectorSet(ang, 0, p->yaw, 0);
        AngleVectors(ang, fwd, right, NULL);
        ucmd->forwardmove = DotProduct(fwd, dir) * 400;
        ucmd->sidemove = DotProduct(right, dir) * 400;
    }

    // Jumps: a bot thinks ten times a second and misses the key release a
    // jump needs, so it jumps when the recording leaves the ground rising
    // within the next 100 ms, and otherwise keeps its feet down.
    if (p->ground >= 0 && bot->groundentity) {
        bool takeoff = false;

        for (j = b->idx, ms = 0; j < r->npoints && ms < 100; j++) {
            if (r->p[j].ground == 0 && r->p[j].zvel > 100) {
                takeoff = true;
                break;
            }
            ms += r->p[j].msec;
        }
        if (takeoff) {
            ucmd->upmove = 400;
            bot->client->ps.pmove.pm_flags &= ~PMF_JUMP_HELD;
        } else if (ucmd->upmove > 0) {
            ucmd->upmove = 0;
        }
    }

    // the next hook: aim at its anchor from a few points before the fire
    if (b->hook < r->nhooks && !b->hooked && b->idx >= r->hooks[b->hook].fire - 2) {
        rt_hook_t *h = &r->hooks[b->hook];

        if (!h->ok) {
            b->hook++;
        } else {
            VectorCopy(bot->s.origin, v);
            v[2] += bot->viewheight;
            VectorSubtract(h->anchor, v, dir);
            vectoangles(dir, ang);
            if (ang[PITCH] > 180)
                ang[PITCH] -= 360;
            RT_SetAngles(bot, ucmd, ang[PITCH], ang[YAW]);
            b->fireangle = true;
        }
    }
    return true;
}

/*
=================
IG_RouteAfter

From BotExecuteInput after ClientThink: fires the hook now that the view
points at the anchor, and lets go where the recording did.
=================
*/
void IG_RouteAfter(edict_t *bot)
{
    int c = bot - g_edicts - 1;
    rt_bot_t *b;
    rt_route_t *r;
    rt_hook_t *h;

    if (c < 0 || c >= MAX_CLIENTS || !ig_botroutes || !ig_botroutes->integer)
        return;
    b = &rb[c];
    if (b->route < 0 || b->route >= rt.nroutes || !G_IsOspRuleset())
        return;
    r = &rt.routes[b->route];
    if (b->hooked) {
        h = &r->hooks[b->hook];
        if (h->release >= 0 && b->idx >= h->release) {
            if (G_IsOspRuleset())
                OSP_hookoff_cmd(bot);
            b->hooked = false;
            b->hook++;
        }
        return;
    }
    if (b->fireangle && b->hook < r->nhooks && b->idx >= r->hooks[b->hook].fire) {
        if (G_IsOspRuleset()) {
            if (bot->client->ctf_grapple)
                OSP_hookoff_cmd(bot);
            OSP_hookon_cmd(bot);
        }
        b->hooked = true;
        b->firedframe = level.framenum;
        if (ig_routedebug->integer) {
            vec3_t eye;
            trace_t tr;
            VectorCopy(bot->s.origin, eye);
            eye[2] += bot->viewheight;
            tr = gi.trace(eye, NULL, NULL, r->hooks[b->hook].anchor, bot, MASK_SHOT);
            gi.dprintf("ig_route: %s hook %d at %d (fire %d) clear %.2f dist %.0f\n", bot->client->pers.netname,
                       b->hook, b->idx, r->hooks[b->hook].fire, tr.fraction,
                       Distance(eye, r->hooks[b->hook].anchor));
        }
    }
}

/*
=================
IG_RouteBlocksCommand

From BotClientCommand: the brain's hook and unhook are ignored while a route
drives the bot.
=================
*/
bool IG_RouteBlocksCommand(edict_t *bot)
{
    int c = bot - g_edicts - 1;
    const char *cmd;

    if (c < 0 || c >= MAX_CLIENTS || !ig_botroutes || !ig_botroutes->integer || rb[c].route < 0)
        return false;
    // the two the brain sends are libvars cmd_grappleon / cmd_grappleoff,
    // set in IG_BotLibVars: compared against those, not spelled out here
    cmd = gi.argv(0);
    return IG_BotHookCommand(cmd);
}
