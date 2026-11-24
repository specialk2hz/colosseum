# IG specs -- the instagib fork

The fork's own requirements, numbered like upstream's `SPECS.md`. Each row: the requirement,
why, and how it is tested. Every switch defaults to upstream behaviour.

## Index: every IG number and where it stands (2026-11-24)

**The goal:** a Quake 2 instagib server whose bots move like a seasoned player (fast, varied, never
scripted-looking) without changing the other modes. Every change is a switch, off by default, reset in
`colosseum/server.cfg`, turned on per mode by the server repo's feature groups (`groups/*.cfg`).

Status: **live** = on 27912 (pushed, `ig-0.4-speed` + IG-22..24) · **ready** = on in its group file,
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
| 9 | `ig_botsling` | slingshots, chains | A | live | 9.1 slingshots/bot/min |
| 10 | `ig_botfighthook` | hook mid-fight, reposition | B | live | |
| 11 | `ig_botstrafe 1` | straight strafe jumping | C | parked | no gain; replaced by IG-31 (`2`) |
| 13 | `ig_botpadhook` | Billy's jump-pad hook tricks | A | live | variety, not speed |
| 14 | `ig_botstrafehack` | bots land like Q2PRO clients | C | live | no change, parity |
| 15 | `ig_moverec` | record a player's movement | -- | tool | |
| 16 | -- | (number not used) | | | |
| 17 | `ig_botpullhop` | hop off the floor while pulled | A | live | 301 -> 313 u/s |
| 18, 19 | `ig_scoremap`, `ig_votemaplist` | map on scoreboard, vote map list | M | live | |
| 20 | `ig_botstairhook` | stairwell wall hook | A | live | |
| 21 | `ig_botaffinity` | hook affinity per character | A | live | |
| 22, 23 | `ig_endvote`, `ig_menufast` | end-of-match map vote, fast menus | M | live | |
| 24 | `ig_nofallfx` | no landing dip or grunt | B | live | Billy: "feels great" |
| 25 | `ig_railsplash` | rail jumping (shooter only) | B | trial | Billy: "seems good" |
| 26 | `ig_botroutes` | replay recorded routes | -- | parked | 0/12 finished; rework around hook events |
| 27 | `ig_botpeace` | bots only move | -- | tool | |
| 28 | `ig_botaffinitychance` | affinity = slingshot chance | A | trial | spread sd 1.43 -> 1.67 |
| 29 | `ig_botwalkway` | slingshots stay on walkways near lava | C | trial | q2rdm11 lava 0.39 -> 0.23/min |
| 30 | `ig_botunstuck` | stuck bots back to a spawn point | C | trial | fixes q2dm1pro lift trap |
| 31 | `ig_botstrafe 2`, `ig_botjumpland` | strafe-jump chains | C | trial | chains 7 -> 20%, >400 u/s 16 -> 19% |
| 32 | `ig_botclimbrun 2` | let go over the ledge | A | trial | hang 0.19 -> 0.06 s |
| 33 | `ig_lavarescue` | lava/pits respot, no frag lost | B | ready | q2rdm11 suicides 0.33 -> 0 |
| 34 | `hook_type`, `ig_hookvote` | LMCTF hook style, vote row | server.cfg | ready (vote on 27913 only) | LMCTF bots 238 -> 287 u/s |
| 35 | `ig_botfuncwall` | stand on func_wall pads, no circling | B | trial | circling 437 -> 311 |
| 36 | `ig_botclimbaim`, `ig_botclimbland` | climbs land in the middle; climb tables | B | trial | landed 88.5 -> 91.3%, centred 44 -> 59%; Billy: "looks great" |
| 37 | `ig_drill` | climb drill | -- | tool | |
| 38 | `ig_botstrafeplan` | strafe chains plan ahead | B | trial | q2rdm11 lava 0.52 -> 0.335/bot-min; >400 u/s -1 pt |
| 39 | `ig_botarcsafe` | flight check before let-go / hop | B | stopped | neutral; replaced by the planner (IG-40) |
| 40 | (planner, branch `planner`) | one movement planner: simulate options, pick among the best | B | WIP | step 1 done: Q2 movement port, 0.9 units off at 0.5 s |

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
- **Next:** step 2, the hook pull (OSP and LMCTF) and jump pads in the simulation; step 3, the planner.

