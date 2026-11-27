# IG specs -- the instagib fork

The fork's own requirements, numbered like upstream's `SPECS.md`. Each row: the requirement,
why, and how it is tested. Every switch defaults to upstream behaviour.

## Index: every IG number and where it stands (2026-11-27)

**The goal:** a Quake 2 instagib server whose bots move like a seasoned player (fast, varied, never
scripted-looking) without changing the other modes. New work is a switch, off by default, reset in
`colosseum/server.cfg`; a trial runs on 27913 only (`groups/trials.cfg`, at most about five open); once
Billy approves it, it ships in its feature group or is **folded** into one of the two bot layers.

**Layers (ig-0.5, Billy 2026-11-27):** `ig_botplan` -- the movement: the planner (IG-40) and what is
folded into it (IG-28, 31, 32, 35, 36, 38, 42); the planner runs with or without the hook (without
it: runs and strafe hops only, no hook options), the hook moves and climbs need the bots' hook. `ig_botnav` -- the map knowledge, read at map
load: IG-43 (with the hook), 44, 46. A folded behaviour has no switch of its own; it keeps its brain libvar
as a **kill switch** for tests, set with `ig_botlibvars` (pushed last, it wins):

| Kill switch (libvar, 0 = off) | Folded behaviour | IG |
|---|---|---|
| `ig_plan` | the planner itself, with the strafe chains (IG-31, 38) and affinity-weighted slingshots (IG-28) it does natively -- 0: the stock movement | 40 |
| `ig_jumpland` | the landing hop | 31 |
| `ig_climbrun` (2) | climbs let go over the ledge | 32 |
| `ig_funcwall` | func_wall item pads are floor | 35 |
| `ig_climbaim`, `ig_climbland` | climbs hook the frame, land in the middle | 36 |
| `ig_padbounce` | off a pad that bounces straight up | 42 |
| `ig_padreach`, `ig_roamgoal`, `ig_gapjumps` (`ig_gapjumpreprice`) | pad routes, fair roam goals, gap jumps | 43, 44, 46 |

Tuning values are not switches: their code defaults are the tuned values (27913's, folded in ig-0.5:
`ig_planpadsling 1`, `ig_planreleasewallmul 1.5`, `ig_planwallsoon 2`, `ig_planpadair 0`, `ig_repokill 0.2`,
`ig_repomisschance 0.5`, `ig_fighthookchance 0.25`, `ig_fighthookcooldown 4`); overrides belong in benches.
Proof of the fold (28 maps x 4 seeds, fighting): the folded build with `ig_botplan 1 ig_botnav 1` against the
pre-fold build with every trial switch and tuning value of 27913 -- all figures equal (frags 4.58/4.58,
329.0/329.0 u/s), no map significant.

**Removed in ig-0.5:** the trick-by-trick controllers the planner replaced (game_q3/ai_ig_sling.c 1826 ->
~1090 lines: IG-9 slingshots and chains, IG-17 pull hop, IG-20 stairwell hook, IG-31/38 strafe controller with
IG-11's straight strafe, IG-39 flight check) and IG-26's recorded routes (src/ig/ig_route.c, the route file and
`tools/route-extract.py`; Billy's recordings stay in work/); their switches gone (`ig_botsling`, `ig_botpullhop`,
`ig_botstairhook`, `ig_botarcsafe`, `ig_botroutes`, `ig_routechance`, `ig_routedebug`). First the planner was made
to run without the hook, so no-grapple modes do not lose the strafe chains: no grapple, 28 maps x 4, old strafe
controller vs planner -- 275 -> 298 u/s, time over 400 u/s 8.0 -> 14.6% (both significant), frags and suicides
level (stock: 263 u/s). With the hook, after the removal vs the original 27913 setup: all figures equal, no map
significant. Kept from ai_ig_sling.c: the pad approach and end-of-arc pad hook (IG-13), the lava escape, the
fight hook and reposition (IG-10), the walkway corridor (IG-29).

Status: **live** = on 27912 (pushed, `ig-0.4-speed` + IG-22..24) · **folded** = part of a layer, approved for ig-0.5 · **approved** = on in its group for ig-0.5 · **ready** = on in its group file,
committed locally, not pushed · **trial** = 0 in its group, on only on 27913 until Billy has played it ·
**WIP** = being built · **parked** = in the code, off, with a reason · **tool** = testing aid, never on in play.

Groups: A hook movement (modes with the hook) · B instagib (instagib modes only) · C map movement (every
mode) · D roaming · M menus (every mode). Brain libvars are named without `bot` (`ig_botsling` -> `ig_sling`).

| IG | Switch | What | Group | Status | Measured / verdict |
|---|---|---|---|---|---|
| 1 | `ig_noblaster` | no blaster in the loadout | instagib.cfg | live | |
| 2 | server.cfg resets | leaving instagib restores stock rules | -- | live | |
| 3 | `nochat 1` | no bot chat | server.cfg | live | |
| 4, 12 | `sv igstats` | movement and fight counters | -- | live (tool) | |
| 5 | `tools/igbench.py` | seeded bot matches, compared | -- | tool | |
| 6 | `ig_bothook` | bots take hook climb routes | A | live | hooks 0 -> 17/min |
| 7 | `ig_botroam` | hunt goals instead of random roaming | D | live | no effect in DM |
| 8 | `ig_botrevenge` | grudge against a repeat killer | B | parked | trigger rule undecided (Billy) |
| 9 | `ig_botsling` | slingshots, chains | A | retired (ig-0.5): the planner's | 9.1 slingshots/bot/min |
| 10 | `ig_botfighthook` | hook mid-fight, reposition | B | live | |
| 11 | `ig_botstrafe 1` | straight strafe jumping | C | retired, removed in ig-0.5 | no gain; replaced by IG-31 (`2`) |
| 13 | `ig_botpadhook` | Billy's jump-pad hook tricks | A | live | variety, not speed |
| 14 | `ig_botstrafehack` | bots land like Q2PRO clients | C | live | no change, parity |
| 15 | `ig_moverec` | record a player's movement | -- | tool | |
| 16 | -- | (number not used) | | | |
| 17 | `ig_botpullhop` | hop off the floor while pulled | A | retired (ig-0.5): the planner's | 301 -> 313 u/s |
| 18, 19 | `ig_scoremap`, `ig_votemaplist` | map on scoreboard, vote map list | M | live | |
| 20 | `ig_botstairhook` | stairwell wall hook | A | retired (ig-0.5): the planner's | |
| 21 | `ig_botaffinity` | hook affinity per character | A | live | |
| 22, 23 | `ig_endvote`, `ig_menufast` | end-of-match map vote, fast menus | M | live | |
| 24 | `ig_nofallfx` | no landing dip or grunt | B | live | Billy: "feels great" |
| 25 | `ig_railsplash` | rail jumping (shooter only) | B | approved (ig-0.5) | Billy: "seems good" |
| 26 | `ig_botroutes` | replay recorded routes | -- | retired, removed in ig-0.5 | 0/12 finished; rework around hook events |
| 27 | `ig_botpeace` | bots only move | -- | tool | |
| 28 | `ig_botaffinitychance` | affinity = slingshot chance | B | folded: ig_botplan | spread sd 1.43 -> 1.67 |
| 29 | `ig_botwalkway` | slingshots stay on walkways near lava | C | approved, every mode (ig-0.5) | q2rdm11 lava 0.39 -> 0.23/min |
| 30 | `ig_botunstuck` | stuck bots back to a spawn point | C | approved, every mode (ig-0.5) | fixes q2dm1pro lift trap |
| 31 | `ig_botstrafe 2`, `ig_botjumpland` | strafe-jump chains | B | folded: ig_botplan | chains 7 -> 20%, >400 u/s 16 -> 19% |
| 32 | `ig_botclimbrun 2` | let go over the ledge | B | folded: ig_botplan | hang 0.19 -> 0.06 s |
| 33 | `ig_lavarescue` | lava/pits respot, no frag lost | B | approved (ig-0.5) | q2rdm11 suicides 0.33 -> 0 |
| 34 | `hook_type`, `ig_hookvote` | LMCTF hook style, vote row | server.cfg | approved (ig-0.5; vote in group A) | LMCTF bots 238 -> 287 u/s |
| 35 | `ig_botfuncwall` | stand on func_wall pads, no circling | B | folded: ig_botplan | circling 437 -> 311 |
| 36 | `ig_botclimbaim`, `ig_botclimbland` | climbs land in the middle; climb tables | B | folded: ig_botplan | landed 88.5 -> 91.3%, centred 44 -> 59%; Billy: "looks great" |
| 37 | `ig_drill` | climb drill | -- | tool | |
| 38 | `ig_botstrafeplan` | strafe chains plan ahead | B | folded: ig_botplan | q2rdm11 lava 0.52 -> 0.335/bot-min; >400 u/s -1 pt |
| 39 | `ig_botarcsafe` | flight check before let-go / hop | B | retired, removed in ig-0.5 | neutral; replaced by the planner (IG-40) |
| 48 | `ig_botplanclimb` | climbs as planner moves: fired on the run, let go by the sim | B | trial | 4 maps: standstill fires 71 -> 15%, speed at the fire 0 -> 285 u/s, climbs landing up top ~280 -> ~390; 28 maps level; LMCTF: hooks that climbed -2.7 pts, so OSP only |
| 47 | `ig_botfastjumps` | momentum jumps: gaps cleared only with speed, offered when fast, planned from 250 u/s; Billy's lip slingshot before them | B | trial | q2rdm11 walkway end jumped 46 -> 86%; ztn2dm3 ledge: column hits 91 -> 0%, lip slingshots 0.2 -> 0.88/try; 28 maps level; taken 0.02/bot-min in fights |
| 46 | `ig_botgapjumps` (+ `ig_botroutemomentum`, WIP) | routes jump gaps: bspc's gap jumps priced at their flight, missing ones added | B | folded: ig_botnav (momentum WIP) | q2rdm11 pad-to-walkway drill 4/144 -> 165/165 finished; route from the pad landing 9.5 -> 3.3 s; 28 maps x 4: everything level |
| 44 | `ig_botroamgoal` | fair random roam goal: floor spots weighted by size, no water | B | folded: ig_botnav | q2dm1/q2dm1pro 6 seeds: pool room time 7.4 -> 4.0% / 10.7 -> 7.9%, in water 5.1 -> 2.5% / 7.0 -> 4.5%; frags, speed, hooks unchanged |
| 43 | `ig_botpadreach` | routes through straight-up jump pads, taken with a hook from the top | B | folded: ig_botnav | q2rdm11: 15 routes; drill: far floors 0/54 -> 54/66 with the pad top; 28 maps level, q2rdm11 +10 u/s, upper +4.8 pts |
| 42 | `ig_botpadbounce` | steer off a pad that bounces a bot straight up (bot frozen forever) | B | folded: ig_botplan | q2rdm11 6 bots x 8 matches: bots frozen 2+ s 4 -> 0; other stats unchanged |
| 41 | `ig_chasespeed` | chase-camera speedometer under the chase line | M | approved (ig-0.5) | -- |
| 40 | (planner, branch `planner`) | one movement planner: simulate options, pick among the best | B | layer `ig_botplan` (ig-0.5) | v1 works: 28 maps 309 -> 330 u/s, upper 27.7 -> 31.1%, frags +5%; q2rdm11 lava trips halved |

The tricks these features carry, and how the movement planner must keep each one: `BOT-TRICKS.md`.

Config-only change, not an IG switch: `allow_railgun 0` in instagib.cfg (no railgun/slug pickups, ready).

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

## IG-11: strafe jumping (experimental, off)

- **Requirement:** `ig_strafe` (brain libvar; game cvar `ig_botstrafe` since IG-31) 1: on a long straight route stretch at
  running speed the bot jumps and, in the air, wishes at the angle whose cosine is
  (300 - 30) / speed to its velocity, toward the route's side, and jumps again on landing.
- **Why:** Billy: the bots should be faster; Quake II's air acceleration (PM_Accelerate with 1) adds
  speed this way.
- **Tested:** no gain. Corridor maps (q2dm1, q2rdm11, ztn2dm3) and open ones (aerowalk, campgrounds,
  q2rdm12): average speed unchanged, landing speed no higher than take-off, and it costs some
  slingshots. Left in, off, for a later look (a bot thinks at 10 Hz; routes bend too soon).

## IG-12: speed counters in `sv igstats`

- `fast` / `faster`: frames moving faster than 400 / 600 units/s (running is 300). The whole-match
  average speed is mostly fighting; these show the bursts hooks give. Stock Q3 bots vs the fork's
  instagib.cfg (2026-11-21): over 400 u/s 2.2% -> 13.2% of the time, over 600 0.6% -> 7.5%.

### IG-9 addendum (2026-11-21, evening): the slingshot, nailed

Billy's move: jump, hook the floor (or a far wall) ahead, let go quickly, about two hops, then
sling again or stop. What was wrong, found by logging the hook game-side against the brain:
- `AngleDifference` was undeclared in ai_ig_sling.c: every aim check read garbage.
- The body-clear check traced along the hook line, which for a floor anchor runs into the floor.
- Q3's view turn eases and carries angular speed between frames; the game fires a hook command
  along the LAST usercmd's view. Now every hook (slingshot, combat, lava) flicks onto its anchor
  and fires on the very next think, only if no other view was sent in between (`IG_FlickReady`).
- A shallow floor hook catches up to 300 units off the aimed spot: the latch is also read from the
  pull itself (speed > 800 toward the anchor).
Results (30 matches, q2dm1/q2rdm11/ztn2dm3): hooks caught within 100 units of the aim 28% -> 82%,
never caught 14% -> 2%; slingshots 3.5 -> 9.1/bot/min; hooks 15.5/min; > 400 u/s 14% of the time.
Also: igstats `railhits` (OSP's rail-only table). botstats' hits counted hook damage, so the
"rail accuracy 47%" of ig-0.3 was hooks hitting players; real rail accuracy is ~35% before and after.

### IG-9 addendum 2 (2026-11-21): longer chains

A chain no longer ends where the route bends: from 200 u/s it goes on toward the route's nearer
part (`IG_RouteNear`), and the anchor search, after the way the bot moves, also looks toward the
route ahead (round the corner). 30 matches: chains of 3+ hooks 10% -> 16% of slingshots, single
slingshots 54% -> 46%; hooks 15.5 -> 17.1/min (Billy 18); slingshots 9.1 -> 10.6/bot/min.

## IG-13: `ig_botpadhook` -- hook to a jump pad's landing (experimental, off)

- **Requirement:** `ig_botpadhook 1` (libvar `ig_padhook`): flying a jump pad (the brain notes the
  pad reachability's landing spot each frame, `IG_NotePadFlight` in BotFinishTravel_JumpPad), the
  bot hooks through a point above the landing (the whole body's path must clear the ledge lip) and
  lets go within 110 units of it.
- **Why:** Billy: "when a jump pad is taken, the bot uses the grappling hook to get to the intended
  landing spot faster".
- **Tested:** `igstats padflight` lines (game-side, pad touch to landing, hooked or not). On
  q2rdm11/campgrounds/tltf2 (8 seeds each) it is not faster: flights of 500-800 units 1.44 s off,
  1.58 s on (73% hooked). These pads launch at 650-950 u/s; OSP's 975 u/s pull replaces that
  velocity rather than adding to it. Off until a variant beats the arc.
- **Billy's trick, both halves** (2026-11-21, after he named q2rdm11's big room): (1) into the pad:
  when the route takes a jump pad 200-700 units ahead (AAS_PredictRoute stopping at TFL_JUMPPAD),
  jump, flick onto the pad's floor, pull, and let go 90 units short so the momentum carries the bot
  onto the trigger; (2) mid-air: a short pull toward the landing (0.4 s from the fire), then let go.
  q2rdm11 + campgrounds, 10 seeds: 268 into-pad hooks, 81% let go near the pad, 83% of those launched
  by it. Averages unchanged (pads/min, speed): pad use follows goals, and the saved run-up (~0.5 s
  per pad, 975 u/s pull against 300 u/s running) is too small a slice of a match to show. Still
  off by default; on for Billy to judge on the 27913 test server.
- **End of the arc** (Billy: "just before the landing, a slight advantage in speed, towards the end
  of the arc"): the mid-air hook waits until the bot is near the top or coming down (vertical speed
  under 100) within 450 units of the landing, and then, from above, hooks the floor just past the
  landing (a pull down and forward), 0.35 s. Matched pads (same distance and rise), off vs on: the
  big room's flights 1.40 vs 1.41 s (145/168 flights); other groups within noise. No cost in
  suicides or speed.

### IG-9 addendum 3 (2026-11-21): real slingshot distances

Billy, watching: "grapple far too close to themselves, not a proper slingshot". 38% of anchors were
under 200 units (the 24-30 degree pitches added with the wider search). Now 6-18 degrees down, 12
first, and at least 250 units: anchors mean 329 (Billy 275), average speed +5.7 u/s, hooks
17.1 -> 15.2/min, slingshots 10.6 -> 9.2/bot/min (30 matches).

### IG-13 addendum (2026-11-21): a pad flight is the pad hook's alone

Billy (q2rdm11 big room): bots hooked "below or before the landing point" in mid-air. Those were
slingshot chains and combat hooks carrying on in the pad's arc (floor anchors along the current
motion). While a pad flies the bot (`IG_InPadFlight`), a chain ends, strafing stops, and combat /
reposition hooks wait for the landing; only the pad hook may fire. Also measured: on the twin pads
of q2rdm11 the real landing is on average 60-90 units short and 64-80 lower than the planned
(reach end) spot, sometimes the level below -- the end-of-arc hook is aimed at the planned spot.

## IG-14: `ig_botstrafehack` -- bots land like Q2PRO clients (off)

- **Requirement:** `ig_botstrafehack 1` clears PMF_TIME_LAND before a bot's pmove (`IG_BotPmove`,
  p_client.c ClientThink), which is what Q2PRO's `sv_strafejump_hack` does for Q2PRO/R1Q2 clients.
- **Why:** a bot's pmove runs with the server's default parameters (svs.pmp), lock on, while Billy
  on Q2PRO never has the 144-200 ms post-landing jump lock.
- **Tested** (30 matches, q2dm1/q2rdm11/ztn2dm3): no measurable change, alone or with IG-11 strafe
  jumping (speed 301 vs 300-302, >400 u/s 15.0 vs 14.7-15.5%). Bots' landing hops already average
  ~620 u/s: the lock is not where their speed goes. Kept, off, for parity.

## IG-15: `ig_moverec` -- record a person's movement (off)

- **Requirement:** `ig_moverec 1` logs every movement command of a person (never a bot) as an
  `igmove` line (`IG_MoveRecord`, after the pmove in ClientThink): horizontal and vertical speed,
  ground, hook state, forward/side/up keys, view angles, position.
- **Why:** Billy keeps his slingshot momentum by strafe jumping during it; the bots' strafe code
  (IG-11) guesses the timing. His recorded keys and view turns are the reference to copy.
- **Tested:** in his recording session on 27913 (lines present, one per usercmd).

## IG-17: `ig_botpullhop` -- hop off the floor while a slingshot pulls (off)

- **Requirement:** `ig_botpullhop 1` (libvar `ig_pullhop`): while a slingshot's hook pulls, a bot
  touching the floor jumps that frame, where the floor ahead along its motion is safe.
- **Why:** from Billy's recording (IG-15), slingshot for slingshot: same anchors (315 vs 313 units,
  14 deg down), same 0.2 s pull -- but the bots were on the floor 41% of the pull (Billy 20%) and
  let go at 462 u/s (Billy 754). A floor anchor drags the body into the floor, where ground friction
  takes ~60% of OSP's 975 u/s each frame; a jump clears the ground before the friction.
- **Tested** (30 matches): speed 301 -> 313 u/s, > 600 u/s 8.2 -> 10.8% of the time, let-go speed
  462 -> 830. Cost: q2rdm11 falls/lava 0.24 -> 0.35/min (0.41 without the safe-floor check).
  Tried and dropped: pushing toward the hook while pulled (no gain), delaying the let-go for a hop
  (lost the gain: a held jump key cannot jump again).

## IG-18: `ig_scoremap` -- the map's name on the scoreboard (off)

- **Requirement:** `ig_scoremap 1`: the OSP scoreboard (OSP_showScores) shows `level.mapname` (the
  file name, no `.bsp`) centred above the leader banner (`IG_ScoreMapLine`, src/ig/ig_menu.c).
- **Why:** Billy wants the board to say which map it is.
- **Tested:** in game on 27913.

## IG-19: `ig_votemaplist` -- pick the map from a list (off)

- **Requirement:** `ig_votemaplist 1`: the vote menu's Map row opens a list of the rotation's maps,
  12 a page with next/previous, the staged one starred; picking one stages it as the stepping row did
  (resp.osp_r290 / osp_r254 bit 1) and returns to the vote menu on the Map row.
- **Why:** Billy: show all the options instead of cycling through each one (28 instagib maps).
- **Tested:** in game on 27913.

## IG-20: `ig_botstairhook` -- Billy's stairwell trick (off)

- **Requirement:** `ig_botstairhook 1` (libvar `ig_stairhook`), with the slingshot on: when the route
  500 units ahead rises 48-320 units (stairs, a ramp), the slingshot's jump is followed by an upward
  hook: 12-36 degrees up, straight on or up to 35 degrees to a side (the stairwell's walls), 200-650
  units out, hitting above the top of the climb, the body's path along the pull clear of the steps.
  It lets go at the top's height (or after 0.5 s, or stalled).
- **Why:** Billy: "grapple the side of a wall when going up stairs to slingshot up the stairwell".
- **Tested** (24 matches, q2dm1/q2rdm11/ztn2dm3, with IG-17 on): 940 stair hooks (q2dm1 547),
  72% let go at the top, 12% never latched. Averages unchanged (speed +1.4 u/s, upper +1.5 points,
  both within noise; falls/lava unchanged): stairs are a small slice of a match.
- **Not every time** (Billy): a climb is hooked with chance `ig_stairchance` (libvar, default
  0.35); declined, the bot runs up and does not ask again for 2.5 s. Stair hooks q2dm1 547 -> 209,
  q2rdm11 311 -> 111; other averages unchanged.

## IG-21: `ig_botaffinity` -- hook affinity per character (off)

- **Requirement:** `ig_botaffinity 1` (libvar `ig_affinity`): each Q3 bot has a hook affinity 0-1,
  character trait 49 (`CHARACTERISTIC_IG_HOOK`, chars.h); default_c.c sets 9 = "not set", worked out
  as 0.10 + 0.85 x (0.6 x JUMPER + 0.4 x AGGRESSION) (stock bots: Phobos 0.20 ... Klesk 0.95).
  scale = affinity / 0.55 (0.55 = the former behaviour) multiplies: slingshot start chance (and its
  cooldown), chain chance, stair chance, reposition and combat-hook chances, both pad tricks. Climb
  routes and the lava escape are never scaled.
- **Why:** Billy: "a grapple master value so some bots are heavy on grapple and some aren't".
- **Tested** (24 matches, with IG-17/IG-20): averages unchanged; per bot, hooks/min spread 12.0-17.0
  (off, random) -> 8.7-17.5 (on), the runners at the bottom (Phobos 8.7, Slash 10.4, Crash 11.1);
  the masters gain less (Klesk 13.9 -> 15.7): at 0.55 nearly every slingshot chance is already taken.
  Note: the character parser takes no negative numbers (a -1 broke default_c.c).

### IG-21 addendum (2026-11-22): fewer runners

Billy: fewer runners, more above average. The derived affinity is now 0.30 + 0.65 x (0.6 x JUMPER +
0.4 x AGGRESSION): stock bots 4 runners (< 0.45: Phobos 0.38, Ranger 0.40, Crash 0.42, Wrack 0.45),
6 average, 16 above average, 9 masters (was 8 / 10 / 10 / 7). Calibration unchanged (0.55 = former
behaviour), so the roster as a whole hooks somewhat more.

### Hang fixes (2026-11-22, Billy saw Razor stuck on q2rdm11)

- A combat/reposition hook still out when the fight ended was left to travelling, which only waited
  for it ("a combat hook still finishing"); only the fight step lets go, so the bot hung until OSP's
  hook_holdtime (7.5 s). Travelling now runs that step.
- A route (climb) hook past its 1.5 s hold that gets no closer (8 units) to the landing for 0.6 s
  lets go (`IG_GrappleDist`): Q3's own give-up waits for the hook to go out of sight.
- 24 matches (q2rdm11/q2dm1/ztn2dm3): bot hooks hung 1 s+ 28 -> 9, 3 s+ 7 -> 0, 6 s+ 3 -> 0;
  hooks/min 14.4 -> 15.8, speed +6 u/s (n.s.).

## IG-22: `ig_endvote` -- the end-of-match map vote (off)

- **Requirement:** `ig_endvote 1` (OSP rulesets): 3 s into the intermission every person (not bots)
  gets a menu of `ig_endvotemaps` (default 4, 2-8) maps from the mode's map list (`map_file`): the
  rotation's pick first, then random maps other than the one just played, unplayed ones first. Live
  counts and a countdown; a vote can be changed. It closes after `ig_endvotetime` s (default 15, 5-60)
  or once everybody has voted; most votes wins (a tie drawn at random, no votes = the rotation's pick),
  the level changes 3 s later. `mapvote` reopens the menu, `mapvote <n>` votes. Not held after a passed
  map or config vote or `map` (`manual_map`), on dmflags same-level, or with only bots on.
  Code: src/ig/ig_endvote.c; hooks in G_RunFrame (holds the exit while open), ClientCommand (menu keys
  ahead of the intermission guard) and p_view.c (the hi-score page refresh does not draw over the menu).
- **Why:** Billy: end-of-match map vote (Phase 2 roadmap).
- **Tested:** on 27913 with Billy (2 min timelimit). Bots-only match: no vote, rotation unchanged.
  Two bugs found there: OSP closes every menu 2 s into the intermission (ClientThink), so the menu now
  opens at 3 s; the intermission skips the menu flush, so the vote menu redraws on each key press.

## IG-23: `ig_menufast` -- menus redraw on the key press (off)

- **Requirement:** `ig_menufast 1`: osp_PMenu_Update sends the menu at once (reliable) instead of
  marking it dirty for ClientEndServerFrame's flush 0.2 s after the last press.
- **Why:** Billy: the menus feel laggy. The donor (OSP) sent every press at once; the delay is
  upstream Colosseum's bandwidth saving (~1.3 KB a redraw), not a 1998 limit.
- **Tested:** in game on 27913 ("works great").

## IG-24: `ig_nofallfx` -- no landing dip or grunt without fall damage (off)

- **Requirement:** `ig_nofallfx 1`, outside campaigns, with dmflags DF_NO_FALLING (8): a landing hard
  enough for fall damage (delta >= 15) makes the short landing thud (EV_FALLSHORT) instead of the grunt
  (EV_FALL / EV_FALLFAR), and sets no `fall_value`, so the view does not dip. Softer landings are
  unchanged (footstep). With fall damage on it changes nothing. Hook: P_FallingDamage (p_view.c).
- **Why:** Billy: instagib already has no fall damage, but the grunt and the dip still feel like it.
- **Tested:** in game on 27913.

## IG-25: `ig_railsplash` -- rail jumping (off)

- **Requirement:** `ig_railsplash 1` (or `2`), outside campaigns: where a rail slug stops on a solid,
  non-sky surface, every client (`1`: only the shooter) within `ig_railsplash_radius` (120) with a
  clear line from the impact is pushed away from it by `ig_railsplash_kick` x (1 - dist/radius) u/s
  (900 next to it), no damage. Hook: the end of fire_rail (src/ig/ig_rail.c).
- **Why:** Billy: rail jumping in instagib; a setting, shooter only by default.
- **Tested:** on 27913 by Billy: 450 too weak, 700 better, 900 "seems good".
- **Next:** bot rail jumps (bspc rail-jump routes with this push, the railgun on the weapon-jump travel,
  scaled by hook affinity).

## IG-26: `ig_botroutes` -- bots replay recorded routes (off, experimental)

- **Requirement:** `ig_botroutes 1` (OSP, hook on): routes in `q3bot/maps/<map>.route` (next to the
  navigation file; made by server/tools/route-extract.py from an `ig_moverec` log, chat-marked
  start/stop/bad or `--auto`). A bot near the first quarter of a route, on the ground, with no enemy
  in sight, takes it with chance `ig_routechance` (0.5, checked once a second); it follows the
  recording's keys and view at the nearest point ahead, jumps where the recording took off, fires each
  hook at the recorded anchor and lets go where the recording did; the brain's hook commands are
  dropped meanwhile. It leaves at the end, 128 units off the path on the ground (256 in the air or
  hooked), on a missed hook, an enemy in sight on the ground, or twice the recorded time.
  `ig_routedebug 1`/`2` logs it. Code: src/ig/ig_route.c, hooks in BotExecuteInput and
  BotClientCommand.
  A route plays only on the hook style it was recorded with (`hook <type>` line in its block, OSP when
  absent), and a hook that has latched while the bot moves under 60 u/s for 0.3 s is let go of and
  the route left ("stalled").
- **Why:** Billy: teach the bots his fast grapple route on q2rdm11.
- **Tested (2026-11-22, q2rdm11, 5 recorded runs):** bots take it and fire the hooks at the right
  points, but 0 of 12 attempts reached the end (best ~half): a frame-by-frame copy diverges after a
  different entry speed. **Next:** rework around hook events (jump, hook the anchor, let go near the
  recorded release point, steer to the next hook with the bot's own movement).
- **Fixed (2026-11-23):** on LMCTF's hook (half OSP's speed) Mynx replayed Billy's OSP gap crossing on
  q2rdm11: the hook latched ~0.3 s late, 90 units down in the gap, and the pull pinned her to the far
  wall under the lip for the hook's 7.5 s hold. Now LMCTF skips OSP recordings, and the stall rule
  lets go on any hook.

## IG-27: `ig_botpeace` -- bots that only move (off)

- **Requirement:** `ig_botpeace 1` (libvar `ig_peace`): BotFindEnemy returns no enemy and the roam
  code keeps no traces to hunt, so Q3 bots never aim, shoot or chase; everything else (routes, hooks,
  roam goals) runs.
- **Why:** Billy: watch the bots grapple without fighting; also takes enemy interruptions out of
  route tests.
- **Tested:** 27913, q2rdm11, 5 bots, 1 min: 0 kills, 74 slingshots.

## IG-28: `ig_botaffinitychance` -- affinity is the slingshot chance (off)

- **Requirement:** with `ig_botaffinity 1` and `ig_botaffinitychance 1` (libvar `ig_affinitychance`):
  a slingshot chance is taken with probability = the bot's hook affinity, instead of
  clamp(1 + (affinity/0.55 - 1) x 0.77, 0.4, 1).
- **Why:** Billy (no-fighting test): "are all the bots grapple masters?" Every derived affinity is
  >= 0.5 since the IG-21 floor, so the old formula took ~every chance for every bot (5 bots: 368-394
  slingshots in the same time).
- **Tested** (igbench, 60 matches: q2dm1/q2rdm11/ztn2dm3 x 20 seeds, 5 min): slingshots/bot/min
  9.9 -> 8.8 (q2dm1), 9.9 -> 8.4 (q2rdm11); frags, deaths, rail accuracy unchanged. Per bot
  (34 characters) hooks/min 9.6-16.4 (sd 1.43) -> 9.2-15.4 (sd 1.67): runners drop (Ranger 11.1 -> 9.5,
  Slash 12.6 -> 10.4), masters stay (Xaero 15.5, Sarge 14.1 -> 15.2). A modest spread: climbs and
  fight hooks are not scaled by it.

## IG-29: `ig_botwalkway` -- slingshots stay on the walkway near lava (off)

- **Requirement:** `ig_botwalkway 1` (libvar `ig_walkway`): where lava or slime lies within 192 units
  to either side of a slingshot's line (out to its length + 300), the floor along the line may not
  fall more than 48 below the lower of the bot's feet and its aim point; checked for the slingshot's
  start, a chain's next jump, and each candidate anchor's flight line. Away from lava nothing changes.
- **Why:** Billy (q2rdm11 lava room): bots hook at angles that throw them off the walkways.
- **Tested** (igbench as IG-28): applied everywhere, the rule cut q2rdm11 lava landings 135 -> 86 but
  cost q2dm1 a third of its slingshots and 24 u/s. Near lava only, side 192: q2rdm11 lava landings
  135 -> 103, lava deaths 0.39 -> 0.23/min, speed -2.8 u/s (not significant), slingshots 9.9 -> 9.1/min;
  q2dm1 and ztn2dm3 identical to the baseline. Lava landings after combat hooks and high-ground walks
  (half of them) are not covered.

## IG-30: `ig_botunstuck` -- stuck bots go back to a spawn point (off)

- **Requirement:** `ig_botunstuck 1`: a bot whose box is inside something solid (MASK_PLAYERSOLID) for
  a second, or that stays within 16 units across the floor for `ig_botunstuck_time` seconds (10)
  while asking to walk and not on the hook, is moved to a spawn point the ruleset picks, like a
  teleporter (flash, hook let go, no frag change). Never onto another player: it waits a think if
  the spot is taken. Logs `ig: unstuck <slot> solid|still at x y z`. Bots only; any mode.
- **Why:** Billy saw four bots ride q2dm1pro's lift up and down for minutes, "staring at the sky".
  The lift (func_plat, x 1664-1792, y 960-1088) rises into a player-clip lid that starts at its top
  height (z 1024) and covers all of it but a strip at the south edge. Bots stand in the middle, are
  carried into the clip, and pmove can no longer move them; they ride with the lift for the rest of
  the map, looking up and down at their goals.
- **Tested** (igbench, q2dm1pro/q2rdm11/q2dm1/ztn2dm3, 6 seeds, 5 bots, 5 min, no fighting): 4 moves
  in 24 matches, all "still", all real traps on replay with ig_moverec (treading water in place,
  wedged mid-air twice, pinned in a corner). No false moves. The lift case itself is rare (none in
  these matches); on 27913 for Billy to watch.

## IG-31: strafe jumping that chains -- `ig_strafe 2` and `ig_botjumpland` (off)

- **Requirement:** brain libvar `ig_strafe 2`, set from the game cvar `ig_botstrafe` (IG-11's strafe jumping, extended): a chain already
  going follows the route 300 units ahead through its bends; in the air the bot wishes toward the side
  the route bends, at the most-speed angle or at the route itself up to 75 degrees off its velocity;
  a route point behind it or closer than 96 units (it flew past) keeps the last good direction, and a
  landing just past one gets one more hop that way; coming down it asks for jump. Game cvar
  `ig_botjumpland 1`: a bot that asked for jump in the air keeps it held until it lands (the botlib
  drops jump every other frame), so it takes off again on the landing frame.
- **Why:** Billy wants bots to strafe jump when not hooking. His recording (route8, q2rdm11): in the air
  he holds forward and strafe 45-55 degrees off his velocity, gaining ~90 u/s per second, swaps
  sides near the top, carves through bends (46 degrees of view turn per hop), and presses jump just
  before he lands. IG-11 already aimed at that angle and bots gain as much in the air (+100-126 u/s
  per s at 30-50 degrees); it failed on chaining: 78% of chains ended before a second hop, mostly
  "the route turned" -- the bot overshoots the route's next point mid-jump.
- **Tested** (igbench, q2rdm11/q2dm1/ztn2dm3/q2dm1pro, 20 seeds, 5 bots, base = IG-28/29/30 on):
  chains of 2+ hops 7% -> 20%; time over 400 u/s 16.2 -> 18.6% (q2rdm11 16.7 -> 20.6%); speed
  +3.5 u/s (q2dm1 +5.7, q2rdm11 +5.0, both significant); slingshots 7.6 -> 6.1/bot/min and hooks
  -1.6/min (hops take some of their stretches); frags, accuracy, suicides unchanged.

## IG-32: `ig_climbrun` -- hook climbs let go at the top (off)

- **Requirement:** brain libvar `ig_climbrun` (bits), set from the game cvar `ig_botclimbrun` (with `ig_bothook`). 2: on a grapple route, once the hook is attached
  and the bot's origin is 8 units over the landing area's bottom (feet over its floor) within 48
  units of the area horizontally, it lets go that frame, and the pull's momentum carries it over
  the lip. 1 (experimental): within 40 units of the route's start it fires on the move once its view
  is within atan(16 / distance) degrees (2-6) of the aim and a body-sized trace clears 80% of the line
  to the anchor, instead of stopping until the aim is inside 2 degrees.
- **Why:** Billy: climbs are "awkwardly slow and weirdly positioned ... with a slight hold at the
  end". Route climbs: 74% fired from a standstill (Billy fires at ~250 u/s); after the pull the bot
  hung 0.19 s on average (29% over 0.3 s) because Q3 let go only when the pull stopped bringing it
  closer.
- **Tested** (igbench, 4 maps x 6 seeds, route climbs only, marked by `ig: climb fire`): bit 2 alone:
  hang 0.19 -> 0.06 s, hangs over 0.3 s 29 -> 5%, climbs landing on the higher level 79 -> 82%, speed
  and everything else unchanged. Bit 1: standstill fires 74 -> 6%, landings unchanged at 3 degrees (66%
  at an 8-degree tolerance: the hook landed ~40 units off the anchor), but a third fewer climbs and
  -7.6 u/s (a bot that runs past the start before its aim lines up loses the route). On 27913: 2.
- **Run-in retry (2026-11-27, bit 1 still off):** Billy: bots "completely stop moving and grapple and pull to move
  when a seasoned player would be moving in the direction that they want to grapple", and with a slow hook (LMCTF)
  he prefires. Bit 1 now: a run-in window from up to 200 units before the start (the run during the hook's flight:
  speed x anchor distance / `ig_hookfly`) to 160 past it along the line to the anchor, the route kept inside it,
  full speed only once the aim is within 15 degrees, and running on toward the anchor while the hook flies. 4 maps x
  4 seeds fighting: OSP standstill fires 71 -> 30%, speed at the fire 0 -> 222, but climbs landing up top ~280 ->
  ~170, -12 u/s, upper -3 pts; LMCTF alike. The stock grapple travel aborts and avoids a climb whenever the run-in
  strays from its start: the fix is a climb option in the planner (BOT-TRICKS T9).

## IG-33: `ig_lavarescue` -- lava, slime and pits put you back at a spawn point (off)

- **Requirement:** `ig_lavarescue 1`: damage of kind lava, slime or trigger_hurt (pits, the void) to a
  live player is cancelled in T_Damage before anything is taken, and the player is moved to a spawn
  point the ruleset picks, like a teleporter (flash, hook let go, velocity cleared): no death, no
  suicide, no frag lost. People and bots; any ruleset but the campaign. Server configs: on in instagib.cfg only, server.cfg resets it to 0. Logs
  `ig: respot <slot> lava|slime|pit at x y z`. Never onto someone: up to three spawn picks.
- **Why:** Billy: the Corkscrew mod's pit-fall recovery, which returns players without losing a frag.
- **Tested** (igbench q2rdm11, 10 seeds, 5 bots): 109 rescues, suicides 0.33 -> 0.00/min, frags/min
  3.29 -> 3.58 (no -1s), speed and deaths unchanged.
- **Fix found on the way (IG-30 too):** a body goes 10 units over the spawn spot, as PutClientInServer
  places it; at 9 the box touched the floor on q2rdm11's spots, every try read as blocked, and no one
  was moved.
- **Sound** (Billy: "it makes the lava sound and no teleport sound"): lava and slime are caught in
  P_WorldEffects as the player first touches them, before the splash sound (`IG_HazardRescueEnter`),
  so only the teleport is heard. Respot also clears the stored fall speed: the landing check in the
  same frame had turned the teleport event into a footstep. Retested: 58 rescues in 3 q2rdm11 matches,
  no lava deaths.

## IG-34: `hook_type` -- hook styles, LMCTF first (default `osp`)

- **Requirement:** `hook_type lmctf` makes the offhand hook behave, look and sound like LMCTF 6.0's:
  flies at 800 u/s (`hook_lmctf_speed`), no sky; pulls at 800 (`hook_lmctf_pull`) with gravity still
  pulling the player down; inside 120 units of the anchor the pull eases in steps (5x the distance at
  120 down to 1x at 10) so the player glides in and settles; a player hit takes
  `hook_lmctf_damage` (0: the hook lets go); the ghook model on a CTF grapple cable, blaster sparks
  where it bites, LMCTF's fire, flight (every 0.4 s), bite (wall / player) and pull (every 0.8 s)
  sounds. `hook_type osp` (default) is upstream. `ig_hookvote "osp lmctf"` turns the vote menu's
  hook row into Disabled -> OSP -> LMCTF, and `vote hook lmctf` / `vote hook osp` / `vote hook 0`
  work in the console; a passed vote sets hook_enable and hook_type. Empty = upstream's on/off row.
  server.cfg resets both, so a Config vote goes back to OSP's hook.
- **Why:** Billy wants other hook styles, LMCTF first, with LMCTF's sound and graphics, picked in the
  vote menu or set per config. LMCTF's source (packetflinger/lmctf) carries no licence: it was read
  for behaviour (speeds, the ease steps, which sound plays when), none of it is copied.
- **Assets:** LMCTF's pak4 sounds and model, extracted into `lmctf-hook.pak` (work/lmhook, never
  committed: LMCTF's assets) and installed in the gamedir; clients download it. Sounds are renamed to
  `sound/lmhook/` because LMCTF's `weapons/grapple/grfire.wav` shares its path with CTF's in baseq2
  (zacmemappak.pak), which clients would play instead. The cable model (`models/ctf/segment`) is in
  zacmemappak.pak.
- **Bots** (brain): the game passes the live hook's speeds (libvars `ig_hookfly`, `ig_hookpull`) and
  `ig_slingup`. The brain (1) recognises LMCTF's hook model as a hook -- it saw only OSP's beam and
  Threewave's model, so every LMCTF hook read as "no hook" (climbs bit 1% of the time); (2) scales the
  slingshot's OSP-tuned times and speeds to the live hook (flight timeout, latch-by-speed, "fast"
  release, pull time, stall speed, combat and stairwell pulls, pad pull) and the orphan watchdog to
  the flight time -- on OSP's 1650/975 every value is what it was; (3) on LMCTF hooks its slingshot
  anchors the same 6-18 degrees UP instead of down (`ig_slingup 100`): the pull's gravity drags a
  floor anchor's pull along the floor. Tried and dropped: flatter floor anchors, a higher climb aim
  (+32/+64), confirming a latch across a frame.
- **Tested** (igbench q2rdm11 + q2dm1, 5 seeds each; vs the brain before):
  OSP unchanged in every figure. LMCTF before -> after: 238 -> 287 u/s, > 600 u/s 2.9 -> 8.8%, climbs
  landing higher 1 -> 72%, upper levels 37 -> 48.6% (OSP 48.3), frags 3.69 -> 3.77. Reference, OSP's
  hook slowed to LMCTF's 800/800 (no gravity, no ease): 300 u/s, 12.0%, 70%, 44.5% -- so the bots are
  within 13 u/s of what that speed allows, and climb as well. Slingshots/bot/min 7.6 (reference
  10.4): LMCTF's hook flies half as fast, so each takes longer.

## IG-35: `ig_botfuncwall` -- a func_wall is the map, not an obstacle (off)

- **Requirement:** `ig_botfuncwall 1` (libvar `ig_funcwall`): a Quake III bot standing on a `func_wall`
  (a brush entity that never moves) is moved as if it stood on a `func_static`: in its area, on its
  route. Off: Q3's BotMoveToGoal, which knows plats, bobbers, trains, doors and func_static only, calls
  it "on top of an obstacle" (MOVERESULT_ONTOPOFOBSTACLE) and BotAIBlocked moves it at random. Code:
  botlib/be_ai_move.c (BotSetBrushModelTypes notes func_wall models; BotMoveToGoal), game cvar in
  src/ig/ig_bot.c. Group B, trial (27913).
- **Why:** Billy: bots circle the pentagon pads on q2rdm11. Those pads are func_wall plates (66x66, 6
  high) under every item spot; instagib's items are hidden but the pads stay, and high-ground goals
  (IG-7) land on them because a pad is a slightly higher floor.
- **Diagnostics added:** `ig_movedebug 1` logs each think of a bot seeking its long-term goal ("ig:
  move": area, goal, travel type, failure, block, blocking entity, area reachabilities; `2`: failures
  and blocks only) and, 3 s into a map, every non-client entity ("igent"); the brain logs its
  high-ground points ("ig: high ground point N at x y z area a").
- **Tested (2026-11-23, igbench, q2rdm11, 10 seeds, 5 bots, 27913's settings):** no-fighting matches
  (ig_botpeace), circling (2 s with > 400 units moved inside a 96-unit circle) 437 -> 311 windows; on
  the armor pad 35 -> 0, quad pad 13 -> 0. What is left is mostly strafe chains overshooting and
  turning back (next item). Fighting matches: every figure within noise (frags 3.26 -> 3.37, rail acc
  26.0 -> 25.7%, speed 329 -> 327 u/s, suicides 0.36 -> 0.35/min).
- **All 28 instagib maps (2026-11-23):** it is the entity type, not the items: any func_wall a bot stands
  on (pads, ledges, pillar tops). Bot thinks "on top of an obstacle" on a func_wall (no-fighting, 3 x 5
  min, 5 bots, ig_movedebug 2) went to 0 on every map that had them: koldduel1 5922 (13% of bot time; its
  func_walls are not pads), q2rdm11 1431, q2rdm12 809, ztn2dm4 651, ztn2dm5 284, omf 150, in2obl 95,
  brutalo 54, ztn2dm3 19. Fighting, 140 matches: frags 4.38 -> 4.38/min, rail acc 26.7 -> 26.6%, speed
  311 -> 311 u/s, all within noise. Still left, other entity types: func_door (q2rdm12, alkdm15, jtorque,
  q2rdm4), func_plat (in2obl), func_rotating (peacefrag).

## IG-36: `ig_botclimbaim`, `ig_botclimbland` -- hook climbs the way Billy takes them (off)

- **Requirement:** `ig_botclimbaim 1` (libvar `ig_climbaim`, with `ig_bothook`): for each hook-climb
  route, once, the brain finds the SPOT (the landing floor point with the most room, near the route's
  landing area) and a FRAME anchor: a wall point above it that faces the bot's eye, sits on a flat
  part of its plane (16 units around), and from which the pull -- the player's box 4 units wider --
  carries the body over the landing floor. The climb aims and fires at it, looks at the landing
  while pulled, and lets go once the landing floor is under it. `ig_botclimbland` (libvar
  `ig_climbland`, bits) says what it does next: 1 air strafe to the spot (wish the velocity that
  reaches it in 0.6 s, braking the speed it arrived with), 2 grapple down onto the floor at the spot,
  4 let go at the first moment a gravity arc would land it, 8 hold on until the pull stalls against
  the frame. `ig_botclimbaim 2` keeps bspc's anchor with the same landing. A per-map CLIMB TABLE
  (`q3bot/maps/<map>.climbs`, server repo, made by tools/climb-table.py from the IG-37 drill) lists
  the routes where bspc's anchor lands more often; those are plain bspc. A table made for another
  .aas (its `reachsize` line) is ignored. Floor anchors are worked out but never used on a climb.
  Code: botlib/be_ig_hook.c (IG_ClimbAim*, IG_ClimbLand, IG_FrontFlat, IG_PullLands), hooks in
  BotTravel_Grapple and BotMoveToGoal (be_ai_move.c).
- **Why:** Billy: at q2rdm11's lava-room door bots hooked the side of the frame (it sticks out on
  both sides) and landed in the corner; he hooks the frame and air-strafes to the middle.
- **Tested (2026-11-24, IG-37 drill):** the door climb (route 3059), 40 tries each: bspc 100% landed,
  0/40 within 64 of the middle; frame + air strafe 100% landed, 40/40 (median 16 units). On the way:
  floor anchors from below landed 0/712; the plain push off a narrow ledge; holding to the stuck rule
  (0.6 s, "too long"); side faces. All 28 instagib maps, fresh seed, ~100,000 tries each, bspc vs the
  bot's choice with the table: landed 88.5 -> 91.3%, landings within 64 of the middle 44 -> 59%,
  better on 27 of 28 maps (brutalo -1.7). Bot matches (28 maps x 5 seeds): frags, rail accuracy,
  speed, hooks, suicides within noise; q2rdm11 upper levels 52.7 -> 58.8% (significant).

## IG-37: `ig_drill` -- the climb drill (testing aid, off)

- **Requirement:** `ig_drill 1`: the first Quake III bot is put at each hook-climb route's start
  (the game places it on the brain's `igdrillpos x_y_z_yaw` command; a bot command reaches the game
  with its first argument only), climbs, and every try is logged: `igdrill reach R try N aim A result
  up|fell|nofire|hung dist D fire T land T at x y z start x y z pick P`. `ig_drilltries` per route,
  `ig_drillaim` the aims taken in turn (0 bspc, 1 frame, 2 bspc + landing, 3 the bot's own choice),
  `ig_drillpause` between tries; `ig_drill 2` repeats one route (the nearest to `ig_drillpoint x y z`,
  or `r<n>`). `ig_drillpart k/n` drills every n-th route (OSP ends a level after an hour without a
  human, and a map takes hours of game time; igbench runs the parts side by side in seconds). While
  drilling nothing else hooks for that bot and no route is taken out. Code: be_ai_move.c (IG_Drill),
  src/ig/ig_bot.c (IG_DrillCommand). igbench now copies a map's `.climbs` and `.route` with its .aas.
- **Why:** Billy: "are you able to reproduce a bot in that location and grappling?" -- matches only
  climb each route now and then; the drill gives every route clean tries, side by side.

## IG-38: `ig_botstrafeplan` -- strafe chains plan ahead (off)

- **Requirement:** with `ig_botstrafe 2` and `ig_botstrafeplan 1` (brain libvar `ig_strafeplan`), a
  strafe-jump chain reads the route ahead and starts only with a 400-unit runway clear of sharp turns,
  narrow gaps and lava; it keeps hopping only while the first of those is further than half a second
  away at the bot's speed, and stops (lands and runs) before it.
- **Why:** Billy (2026-11-23): bots lose control in chains; flying up q2rdm11's lava-room stairs they
  turn left and momentum carries them into the lava.
- **Tested (2026-11-24):** q2rdm11, 10 matches: lava rescues 0.52 -> 0.335/bot-min, air collisions
  6.64 -> 5.48/bot-min. 28 maps: neutral except time over 400 u/s -1 point (the narrow-gap check is too
  strict; loosen it). Trial on 27913.

## IG-39: `ig_botarcsafe` -- a flight check before letting go or hopping (WIP, off)

- **Requirement:** `ig_botarcsafe 1` (libvar `ig_arcsafe`): before a slingshot or fight hook lets go,
  or a strafe hop, the brain simulates the flight; it holds on (up to 0.8 s) or skips the hop when the
  flight would end in lava or slime or a drop of 400+. Hooks ignore wall knocks (holding for them cost
  fight hooks 12 u/s); strafe hops also refuse a wall at more than 350 u/s.
- **Why:** the lava trips IG-38 leaves (q2rdm11: after strafe hops 24, slingshots 21, climbs 10, fight
  hooks 8 in 10 matches).
- **Tested so far:** q2rdm11, 10 matches: neutral (lava 0.335 -> 0.328/bot-min, speed unchanged).
  The check flies a straight hop while the chain steers in the air; next it must follow that steering.

## IG-40: the movement planner (WIP, branch `planner`)

- **Requirement:** one planner replaces the trick-by-trick movement control (most of `ai_ig_sling.c`,
  the climb landing): each think it lists options (run, hop, slingshot to a few anchors, climb, let go,
  hold), simulates each 1-2 s with Quake II's movement and the hook pull, scores progress along the route,
  speed kept, landing room and hazards, and picks among the near-best with personality weights. Every
  trick in `BOT-TRICKS.md` is kept, produced, or dropped on purpose there.
- **Why:** Billy (2026-11-24): the fixes had become spaghetti, one patch per trick per hazard (IG-29, 38,
  39 and the climb tables are the same missing piece: knowing where a move ends).
- **Step 1, done -- a predictor that matches the game.** `ig_predcheck 1` (brain libvar, testing aid)
  replays each bot's real inputs from 1, 5 and 10 thinks back and compares with where it really is
  (`game_q3/ai_ig_pred.c`). q2rdm11, 3 matches, median horizontal error:

  | ahead | stock Q3 predictor | same, Q3 bugs fixed | Q2 pmove port | position + velocity * t |
  |---|---|---|---|---|
  | 0.1 s | 2.3 | 2.1 | 0.2 | 2.3 |
  | 0.5 s | 39.7 | 39.4 | 0.9 | 61.7 |
  | 1.0 s | 124.5 | 120.0 | 1.8 | 199.9 |

  The stock predictor (be_aas_move.c) collides with the navigation mesh and has ground and water
  friction swapped and gravity right only at 0.1 s steps (`aas_ig_q2phys` fixes those, off by default;
  it barely helps). The port (`game_q3/ai_ig_pmove.c`, from Q2PRO's pmove, GPL) uses real map traces:
  1.6 us and ~5.7 traces per 50 ms step. 80% of 0.5 s predictions are within 8 units; the rest are
  mostly hook pulls (90% with hooks off), then jump pads, movers and other players.
- **Step 2, done -- the hook and the rest of the body.** The simulation now fires the hook from the hand,
  flies it once per 0.1 s game frame at `ig_hookfly` (sky only with `ig_hooksky`), and after each pmove
  sets the pull as GrapplePull does: OSP at `ig_hookpull`, 425 inside 64; LMCTF (`ig_hookstyle 1`, a new
  libvar from the game) with a frame of gravity and the glide-in. Also ducking (Q3 crouchers: 100 u/s,
  box top 4), and jump pads and teleporters flagged by navigation area (`pad`, `teleport`), not
  simulated: the planner takes a pad's landing from the navigation file. A bot tells its own hook by
  its hand (`IG_OwnHook`: the hook's old_origin, which the game re-projects to its owner every frame);
  for that the LMCTF hook now keeps old_origin (`FL_OLDORGNOTSET`; old_origin is not sent to clients
  for a model, so nothing changes for players). Windows right after a teleport or respawn are skipped
  (Q2 holds a player still there). q2rdm11, 3 matches, share of predictions within 8 units:

  | ahead | all | no hook | hooked throughout | hook fired or let go |
  |---|---|---|---|---|
  | 0.5 s, OSP hook | 96.3% | 97.6% | 86.8% | 92.3% |
  | 1.0 s, OSP hook | 91.0% | 93.6% | 71.8% | 85.5% |
  | 0.5 s, LMCTF hook | 96.2% | 97.4% | 88.3% | 93.7% |

  Medians 0.5-1.6 units. Cost: 2 us and 6 traces per 50 ms step.
- **Step 3, v1 -- the planner** (`game_q3/ai_ig_plan.c`; game `ig_botplan` -> libvar `ig_plan`, off).
  From the ground: run (the stock route move), strafe-hop, or slingshot to one of 12 anchors (3 yaws
  around the route x pitches 20/8 down, 8/25 up; 200-900 units, solid, sky only with `ig_hooksky`).
  In the air: the same, flown on; flicked last think: fire. Hooked (our hook): let go, let go and hop,
  hold 1/2/3/8 more thinks. Each is flown 1.2 s in IG_Pmove with the input the bot would send (route
  wish on the ground, the strafe-jump angle toward the route in the air, the jump pressed while coming
  down, the game's `ig_botjumpland` hold) and costed: time + the navigation's travel time from where it
  ends - 0.35 s per 300 u/s carried along the route + 0.15 s per hard wall hit + a hook price
  (`ig_planhookcost`, 0.5 s at affinity 0.55 since v1.3, was 0.3, scaled by the bot's hook affinity). Lava or slime touched,
  or a pit or lava under where it comes down: out. A pick among those within 0.06 s of the best, hooks
  weighted by affinity; the option under way is favoured by 0.04 s. Steps back for anything the
  navigation does itself (ladders, lifts, climb routes, pads), and keeps for now: the jump-pad hooks
  (IG-13), the lava escape, the fight hook (IG-10), now checked by the simulation before it fires and
  before it lets go (`IG_PlanFireUnsafe`, `IG_PlanHookUnsafe`).
  **The guard** (`IG_PlanGuard`, every think, battle nodes only): an input the planner did not make
  -- a fight's dodge -- that would reach lava, slime or a pit within 0.5 s is changed for the nearest
  safe one (no jump; turned 45/90/135 either way; still). Combat strafing walked bots off q2rdm11's
  walkways: most of what the planner left.
  **Found on the way:** the adapter's roaming-view clamp (be_interface_q2.c, 45 degrees from the walk)
  turned flicks off their anchor -- 64% of the planner's hooks flew past (now skipped when the planner
  set the view: `IG_PlanViewSet`); a flying hook's direction is the one it was fired along.
  Diagnostics: libvar `ig_plandebug` 1 (decisions), 2 (also every think: `igpl`, `igfire`).
  **Measured** (5 bots, skill 3, trial settings of 27913, vs the same without the planner):

  | | current build | planner |
  |---|---|---|
  | q2rdm11 (10 matches): lava trips / bot-min | 0.335 | 0.167 |
  | q2rdm11: upper level % | 58.6 | 66.5 |
  | q2rdm11: speed u/s | 327 | 344 |
  | 28 maps (3 each): speed u/s | 309 | 330 |
  | 28 maps: over 400 u/s % | 15.4 | 19.7 |
  | 28 maps: upper level % | 27.7 | 31.1 |
  | 28 maps: frags/min | 4.40 | 4.60 |
  | 28 maps: hooks/min (Billy 18) | 12.4 | 16.9 |
  | 28 maps: rail accuracy % | 26.5 | 26.2 |
  | 28 maps: suicides/min | 0.14 | 0.09 |

  Worse only on brutalo (platforms over a void: 301 -> 285 u/s, falls 1.21 -> 0.93/min): the guard
  turns fight dodges away from the edges. Cost: a 5-minute match takes ~4 s instead of 1.4 s (about
  1% of a core per bot at real speed).
- **v1.1 (same day):** (a) options are scored where their momentum is spent: after the 1.2 s, the
  route is run on (up to 1.5 s) until the bot is on the ground under 330 u/s. v1 scored speed at the
  1.2 s mark with a bonus; a slingshot's speed fades after the let-go, so its score worsened every
  think as the mark slid on and 45% of flicks never fired (the next think's fire cost +0.05 s on
  the flick's prediction, p90 +0.72; now -0.09, p90 +0.19 -- one think is 0.1 s). (b) A flick
  commits: its fire next think is favoured by 0.25 s (`PLAN_COMMIT`). (c) The planner decides once
  per think (a node switch ran it twice; a second call could fire along a view not yet sent).
  (d) A simulated run stops where the route takes a step it does not model (gap or barrier jump,
  ladder, lift, climb, pad, teleporter) and is scored from there: it walked off brutalo's and
  q2rdm4's gaps and was ruled out as a pit (q2rdm4 upper 18.2 -> 22.5%, now level with no planner).
  (e) The pull hop (T3) is an option while hooked on the floor; the simulation rarely finds it pays.
  28 maps: 309 -> 330 u/s, frags 4.40 -> 4.62, hooks 17.4/min, suicides 0.14 -> 0.08.
  brutalo (8 matches): the guard costs upper level 34 -> 28% and 303 -> 281 u/s for falls 1.22 ->
  0.87/min (guard off: 32%, 299 u/s, 1.17; libvar `ig_planguard 0`, testing).
- **v1.2:** pits. bspc makes a `trigger_hurt` (a pit's death zone) lava in the navigation file, where
  the map has none; the simulation now treats an area the navigation file calls lava or slime as lava,
  and flies a fall out to its landing (up to 2.5 s) instead of tracing 1000 units down -- tltf2's and
  brutalo's voids have a floor below the death zone. 28 maps, route planner vs no planner: suicides
  0.14 -> 0.05/min (-64%), frags 4.40 -> 4.65, 309 -> 332 u/s, hooks 17.3/min.
- **Fight movement (built, off: libvar `ig_planfight`, default 0).** `IG_PlanFight` in place of
  BotAttackMove: 14 dodges (strafe either side square/in/out, with and without a hop; in; out) flown
  0.8 s against the enemy where it is, costed by speed across the enemy's line, peeking (in sight while
  the rail is loaded, out of it during the 1.5 s reload, from the bot's own shots), range, height; kept
  0.3-0.8 s. All bots on it: kills and deaths both -8%, rail accuracy 26 -> 22.5% (everyone is harder
  to hit), upper level -4 points. Head to head (`ig_pfodd 1`: odd clients on it, 28 maps x 4, 6 bots):
  K/D 0.99 vs 1.00 stock, pit falls 0.11 vs 0.29 a bot, 6% fewer fights. Even, slower-paced, worse
  positioned: left off until it values position (high ground, cover) better.
  **2026-11-25, position and range** (head to head, `ig_pfodd 1`, 28 maps x 5 seeds, 6 bots; read
  against a NULL run with stock fights for all -- the odd/even split alone gives odd K/D 0.94-0.96 vs
  even 1.00-1.03, the slots draw different characters). New libvars, all default to the old scoring:
  `ig_pfroute` (cost per second of travel to the long-term goal, from the goal stack), `ig_pfhigh`
  (high ground, was a fixed 0.03), `ig_pfnear`/`ig_pfnearat` (cost per think per 1000 units beyond
  `nearat`). Finding: the old fight planner lost on AIM, not position -- rail accuracy 21.9% vs 24.9%
  null -- because it never closed in (stock walks to 140 units; a rail's aim error is an angle).
  Ruled out by test: sideways-speed reward, hops, peeking, and leading the aim by the bot's own
  velocity (`ig_aimlead` 0.05/0.1 s: level / worse; removed). Route 0.3 and high 0.1 alone: upper
  level back to null, K/D unchanged. Best: `ig_pfroute 0.3 ig_pfhigh 0.1 ig_pfnear 0.3 ig_pfnearat 200
  ig_pflat 0.3`: odd bots vs null over 5 seeds: K/D 0.946 vs 0.954 (level), kills/bot 25.5 vs 23.4
  (+9%), rail acc 28.1 vs 25.0%, suicides 0.15 vs 0.35/bot (-57%), contacts +11%, 353 vs 334 u/s,
  upper level 25.6 vs 28.5%. A faster, closer fight at the same K/D; still off (Billy to watch).
  Bench data work/fight, script work/plan/fightvs.py.
  **Stances** (`ig_pfstance 1`; Billy: "I dont necessarily want them always closing in though, railgun
  is about angles and keeping distance sometimes"): each fight (new enemy, or every 3-6 s) picks a
  range to hold -- close 250, mid 550, long 900 (+-150 free), odds from aggression: close 0.15 + 0.5a,
  long 0.45 - 0.4a; `ig_pfnear` then prices distance either side of it. Same 5 seeds: K/D 0.951 (null
  0.954, always-close 0.946), kills/bot 21.4 (23.4 / 25.5), rail acc 22.2% (25.0 / 28.1), suicides 0.17
  (0.35 / 0.14), upper 26.5% (28.5 / 25.6). Range sets the pace, not the K/D. On 27913 for Billy.
- **No jumping in place** (`ig_planhopmin`, libvar, default 250 u/s; 0 = off). Billy (27913): "the bots
  sometimes just jump in place when they dont have any movement, that shouldnt be happening, the jumping
  should be in use for movement purposes". Measured (8 maps x 2, `ig_moverec 2`, a jump = leaving the
  ground at 200-300 u/s up from under 100 u/s across): 1530 such jumps (3.2/bot-min), mostly landing
  hops after a slow landing, the stock fight move's jumps (jumper trait), fight-planner hops and the
  planner's hop option (after a respawn). Now: on the ground the hop and landing-hop options need 250
  u/s, fight hops too, and a last check on the final input (IGX_NoStandingJump) drops any jump the
  planner, fight planner or stock fight move set under 250 u/s -- keeping the slingshot's start and the
  route's own jumps (ledges, gaps). 1530 -> 350 (-77%); of those left, 200 are slingshot starts whose
  hook fired, 141 (0.3/bot-min) starts that changed their mind. Frags 4.43 -> 4.47, 337 -> 340 u/s,
  hooks, upper, suicides level.
- **Pad top (WIP, off: libvar `ig_planpadsling`).** Billy recorded his pad trick (2026-11-25, q2rdm11,
  work/hook-live/billy-route-2026-11-25.log; "a jump pad into a grapple and strafe jumping to get
  across the map fast"): the vertical pad at (1551, 763) throws him straight up (718 up, 91 across);
  rising he turns ~150 degrees; just past the top (falling at 214) he hooks LEVEL and far along the
  upper walkway (4 up, a wall ~480 out; 0.30 s hook flight), a 0.27 s pull to 938 u/s, lets go still
  rising, clips the walkway wall (706 kept) and hops it 4 times with no frame on the ground (jump
  held before each landing), ~2000 units at 706 u/s. Built: in a pad flight the planner runs in pad
  mode (IG_PlanPadTop): the route from the pad's landing (or toward the goal), "run" = ride the pad,
  and once a flight, below 150 u/s up (`ig_planpadtopvz`), slingshots to IGA_PADTOP anchors (+-16
  yaw, 6 down to 12 up, 300+ units, walls allowed; price x `ig_planpadmul` 0.3), simulated like any
  option; the old end-of-arc hook stands down while it is on. q2rdm11/campgrounds/tltf2 x 6: 45
  pad-top slingshots in 862 launches; distance in the 3 s after a launch 1010 -> 1031 (noise). WHY
  so few: the bots never take Billy's pads -- 0 launches from (1551, 763) and 1 from its twin
  (-880, 973) in 6 q2rdm11 matches. These pads push straight up with no landing of their own; bspc
  made no reachability through them (the twin is the pad bots got stuck bouncing on, IG-42). Next:
  pad reachabilities for vertical pads (apex -> floors reachable by air control or a hook).
- **Billy's line, drilled** (IG-43 pad drill route mode: `ig_paddrillstart 1418_738_96_0
  ig_paddrillgoal -963_1087_280 ig_paddrillreach 4060 ig_paddrillvia 300_1110_344`, one bot, peace).
  Billy, watching: "I was hoping to have just the one grapple and strafe jumping used to keep
  momentum ... the grapple as the start to the speed". Found and fixed (libvars, default on):
  `ig_planplainlandhop` -- the landing-hop option pressed jump only after a hook's let-go, so on a
  plain flight it was "run" and the bot landed flat; `ig_planpadover` -- a pad's flight ends at its
  first landing (a landing hopped at once kept the stock pad travel "finishing", the next hop was
  flown in pad mode, no landing hop: 808 -> 396 u/s); `ig_planlandhopbonus` 0.25 -- in the air at
  400+ u/s the landing hop is the habit; `ig_planfasthook` (`ig_planfasthookdiv` 150) -- above 450
  u/s a hook's price grows with speed. Drill (2 x 5 min): median 7.1 -> 6.2 s (best 5.0; Billy ~5.5),
  walkway average 497 -> 583 u/s (Billy 706), extra hooks after the pad's 2.3 -> 1.5 a try, fast
  landings hopped 79%. The extra hooks now come after speed is lost (98 of 126 at 200-600 u/s),
  mostly where a hop comes down on the face of the walkway's raised step (x ~590: 790 -> 255 u/s);
  Billy's hops land on top of it. Steeper hook prices changed nothing. 28 maps x 2 (all four vs
  none): everything level (334.6 vs 333.5 u/s, frags 4.84 vs 4.81, hooks 14.5 vs 14.2/min).
- **The view in a strafing flight** (`ig_planflightview`, libvar, default 2). Billy on the drill:
  "the model converts into strafe jumping backwards". With no enemy the view was the stock
  look-ahead (or a random roam spot), clamped near the push direction -- 70-90 degrees off the
  travel at speed -- and chased a target that jumped around at its 360 deg/s limit: the model spun,
  facing backwards in 43% of fast frames, 79 degrees off the travel (median). Now, on a flight the
  planner steers above 250 u/s: the travel, leaning a tenth toward the push (mode 2; mode 1: 45
  degrees from the push), level. Drill: backwards 43 -> 2%, off the travel 79 -> 9 degrees (Billy's
  route: 5), walkway 583 -> 643 u/s, median 6.2 -> 6.0 s. 28 maps x 2: level (frags 4.81 vs 4.77,
  rail acc 26.1 vs 25.9, to contact 2.77 vs 2.82 s). Diagnostic: libvar `ig_viewdebug 1` logs
  `igview` (view, ideal, move direction) each think.
- **Hop chains everywhere** (Billy: chain the route with strafe jumps and hooks; first, hop every fast
  landing). Roaming (peace), 4 maps (q2rdm11, campgrounds, q2dm1, ztn2dm3): 52% of landings above 350
  u/s were hopped (Billy nearly all). Of the flat ones: ~500 "run" (or "let go") picked by chance
  over a near-equal "hop the landing"; ~100 landings that came inside the think while still rising
  (steps, stairs), before the jump went down; ~260 with jump down in the step that landed -- Quake II
  checks the jump at a step's start -- and "run" on the next think. Now (libvars, default on): in the
  air at 300+ u/s, or on the think after a landing with the hop pending, a "run"/"let go" pick gives
  way to "hop the landing" unless that is 0.5 s worse (`ig_planairhop`, `ig_planairhopspeed` 300,
  `ig_planairhopmargin` 0.5); still rising but down within the think, a delayed jump (released the
  first half-think, pressed the second; `ig_planlatejump`; EA_IGMove jump 2). Same 4 maps: hopped 52
  -> 72%, average chain 2.0 -> 3.4 hops, chains of 3+ 23 -> 50%. 28 maps x 2 with fights, on vs off:
  level (334.3 vs 333.1 u/s, frags 4.77 vs 4.76, suicides 0.08 vs 0.07) except brutalo, suicides
  1.08 -> 1.27/min (interval excludes 0): its edges.
- **Corners** (Billy, watching roaming bots on 27913: "hitting the end of the walls before changing
  directions instead of curving"; the hops before q2rdm11's railgun overlook hit the wall before
  turning). The route's way was the next step's start -- a corner -- until within 40 units of it.
  Added a look-ahead (`ig_planlook`, `ig_planlookdist` 500): once a think, the route walked on
  through plain walking steps, the farthest point in a straight line (body trace raised a step,
  floor under the line, no lava/slime) steered at by the decision and the simulation until reached.
  Neutral alone (air wall hits 14.5 vs 14.1/bot-min). The hop habit was the wall-hitter: roaming, 4
  maps x 2, 3 bots -- no habit 53% hopped / 10.3 air wall hits per bot-min; habit unchecked 71% /
  14.1; habit only where the hop's simulation hits no wall (200+ u/s lost) within the first 4 thinks
  (`ig_planwallsoon` 4, default) 59% / 11.9; within 7: 56% / 10.8. A fast bot barely turns in the
  air (~30 deg/s at 600 u/s): a hop just before a corner carries it into the wall. Billy hops and
  curves: how he sets a turn up is the corner recording still to make.
- **Billy's corner recordings** (2026-11-25, no bots, no hooks: work/hook-live/billy-corners-2026-11-25.log,
  billy-corners2-2026-11-25.log; 13 + 25 turns). Gentle turns (50-75 deg) he hops through, on the
  ground 13-35%, keeping 300-390 u/s; sharp turns in open space (109 deg at 460, a U-turn at 486) he
  also hops through, 0-12% on the ground; sharp tight ones he stops hopping before -- on the last
  hop forward released, side key only, the view turned ~55 degrees in the air -- lands and turns on
  the ground (610 -> 130-180 u/s), then rebuilds. Room decides, not the angle. Billy vs the bots
  (roaming, 50 ms samples): fast landings hopped 66/59% vs 58%, average speed 382/396 vs 333, air
  wall hits 6.5/2.8 vs 11.9 per minute: the bots hop as often; they hit walls 2-4x as often and are
  ~60 u/s slower. A corner check on the hop habit (`ig_planturnhop`: no habit hop when the route
  turns more than N degrees within a hop's reach): 70 -> wall hits 10.4, hops 54%, 324 u/s; 100
  (default) 11.5 / 57% / 332; off 11.9 / 58% / 333.
  Billy's 55 turns matched with bot passes (q2rdm11, peace, 6 x 10 min, same spot and headings in/out):
  per turn Billy in 394 / low 288 / out 385 u/s, ground 33%, wall hits 0.05; bots 402 / 234 / 388, 35%,
  0.23 -- close, except the corners the bots enter far faster (U-turn 477 1616: 726 vs 432; 1674 883:
  630 vs 148) and hit. Two thirds of the bots' air wall hits come within 1.5 s of a hook let-go,
  mostly at 700+ u/s: slingshots into corners. A heavier wall penalty everywhere (`ig_planwallmul`
  2/4) makes bots cautious all round (hops 56 -> 53/48%, 330 -> 321/314 u/s); only after a let-go
  (`ig_planreleasewallmul`, default 3): roaming 4 maps x 2 -- air wall hits 11.5 -> 7.6/bot-min
  (Billy 3-6), slingshots 16.8 -> 12.8/bot-min, 330 -> 319 u/s, hops 56 -> 49%. 28 maps x 2 with
  fights: slingshots 5.27 -> 3.82/bot/min and hooks 14.5 -> 12.8/min (both intervals exclude 0),
  330 vs 335 u/s, frags 4.71 vs 4.79 (noise). For Billy to judge on 27913.
- **v1.3: hook price 0.3 -> 0.5 s.** Billy on 27913 (in2obl): bots "grapple random walls instead of
  taking the paths, it definitely seems pushed". in2obl, 8 matches: slingshots 2.9 (no planner) ->
  8.0 (0.3) / 5.6 (0.5) / 3.6 (0.8) a bot a minute; neutral tie-breaking among near-equal options
  (`ig_planhookpick 0`) changed nothing. 28 maps at 0.5: hooks 14.8/min, slingshots 5.8/bot/min (old
  code 5.1), 326 u/s (no planner 309), frags 4.58, suicides 0.05. At 0.8: 12.5 hooks/min, 318 u/s.
  Per map and per style later, from Billy's recordings.
- **v1.4: anchor classes.** Billy: "more slingshots and fewer random wall grapples for no reason".
  A flat price cut both. Anchors now come in three classes, priced apart: SLING (along the route,
  +-12 degrees, 6/12/18 down, 250+ units: his slingshot, price x `ig_planslingmul` 0.3), UP (25 up,
  only where the route climbs 48+ within 500 units: stairwell/ledge, x `ig_planupmul` 1), SIDE (40 off
  the route, 8 up or down: a corner turn, x `ig_plansidemul` 2). The slingshot line names the class.
  28 maps vs no planner: slingshots 5.1 -> 7.2/bot/min, hooks 12.4 -> 16.3/min, of the planner's hooks
  77% sling, 13% up, 11% side; 322 u/s, frags 4.62, suicides 0.05. in2obl: sling 71%, slingshots 2.9 ->
  5.2/bot/min (at slingmul 0.5).
- **v1.5: slingshots hook the floor.** Billy on 27913 (q2rdm4): still "random wall grapples instead of
  ground movement". Of q2rdm4's slingshot-class hooks, 575 of 898 were walls ~400 units out (a
  corridor's end or turn): from the eye on the ground, 12 and 18 down meet the floor inside the 250
  minimum, so only 6 down survived and it often met a wall first. Now SLING anchors are 4/8/12 down,
  180+ units at the flick (the bot jumps before it fires, so the hook bites further), and must be
  floor or a wall 600+ units out. q2rdm4 (6 matches): 741 floor, 72 far wall; 287 -> 306 u/s vs no
  planner, slingshots 3.9 -> 6.8/bot/min. The slingshot line says floor/wall/ceiling.
- **Open: "pull hop".** Billy: "pull hops are the start to the fastest movement". The let-go speed
  depends on how the pull ends (q2rdm11): a jump in the let-go step keeps ~930 u/s, a let-go in the
  air 720-850, on the floor 180-560 (friction takes the first step: OSP's pull SETS the velocity each
  step, so a hop during the pull is wiped at once). Hopping through the pull (delayed jump every think)
  put bots on the floor more (40 vs 31%); one jump at the let-go scored ~1.1 s worse in the planner (it
  flies 1000+ units down corridors into walls). Billy may mean something else: waiting for his recorded
  demonstration (27913, chat start/stop). Experiment kept as work/plan/pullhop-experiment.patch.
- **WIP: Billy's slingshot, from his demo** (27913, 2026-11-25, 5 marked tries, 8 ms commands). He
  jumps 130-200 ms before firing, fires in the air 10-19 down, the hook bites 160-290 ms later, he lets
  go 80-180 ms after the bite STILL IN THE AIR (860-940 u/s), has jump down before he lands (off the
  floor again within 8 ms), and hops on, holding 860-930 u/s for a second or more. The "fast start" is
  the start of this slingshot (Billy: "slingshots and fast starts are the same"); "pull hop" (hopping
  during the pull) was a wrong reading of his older data and is shelved. Built so far: a landing-hop
  option (`landhop`: jump down coming down), made sticky once chosen and part of every planner let-go
  unless unsafe; a flow value (`ig_planflow`, s per 300 u/s of average speed kept over the flight);
  the planner keeps steering its own flight through a fight that starts mid-air and lets go of its own
  hook there. Planner slingshots, q2rdm11: landing hopped 18 -> 34% (Billy 100%), let-go speed 673 ->
  757 (Billy ~900), let go in the air 50% (Billy 100%), 0.5 s later 310 -> 315 u/s (Billy ~900); 336 ->
  347-355 u/s overall, BUT lava trips 0.195 -> 0.29/bot-min (the mid-fight flight handling hops and lets
  go unchecked): not deployed. Next: let go before the pull drags to the floor; simulate the mid-fight
  flight like everything else; then hop chains at speed.
  **Then:** (a) every 50 ms pulled along the floor costs (`ig_planpullfloor` 0.25 s) and a simulated
  slingshot lets go when the floor is within a think's fall -- let go in the air 50 -> 98%, let-go speed
  673 -> 939 (Billy ~900); (b) the mid-fight flight is simulated before it is steered or hopped, and its
  hook let go only when that is safe; (c) a landing hop is decided with every let-go and favoured like
  a flick's fire (`PLAN_COMMIT`) until it is done, on the ground just after touchdown too -- through the
  simulation: forced, it hopped 77% of landings but doubled wall hits (4.5 -> 8.1/bot-min), raised lava
  trips and cost 13% of frags; (d) slingshot options are flown with that finish (`ig_planslinghop`), so
  anchors whose line has room to hop score best. q2rdm11, planner slingshots: landing hopped 39% (Billy
  100%), 0.5 s after 327 u/s (Billy ~900), lava 0.173 (v1.5 0.195), frags 4.02 -> 3.79, speed level.
  Checked properly, most landing hops at ~940 u/s on q2rdm11 meet a wall or a turn: Billy picks lines
  with room; the planner does so only where the simulation finds one.
  **Two old behaviours the planner had dropped, back:** a reposition urged after a kill or two misses
  (IG-10) is taken while travelling as a strong reason (0.5 s) to hook away now, and spent (it had been
  left pending until the next fight); a fight hook's let-go hands its flight to the planner, which hops
  the landing through its simulation (the old IG_Hop after every fight-hook let-go only ran in the old
  slingshot step). q2rdm11 (6 matches): lava trips 0.173 -> 0.104, the rest level.
- **Strafe jumping (2026-11-25).** Billy wants it like his. His route8 session, no hook for 1.5 s:
  chains of ~4 hops from 330 to 542 u/s (best 737), +58 u/s a second, 31% of the time over 400. Bots
  (planner): 300 -> 344, +2 u/s a second, 1% over 400. Two causes: (1) `IGX_AirWish` wished along the
  route when it lay within the strafe angle -- along the velocity above 300 Q2 adds nothing; now
  always at the strafe-jump angle (acos(285/speed)) on the route's side, so a straight run swings
  across it; (2) 40% of air time is in fights, where the planner's flight steered along the velocity
  and the stock attack move gave no input: now, airborne in a fight and not hooked, the bot strafes at
  that angle with the side swapped every think (flown first, as everything). Measured per think: 10-30
  degrees off the velocity +14 u/s, 30-60 +11. q2rdm11 chains 300 -> 386 u/s (still ~3 hops: they end
  sooner than Billy's). 28 maps vs v1.4: 322 -> 334 u/s, over 400 17.4 -> 20.1%, frags 4.62 -> 4.44 and
  suicides 0.05 -> 0.08 (both within noise, both the wrong way). Next: longer chains.
- **Strafe skill per bot (2026-11-25).** Billy: "some great bots and some not so great in the game at
  once". The theoretical limit (perfect angle, a jump every landing frame, from 300 u/s): 540 u/s after
  500 units, 664 after 1000, 823 after 2000, 933 after 3000; 50 ms and 8 ms steps give the same limit.
  A bot's wish holds for two 50 ms pmoves, and the one-step best angle acos(285/v) adds +12.3 a think at
  350 u/s where the two-step best (39.6 degrees) adds +23.6: `IGX_StrafeAngle` uses the two-step best
  (a table by speed, made once) and, for a skill s < 1, the wider angle that adds s times it. The skill
  is characteristic 50 (`CHARACTERISTIC_IG_STRAFE`, read with `Characteristic_IGFloat`, which is silent
  when it is unset) or 0.45 + 0.45 x (jumper + attack skill)/2; libvar `ig_planstrafeskill` 0..1 sets it
  for all (testing). Measured, planner strafing at 330-380 u/s: skill 1.0 +22 a think, 0.65 +15, 0.45 +10
  (targets 23.6 / 15.3 / 10.6). Stock bots at bot skill 3: 0.56 (Ranger) .. 0.84 (Xaero); Billy ~0.65.
  28 maps vs the build before: over 400 u/s 20.1 -> 21.8%, the rest level.
- **Next:** fight movement that values position; the pad tricks as options; richer options (turn and
  stop hooks, side-wall stair hooks); then tuning from Billy's recordings and the verification pass.

## IG-41: `ig_chasespeed` -- a speedometer for chase cameras (off)

- **Requirement:** `ig_chasespeed 1`: a spectator chasing a player sees that player's horizontal speed
  as small text under the "Chasing" line (`612 ups`; OSP: xv 44 yb -51, others xv 0 yb -60). It rides
  on the chase layout (UpdateChaseCam, g_chase.c), which is then sent every frame instead of every 32
  frames -- about 60 bytes a frame per spectator, and not while a scoreboard or menu holds the layout.
  A statusbar stat would have been the big HUD digits. Code: src/ig/ig_hud.c.
- **Why:** Billy (2026-11-25): "an on screen speedometer for chase modes ... out of the way, maybe near
  the chasing text" -- to watch bots' (and players') speed.
- **Tested:** on 27913 by Billy.

## IG-42: `ig_botpadbounce` -- off a pad that bounces a bot straight up (off)

- **Requirement:** `ig_botpadbounce 1` (libvar `ig_padbounce`): in BotMoveToGoal's airborne branch, when
  no reachability is being finished (stock: no input at all), a bot that has been in the air 1 s with
  under 50 u/s across steers along its route from the area it is in (or straight at its goal when that
  area has no route). Logs `ig: padbounce c area a reach r at x y z` once per bounce run.
  Code: botlib/be_ai_move.c IG_AirNoRoute.
- **Why:** Billy (2026-11-25, 27913, peace mode): "bunch of bots stuck on a jump pad". The q2rdm11 pad at
  (-880, 973, -32) throws a bot straight up; one that drops onto it from the upper level has no pad
  reachability (its area 1250 has no route), lands back on the pad every launch and never touches the
  ground, so it never picks a route: frozen for good. Others then stood on it (three stacked at zero
  speed). The stuck watchdog (IG-30) ignores bots giving no input, so it never fired.
- **Measured:** q2rdm11, 6 bots, 4 seeds, peace off and on, `ig_moverec 2`: bots frozen 2+ s with no
  input 1+3 -> 0+0; the fix fired twice in 8 matches, both on that pad. Frags, speed, hooks, upper time
  unchanged (all inside the noise).
- **Tested:** on 27913.

- **Fight hooks halved (2026-11-26, IG-40 tuning).** Billy on 27913: bots grapple the walls too much, "its being
  overused". His hooks (919 recorded): short taps, anchor 327 units away and 28 below; 34% anchored 64+ above him.
  Bots' (860): anchor 557 away, 81 above; fight hooks (incl. repositions) were 35% of them, 80% high on walls.
  New libvars (defaults = old constants): `ig_fighthookchance` 0.5, `ig_fighthookidle` 0.03, `ig_fighthookcooldown`
  2.5, `ig_repokill` 0.4, `ig_repomisses` 2, `ig_repomisschance` 1. 6 maps x 3 seeds, fights: 0.25 / 4 s / 0.2 /
  0.5 cuts fight hooks 4.88 -> 3.01 a bot-minute, all hooks 12.8 -> 11.1, frags, accuracy, speed level (on 27913);
  a quarter (0.1 / 6 s / 0.1 / 0.25) 1.75 a minute but -13 u/s.

- **Fight style tried, dropped (2026-11-26).** Billy (27913 logs with bots recorded, 377 shots) vs bots, 2 s around a
  shot: speed 283 vs 286, airborne 33 vs 52%, back key 8 vs 28%, strafe flips 1.1 vs 2.3 a second -- but on the
  ground only the flips are level (most bot flips are the air strafe swapping sides per hop), and by body motion
  near an enemy he and they move alike (moving frames: toward 38 vs 41%, across 38 vs 35%, away 24 vs 24%; range
  ~500 vs ~450). 97% of bot shots come from the Battle_Fight node. A replacement for BotAttackMove (strafes held
  0.6-1.3 s, a stance range kept by circling, jumps only when moving) left those figures level and cost frags
  4.11 -> 3.60 and rail accuracy 23.9 -> 20.4% (6 maps x 2): removed (source kept out of tree). Remaining real
  gaps: airborne when shooting (35 vs 53%), view on the travel direction (58 vs 49% of moving frames).

- **Pad flights: view and air control (2026-11-26, libvars `ig_planpadview` default 1 and `ig_planpadair` default 0,
  with the planner).** Billy on 27913, q2rdm11's twin pads in the big room: "they look like they're spinning ...
  they should be facing the direction they want to land unless they are trying to do a trick or shoot someone",
  and "not bouncing to the optimal spot". In a pad flight the planner did not steer, so the view fell to the roam
  clamp, which follows the stock pad steering's push as it swings; and that steering (BotAirControl) pushes at
  full strength at a landing predicted without the push, so it overcorrects (one bot drifted 240 units east into
  the side wall and fell to the floor). Now: the last 128 units onto the pad (`ig_planpadviewnear`) and the
  flight look level at the pad's landing (IG_PlanPadView; an enemy, a hook's flick or the planner's own steering
  first); in the flight, unless the pad top hooks, the push is the one of 17 (none, 16 directions) whose
  simulated flight lands nearest the landing (IG_PlanPadAir; below it priced x3, lava out). q2rdm11, peace, 5
  bots, 16 seeds, off vs on: twin-pad flights landing up on the ledge 56 -> 76% (24/43 vs 19/25); view vs
  travel 49 -> 26 deg (flights with no hook: 49 -> 10 deg). Still: the view turns ~150 deg/s on hook-free
  flights (the turn at the launch). On 27913.
  **Air control dropped (2026-11-26):** Billy, with both on: "still seeing bots miss the landing spot when they
  previously didnt"; with the view only (`ig_planpadair 0`): "jump pads look a lot better". The bench had shown no
  loss (hook-free flights landing within 160 units: old 84%, both 88%, view only 86%, 12 seeds), so the miss is
  one the bench does not count. IG_PlanPadAir stays in the code, default 0; the stock steering is back.
- **Pad flights everywhere (2026-11-27, libvar `ig_planpadfight` 1):** Billy: "bots are hitting the jump pads
  but steering away from the intended landing spot causing them to miss them more than make them". The drill (a
  clean run-up, no fight) landed q2rdm11's twin pads 100%; in fights only 56% of hook-free flights made the
  ledge, the air steering 114 deg off the landing on the misses. Two causes: the fight node runs no travel, so the
  stock attack dodge steered the flight; and a bot that runs over a pad on its way elsewhere steered by its own
  route's air control toward its goal. Now every pad flight not on a pad route, in a fight or not, is steered by
  the pad's own air control (BotAirControl) to the landing of the pad under it (IG_PadLaunchEnd: its jump-pad
  route), the aim and shots go on, and no fight hook starts mid-flight. 16 fight matches: hook-free flights making
  the ledge 56 -> 78%, steering off the landing on the made ones 70 -> 32 deg; 28 maps x 4: all level.
- **Look-ahead fix (2026-11-26):** IGX_LookAhead filled its route points with `VectorCopy(r.start, pts[n++])`;
  VectorCopy is a macro that evaluates its target three times, so n moved 3 per copy, x/y/z landed in different
  points and the writes ran past pts[16]. FreeBSD's stack protector aborted the server the first time a bot planned
  (Billy's own build); on Linux the look-ahead silently read bad points. Fixed (separate n++); no other Vector* call
  in game_q3/, botlib/ or the game side has a side effect in an argument. 28 maps x 3 seeds, trials on: time over
  400 u/s 19.8 -> 21.0% (significant), units/s 325.6 -> 328.0, frags, suicides, hooks level.

## IG-46: `ig_botgapjumps` -- routes that jump gaps (off); `ig_botroutemomentum` (WIP, off)

- **Requirement:** `ig_botgapjumps 1` (libvar `ig_gapjumps`, read at map load; IG_GapJumpsAtLoad in
  botlib/be_ig_jump.c, after IG-43's pad routes). (1) **Reprice** (`ig_gapjumpreprice`, default 1):
  every bspc TRAVEL_JUMP that is level (within 48 units of height, 32+ across) over a floor -- not
  lava, slime or nothing -- and whose flight (AAS box, 300-400 u/s, the jump velocity) lands in its
  area costs flight + `ig_gapjumpprice` 0.3 s when that is cheaper than bspc's price. (2) **Add**:
  from each grounded area's centre, 16 directions to the floor's edge, a floor at the same height
  40-`ig_gapjumpmax` 260 units on, same cluster, no route between the two areas yet, the same floor
  test and flight: a TRAVEL_JUMP route, the narrowest per pair of areas, appended as IG-43's are (no
  route number changes). Debug: `ig_gapjumpdebug 1` lists both; 2/3 with `ig_gapjumpwatchx/y` print
  every rejected edge and area near a point. `ig_botroutemomentum` (be_ig_route.c, x0.6 the walking
  time of a straight crossing up to 20 degrees of turn, unchanged from 70) is built but not measured.
- **Why:** Billy (2026-11-25): bots should find fast lines like his q2rdm11 pad-and-walkway route
  themselves ("that sort of optimization is what I would love to see done automatically"). The route
  from the pad's landing went down into the pit and up the teleporter because bspc priced the
  walkway's two gaps (x 1072-1232, y 1091 and y 408-432) at 7.28 s and 37.28 s; the only crossings
  bots made were improvised hooks and fast strafe hops (Billy 2026-11-26).
- **Measured:** q2rdm11: 19 jumps repriced (both walkway gaps 7.28 s -> 1.0 s), 7 added; route from
  the pad landing (area 141) 9.47 -> 3.28 s. Pad-to-walkway drill (`ig_paddrill`, start 1418 738 96,
  pad reach 4060, goal -963 1087 280, one bot, peace, 6 seeds): off 4/144 finished (the rest timed
  out), on 165/165, median 10.3 s from the drill start (Billy's line 5-6 s: the next gap). q2rdm11 5
  bots x 12 seeds fighting: all level (upper +1.2 pts, in the noise). 28 maps x 4 seeds: everything
  level, no map changed significantly. Added per map: 0-21; repriced: 1-60. On 27913 from 2026-11-26.

## IG-48: `ig_botplanclimb` -- climbs as planner moves (trial)

- **Requirement:** `ig_botplanclimb 1` with `ig_botplan` and the bots' hook (libvar `ig_planclimb`; not with
  LMCTF's hook unless `ig_planclimblmctf`). IGX_FindClimb: a TRAVEL_GRAPPLEHOOK route within `ig_planclimbdist`
  600 units along the route is the planner's -- its anchors (bspc's, and the climb table's frame and landing floor,
  IG-36: IG_ClimbAnchorList) become slingshot targets (class `climb`, priced `ig_planclimbmul` 0.2), simulated from
  where the bot is, moving, with the hook's own flight time (a slow hook fires earlier) and pull; the climb step is
  not a stop for the sim; while hooked, replanning decides the let-go. The stock grapple travel stands down while the
  planner notes the climb (IG_PlanClimbOwns); after `ig_planclimbgive` 3 s on one climb without firing, it is the
  stock's again.
- **Why:** Billy (2026-11-27): bots "completely stop moving and grapple and pull to move when a seasoned player
  would be moving in the direction that they want to grapple"; with a slow hook he prefires. The stock travel walks
  to the route's start, stops and aims (71% of climbs from a standstill); loosening it lost climbs (IG-32 note).
- **Measured:** q2rdm11, q2dm1, q2rdm4, q2dm1pro x 4 fighting, OSP: climbs 516 -> 730 (649 the planner's), fired
  from a standstill 71 -> 15%, speed at the fire 0 -> 285 u/s, landing up top 54 -> 53% (~280 -> ~390 climbs).
  28 maps x 4: all level, slingshots/bot/min +0.9 (the climbs). LMCTF's hook, 28 maps: hooks that climbed 38.9 ->
  36.2% (significant), so off with it for now.

## IG-47: `ig_botfastjumps` -- momentum jumps and the lip slingshot (trial)

- **Requirement:** `ig_botfastjumps 1` with `ig_botnav` (libvar `ig_fastjumps`, read at map load;
  botlib/be_ig_jump.c, the planner in game_q3/ai_ig_plan.c). At load the gap scan also takes gaps a run
  cannot clear but speed can: up to `ig_fastjumpmax` 420 across and `ig_fastjumpdrop` 160 down, take-off
  points along long areas, the slowest take-off speed that lands found by simulation (up to
  `ig_fastjumpneed` 450), priced at the flight at 500 u/s + `ig_fastjumpprice` 0.1 s; and the same jump
  from the area the straight run-up starts in (bspc joins thin floor strips through their middles). These
  are TRAVEL_STRAFEJUMP routes, offered to a bot moving `ig_fastjumpon` 500 u/s (kept down to
  `ig_fastjumpoff` 380); the planner plans with them from `ig_fastjumpplan` 250 u/s, aims at the landing
  and sees it from `ig_planlookjump` 1200 units, and its simulation proves the jump or rules it out. The
  take-off keeps the speed (no run-up back-off). Before a momentum jump, Billy's lip slingshot
  (BOT-TRICKS T27, `ig_fastjumplip`): steep floor anchors (20-32 down) and low ceiling anchors (10-18 up)
  short of the edge, and a hook held past the edge costs `ig_fastjumphookacross` 1 s per 50 ms.
  Debug: `ig_fastjumplog`, `ig_gapjumpdebug`, `ig_plandebug 2` (igfj, iglook), `ig_routedebugfast`.
- **Why:** Billy (2026-11-26): bots should "use its momentum to jump over the gap" (q2rdm11's walkway end)
  and find such spots on every map; then (2026-11-27) a slingshot should make the momentum, not a pull
  straight across to the far wall ("predictable"); his recording
  `work/hook-live/billy-ztn2dm3-lip-slings-2026-11-27.log`.
- **Measured:** found on 27 of 28 maps (0-74). q2rdm11 pad-line drill: walkway-end gap jumped 46 -> 86%
  of tries. ztn2dm3 spawn drill (the ledge by the mega health): planning only from 500 u/s the slingshot set
  a heading into the column (91% hit it); planning from 250: 0% hits, 89% clean; with the lip slingshot:
  lip slingshots 0.2 -> 0.88/try, far-wall pulls 1.5 -> 0.24/try, 77% clean, median 1.4 s. 28 maps x 4
  fighting: level, no map significant; taken 0.02/bot-min in fights (bots seldom meet one fast) -- the next
  step is speed-aware routing (measured link times). No grapple (28 maps): no change.

## IG-44: `ig_botroamgoal` -- a fair random roam goal (off)

- **Requirement:** `ig_botroamgoal 1` (libvar `ig_roamgoal`). With no item to fetch (instagib:
  always) and no hunt trace (IG-7), BotChooseLTGItem falls back to AAS_RandomGoalArea; with the
  switch on, a one-line hook there calls IG_RandomGoalArea (botlib/be_ig_roamgoal.c) instead. Once
  per map it lists the floor areas -- reachable, ground face over 300, the trace down from the centre
  lands in the area, no water, lava, slime or do-not-enter -- each weighted by its floor size, so a
  goal is a uniform spot on the map's floor. Each call picks among those reachable from the bot,
  at least 256 units away, with a travel time (8 tries). AAS_MarkReachableAreas lost its `static`
  for it. Debug: libvar `ig_roamgoaldebug` prints the list size at build.
- **Why:** Billy, 2026-11-26, q2dm1/q2dm1pro: "we end up with everyone in that room or in the water
  too often". Stock picks a random area index and takes the first acceptable area from there,
  accepting any swimming area outright while a floor area must pass the ground check -- so the pool
  is a goal far more often than its size, and an area just after a long run of rejected ones is
  favoured. The same water time with stock bots (planner off, or no IG switches at all) showed it was
  the goal, not the movement.
- **Measured:** q2dm1 + q2dm1pro, 4 bots, fights on, 6 seeds, off vs on: bot time in the pool room
  (x -100..720, y -140..330) 7.4 -> 4.0% and 10.7 -> 7.9%; time in water 5.1 -> 2.5% and 7.0 -> 4.5%;
  water entries per bot-minute 0.55 -> 0.36 and 0.84 -> 0.61. Frags, accuracy, hooks, speed and
  upper-level time within noise. 28 maps x 2 seeds, 5 bots, off vs on: every pooled figure level (frags
  4.48 vs 4.43, 338 vs 337 u/s, slingshots 5.49 vs 5.55, crowded 31.8 vs 31.1%); per-map flags with 2
  seeds go both ways (semper06 frags -0.8, alkdm15 +0.4, fragile crowded -17 pts), read as noise.
- **Tested:** on 27913 (Billy, from 2026-11-26).

## IG-43: `ig_botpadreach` -- routes through straight-up jump pads (off)

- **Requirement:** `ig_botpadreach 1` (libvar `ig_padreach`; only with the bots' hook, read at map
  load). For each trigger_push that throws (nearly) straight up -- under 200 u/s across, 300+ up --
  whose pad area has no pad route out, IG_PadReachAtLoad (botlib/be_ig_hook.c, after the links and
  settings are set up, before the routing) takes the top of its arc and, from it, the floors in
  sight 150-900 units across, 350 below to 32 above the top, no lava/slime/pad, in the pad's routing
  cluster (the routing ignores a route out of its cluster); the farthest in each of 8 directions
  becomes a TRAVEL_JUMPPAD route (time: up + 0.3 s hook flight + distance/800 + `ig_padreachprice`
  0.2 s). The pad area's block of routes moves to the end of the table, so no other route number
  changes (the climb tables and recorded routes name routes by number); IG-6's per-route marks grow
  with it. In the flight the planner's pad top (IG-40, `ig_planpadsling`) hooks toward the landing
  or along the route past it; anchors toward the landing were added because the drill's goal is the
  landing itself. Log: `ig: padreach area A -> B end x y z time t top z`.
- **Why:** Billy's pad trick (recorded 2026-11-25, IG-40 "Pad top"): q2rdm11's up-pads lead nowhere in
  bspc's routes (air control alone cannot leave them), so bots never took them (0 launches in 6
  matches) and one dropped onto them bounced forever (IG-42).
- **Measured:** q2rdm11: 15 routes from 2 pads (1600 768: 8; the one at -832 960 already had a pad
  route; 1152 192: 7); tltf2: 1 pad, every floor in another cluster, none. Pad drill (`ig_paddrill 1
  ig_paddrillforce 1`, one bot, peace; testing aid: placed 48 units short of each new route in turn
  with its landing as goal, "ok" standing on that floor within 6 s): the near floors (6 routes) are
  reached by riding alone either way; the far ones, Billy's walkway among them: ride only 0/54,
  with the pad top 54/66 (route 4053 18/18, 4059 16/16, 4060 15/16, the twin's 4063 5/16). In play
  bots seldom pass those pads (0.14% of their time within 300 units) and the hook climbs beside
  them are as cheap, so the routes are rare -- but 28 maps x 2, all on vs off: everything level;
  q2rdm11 358.7 vs 348.8 u/s and upper level 54.8 vs 50.0% (both intervals exclude 0), frags 4.20 vs
  4.42 (noise).
- **Tested:** on 27913.
