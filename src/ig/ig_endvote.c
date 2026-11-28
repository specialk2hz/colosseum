/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ig_endvote.c -- IG-22 `ig_endvote 1`: the end-of-match map vote.
//
// When a match ends, every person on the server (bots do not vote) gets a menu
// of `ig_endvotemaps` maps drawn at random from the rotation's map list: the
// rotation's own pick first, then others not played yet. Votes show live; a
// vote can be changed. The vote closes after `ig_endvotetime` seconds, or once
// everybody has voted; the most voted map wins (a tie is drawn at random, no
// votes keeps the rotation's pick), and the level changes 3 seconds later.
//
// Not held when a map was already chosen (a passed map or config vote, `map`),
// on `dmflags` same-level, without a map list, or with nobody but bots.
// Default 0: upstream's intermission, untouched. OSP rulesets only.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "tourney/p_menu.h"
#include "ig/ig.h"

#define IG_EV_MAX       8
#define IG_EV_OPENAT    3.0f    // after OSP closes every menu at 2 s (ClientThink)
#define IG_EV_GRACE     3.0f    // seconds from the result to the level change

enum { EV_IDLE, EV_OPEN, EV_DONE, EV_SKIP };

static struct {
    int     state;
    int     intermission;           // level.intermission_framenum it belongs to
    int     nmaps;
    int     maps[IG_EV_MAX];        // indices into map[]
    int     vote[MAX_CLIENTS];      // option per client, -1 none
    bool    menu[MAX_CLIENTS];      // our menu is the one open
    int     closes;                 // framenum the vote closes at
    int     exits;                  // framenum the level changes at
    int     shown_left;             // seconds left when the menus were drawn
    bool    dirty;
} ev;

static cvar_t *ig_endvote, *ig_endvotemaps, *ig_endvotetime;

static void EV_Cvars(void)
{
    if (!ig_endvote) {
        ig_endvote = gi.cvar("ig_endvote", "1", 0);
        ig_endvotemaps = gi.cvar("ig_endvotemaps", "4", 0);
        ig_endvotetime = gi.cvar("ig_endvotetime", "15", 0);
    }
}

static bool EV_Human(edict_t *e)
{
    return e->inuse && e->client && e->client->pers.connected &&
           !(e->flags & (FL_BOTCLIENT | FL_BOT));
}

static int EV_Client(edict_t *ent)
{
    int c = ent - g_edicts - 1;

    return (c >= 0 && c < game.maxclients && c < MAX_CLIENTS) ? c : -1;
}

/*
=================
EV_Pick

The options: the rotation's pick (level.changemap) when it is on the list, then
random maps that are neither this one nor already an option, preferring maps
the rotation has not used yet.
=================
*/
static void EV_Pick(void)
{
    int want, i, j, n, pool[1024], npool, pass;
    bool taken;

    want = ig_endvotemaps->integer;
    if (want < 2)
        want = 2;
    if (want > IG_EV_MAX)
        want = IG_EV_MAX;

    ev.nmaps = 0;
    if (!G_IsOspRuleset())
        return;
    if (G_IsOspRuleset()) {
    for (i = 0; i < (int)map_size; i++)
        if (level.changemap && !Q_stricmp(map[i].name, level.changemap) &&
            Q_stricmp(map[i].name, level.mapname)) {
            ev.maps[ev.nmaps++] = i;
            break;
        }

    for (pass = 0; pass < 2 && ev.nmaps < want; pass++) {
        npool = 0;
        for (i = 0; i < (int)map_size && npool < 1024; i++) {
            if (!Q_stricmp(map[i].name, level.mapname))
                continue;
            if (pass == 0 && map[i].used)
                continue;
            taken = false;
            for (j = 0; j < ev.nmaps; j++)
                if (ev.maps[j] == i || !Q_stricmp(map[ev.maps[j]].name, map[i].name))
                    taken = true;
            if (!taken)
                pool[npool++] = i;
        }
        while (npool > 0 && ev.nmaps < want) {
            n = Q_rand_uniform(npool);
            ev.maps[ev.nmaps++] = pool[n];
            pool[n] = pool[--npool];
        }
    }
    }
}

static int EV_Count(int option)
{
    int c, n = 0;

    for (c = 0; c < game.maxclients && c < MAX_CLIENTS; c++)
        if (ev.vote[c] == option && EV_Human(g_edicts + 1 + c))
            n++;
    return n;
}

static void EV_Show(edict_t *ent);

static void EV_Choose(edict_t *ent, osp_pmenu_t *p)
{
    int c = EV_Client(ent), opt = (int)(intptr_t)p->arg;

    if (c < 0 || ev.state != EV_OPEN || opt < 0 || opt >= ev.nmaps)
        return;
    if (ev.vote[c] != opt) {
        ev.vote[c] = opt;
        gi.bprintf(PRINT_HIGH, "%s votes for %s\n", ent->client->pers.netname,
                   map[ev.maps[opt]].name);
    }
    ev.dirty = true;
}

static void EV_CloseMenu(edict_t *ent)
{
    int c = EV_Client(ent);

    if (c < 0 || !ev.menu[c])
        return;
    ev.menu[c] = false;
    if (G_IsOspRuleset()) {
        if (ent->client->menu_owner == MENU_TOURNEY)
            osp_PMenu_Close(ent);
        DeathmatchScoreboard(ent);
    }
}

static void EV_Scores(edict_t *ent, osp_pmenu_t *p)
{
    EV_CloseMenu(ent);
}

/*
=================
EV_Show

Draws (or redraws) the vote menu for one person, keeping the cursor.
=================
*/
static void EV_Show(edict_t *ent)
{
    osp_pmenu_t rows[IG_EV_MAX + 6];
    char lines[IG_EV_MAX][40], title[40], left[40];
    int c = EV_Client(ent), i, n = 0, cur = 3, secs;

    if (c < 0 || !G_IsOspRuleset())
        return;
    if (ev.menu[c] && ent->client->menu_owner == MENU_TOURNEY && ent->client->osp_menu)
        cur = ent->client->osp_menu->cur;
    else if (ev.vote[c] >= 0)
        cur = 3 + ev.vote[c];

    memset(rows, 0, sizeof(rows));
    Q_strlcpy(title, "[ Vote: next map ]", sizeof(title));
    rows[n].text = title;
    rows[n++].align = osp_PMENU_ALIGN_CENTER;
    secs = (ev.closes - level.framenum + BASE_FRAMERATE - 1) / BASE_FRAMERATE;
    if (secs < 0)
        secs = 0;
    Q_snprintf(left, sizeof(left), "%d s left", secs);
    rows[n].text = left;
    rows[n++].align = osp_PMENU_ALIGN_CENTER;
    rows[n++].align = osp_PMENU_ALIGN_CENTER;              // blank
    for (i = 0; i < ev.nmaps; i++) {
        Q_snprintf(lines[i], sizeof(lines[i]), "%c %-18.18s %2d",
                   ev.vote[c] == i ? '*' : ' ', map[ev.maps[i]].name, EV_Count(i));
        rows[n].text = lines[i];
        rows[n].align = osp_PMENU_ALIGN_LEFT;
        rows[n].arg = (void *)(intptr_t)i;
        rows[n++].SelectFunc = EV_Choose;
    }
    rows[n++].align = osp_PMENU_ALIGN_LEFT;                // blank
    rows[n].text = "Show the scores";
    rows[n].align = osp_PMENU_ALIGN_LEFT;
    rows[n++].SelectFunc = EV_Scores;

    if (cur < 0 || cur >= n || !rows[cur].SelectFunc)
        cur = 3;
    if (G_IsOspRuleset()) {
        if (ent->client->menu_owner == MENU_TOURNEY)
            osp_PMenu_Close(ent);
        osp_PMenu_Open(ent, rows, cur, n);
        ev.menu[c] = true;
    }
}

static bool EV_ShouldHold(void)
{
    if (!ig_endvote->integer)
        return false;
    if (G_IsOspRuleset()) {
        if (manual_map || ((int)dmflags->value & DF_SAME_LEVEL))
            return false;
        if (!map || map_size < 2 || !level.changemap || strchr(level.changemap, '*'))
            return false;
        return OSP_humanCount(false) > 0;
    }
    return false;
}

static void EV_Finish(void)
{
    int i, c, best = -1, bestn = 0, n, ties = 0;

    for (i = 0; i < ev.nmaps; i++) {
        n = EV_Count(i);
        if (n > bestn) {
            best = i;
            bestn = n;
            ties = 1;
        } else if (n == bestn && n > 0 && Q_rand_uniform(++ties) == 0) {
            best = i;
        }
    }
    ev.state = EV_DONE;
    ev.exits = level.framenum + (int)(IG_EV_GRACE * BASE_FRAMERATE);

    for (c = 0; c < game.maxclients && c < MAX_CLIENTS; c++)
        EV_CloseMenu(g_edicts + 1 + c);

    if (best >= 0) {
        level.changemap = map[ev.maps[best]].name;
        map[ev.maps[best]].used = 1;
        gi.bprintf(PRINT_HIGH, "Next map: %s (%d vote%s)\n", level.changemap,
                   bestn, bestn == 1 ? "" : "s");
    } else {
        gi.bprintf(PRINT_HIGH, "No votes. Next map: %s\n", level.changemap);
    }
}

/*
=================
IG_EndVoteFrame

From G_RunFrame, ahead of the intermission exit test. While the vote is open
it holds the exit; once it has a result it ends the intermission itself.
=================
*/
void IG_EndVoteFrame(void)
{
    int c, humans, voted, secs;
    edict_t *e;

    EV_Cvars();

    if (!level.intermission_framenum) {
        ev.state = EV_IDLE;
        return;
    }
    if (ev.state != EV_IDLE && ev.intermission != level.intermission_framenum)
        ev.state = EV_IDLE;

    if (ev.state == EV_IDLE) {
        if (level.framenum < level.intermission_framenum + IG_EV_OPENAT * BASE_FRAMERATE)
            return;
        ev.intermission = level.intermission_framenum;
        if (!EV_ShouldHold()) {
            ev.state = EV_SKIP;
            return;
        }
        EV_Pick();
        if (ev.nmaps < 2) {
            ev.state = EV_SKIP;
            return;
        }
        for (c = 0; c < MAX_CLIENTS; c++) {
            ev.vote[c] = -1;
            ev.menu[c] = false;
        }
        ev.state = EV_OPEN;
        secs = ig_endvotetime->integer;
        if (secs < 5)
            secs = 5;
        if (secs > 60)
            secs = 60;
        ev.closes = level.framenum + secs * BASE_FRAMERATE;
        ev.dirty = true;
        gi.bprintf(PRINT_HIGH, "Vote for the next map: [ and ] to move, Enter to vote\n");
        for (c = 0; c < game.maxclients && c < MAX_CLIENTS; c++)
            if (EV_Human(g_edicts + 1 + c))
                EV_Show(g_edicts + 1 + c);
        ev.shown_left = -1;
    }

    if (ev.state == EV_OPEN) {
        level.exitintermission = 0;     // hold

        humans = voted = 0;
        for (c = 0; c < game.maxclients && c < MAX_CLIENTS; c++) {
            e = g_edicts + 1 + c;
            if (!EV_Human(e)) {
                ev.vote[c] = -1;
                ev.menu[c] = false;
                continue;
            }
            humans++;
            if (ev.vote[c] >= 0)
                voted++;
        }
        if (!humans || voted == humans || level.framenum >= ev.closes) {
            gi.dprintf("ig_endvote: closed, %d of %d voted, frame %d of %d\n",
                       voted, humans, level.framenum, ev.closes);
            EV_Finish();
            return;
        }

        secs = (ev.closes - level.framenum + BASE_FRAMERATE - 1) / BASE_FRAMERATE;
        if (ev.dirty || secs != ev.shown_left) {
            for (c = 0; c < game.maxclients && c < MAX_CLIENTS; c++) {
                e = g_edicts + 1 + c;
                if (ev.menu[c] && e->client->menu_owner == MENU_TOURNEY)
                    EV_Show(e);
                else
                    ev.menu[c] = false;
            }
            ev.dirty = false;
            ev.shown_left = secs;
        }
        return;
    }

    if (ev.state == EV_DONE) {
        level.exitintermission = level.framenum >= ev.exits;
    }
}

/*
=================
IG_EndVoteCommand

From ClientCommand, ahead of its intermission guard: the menu keys while the
vote menu is open, and `mapvote` (reopens the menu) / `mapvote <n>` (votes for
option n) while the vote is open. True when the command was taken.
=================
*/
bool IG_EndVoteCommand(edict_t *ent, const char *cmd)
{
    int c = EV_Client(ent), opt;
    osp_pmenu_t p;

    if (ev.state != EV_OPEN || c < 0 || !level.intermission_framenum || !G_IsOspRuleset())
        return false;

    if (!Q_stricmp(cmd, "mapvote")) {
        if (gi.argc() > 1) {
            opt = atoi(gi.argv(1)) - 1;
            memset(&p, 0, sizeof(p));
            p.arg = (void *)(intptr_t)opt;
            EV_Choose(ent, &p);
        } else {
            EV_Show(ent);
        }
        return true;
    }

    if (!ev.menu[c] || ent->client->menu_owner != MENU_TOURNEY || !ent->client->osp_menu)
        return false;
    if (G_IsOspRuleset()) {
        // The intermission skips ClientEndServerFrame's menu flush (p_view.c
        // returns before it), so a moved cursor is sent here, at once.
        if (!Q_stricmp(cmd, "invnext") || !Q_stricmp(cmd, "invprev")) {
            if (!Q_stricmp(cmd, "invnext"))
                osp_PMenu_Next(ent);
            else
                osp_PMenu_Prev(ent);
            if (ent->client->osp_menu) {
                osp_PMenu_Do_Update(ent);
                gi.unicast(ent, true);
                ent->client->menudirty = false;
            }
            return true;
        }
        if (!Q_stricmp(cmd, "invuse") || !Q_stricmp(cmd, "invdrop")) {
            osp_PMenu_Select(ent);
            return true;
        }
    }
    return false;
}

/*
=================
IG_MenuFast

IG-23 `ig_menufast 1`: an OSP menu is redrawn on the key press that moved it,
as Quake II's own menus were, instead of 0.2 s after the last press
(osp_PMenu_Update's bandwidth saving). Default 0, upstream's delay.
=================
*/
bool IG_MenuFast(edict_t *ent)
{
    static cvar_t *fast;

    if (!fast)
        fast = gi.cvar("ig_menufast", "1", 0);
    if (!fast->integer)
        return false;
    if (G_IsOspRuleset()) {
        if (!ent->client->osp_menu)
            return false;
        osp_PMenu_Do_Update(ent);
        gi.unicast(ent, true);
        ent->client->menutime = level.time;
        ent->client->menudirty = false;
        return true;
    }
    return false;
}

/*
=================
IG_EndVoteMenuOpen

True while this person's screen is the vote menu: the intermission's periodic
scoreboard refresh (p_view.c) must not draw over it.
=================
*/
bool IG_EndVoteMenuOpen(edict_t *ent)
{
    int c = EV_Client(ent);

    return ev.state == EV_OPEN && c >= 0 && ev.menu[c];
}
