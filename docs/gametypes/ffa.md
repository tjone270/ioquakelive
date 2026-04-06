# Free For All (GT_FFA = 0)

## Overview

Free For All is the base gametype in Quake Live. Every player competes individually -- there are no teams. The objective is to reach the fraglimit before other players or to have the highest score when the timelimit expires. Any number of players can participate (limited by server `sv_maxclients`).

## Source Files

- **Primary:** `code/game/g_gametype_ffa.c` -- scoreboard functions
- **Exit rules:** `code/game/g_main.c` -- `CheckExitRules()` (fraglimit, timelimit, overtime)
- **Scoring:** `code/game/g_combat.c` -- `AddScore()`, `player_die()`
- **Quad Hog:** `code/game/g_items.c` -- `G_QG_Touch_Item()`, `G_QG_RespawnQuad()`, `G_QG_Think()`

### Binary Addresses (qagamex86.dll)

| Function | Address |
|---|---|
| DeathmatchScoreboardMessage | 0x1003d280 |
| FFAScoreboardMessage_impl | 0x1003cf60 |

## Cvars

| Cvar | Default | Description |
|---|---|---|
| `fraglimit` | 20 | Match ends when a player reaches this score. Applies to GT_FFA, GT_DUEL, and GT_TEAM. |
| `timelimit` | 0 | Match time limit in minutes. 0 = no time limit. |
| `g_overtime` | 0 | Overtime period in seconds when score is tied at timelimit. 0 = infinite sudden death. |
| `g_quadHog` | 0 | Enables Quad Hog mode. A single quad damage spawns randomly and transfers on kill. FFA-specific. |
| `g_quadHogTime` | 30 | Duration in seconds for the Quad Hog powerup. |
| `g_quadDamageFactor` | 3 | Quad damage multiplier. |
| `g_doWarmup` | 1 | Enable warmup phase before match starts. |
| `sv_warmupReadyPercentage` | 0.51 | Fraction of players that must `/ready` to start the countdown. |
| `g_warmup` | 20 | Warmup countdown duration in seconds. |
| `g_startingHealth` | 100 | Base spawn health. |
| `g_startingHealthBonus` | 25 | Bonus health added on spawn (total = 125 by default). |
| `g_startingArmor` | 0 | Armor given on spawn. |
| `g_startingWeapons` | 3 | Bitmask of weapons given on spawn (3 = gauntlet + machinegun). |
| `g_loadout` | 0 | Enable loadout weapon selection. |
| `g_damagePlums` | 1 | Show floating damage numbers. |

## Scoring

- **Kill:** +1 point to the attacker.
- **Suicide / self-kill:** -1 point to the player.
- **Team kill:** Not applicable (no teams).
- **Win condition:** First player to reach `fraglimit`, or highest score at `timelimit`.
- **Tied at timelimit:** If `g_overtime` is set, overtime periods are added (EV_OVERTIME event). If `g_overtime` is 0, sudden death continues indefinitely.
- **Quad Hog mode (`g_quadHog 1`):** A single quad damage item spawns at a random `info_player_deathmatch` spot. When the carrier is killed, the quad respawns at the victim's death location. The quad does not expire on a timer; it persists until the carrier dies.

## Spawn Behavior

- Players spawn at `info_player_deathmatch` entities using standard telefrag-avoiding spawn selection.
- Spawn health: `g_startingHealth` + `g_startingHealthBonus` (default 125).
- Spawn armor: `g_startingArmor` (default 0).
- Spawn weapons: controlled by `g_startingWeapons` bitmask (default: gauntlet + machinegun).
- If `g_loadout` is enabled, players select their starting weapons via the loadout UI.

## Key Functions

| Function | File | Description |
|---|---|---|
| `DeathmatchScoreboardMessage` | g_gametype_ffa.c | Sends `scores_ffa` command with 18 fields per player: client, score, ping, time, accuracy, impressive, excellent, gauntlet, defend, assist, perfect, captures, alive, frags, deaths, bestWeapon, bestWeaponAccuracy, damageDone. |
| `FFAScoreboardMessage_impl` | g_gametype_ffa.c | Sends simplified `smscores` command with 8 fields per player for match-end stats. |
| `CheckExitRules` | g_main.c | Checks fraglimit (per-player score >= fraglimit), timelimit, and overtime. |
| `AddScore` | g_combat.c | Adds score to player and team totals. Blocked during warmup. |
| `STAT_GetBestWeapon` | g_gametype_common.c | Determines the player's best weapon for scoreboard display. |
| `G_QG_Think` | g_items.c | Quad Hog frame logic: spawns quad at random spawnpoint if no carrier exists. |
| `G_QG_Touch_Item` | g_items.c | Handles picking up the Quad Hog quad item. |
| `G_QG_RespawnQuad` | g_items.c | Creates a new quad item entity at a given origin (used when carrier dies). |

## QL vs Q3 Differences

- **Gametype index:** GT_FFA remains index 0 in both Q3 and QL.
- **Scoreboard protocol:** QL uses `scores_ffa` with 18 fields per player (Q3 used fewer fields). Added fields include: alive, frags, deaths, bestWeapon, bestWeaponAccuracy, damageDone.
- **Match-end stats:** QL adds the `smscores` command for simplified end-of-match scoreboard data.
- **Expanded stats tracking:** QL tracks per-weapon accuracy, damage dealt/taken, kills, deaths via `expandedStatObj_t` -- none of this existed in Q3.
- **Quad Hog mode:** Entirely new to QL. Q3 had no equivalent persistent-quad-transfer mechanic.
- **Warmup system:** QL uses `g_doWarmup`, `sv_warmupReadyPercentage`, and a `g_gameState` cvar (PRE_GAME / COUNT_DOWN / IN_PROGRESS). Q3 had a simpler warmup with just `g_warmup`.
- **Overtime:** QL adds configurable overtime periods via `g_overtime`. Q3 had no overtime system.
- **Starting loadout:** QL adds `g_startingHealth`, `g_startingHealthBonus`, `g_startingArmor`, `g_startingWeapons`, and the `g_loadout` system. Q3 had hardcoded spawn values.
- **Damage plums:** QL adds floating damage numbers (`g_damagePlums`), not present in Q3.
- **Best weapon calculation:** `STAT_GetBestWeapon()` selects the weapon with highest kill count for scoreboard display -- new to QL.
