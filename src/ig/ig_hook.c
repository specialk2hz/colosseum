/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ig_hook.c -- IG-34 hook styles for the offhand hook (`hook_type`).
//
// The offhand hook is OSP's (src/tourney/osp_hook.c).  `hook_type` picks how
// it behaves; each style keeps its own tuning:
//   hook_type "osp"     upstream: hook_speed / hook_pullspeed, straight pull
//                       with no gravity, hook_sky, OSP's hook damage
//   hook_type "lmctf"   LMCTF 6.0's hook, written from its behaviour (its code
//                       carries no licence, so none of it is used here):
//     hook_lmctf_speed   800  projectile speed
//     hook_lmctf_pull    800  pull speed, with gravity still pulling you down;
//                             inside 120 units of the anchor it eases off in
//                             steps (5x the distance at 120 down to 1x at 10)
//                             so you glide in and settle
//     hook_lmctf_damage  0    damage when it hits a player (0: it lets go)
//                       No sky.  Sticks to movers (as OSP's does).  No fall
//                       damage while pulled (P_FallingDamage's grapple rule).
//                       Looks and sounds like LMCTF: the ghook model on a CTF
//                       grapple cable, blaster sparks where it bites, its fire,
//                       flight, bite and pull sounds.  Assets in lmctf-hook.pak
//                       (LMCTF's pak4, not ours to commit): sounds under
//                       sound/lmhook/ because LMCTF's grfire.wav shares its
//                       name with CTF's in baseq2.
//
// The vote menu's hook row cycles through the styles in `ig_hookvote`
// ("osp lmctf": Disabled -> OSP -> LMCTF); empty = upstream's on/off row.
// Console: `vote hook lmctf`, `vote hook osp`, `vote hook 0`.  A vote value is
// a number (OSP's votes carry numbers only): 0 off, 1 osp, 2 lmctf.
// Default "osp" and "": exactly upstream.

#include "g_local.h"
#include "ig/ig.h"

static const char *const ig_hooknames[IG_HOOK_TYPES] = { "osp", "lmctf" };

static cvar_t *hook_type, *ig_hookvote;
static cvar_t *lmctf_speed, *lmctf_pull, *lmctf_damage;

static bool IG_HookVoteOffered(int type);

static void IG_HookCvars(void)
{
    if (hook_type)
        return;
    hook_type = gi.cvar("hook_type", "osp", 0);
    ig_hookvote = gi.cvar("ig_hookvote", "", 0);
    lmctf_speed = gi.cvar("hook_lmctf_speed", "800", 0);
    lmctf_pull = gi.cvar("hook_lmctf_pull", "800", 0);
    lmctf_damage = gi.cvar("hook_lmctf_damage", "0", 0);
}

int IG_HookTypeFromName(const char *name)
{
    int i;

    for (i = 0; i < IG_HOOK_TYPES; i++)
        if (!Q_stricmp(name, ig_hooknames[i]))
            return i;
    return -1;
}

int IG_HookType(void)
{
    int t;

    IG_HookCvars();
    t = IG_HookTypeFromName(hook_type->string);
    return t < 0 ? IG_HOOK_OSP : t;
}

int IG_HookFlySpeed(int osp)
{
    if (IG_HookType() == IG_HOOK_LMCTF)
        return (int)lmctf_speed->value;
    return osp;
}

bool IG_HookSky(int osp)
{
    if (IG_HookType() == IG_HOOK_LMCTF)
        return false;
    return osp != 0;
}

int IG_HookInitDamage(int osp)
{
    if (IG_HookType() == IG_HOOK_LMCTF)
        return (int)lmctf_damage->value;
    return osp;
}

bool IG_HookIncDamage(void)
{
    return IG_HookType() == IG_HOOK_OSP;
}

/*
IG_HookPull: from GrapplePull, with the owner's eye-to-hook vector and its
length.  True when this style set the owner's velocity (OSP's straight pull
is skipped).
*/
bool IG_HookPull(edict_t *hook, vec3_t hookdir, float vlen)
{
    // LMCTF's glide-in: speed = distance x this, by how close the anchor is
    static const struct { float above, times; } ease[] = {
        { 100, 5 }, { 80, 4 }, { 40, 3 }, { 20, 2 }, { 10, 1 }
    };
    edict_t *owner = hook->owner;
    float   speed = 1;
    int     i;

    if (IG_HookType() != IG_HOOK_LMCTF)
        return false;
    VectorNormalize(hookdir);
    if (vlen > 120) {
        VectorScale(hookdir, lmctf_pull->value, owner->velocity);
        // gravity still pulls: the pull is set afresh each frame, so this is
        // a steady sag (OSP dropped CTF's SV_AddGravity here; LMCTF kept it)
        owner->velocity[2] -= sv_gravity->value * BASE_FRAMETIME_1000;
        return true;
    }
    for (i = 0; i < (int)(sizeof(ease) / sizeof(ease[0])); i++) {
        if (vlen > ease[i].above) {
            speed = vlen * ease[i].times;
            break;
        }
    }
    VectorScale(hookdir, speed, owner->velocity);
    return true;
}

/*
LMCTF's look and sound.  IG_HookLook from FireGrapple once the hook exists;
IG_HookFireSound / IG_HookLatchFx in place of OSP's (true = done, skip OSP's);
IG_HookFrame every frame from OSP_HookFrame, after the hand is placed in
s.old_origin.  `wait` on the hook is when its next flight/pull sound plays.
*/
#define IG_LMSND(n) gi.soundindex("lmhook/" n ".wav")

/*
IG_HookPrecache: at map load, when LMCTF's hook is on or offered.  A client
downloads only what the server lists while it connects, so a sound first
indexed mid-game would never reach someone without the pak.
*/
void IG_HookPrecache(void)
{
    IG_HookCvars();
    if (IG_HookType() != IG_HOOK_LMCTF && !IG_HookVoteOffered(IG_HOOK_LMCTF))
        return;
    gi.modelindex("models/objects/ghook/tris.md2");
    IG_LMSND("grfire");
    IG_LMSND("gflyair");
    IG_LMSND("ghit");
    IG_LMSND("ghitwall");
    IG_LMSND("gpulling");
}

void IG_HookLook(edict_t *hook)
{
    if (IG_HookType() != IG_HOOK_LMCTF)
        return;
    hook->s.renderfx &= ~(RF_BEAM | RF_TRANSLUCENT);
    hook->s.modelindex = gi.modelindex("models/objects/ghook/tris.md2");
    hook->s.frame = 0;
    hook->s.skinnum = 0;
    hook->s.angles[PITCH] += 90;    // the model points along +z
    hook->wait = level.time + 1.0f;
    gi.linkentity(hook);
}

bool IG_HookFireSound(edict_t *ent, float volume)
{
    if (IG_HookType() != IG_HOOK_LMCTF)
        return false;
    gi.sound(ent, CHAN_AUTO, IG_LMSND("grfire"), 0.8f * volume, ATTN_NORM, 0);
    return true;
}

bool IG_HookLatchFx(edict_t *hook, edict_t *other, cplane_t *plane, float volume)
{
    if (IG_HookType() != IG_HOOK_LMCTF)
        return false;
    if (other && other->client)
        gi.sound(hook, CHAN_AUTO, IG_LMSND("ghit"), volume, ATTN_NORM, 0);
    else
        gi.sound(hook, CHAN_AUTO, IG_LMSND("ghitwall"), 0.8f * volume, ATTN_NORM, 0);
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BLASTER);
    gi.WritePosition(hook->s.origin);
    gi.WriteDir(plane ? plane->normal : vec3_origin);
    gi.multicast(hook->s.origin, MULTICAST_PVS);
    hook->wait = level.time + 0.8f;
    return true;
}

void IG_HookFrame(edict_t *hook)
{
    edict_t *owner = hook->owner;
    vec3_t  d;
    float   len;

    if (IG_HookType() != IG_HOOK_LMCTF)
        return;
    VectorSubtract(hook->s.origin, hook->s.old_origin, d);
    len = VectorLength(d);
    if (len > 64) {     // LMCTF leaves a short cable undrawn
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_GRAPPLE_CABLE);
        gi.WriteShort(owner - g_edicts);
        gi.WritePosition(hook->s.old_origin);
        gi.WritePosition(hook->s.origin);
        gi.WritePosition(vec3_origin);
        gi.multicast(owner->s.origin, MULTICAST_PVS);
    }
    if (len > 126 && level.time >= hook->wait) {
        if (owner->client->ctf_grapplestate == CTF_GRAPPLE_STATE_FLY) {
            gi.sound(hook, CHAN_AUTO, IG_LMSND("gflyair"), 1, ATTN_NORM, 0);
            hook->wait = level.time + 0.4f;
        } else {
            gi.sound(hook, CHAN_AUTO, IG_LMSND("gpulling"), 1, ATTN_NORM, 0);
            hook->wait = level.time + 0.8f;
        }
    }
}

/*
Vote: states are 0 (off) or 1 + a style number.
*/
static bool IG_HookVoteOffered(int type)
{
    char        buf[MAX_STRING_CHARS];
    const char  *p = buf;
    char        *tok;

    Q_strlcpy(buf, ig_hookvote->string, sizeof(buf));
    while ((tok = COM_Parse(&p)) && *tok)
        if (IG_HookTypeFromName(tok) == type)
            return true;
    return false;
}

static bool IG_HookVoteStyles(void)
{
    IG_HookCvars();
    return ig_hookvote->string[0] != 0;
}

int IG_HookVoteCurrent(int enabled)
{
    if (!enabled)
        return 0;
    return IG_HookVoteStyles() ? 1 + IG_HookType() : 1;
}

int IG_HookVoteNext(int state)
{
    int i, s;

    if (!IG_HookVoteStyles())
        return 1 - state;
    for (i = 1; i <= IG_HOOK_TYPES; i++) {
        s = (state + i) % (IG_HOOK_TYPES + 1);
        if (s == 0 || IG_HookVoteOffered(s - 1))
            return s;
    }
    return 0;
}

const char *IG_HookVoteLabel(int state)
{
    static const char *const labels[IG_HOOK_TYPES] = { "OSP", "LMCTF" };

    if (!state)
        return "DISABLED";
    if (!IG_HookVoteStyles() || state < 1 || state > IG_HOOK_TYPES)
        return "ENABLED";
    return labels[state - 1];
}

/*
IG_HookVoteArg: `vote hook <style>` -- a style name becomes its vote number.
False (and says why) for a style this server does not offer.
*/
bool IG_HookVoteArg(edict_t *ent, char **arg)
{
    static char num[8];
    int         t;

    if (!*arg || !IG_HookVoteStyles())
        return true;
    t = IG_HookTypeFromName(*arg);
    if (t < 0) {
        t = Q_atoi(*arg) - 1;
        if (t < 0)
            return true;        // 0: off
    }
    if (t >= IG_HOOK_TYPES || !IG_HookVoteOffered(t)) {
        gi.cprintf(ent, PRINT_HIGH, "Hook styles here: %s\n", ig_hookvote->string);
        return false;
    }
    Q_snprintf(num, sizeof(num), "%d", t + 1);
    *arg = num;
    return true;
}

/*
IG_HookVoteApply: after a hook vote passed and set hook_enable.
*/
void IG_HookVoteApply(const char *value)
{
    int state = Q_atoi(value);

    if (!IG_HookVoteStyles() || state < 1 || state > IG_HOOK_TYPES)
        return;
    gi.cvar_set("hook_type", ig_hooknames[state - 1]);
    gi.cvar_set("hook_enable", "1");
    gi.bprintf(PRINT_HIGH, "Hook style: %s\n", IG_HookVoteLabel(state));
}
