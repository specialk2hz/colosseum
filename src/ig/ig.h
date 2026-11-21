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
void IG_StatsPush(edict_t *other);

// IG-6: the Q3 bots' hook (ig_bot.c).  bot_library_t is bot/bl_main.h's.
struct bot_library_s;
void IG_BotLibVars(struct bot_library_s *lib);

// IG-14: bots keep their momentum on landing, as Q2PRO clients do.
void IG_BotPmove(edict_t *ent, pmove_state_t *s);

#endif
