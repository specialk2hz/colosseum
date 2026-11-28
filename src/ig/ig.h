/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// src/ig/ -- the instagib fork's own game code (specialk2hz/colosseum).
// Every switch here is a cvar that defaults to upstream behaviour; upstream
// files reach this code only through one-line calls marked `// IG:`.
// Requirements are the IG- rows in IG-SPECS.md.

#ifndef IG_H
#define IG_H

// IG-1: 1 if an OSP loadout includes the blaster, 0 under `ig_noblaster 1`.
int IG_StartBlaster(void);

// IG-4: `sv igstats` (ig_stats.c).
bool IG_ServerCommand(const char *cmd);
void IG_StatsFrame(void);
// ig_ctfstats.c: CTF counters in the igstats window (CTF-PLAN.md, the benchmark)
void IG_CtfStatsReset(void);
void IG_CtfStatsFrame(void);
void IG_CtfStatsReport(void);
void IG_CtfEvent(const char *kind, edict_t *player, int flagteam);
void IG_StatsPush(edict_t *other);

// IG-6: the Q3 bots' hook (ig_bot.c).  bot_library_t is bot/bl_main.h's.
struct bot_library_s;
void IG_BotLibVars(struct bot_library_s *lib);

// IG-14: bots keep their momentum on landing, as Q2PRO clients do.
void IG_BotPmove(edict_t *ent, pmove_state_t *s, usercmd_t *cmd);

// IG-15: `ig_moverec 1` logs a person's every movement command (ig_stats.c).
void IG_MoveRecord(edict_t *ent, const usercmd_t *ucmd, bool onground);

// IG-18 / IG-19: scoreboard map name, vote menu map list (ig_menu.c).
int IG_ScoreMapLine(char *buf, size_t size, int outlen);
bool IG_VoteMapList(edict_t *ent);

// IG-41: a speedometer under the chase line (ig_hud.c).
bool IG_ChaseSpeedOn(void);
void IG_ChaseSpeed(const edict_t *targ, char *s, size_t size, bool osp);

// IG-22: the end-of-match map vote (ig_endvote.c).
void IG_EndVoteFrame(void);
bool IG_EndVoteCommand(edict_t *ent, const char *cmd);
bool IG_EndVoteMenuOpen(edict_t *ent);
// IG-23: OSP menus redraw on the key press (ig_endvote.c).
bool IG_MenuFast(edict_t *ent);

// IG-24: no landing dip or grunt when fall damage is off (ig_main.c).
bool IG_NoFallFx(edict_t *ent);

// IG-25: rail jumping, a no-damage push at the slug's impact (ig_rail.c).
void IG_RailSplash(edict_t *self, const trace_t *tr);

// IG-26: bots replay recorded routes (ig_route.c).
// the OSP hook commands the Q3 brain is given (ig_bot.c)
#define IG_HOOKON   "hook"
#define IG_HOOKOFF  "unhook"

// IG-34: hook styles (ig_hook.c), `hook_type`
#define IG_HOOK_OSP     0
#define IG_HOOK_LMCTF   1
#define IG_HOOK_TYPES   2
int  IG_HookType(void);
int  IG_HookTypeFromName(const char *name);
int  IG_HookFlySpeed(int osp);
int  IG_HookPullSpeed(int osp);
bool IG_HookSky(int osp);
int  IG_HookInitDamage(int osp);
bool IG_HookIncDamage(void);
bool IG_HookPull(edict_t *hook, vec3_t hookdir, float vlen);
void IG_HookLook(edict_t *hook);
bool IG_HookFireSound(edict_t *ent, float volume);
bool IG_HookLatchFx(edict_t *hook, edict_t *other, cplane_t *plane, float volume);
void IG_HookFrame(edict_t *hook);
void IG_HookPrecache(void);
int  IG_HookVoteCurrent(int enabled);
int  IG_HookVoteNext(int state);
const char *IG_HookVoteLabel(int state);
bool IG_HookVoteArg(edict_t *ent, char **arg);
void IG_HookVoteApply(const char *value);

// IG-30: a bot stuck in solid or in one spot goes back to a spawn point (ig_bot.c).
void IG_BotUnstuck(edict_t *ent, const usercmd_t *ucmd);
bool IG_Respot(edict_t *ent, const char *why);
bool IG_DrillCommand(edict_t *ent);   // IG-37

// IG-33: lava, slime and pits put a player back at a spawn point (ig_bot.c).
bool IG_HazardRescue(edict_t *targ, int mod);
bool IG_HazardRescueEnter(edict_t *ent);

#endif
