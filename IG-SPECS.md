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

## IG-11: strafe jumping (experimental, off)

- **Requirement:** `ig_strafe` (brain libvar; no server cvar yet) 1: on a long straight route stretch at
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
