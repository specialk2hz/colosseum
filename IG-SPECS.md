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
  `weapon_initial`, `start_slugs`, `max_slugs`, `damage_railgun`, `motd_file`, all 26 `allow_*` and
  `ig_noblaster`.
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
