# Duel (GT_DUEL = 1)

## Overview

Duel is a 1v1 gametype with a spectator queue. Exactly two players fight at a time; all other connected players wait as spectators and rotate in when a slot opens. The objective is to reach the fraglimit or have the higher score at timelimit. Duel was called "Tournament" (GT_TOURNAMENT) in Quake 3.

## Source Files

- **Primary:** `code/game/g_gametype_duel.c` -- scoreboard and queue management
- **Queue logic:** `code/game/g_main.c` -- `AddTournamentPlayer()`, `AddTournamentQueue()`, `RemoveTournamentLoser()`
- **Exit rules:** `code/game/g_main.c` -- `CheckExitRules()` (fraglimit, timelimit, overtime)
- **Scoring:** `code/game/g_combat.c` -- `AddScore()`, `player_die()`

### Binary Addresses (qagamex86.dll)

| Function | Address |
|---|---|
| DuelScoreboardMessage | 0x1003d4a0 |
| DuelScoreboardMessage_impl | 0x1003d0b0 |
| CheckTournament | FUN_100557f0 |

## Cvars

| Cvar | Default | Description |
|---|---|---|
| `fraglimit` | 20 | Match ends when a player reaches this score. |
| `timelimit` | 0 | Match time limit in minutes. 0 = no time limit. |
| `g_overtime` | 0 | Overtime period in seconds when score is tied at timelimit. 0 = infinite sudden death. |
| `g_doWarmup` | 1 | Enable warmup phase before match starts. |
| `sv_warmupReadyPercentage` | 0.51 | Fraction of players that must `/ready` to start the countdown. |
| `g_warmup` | 20 | Warmup countdown duration in seconds. |
| `g_startingHealth` | 100 | Base spawn health. |
| `g_startingHealthBonus` | 25 | Bonus health added on spawn. |
| `g_startingArmor` | 0 | Armor given on spawn. |
| `g_startingWeapons` | 3 | Bitmask of weapons given on spawn. |
| `g_loadout` | 0 | Enable loadout weapon selection. |

## Scoring

- **Kill:** +1 point to the attacker.
- **Suicide / self-kill:** -1 point to the player.
- **Win condition:** First player to reach `fraglimit`, or higher score at `timelimit`.
- **Tied at timelimit:** Overtime or sudden death (same rules as FFA).

## Spawn Behavior

- Exactly 2 players are on the field at any time. Additional players are forced to spectator.
- Spawn health/armor/weapons follow the same system as FFA.
- When a duel ends (player disconnects or match concludes), the loser is moved to spectator and the next spectator in queue is promoted to play.

## Queue System

The duel queue is managed by spectator ordering:

1. **`AddTournamentQueue(client)`** -- Called when a client becomes a spectator. Sets the client's `spectatorTime` to 0 (front of queue) and increments all other spectators' `spectatorTime` by 1.
2. **`AddTournamentPlayer()`** -- Called by `CheckTournament()` when fewer than 2 players are on the field. Finds the spectator with the lowest `spectatorTime` (longest waiting) and promotes them to a playing team.
3. **`RemoveTournamentLoser()`** -- Moves the loser to spectator at the back of the queue.
4. **`CheckTournament()`** -- Runs every frame for GT_DUEL only. Counts non-spectator players; if fewer than 2, calls `AddTournamentPlayer()`. Also resets `warmupTime` and clears `allReadyTime` when pulling in a new player. Resets `specOnly` flag on playing clients.

## Key Functions

| Function | File | Description |
|---|---|---|
| `DuelScoreboardMessage` | g_gametype_duel.c | Full duel scoreboard sent as `scores_duel`. Per player: 14 base fields + per-weapon stats (weapons 1-14, 5 fields each) + item timing (4 categories, 2 fields each). Two passes per player: pass 0 = opponent view (no item timing), pass 1 = self view (with item timing). |
| `DuelScoreboardMessage_impl` | g_gametype_duel.c | Simplified `scores` command with 18 fields per player. Frags/deaths fields are hardcoded to 0 in this format. |
| `CheckTournament` | g_gametype_duel.c | Duel-only queue manager. Pulls spectators into play when a slot opens. Resets warmup state. |
| `AddTournamentPlayer` | g_main.c | Promotes the longest-waiting spectator to a playing team. |
| `AddTournamentQueue` | g_main.c | Adds a client to the spectator queue, ordering by wait time. |
| `RemoveTournamentLoser` | g_main.c | Moves the loser to spectator and re-queues them. |

## Scoreboard Protocol

The `scores_duel` command has a unique two-player format:

**Base fields per player (14):**
client, score, ping, time, frags, deaths, accuracy, bestWeapon, damageDone, impressive, excellent, gauntlet, perfect

**Per-weapon fields (weapons 1-14, 5 fields each = 70 total):**
shotsHit, shotsFired, accuracy, damageDealt, numWeaponKills

**Item timing fields (4 categories, 2 fields each = 8 total):**
redArmorPickups, redArmorAvgTime, yellowArmorPickups, yellowArmorAvgTime, greenArmorPickups, greenArmorAvgTime, megaHealthPickups, megaHealthAvgTime

Item timing averages are calculated by dividing total pickup time by the number of non-first pickups. First pickups are excluded because there is no "time between pickups" for the initial pickup.

**Visibility rules:**
- A player sees their own detailed stats (self view with item timing).
- A player sees the opponent's stats without item timing (opponent view).
- Spectators and intermission see full detail for both players.
- Player stats are archived in `level.scoreboardArchive1` / `level.scoreboardArchive2` for intermission display.

## QL vs Q3 Differences

- **Gametype index:** GT_DUEL = 1 in QL (was GT_TOURNAMENT = 1 in Q3). Name changed but index preserved.
- **Scoreboard protocol:** QL uses `scores_duel` with per-weapon stats and item timing data. Q3 used a simple `scores` command with basic stats.
- **Item timing stats:** Entirely new to QL. Tracks pickup counts and average time between pickups for red/yellow/green armor and mega health. Useful for competitive analysis.
- **Per-weapon breakdown:** QL provides full per-weapon stats (hits, shots, accuracy, damage, kills) for weapons 1-14 on the scoreboard. Q3 had only overall accuracy.
- **Self vs opponent views:** QL hides item timing data from the opponent during the match; only the player themselves (and spectators) see the full breakdown. Q3 had no such distinction.
- **Scoreboard archiving:** QL archives detailed scoreboard strings for display during intermission. Q3 generated scoreboards on demand.
- **Queue management:** The queue system (`CheckTournament`) is functionally similar to Q3 but separated from the warmup state machine. In Q3, `CheckTournament()` handled both queue and warmup logic. In QL, warmup is handled by the separate `CheckWarmup()` function.
- **Warmup separation:** QL's `CheckTournament()` only handles queue management (counting players, pulling in spectators). Warmup state transitions are in `CheckWarmup()`, which runs for all gametypes. Q3 combined both in `CheckTournament()`.
