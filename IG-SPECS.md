# IG specs -- the instagib fork

The fork's own requirements, numbered like upstream's `SPECS.md`. Each row: the requirement,
why, and how it is tested. Every switch defaults to upstream behaviour.

## IG-1: `ig_noblaster` -- no blaster in the OSP loadout

- **Requirement:** with `ig_noblaster 1`, players and bots under the OSP rulesets spawn without
  the blaster. Default `0` is upstream (blaster always given). Read where the loadout is built
  (`OSP_initWeapItem`), so it takes effect at game start or a config reload, like `weapon_have`.
- **Why:** OSP's `weapon_have` has no bit for the blaster (`start_weap[0]` is hardcoded 1), so a
  railgun-only instagib config still spawns a second weapon. Instagib is railgun only.
- **Where:** `src/ig/ig_main.c` (`IG_StartBlaster`), one `// IG:` call in
  `src/tourney/osp_main.c`. Only safe alongside infinite slugs (`dmflags` 8192), since
  `NoAmmoWeaponChange` falls back to the blaster.
- **Tested:** `instagib.cfg` with `ig_noblaster 1` on q2dm1: a player spawns with the railgun only
  (`use blaster` is refused, weapon cycling stays on the railgun); with `ig_noblaster 0` the blaster
  is back. Q3 bots keep firing the railgun.

## IG-2: leaving instagib restores the stock rules

- **Requirement:** after `instagib.cfg`, a Config vote (or `exec`) to any shipped mode config puts back
  everything instagib changed that upstream's mode configs do not set: `dmflags`, `weapon_have`,
  `weapon_initial`, `start_slugs`, `max_slugs`, `damage_railgun`, `motd_file`, all 26 `allow_*`,
  `ig_noblaster` and `ig_bothook`.
- **Why:** cvars persist across configs; without this a vote from instagib to `dm` stays railgun-only.
- **Where:** the "instagib fork" block at the end of `colosseum/server.cfg`, which every mode config
  execs first. Values are the library defaults (engine default 16 for `dmflags`).
  `tools/cfgcvars.py` lists `nochat` and the `allow_*` table names as registered indirectly.
- **Tested:** `+exec instagib.cfg +map q2dm1`, then `exec configs/dm.cfg`: all of the above read back at
  their defaults (2026-11-20). `make check`: cfgcvars clean.

## IG-3: no bot chat in any mode

- **Requirement:** bots never chat, whatever the mode (`nochat 1`). Upstream leaves `nochat` unset,
  which is chat on; this is the server's choice, not a default.
- **Why:** Billy wants quiet bots on every mode, including future ones.
- **Where:** `colosseum/server.cfg`, which every shipped mode config execs first, so a new mode
  config that follows the same pattern gets it too. Pushed to the bot library at load
  (`BotInitLibrary`), which happens every map, so it applies from the next map.
  Q3 bots read it as `bot_nochat` (be_interface_q2.c remap, ai_chat.c).
- **Tested:** `exec configs/dm.cfg` reads back `nochat 1`.

## IG-4: `sv igstats` -- movement and engagement counters

- **Requirement:** a measurement window, like upstream's `sv botstats`, that reports per client:
  frames alive, frames on upper levels, distance travelled, jump pad launches, hook shots, frames
  on the hook, lives, time from spawn to first contact, and rail shots. `reset` opens it, `off`
  closes it. It counts and never acts, so it needs no switch.
- **Why:** the planned bot work (hook routes, slingshots, purposeful roaming) is judged by these
  numbers, which `sv botstats` does not have.
- **Definitions:** "upper" = feet more than 96 units above the mean height of the map's
  `info_player_deathmatch` points. "First contact" = the first rail shot, damage taken or death
  after a spawn inside the window (lives already running when it opens are not timed). A pad
  launch is debounced over 5 frames. A hook shot is the client's hook entity appearing. Rail shots
  come from OSP's accuracy table, so they read 0 outside the four OSP rulesets.
- **Where:** `src/ig/ig_stats.c`; `// IG:` calls in `g_svcmds.c` (command), `g_main.c` (end of
  `G_RunFrame`), `g_trigger.c` (`trigger_push_touch`).
- **Tested:** igbench smoke runs on q2dm1 and q2rdm11: every bot reported, pads counted on
  q2rdm11 and none on q2dm1, contacts close to lives. The same seed replays identically.

## IG-5: `tools/igbench.py` -- seeded bot matches under a server config

- **Requirement:** play N seeded, frame-counted matches of one botlib's bots under a server config
  (default `instagib.cfg`) on installed maps, read `sv botstats` and `sv igstats`, and report
  per map with 95% bootstrap intervals; compare a run against a base run (feature on vs off via
  `--set`). Optional server-side demos (`--demo`).
- **Why:** project rule: every AI change ships with a feature on vs off report. botduel.py compares
  two botlibs on stock maps and cannot run a server config or custom maps.
- **Where:** `tools/igbench.py`, reusing botduel.py's seed shim, waits and parsers. It builds a
  throwaway tree linking the install ($Q2) with the library under test (LIB) and copies the map's
  Q3 .aas; nothing is written into the install.
- **Tested:** the same seed replays a match identically (igstats, botstats and kill lines) and
  another seed differs; demo recording on vs off gives an identical match; skill 1 vs skill 5
  shows a difference (sensitivity control, see the baseline commit).

## IG-6: `ig_bothook` -- the Quake III bots use the offhand hook

- **Requirement:** with `ig_bothook 1` (default 0) and the OSP hook on, Quake III bots take grapple
  routes in the AAS file: walk to the route's start, aim at the anchor, `hook`, ride the pull, and
  `unhook` on arrival or when the pull stops gaining ground. `ig_bothook_cost` (default 150 =
  1.5 s) is a grapple route's fixed cost. Off, the brain gets its own defaults: upstream.
- **Why:** stock Q3 bots never hook (0 hooks/min in `ig-0.1-baseline`): grapple routes need
  `bspc -grapplereach`, the AI's `bot_grapple`, an offhand command, and a hook entity the brain
  can see -- none of which existed for Q2. First step of the bots' hook work.
- **Where:** game: `src/ig/ig_bot.c`, one `// IG:` call at the start of `BotRulesetLibVars`
  (`bl_main.c`). Brain (backport fork): `botlib/be_ig_hook.c` -- hook entities classified in
  `Q2BotUpdateEntity`, the bot's OWN hook judged by its position on the bot-to-anchor line in
  `GrappleState` (stock Q3 counts any visible hook), grapple costs rewritten after the AAS file
  loads. Contract: `docs/botlib-contract.md`, "Instagib fork". AAS files: `bspc -grapplereach`
  (server repo, `tools/build-aas.sh`).
- **Tested:** igbench, feature off vs `ig-0.1-baseline` and feature on vs off, on q2dm1, q2rdm11,
  ztn2dm3 (report in the server repo, bench/ig-0.2-hook-climb).

## IG-7: `ig_botroam` -- hunt goals instead of the random one

- **Requirement:** a Quake III bot with nothing to pick up goes where it last saw (120-degree view,
  3000 units) or heard (firing, 1500 units) an enemy, the freshest trace up to 10 s old, instead of
  `AAS_RandomGoalArea`'s random area. It passes through (picks its next goal within 160 units) and
  re-plans on a trace 3 s fresher. Item goals are never replaced, so deathmatch's item gathering is
  unchanged. `ig_botdebug` (1 quiet, 2 everything) has bots say their choice in chat. Default off.
- **Why:** in instagib every item is removed, so all a stock bot ever does between fights is walk to a
  random area anywhere on the map.
- **Where:** brain `game_q3/ai_ig_roam.c`; `// IG:` calls in `ai_dmnet.c` (BotGetItemLongTermGoal) and
  `ai_dmq3.c` (BotDeathmatchAI). Game: `ig_botroam` -> libvar `ig_roam`.
- **Tested:** igbench, roam on vs off (hook on): frags/min 4.54 -> 5.40 (significant), time to contact
  4.38 -> 3.96 s, accuracy unchanged.

## IG-8: `ig_botrevenge` -- a grudge (trigger rule OPEN)

- **Requirement:** a bot killed twice in a row by the same player within `ig_botrevenge_window` (60 s)
  hunts that player (IG-7's hunt, even with roam off) and prefers them as its enemy when in view, for
  `ig_botrevenge_time` (20 s) or until it kills them. It knows only what it perceived. Default off.
- **Why:** Billy's request: bots that react to being farmed.
- **Where:** brain `game_q3/ai_ig_revenge.c`; `// IG:` calls in `ai_dmq3.c` (BotFindEnemy, the
  obituary). igbench counts grudges and settlements from the brain's `ig: revenge` lines.
- **Tested:** fires ~1.1 per bot per minute in instagib, 40% settled -- too often; the trigger is being
  rethought (humans only / 3 in a row / cooldown). Keep off until decided.

## IG-9: `ig_botsling` -- slingshot: jump, hook ahead, let go

- **Requirement:** with `ig_bothook`, a bot running a long clear stretch of route (700 units ahead,
  at least 450 horizontally, running that way at 200+) jumps, hooks a solid non-sky anchor ahead at
  10-26 degrees up (350-1100 units, 40-450 above the eye, body path and landing floor checked), and
  lets go once pulled half the remaining distance or past a quarter of it at 850+ -- the pull measured
  from where the hook latches -- or after 0.8 s, or when stalled; 3 s cooldown. Default off.
- **Why:** stock Q3 bots only hook standing at a grapple route's start; human players slingshot.
  Billy's brief: include a jump to really boost.
- **Where:** brain `game_q3/ai_ig_sling.c` (one `// IG:` call at the end of AINode_Seek_LTG's
  movement) and `botlib/be_ig_hook.c` (`IG_HookNear`). Game: `ig_botsling` -> libvar `ig_sling`.
- **Tested:** igbench vs roam only: 2.4 slingshots per bot per minute, speed 261 -> 269 u/s
  (significant), frags/accuracy/falls unchanged. Of attempts, 39% are full slingshots (released at
  975 u/s), 57% never latch (hook misses or is refused) -- the next refinement.

## IG-10: `ig_botfighthook` -- a hook mid-fight

- **Requirement:** with `ig_bothook`, a bot fighting a visible enemy 250-1800 units away hooks, mostly
  right after it fires (the railgun's reload): a short anchor (180-450) diagonally sideways and up,
  within 55 degrees of its view (it turns no faster than its character); jumps if grounded; rides about
  0.25 s and lets go; 2.5 s between. Default off.
- **Why:** Billy hooks mid-fight for a better angle and to be harder to hit.
- **Where:** brain `game_q3/ai_ig_sling.c` (`IG_CombatHook`), one `// IG:` call after BotCheckAttack in
  AINode_Battle_Fight. Game: `ig_botfighthook` -> libvar `ig_fighthook`.
- **Tested:** igbench, on vs off (roam, slingshot on): 1.6 fight hooks per bot per minute, 53% latch;
  rail accuracy +2.0 points (borderline), frags/deaths unchanged -- symmetric between bots by design;
  harder-to-hit is judged against a person.

### IG-10 addendum: reposition (2026-11-21)

- **Requirement:** after a kill (40% of the time) or after two misses in a row (a shot with no kill
  within 0.4 s -- exact in instagib), the bot hooks away at once, ignoring the combat hook's cooldown,
  up to 90 degrees off its view; also from the route nodes, since a kill ends the fight node.
- **Why:** Billy: "after a kill I will sometimes grapple away to reposition ... after 2 misses,
  reposition with grapple quickly and try again".
- **Tested:** igbench vs the combat hook alone: 3.4 repositions per bot per minute; rail accuracy
  37.9% -> 47.1% (fewer shots from a spot that just missed twice), hooks 6.9 -> 10.4/min, speed +5.7;
  frags/deaths unchanged.

### IG-9 addendum: staying out of lava (2026-11-21)

- Anchors are traced with lava and slime and refused through them; the slingshot and combat-hook jumps
  check 500/400 units of safe floor first; the flight path is checked to the anchor and beyond. A bot
  in lava or slime hooks nearly straight up (85-65 degrees, 55-45 as a fallback; the sky when
  `hook_sky` allows, pushed as `ig_hooksky`), preferring an anchor with safe floor under it, and
  re-hooks toward safe floor if it hangs over the pool. Hooks fire only after two aimed thinks.
- **Tested:** q2rdm11, 10 matches: lava deaths 157 -> ~80; of escapes started, 66 of 67 ended over
  safe floor.
