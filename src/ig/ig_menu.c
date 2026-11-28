/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ig_menu.c -- the fork's scoreboard and vote-menu additions.
//
// IG-18 `ig_scoremap 1`: the OSP scoreboard shows the map's file name
//       (level.mapname, which carries no ".bsp") above the leader banner.
// IG-19 `ig_votemaplist 1`: the vote menu's Map row opens a list of the maps
//       to pick from, a page at a time, instead of stepping through them one
//       per key press.
// Both default to 0, upstream's behaviour. Everything that touches OSP's
// menus and map list sits inside a G_IsOspRuleset() block (tools/donorgate.py).

#include "g_local.h"
#include "tourney/osp_types.h"
#include "ig/ig.h"

/*
=================
IG_ScoreMapLine

Appends the map line to an OSP scoreboard layout being built in buf (outlen
its length so far) and returns the new length. Called first, so the line sits
on top of the board whatever banner follows.
=================
*/
int IG_ScoreMapLine(char *buf, size_t size, int outlen)
{
    static cvar_t *show;
    int n;

    if (!show)
        show = gi.cvar("ig_scoremap", "1", 0);
    if (!show->integer || !G_IsOspRuleset())
        return outlen;
    n = Q_snprintf(buf + outlen, size - outlen, "xv 0 yv -40 cstring \"%s\" ", level.mapname);
    if (n < 0 || outlen + n >= (int)size) {
        buf[outlen] = 0;
        return outlen;
    }
    return outlen + n;
}

/*
=================
The vote menu's map list.

Rows: title, blank, up to IG_MAPS_PAGE maps, blank, next/previous page, back.
Picking a map stages it exactly as the stepping row did (resp.osp_r290 is the
staged map, resp.osp_r254 bit 1 says "a map change is staged") and returns to
the vote menu with the cursor on the Map row, where "Propose Change" sends it.
=================
*/
#define IG_MAPS_PAGE    12
#define IG_LIST_ROWS    (IG_MAPS_PAGE + 7)

static int  ig_mappage[MAX_CLIENTS];
static char ig_mapline[IG_MAPS_PAGE][40];
static char ig_pagetitle[40];

static void IG_MapListShow(edict_t *ent, int page);

static void IG_BackToVote(edict_t *ent)
{
    if (G_IsOspRuleset()) {
        osp_PMenu_Close(ent);
        OSP_updateVoteMenu(ent);
        osp_PMenu_Open(ent, Vote_Menu, 3, 19);
    }
}

static void IG_MapListPick(edict_t *ent, osp_pmenu_t *p)
{
    int idx = (int)(intptr_t)p->arg;

    if (G_IsOspRuleset()) {
        if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 1) {
            gi.cprintf(ent, PRINT_HIGH, "Sorry, you can change only one item at a time!\n");
            return;
        }
        if (idx < 0 || idx >= (int)map_size || !map)
            return;
        ent->client->resp.osp_r290 = idx;
        ent->client->resp.osp_r254 = 1;
        IG_BackToVote(ent);
    }
}

static void IG_MapListNext(edict_t *ent, osp_pmenu_t *p)
{
    int c = ent - g_edicts - 1;

    IG_MapListShow(ent, ig_mappage[c] + 1);
}

static void IG_MapListPrev(edict_t *ent, osp_pmenu_t *p)
{
    int c = ent - g_edicts - 1;

    IG_MapListShow(ent, ig_mappage[c] - 1);
}

static void IG_MapListBack(edict_t *ent, osp_pmenu_t *p)
{
    IG_BackToVote(ent);
}

static void IG_MapListShow(edict_t *ent, int page)
{
    osp_pmenu_t rows[IG_LIST_ROWS];
    int c = ent - g_edicts - 1, pages, i, n = 0, first;

    if (c < 0 || c >= MAX_CLIENTS)
        return;
    if (G_IsOspRuleset()) {
        if (!map || !map_size)
            return;
        pages = ((int)map_size + IG_MAPS_PAGE - 1) / IG_MAPS_PAGE;
        if (page < 0)
            page = pages - 1;
        if (page >= pages)
            page = 0;
        ig_mappage[c] = page;
        first = page * IG_MAPS_PAGE;

        memset(rows, 0, sizeof(rows));
        if (pages > 1)
            Q_snprintf(ig_pagetitle, sizeof(ig_pagetitle), "[ Vote: pick a map %d/%d ]", page + 1, pages);
        else
            Q_strlcpy(ig_pagetitle, "[ Vote: pick a map ]", sizeof(ig_pagetitle));
        rows[n].text = ig_pagetitle;
        rows[n++].align = osp_PMENU_ALIGN_CENTER;
        rows[n++].align = osp_PMENU_ALIGN_CENTER;          // blank
        for (i = 0; i < IG_MAPS_PAGE && first + i < (int)map_size; i++) {
            Q_snprintf(ig_mapline[i], sizeof(ig_mapline[i]), "%s%s", map[first + i].name,
                       ent->client->resp.osp_r290 == first + i ? " *" : "");
            rows[n].text = ig_mapline[i];
            rows[n].align = osp_PMENU_ALIGN_LEFT;
            rows[n].arg = (void *)(intptr_t)(first + i);
            rows[n++].SelectFunc = IG_MapListPick;
        }
        rows[n++].align = osp_PMENU_ALIGN_LEFT;            // blank
        if (pages > 1) {
            rows[n].text = "Next page...";
            rows[n].align = osp_PMENU_ALIGN_LEFT;
            rows[n++].SelectFunc = IG_MapListNext;
            rows[n].text = "Previous page...";
            rows[n].align = osp_PMENU_ALIGN_LEFT;
            rows[n++].SelectFunc = IG_MapListPrev;
        }
        rows[n].text = "*Back to the vote menu";
        rows[n].align = osp_PMENU_ALIGN_LEFT;
        rows[n++].SelectFunc = IG_MapListBack;

        osp_PMenu_Close(ent);
        osp_PMenu_Open(ent, rows, 2, n);
    }
}

/*
=================
IG_VoteMapList

From the vote menu's Map row (OSP_changeMap_menu): true when the list took
over, false to let the row step through the maps as upstream does.
=================
*/
bool IG_VoteMapList(edict_t *ent)
{
    static cvar_t *list;
    int c = ent - g_edicts - 1;

    if (!list)
        list = gi.cvar("ig_votemaplist", "1", 0);
    if (!list->integer || c < 0 || c >= MAX_CLIENTS)
        return false;
    if (G_IsOspRuleset()) {
        // open on the page holding the staged map, if any
        IG_MapListShow(ent, ent->client->resp.osp_r290 >= 0 ? ent->client->resp.osp_r290 / IG_MAPS_PAGE : 0);
        return true;
    }
    return false;
}
