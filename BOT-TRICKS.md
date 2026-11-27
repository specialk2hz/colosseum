# Bot tricks -- what makes a bot look like a seasoned player

The movement planner (branch `planner`, 2026-11-24) replaces the trick-by-trick control code in
`ai_ig_sling.c` and the climb-landing code. **This list must not be lost in that rework.** Each trick
gets checked against it before the planner ships; a trick is either (a) an option the planner
offers, (b) a result its scoring produces on its own, or (c) a deliberate drop, written down here.

Rules from Billy's brief (memory `bot-movement-brief`):
- **Seasoned, not scripted:** the choice follows the goal and is consistent; the *execution* varies
  (anchor, timing, side). Never the same move every time at the same spot.
- **Instagib only.** Every planner behaviour sits behind a switch, on only in instagib's groups.
- A few clear personality traits (hook affinity first), not many muddy ones.

Billy's own numbers (recordings in work/billy2, billy3, route8; analysers in work/):
417 u/s average (bots ~320), airborne 48-55% of the time, in the air holds forward 81-88% and strafe
79-87%, 18 hooks/min. His hooks by purpose: gain speed 31%, climb 28%, turn 16%, keep speed 9%,
slow/stop 8%, miss 6%, long pull 1%.

Status: **built** = works in today's code (IG number) · **trial** = on 27913 only · **todo** = wanted,
not built. "Planner" says how the planner must express it; "Verify" is the check that proves it survived.

## Hook movement

| # | Trick | Source | Status | How it works today | Planner | Verify |
|---|---|---|---|---|---|---|
| T1 | **Slingshot:** jump, hook the floor or a far wall ahead, let go early, keep the momentum | Billy: "include a jump to really boost" | built IG-9 | anchors 6-18 deg down, >= 250 units (mean 329; Billy 275), ~0.2 s pull, let go when fast | option: slingshot to each of a few anchors along the route; score speed kept | slingshots/bot/min >= 9; anchor distance near Billy's 275-315; not "far too close" |
| T2 | **Chains:** slingshot, land or not, slingshot again | Billy | built IG-9 | next hook from the air | falls out: replanning each think while airborne offers another hook | chains of 3+ >= 16% |
| T3 | **Pull hop:** hop off the floor while a floor hook pulls, so friction doesn't eat it | Billy's recording: floor 20% of the pull vs bots 41% | built IG-17 | jump while pulled and grounded | in the hook sim: a grounded frame with jump held | let-go speed near Billy's ~750 u/s; on-floor share of pull ~20% |
| T4 | **Hook into a jump pad**, let go just before it so the pad launches you | Billy | built IG-13 | pad 200-700 ahead, flick onto the pad floor, let go 90 short | option: hook toward the pad, let go at the frame the sim says carries onto it | pad hooks seen on q2rdm11 big room; pad flight not slower |
| T5 | **End-of-arc hook:** near the end of a pad flight, hook the floor just past the landing | Billy: "slight advantage in speed... mostly to be unpredictable" | built IG-13 | from above, within 450 of the landing | option during pad flight; chosen sometimes (variety), not always | seen some of the time, never "below or before the landing" |
| T6 | **Stairwell hook:** hook the side wall going up stairs to slingshot up | Billy | built IG-20 | route rises 48-320 in 500: upward hook 12-36 deg, up to 35 deg to a side; chance per climb | option: upward anchors on side walls when the route climbs | stair hooks on q2dm1/q2rdm11; not every time |
| T7 | **Hook climbs** to a ledge: hook the frame's front face, look ahead, let go over the landing, air-strafe to its middle | Billy ("looks great") | trial IG-6/32/36 | bspc grapple routes + climb aim + per-map climb tables | option: climb; let-go and landing come from the sim, not tables | drill: landed >= 91%, centred >= 59%; lava-room door frame, not its corner |
| T8 | **Overhead anchor:** hook ceiling/sky above the landing and hold to stall over it | drill finding | trial IG-36 | hold until stall | option: hold vs let go each think | railgun overlook route lands (29/37 today) |
| T9 | **Climbs keep momentum:** come out of a climb moving, not stopped at A then B | Billy complaint (1b) | todo | -- | scoring: speed at landing counts | climb exit speed vs Billy's |
| T10 | **Mid-air hooks:** drop off a ledge and hook to another spot | Billy | partly (chains hook from the air) | -- | falls out: options are offered airborne too | hooks started airborne share |
| T11 | **Hook up hills:** shallow pulls up slopes | Billy | todo | Q3 routes exclude < 15 deg | option: shallow upward anchors | -- |
| T12 | **Double grapple:** hook, release, hook again to reach what one pull can't | Billy | partly (chains) | -- | planner looks two hooks deep where one fails | reaches a spot no single hook reaches |
| T13 | **Hook to turn** (16% of Billy's hooks) and **hook to slow/stop** (8%) | Billy's hook labels | todo | -- | options scored for direction change / stopping on a spot | hook-purpose mix closer to Billy's |
| T14 | **Lava escape:** in lava/slime, hook nearly straight up to get out | measured | built IG-9 | 85-65 deg up, sky allowed | option kept as is (emergency) | escapes still end over floor (66/67 today) |
| T27 | **Lip slingshot:** a super quick, short hook to the floor at a ledge's lip (or the ceiling above it) for speed, then the momentum jump off it | Billy's recording 2026-11-27 (ztn2dm3, `work/hook-live/billy-ztn2dm3-lip-slings-2026-11-27.log`): in the air at 110-290 u/s, floor 120-200 out 20-35 deg down / ceiling 180-270 out 10-16 deg up, held 1-2 frames, let go short of the edge at 670-980 u/s; Billy: "super useful for quick speed and will come in handy" for CTF flag runners | WIP IG-47 (only with a momentum jump ahead) | lip anchors 20-32 deg down and 10-18 up, 100+ units, short of the edge; a hook held past the edge priced 1 s per 50 ms | option: lip anchors; let-go from the sim | ztn2dm3 drill: lip floor slingshots 0.2 -> 0.88/try, far-wall pulls 1.5 -> 0.24/try, clean jumps 77%; later: any quick-speed spot, CTF flag runs |

## Running and air control

| # | Trick | Source | Status | How it works today | Planner | Verify |
|---|---|---|---|---|---|---|
| T15 | **Strafe-jump chains:** wish 45-55 deg off velocity, swap sides near the top, carve through bends, jump just before landing | Billy's recording route8 | trial IG-31 | brain strafe 2 + jump on landing frame | option: hop; air steering simulated, not assumed straight | 2+ hop chains >= 20%; > 400 u/s not lower |
| T16 | **Know when not to hop:** stop before sharp turns, doors, lava | Billy complaint (1d) | trial IG-38 | runway 400, lookahead | falls out: a hop whose sim ends in lava/wall scores badly | q2rdm11 lava trips well under 0.335/bot-min; no lava-room stair overshoots |
| T17 | **Strafe during a slingshot** to keep the momentum | Billy | todo | -- | air steering in the post-let-go sim | air speed after let-go decays slower |
| T18 | **Land like a Q2PRO client** (no post-landing jump lock) | parity | built IG-14 | game side | unchanged | -- |
| T19 | **Rail jump:** shoot the floor for a boost | Billy (player feature IG-25) | todo for bots | -- | option once bots can rail jump; needs splash on | -- |

## Fighting and positioning

| # | Trick | Source | Status | How it works today | Planner | Verify |
|---|---|---|---|---|---|---|
| T20 | **Fight hook:** hook mid-fight for a better angle and to be harder to hit | Billy | built IG-10 | enemy 250-1800 away, within 55 deg of view | option during fights; score safety + line of fire | fight hooks/bot/min ~2.8; rail acc not lower |
| T21 | **Reposition:** after a kill (sometimes) or two misses, grapple away | Billy | built IG-10 | 40% after a kill, after 2 misses | option with a "get away" score | ~3.4 repositions/bot/min |
| T22 | **Hot spots / don't stand in bad spots** | Billy brief | todo (IG-7 hunts only) | -- | goal layer, later | -- |
| T23 | **Quick-peek a corner** | Billy brief | todo | -- | later | -- |
| T24 | **Fake someone out** | Billy brief | todo | -- | later | -- |
| T25 | **Jiggle strafe** in a fight | Billy brief | todo | -- | later | -- |
| T26 | **Break a fall with a ground hook** | Billy | dropped for instagib (no fall damage) | -- | -- | -- |

## Personality and variety

| # | Trait | Status | Planner |
|---|---|---|---|
| P1 | **Hook affinity:** runners vs grapple masters (character trait 49) | built IG-21/28 | a weight on hook options |
| P2 | **Variety in execution:** among near-best options pick by weighted chance, not the single best | todo | core of option choice |
| P3 | **Risk taking:** how close to hazards a bot will fly | todo | a weight on the hazard score |
| P5 | **Strafe skill:** share of the most speed a strafe adds (0.45 casual .. 0.9 near perfect; Billy ~0.65) | built (IG-40, characteristic 50) | the strafe angle |
| P4 | Crouching, jumping habits | stock Q3 characteristics | unchanged; comes with friend bots |

## Verification pass (before the planner ships)

1. Each built/trial row above: run its Verify check, planner vs the current build (igbench, same seeds,
   q2rdm11 + all 28 maps; the climb drill for T7/T8).
2. Billy plays 27913 with this list in hand: each trick seen, missing, or looking wrong.
3. Any row the planner drops is marked "dropped" here, with why.

## Planner v1 (2026-11-24): where each trick stands

| # | In planner v1 | Note |
|---|---|---|
| T1 slingshot | option | 9 slingshot anchors along the route (6-18 down, 250+ units), priced cheapest; 77% of the planner's hooks (Billy: more slingshots, fewer random wall grapples) |
| T2 chains | falls out | slingshot options are offered in the air too |
| T3 pull hop | option | 'holdhop' while hooked on the floor; the simulation rarely finds it pays (the pull resets the velocity every 50 ms) |
| T4, T5 pad hooks | old code kept | IG-13 still runs before the planner |
| T6 stairwell hook | option | 25-degree-up anchors, offered only where the route climbs; side-wall variant not yet |
| T7, T8 climbs | old code kept | climb routes are the navigation's; the planner steps back |
| T9 climbs keep momentum | todo | |
| T10 mid-air hooks | falls out | |
| T11-T13 hills, double grapple, turn/stop hooks | partly | turn/stop come out of the route cost when they pay; not checked |
| T14 lava escape | old code kept | |
| T15 strafe chains | option | the hop option, with the game's landing jump |
| T16 knowing when not to hop | falls out | lava/pit options are out; walls cost |
| T17 strafe after a slingshot | falls out | the air wish after a let-go |
| T20, T21 fight hook, reposition | old code, checked | the simulation vetoes fires and let-gos into lava/pits |
| fight dodges | old code, guarded | turned away from lava and edges |
| P1 affinity | weight | hook price and pick weight |
| P2 variety | yes | a pick among options within 0.06 s of the best |

