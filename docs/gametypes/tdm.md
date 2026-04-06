# Team Deathmatch (GT_TEAM = 3)

## Overview

Team Deathmatch is a team-based fraglimit/timelimit mode. Players are divided into Red and Blue teams. Kills earn points for the team, and the team that reaches the fraglimit first or has the higher score at timelimit wins. TDM supports team balancing, mercy rules, and team size limits.

## Source Files

- **Primary:** `code/game/g_gametype_tdm.c` -- scoreboard functions
- **Exit rules:** `code/game/g_main.c` -- `CheckExitRules()` (fraglimit, timelimit, mercylimit, overtime)
- **Scoring:** `code/game/g_combat.c` -- `AddScore()`, `player_die()`
- **Team balance:** `code/game/g_main.c` -- `G_CheckTeamBalance()`

### Binary Addresses (qagamex86.dll)

| Function | Address |
|---|---|
| TDMScoreboardMessage | 0x1003df10 |
| TDMScoreboardMessage_impl | 0x1003e3a0 |

## Cvars

| Cvar | Default | Description |
|---|---|---|
| `fraglimit` | 20 | Match ends when a team reaches this score. |
| `timelimit` | 0 | Match time limit in minutes. 0 = no time limit. |
| `g_overtime` | 0 | Overtime period in seconds when score is tied at timelimit. 0 = infinite sudden death. |
| `mercylimit` | 0 | Score difference that triggers a mercy win. 0 = disabled. Only checked after `g_mercytime` has elapsed. |
| `g_mercytime` | 0 | Minutes into the match before mercylimit can take effect. |
| `teamsize` | 0 | Maximum players per team. 0 = unlimited. |
| `g_teamSizeMin` | 0 | Minimum players per team to start. |
| `g_teamForceBalance` | 0 | Force teams to stay balanced (prevent joining the larger team). |
| `g_doWarmup` | 1 | Enable warmup phase. |
| `sv_warmupReadyPercentage` | 0.51 | Fraction of players that must `/ready` to start the countdown. |
| `g_warmup` | 20 | Warmup countdown duration in seconds. |
| `g_startingHealth` | 100 | Base spawn health. |
| `g_startingHealthBonus` | 25 | Bonus health added on spawn. |
| `g_startingArmor` | 0 | Armor given on spawn. |
| `g_startingWeapons` | 3 | Bitmask of weapons given on spawn. |
| `g_loadout` | 0 | Enable loadout weapon selection. |
| `g_forfeit` | 0 | Enable forfeit when a team has no players. |
| `g_damagePlums` | 1 | Show floating damage numbers. |

## Scoring

- **Kill an enemy:** +1 point to the attacker and +1 to the team score.
- **Suicide / self-kill:** -1 point to the player and -1 to the team score.
- **Team kill:** -1 point to the attacker (if friendly fire is enabled).
- **Win condition:** First team to reach `fraglimit`, highest team score at `timelimit`, or mercy rule.
- **Mercy rule:** After `g_mercytime` minutes have elapsed, if one team leads by `mercylimit` or more points, that team wins immediately. Only applies to team gametypes (GT_TEAM and above, excluding GT_RR).
- **Tied at timelimit:** Same overtime/sudden death rules as FFA.

## Spawn Behavior

- Players spawn at `info_player_deathmatch` or team-specific spawn points.
- Spawn health/armor/weapons follow the same system as FFA.
- Team assignment can be manual (player choice) or forced by `g_teamForceBalance`.
- `teamsize` limits the maximum players per team.

## Key Functions

| Function | File | Description |
|---|---|---|
| `TDMScoreboardMessage` | g_gametype_tdm.c | Sends `scores_tdm` command. Header contains 28 team item stats (14 categories x 2 teams) + 3 metadata values (numPlayers, redScore, blueScore). Per player: 15 fields. Opponent team item stats are hidden unless the viewer is a spectator or it is intermission. |
| `TDMScoreboardMessage_impl` | g_gametype_tdm.c | Sends `tdmstats` command per player with 11 detailed stats fields: suicides, teamKills, teamKilled, damageDone, damageTaken, plus pickup counts for red/yellow/green armor, mega health, quad, and battlesuit. |
| `CheckExitRules` | g_main.c | Checks team scores against fraglimit, timelimit, and mercylimit. |
| `AddScore` | g_combat.c | Adds score to player and team totals. Blocked during warmup. |
| `G_CheckTeamBalance` | g_main.c | Validates team sizes for warmup readiness. |

## Scoreboard Protocol

### scores_tdm Format

The `scores_tdm` command has a 31-value header followed by per-player data.

**Header (28 team item stats + 3 metadata):**

| Index | Red Team Stat | Blue Team Stat |
|---|---|---|
| 0-1 | Red Armor pickups | Red Armor pickups |
| 2-3 | Yellow Armor pickups | Yellow Armor pickups |
| 4-5 | Green Armor pickups | Green Armor pickups |
| 6-7 | Mega Health pickups | Mega Health pickups |
| 8-9 | Quad Damage pickups | Quad Damage pickups |
| 10-11 | Battle Suit pickups | Battle Suit pickups |
| 12-13 | Regeneration pickups | Regeneration pickups |
| 14-15 | Haste pickups | Haste pickups |
| 16-17 | Invisibility pickups | Invisibility pickups |
| 18-19 | Quad possession time | Quad possession time |
| 20-21 | Battle Suit possession time | Battle Suit possession time |
| 22-23 | Regeneration possession time | Regeneration possession time |
| 24-25 | Haste possession time | Haste possession time |
| 26-27 | Invisibility possession time | Invisibility possession time |

Followed by: numPlayers, teamScoreRed, teamScoreBlue.

**Visibility rule:** When a player on Red views the scoreboard, all Blue team item stats are zeroed out (and vice versa). Spectators and intermission views show both teams' full stats.

**Per-player fields (15):**
client, team, score, ping, time, frags, deaths, accuracy, bestWeapon, impressive, excellent, gauntlet, teamKills, teamKilled, damageDone

### tdmstats Format

Sent per-player as individual `tdmstats` commands. 11 fields per player:
suicides, teamKills, teamKilled, damageDone, damageTaken, redArmorPickups, yellowArmorPickups, greenArmorPickups, megaHealthPickups, quadPickups, battleSuitPickups

## Team Item Tracking

TDM tracks 14 categories of team-level item stats stored in `level_locals_t`:

**Pickup counts (9 categories):**
- Red Armor, Yellow Armor, Green Armor, Mega Health
- Quad Damage, Battle Suit, Regeneration, Haste, Invisibility

**Possession time (5 categories):**
- Quad Damage, Battle Suit, Regeneration, Haste, Invisibility

Each category is tracked per-team using `[TEAM_RED]` and `[TEAM_BLUE]` array indices. These are displayed in the scoreboard header and form the basis of the team comparison stats shown to players during the match.

## QL vs Q3 Differences

- **Gametype index:** GT_TEAM = 3 in both Q3 and QL.
- **Scoreboard protocol:** QL uses `scores_tdm` with a 31-value header containing team item stats. Q3 used the same `scores` command as FFA/Duel with no team-level item tracking.
- **Team item stats:** QL tracks per-team pickup counts for 9 item categories and possession time for 5 powerup categories. Q3 had no equivalent.
- **Stats visibility:** QL hides the opposing team's item pickup stats during the match. Only spectators and intermission views show both teams' data. Q3 had no such restriction.
- **tdmstats command:** QL adds a separate `tdmstats` per-player detail command with 11 fields. Q3 had no equivalent.
- **Expanded per-player stats:** QL tracks teamKills, teamKilled, and damageDone per player on the scoreboard. Q3 only had basic score/ping/time.
- **Mercy rule:** QL adds `mercylimit` and `g_mercytime` for early match termination when one team has a commanding lead. Q3 had no mercy rule.
- **Team size controls:** QL adds `teamsize` (max per team) and `g_teamSizeMin` (minimum to start). Q3 only had `g_teamForceBalance`.
- **Overtime:** Same `g_overtime` system as FFA, applying to tied team scores. Not present in Q3.
- **Forfeit system:** QL adds `g_forfeit` to handle teams that lose all players mid-match. Q3 had no forfeit logic.
- **Best weapon:** QL includes `bestWeapon` per player on the scoreboard via `STAT_GetBestWeapon()`. Q3 did not show this.
