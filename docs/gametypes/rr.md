# Red Rover (GT_RR = 12)

## Overview

Red Rover is a round-based team mode where players switch teams on death. When a player is killed, they respawn on the opposing team. A round ends when one team has no alive players. Red Rover also supports an optional **Infection mode** (`g_rrInfected`) that adds zombie mechanics, periodic forced team switches, and survival scoring.

## Source Files

- **Primary:** `code/game/g_gametype_rr.c` -- round state machine, infection logic, death handling, scoreboard (572 lines)
- **Binary addresses:**
  - `RR_InitRoundState`: `0x100656e0`
  - `RR_CheckExitRules`: `0x10064770`
  - `RR_OnPlayerDeath`: `0x10065760`
  - `ClientSpawn_RedRover`: `0x10064640`
  - `RRScoreboardMessage`: `0x1003f440`

## Cvars

### Core RR Cvars

| Cvar | Default | Flags | Description |
|---|---|---|---|
| `roundlimit` | varies | -- | Total rounds for game end (sum of red+blue wins, not per-team) |
| `roundtimelimit` | varies | -- | Per-round time limit in seconds |
| `g_roundWarmupDelay` | varies | -- | Countdown duration before each round (ms) |
| `g_timelimit` | varies | -- | Overall time limit in minutes |
| `g_spawnArmor` | `0` | -- | If nonzero, grants quad powerup timer at round start |
| `g_lastManStandingWarning` | varies | -- | Enable "last man standing" announcement when a team has 1 player left |

### Infection Mode Cvars (11 total)

| Cvar | Default | Flags | Description |
|---|---|---|---|
| `g_rrInfected` | `0` | CVAR_LATCH | **Master switch.** Enables rounds + infection mechanics. Latched -- requires map restart. |
| `g_rrInfectedSpreadTime` | `40` | -- | Seconds between forced infections (weakest blue player auto-switches to red) |
| `g_rrInfectedSpreadWarningTime` | `10` | -- | Seconds before infection spread to show warning message |
| `g_rrInfectedSurvivorScoreMethod` | `2` | -- | How blue team earns survival points: 0=disabled, 1=periodic timer, 2=on red death |
| `g_rrInfectedSurvivorScoreRate` | `30` | -- | Seconds between periodic survival bonuses (method 1 only) |
| `g_rrInfectedSurvivorScoreBonus` | `1` | -- | Points awarded per blue player per survival event |
| `g_rrInfectedSurvivorMinSpeed` | `500.0f` | -- | Minimum speed for survivors; published to `CS_INFECTED_SURVIVOR_MINSPEED` (CS 709) |
| `g_rrInfectedSurvivorPingRate` | `2000` | -- | Ping rate for survivor location updates |
| `g_rrInfectedZombieSpeed` | `1.15` | -- | Zombie (red team) speed multiplier (1.15 = 15% faster) |
| `g_rrInfectedZombieHealthBonus` | `50` | -- | Bonus health for zombie players on spawn |
| `g_rrInfectedZombieFragBonus` | `2` | -- | Extra frag points for zombie kills |

## Round State Machine

RR uses the standard round state machine with an additional RS_SHUFFLE state:

```
                     +-- RS_SHUFFLE (2) --+
                     |                    |
RS_WARMUP (0) --> RS_COUNTDOWN (1) --> RS_PLAYING (3) --> RS_ROUND_OVER (4)
                      ^                                        |
                      |                                        v
                      +--- RS_SHUFFLE (2) <--------------------+
                           (3500ms delay)                      |
                                                          RS_EXIT (5)
                                                    (timelimit/roundlimit)
```

### RS_WARMUP (0)
- Sets `CS_ROUND_STATUS` to `\time\-1\round\0`.

### RS_COUNTDOWN (1)
- Spawns all non-spectator players and sets `PMF_FROZEN` (movement locked).
- Updates alive counts.
- If `g_roundWarmupDelay` is 0, immediately transitions to RS_PLAYING.
- Otherwise, schedules RS_PLAYING after `g_roundWarmupDelay` ms.
- Calculates round number: `blueScore + redScore + 1`.
- Publishes `CS_ROUND_STATUS` with time and round number.

### RS_SHUFFLE (2)
- **Non-infected mode:** Calls `Svcmd_ForceShuffle_f()` to randomize teams, then schedules RS_COUNTDOWN after 1ms.
- **Infected mode:** No-op (returns immediately). In infected mode, team assignment is handled by the infection mechanic itself.

### RS_PLAYING (3)
- Clears `PMF_FROZEN` on all players.
- Resets per-round stats: `round_shots`, `round_hits`, `round_damage`, `killStreak`.
- Optionally grants `g_spawnArmor` quad timer.
- Resets infection timer (`rr_infectionStartTime = level.time`).
- Resets survival bonus timer (`rr_survivalNextTime = 0`).
- Publishes `CS_ROUND_TIME` and `CS_ROUND_STATUS`.

### RS_ROUND_OVER (4)
- Freezes all non-spectators (`PMF_FROZEN`).
- Determines winner by alive counts: team with players remaining gets +1 score.
- Increments round number and calls `CalculateRanks()`.
- Announces winning team.
- Checks game end (timelimit, roundlimit) -- only if scores are unequal.
- Plays team sound (20=team win, 18=draw).
- Schedules RS_SHUFFLE after 3500ms delay.

### RS_EXIT (5)
- Calls `RR_CheckExitRules(1)` to trigger `LogExit()`.
- Calls `Svcmd_ForceShuffle_f()` to shuffle teams for the next map.

## Scoring

- **Round win:** +1 to winning team's score.
- **Roundlimit:** Uses total rounds (`redScore + blueScore`), not per-team scores.
- **No mercy limit:** RR does not use `g_mercylimit`.
- **Tied score protection:** Game end is skipped if scores are tied (`ScoreIsTied()`).
- **Win condition:** Roundlimit reached or timelimit expired, with scores unequal.

## Team Switch on Death (Core Mechanic)

### `RR_OnPlayerDeath(victim)`

The central mechanic of Red Rover: dead players switch teams.

**Non-infected mode (`g_rrInfected == 0`):**
- The victim's `sess.sessionTeam` is set to the opposite team.
- `ClientUserinfoChanged()` is called to update the client's team info.

**Infected mode (`g_rrInfected != 0`):**
- Only blue-to-red switches occur (blue players "turn zombie" on death).
- Red players who die stay red.
- An `EV_INFECTED` event is broadcast with the victim's client number.
- The infection timer (`rr_infectionStartTime`) resets to current time.
- `RR_SurvivalBonus(1)` is called (awards survival points if method == 2).
- Tracks `rr_lastBlueClient` when the last blue player falls.

**Last man standing warning:**
- When either team reaches 1 alive player (and `g_lastManStandingWarning` is enabled), `LastManStanding()` is called. In infected mode, only blue team gets this warning.

## Infection Mode (Detailed)

When `g_rrInfected` is enabled, Red Rover transforms into a zombie survival mode:

### Round Lifecycle
1. **RS_SHUFFLE:** All players start on Blue team. One random player is moved to Red (the first zombie).
2. **RS_COUNTDOWN:** Players spawn and freeze.
3. **RS_PLAYING:** Zombies (red) hunt survivors (blue). Blue players who die switch to red.
4. **RS_ROUND_OVER:** Round ends when all blue players are eliminated (or round timelimit expires).
5. After round over, transition back to RS_SHUFFLE for a new zombie selection.

### Zombie (Red Team) Buffs
Zombies receive several advantages set in `ClientSpawn_RedRover()`:
- **Speed multiplier:** `g_rrInfectedZombieSpeed` (default 1.15x = 15% faster).
- **Health bonus:** `g_rrInfectedZombieHealthBonus` (default +50 HP).
- **Gauntlet only:** `PMF_LOADOUT_FORCED` (0x80000) restricts zombies to melee weapon.
- **Promode physics:** `PMF_PROMODE` (0x10000) is forced on, giving air control.
- **Double jump:** `PMF_DOUBLE_JUMPED` (0x20000) is forced on.
- **Frag bonus:** `g_rrInfectedZombieFragBonus` (default +2 extra points per kill).

### Survivor (Blue Team) Behavior
- Normal loadout (no forced restrictions).
- `PMF_LOADOUT_FORCED` is cleared.
- `PMF_PROMODE` and `PMF_DOUBLE_JUMPED` respect their respective pmove cvars.

### Forced Infection (`RR_CheckInfection`)
- Every `g_rrInfectedSpreadTime` seconds (default 40), the weakest surviving blue player is automatically switched to red.
- "Weakest" = last in `level.sortedClients` order among blue team members.
- A warning center-print message is shown `g_rrInfectedSpreadWarningTime` seconds (default 10) before infection.
- Only triggers if there are 2+ blue players alive.
- Scoring is temporarily disabled during forced infection (`level.scoringDisabled = qtrue`).
- Infection timer resets after each forced switch.

### Survival Bonus (`RR_SurvivalBonus`)
Blue team members earn survival points via two methods:

| Method | Trigger | Description |
|---|---|---|
| 0 | -- | Disabled |
| 1 | Timer | Every `g_rrInfectedSurvivorScoreRate` seconds, all alive blue players get `g_rrInfectedSurvivorScoreBonus` points |
| 2 | Red death | When a red player dies, all alive blue players get `g_rrInfectedSurvivorScoreBonus` points |

- Survival bonus triggers `EV_GLOBAL_TEAM_SOUND` with sound type 24.
- Calls `CalculateRanks()` after awarding points.

### Configstrings
- `CS_INFECTED_SURVIVOR_MINSPEED` (CS 709): Published at game init with the `g_rrInfectedSurvivorMinSpeed` float value.

## Spawn Behavior

### `ClientSpawn_RedRover(ent)`
Post-spawn adjustments called after `ClientSpawn()`:

**Infected mode:**
- Red team (zombies): Force `PMF_LOADOUT_FORCED | PMF_DOUBLE_JUMPED | PMF_PROMODE`.
- Blue team (survivors): Clear `PMF_LOADOUT_FORCED`; only set `PMF_DOUBLE_JUMPED`/`PMF_PROMODE` if their respective pmove cvars are enabled.

**All modes:**
- During warmup or RS_COUNTDOWN: set `PMF_FROZEN`.

### `ClientBegin_RedRover(ent)`
- During RS_COUNTDOWN: spawn + freeze + set `PMF_TIME_KNOCKBACK`.
- Otherwise: normal spawn + weapon select.

## Round End Detection (`RR_RunFrame`)

Per-frame during RS_PLAYING:
1. Calls `RR_CheckInfection()` for forced infection timing.
2. Calls `RR_SurvivalBonus(0)` for timer-based survival scoring.
3. If either team has 0 alive players: transition to RS_ROUND_OVER.
4. If `roundtimelimit` seconds elapsed: transition to RS_ROUND_OVER.

## Scoreboard

### `RRScoreboardMessage` -- `scores_rr` command
Format: `scores_rr <numPlayers> <redScore> <blueScore> <playerData>`

19 fields per player:
1. client number
2. score
3. round score (`localPersistant[0]`)
4. ping
5. time (minutes connected)
6. frags (kills)
7. deaths
8. accuracy (overall %)
9. best weapon
10. best weapon accuracy (%)
11. damage done
12. impressive count
13. excellent count
14. gauntlet frag count
15. defend count
16. assist count
17. perfect (1 if rank 0 and 0 deaths)
18. captures
19. alive (1 if PM_NORMAL)

## Key Functions

| Function | Description |
|---|---|
| `RR_InitRoundState()` | Initializes round state; enters RS_SHUFFLE or RS_WARMUP based on infection mode |
| `RR_RoundStateTransition()` | Main state machine dispatcher for all round states |
| `RR_RunFrame()` | Per-frame entry point from `G_RunFrame`; infection checks, survival bonus, round end |
| `RR_CheckExitRules()` | Evaluates timelimit and roundlimit (no mercy limit) |
| `RR_OnPlayerDeath()` | Core mechanic: switches victim's team; infection event in infected mode |
| `RR_CheckInfection()` | Periodic forced team switch of weakest blue player to red |
| `RR_SurvivalBonus()` | Awards survival points to blue team (timer or on-red-death) |
| `ClientSpawn_RedRover()` | Post-spawn: zombie buffs, survivor defaults, warmup/countdown freeze |
| `ClientBegin_RedRover()` | Spawn entry point for RR: normal or countdown freeze |
| `RRScoreboardMessage()` | Sends `scores_rr` with 19 per-player fields |
| `PickTeam_RoundAware()` | Team picker for infection mode: red during active rounds, blue between |
| `StartNewRound()` | Between rounds: moves all RED to BLUE, picks new RED player |
| `G_ObfuscateEnemyInfoInSnapshotCheck()` | Snapshot visibility: strips enemy position data in team gametypes (shared, in g_gametype_common.c) |

## Internal State

| Variable | Type | Description |
|---|---|---|
| `rr_lastRedClient` | int | Last red player eliminated (client number, -1 if none) |
| `rr_lastBlueClient` | int | Last blue player eliminated (client number, -1 if none) |
| `rr_survivalNextTime` | int | Next time for timer-based survival bonus |
| `rr_infectionStartTime` | int | Timestamp of last infection event (forced or death-triggered) |
| `level.infectedConscript` | int | Client number of the forced-infection target (-1 if none) |
| `level.lastZombieSurvivor` | int | Client number of the last zombie survivor (-1 if none) |
| `level.zombieScoreTime` | int | Timestamp for zombie scoring (-1 if none) |

## QL vs Q3 Differences

Red Rover did not exist in Quake 3 or Quake 3: Team Arena. It is entirely new to Quake Live.

- **Team switching on death:** The core mechanic -- dying causes you to switch to the other team -- has no Q3 equivalent.
- **RS_SHUFFLE state:** A new round state (enum value 2) added specifically for RR to handle team randomization between rounds.
- **Infection mode:** An entire sub-gametype layered on top of RR, with zombie buffs (`PMF_LOADOUT_FORCED`, speed multiplier, health bonus), forced periodic infections, and survival scoring. This is one of the most complex gametypes in QL.
- **PMF_LOADOUT_FORCED (0x80000):** A new pm_flags bit created specifically for infection mode's gauntlet-only zombie restriction.
- **EV_INFECTED (0x62):** A new event type for the infection visual/sound effect.
- **Roundlimit calculation:** Uses total rounds (red+blue) rather than per-team scores, unlike CA/FT which use per-team roundlimit.
- **No mercy limit:** RR explicitly does not use `g_mercylimit`, unlike FT and AD.
