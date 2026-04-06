# Freeze Tag (GT_FREEZE = 9)

## Overview

Freeze Tag is a round-based team mode. When a player is "killed," they are frozen in place (`PM_FREEZE`) rather than eliminated. Teammates can thaw frozen allies by standing near them within line of sight. A round ends when all members of one team are frozen simultaneously. The team that wins the most rounds wins the game.

## Source Files

- **Primary:** `code/game/g_gametype_ft.c` -- round state machine, thaw logic, scoreboard (734 lines)
- **Binary addresses:**
  - `FTScoreboardMessage`: `0x1003ef80`
  - `FTScoreboardMessage_impl` (per-player detail): `0x1003ee30`
- **Shared round infrastructure:** Uses `CA_CheckExitRules()` from `code/game/g_gametype_ca.c` for RS_EXIT

## Cvars

| Cvar | Default | Flags | Description |
|---|---|---|---|
| `g_freezeThawTime` | `2000` | -- | Total thaw duration in milliseconds |
| `g_freezeThawRadius` | `96` | -- | Distance (units) a teammate must be within to thaw |
| `g_freezeThawThroughSurface` | `0` | -- | If 1, line-of-sight check is skipped for thawing |
| `g_freezeThawTick` | `1` | CVAR_INIT | Thaw progress tick (latched at init) |
| `g_freezeThawWinningTeam` | `1` | -- | If 1, winning team members are thawed between rounds too |
| `g_freezeAutoThawTime` | `120000` | -- | Auto-thaw time in ms (prevents indefinite freezing) |
| `g_freezeProtectedSpawnTime` | `0` | CVAR_INIT | Spawn protection duration in ms |
| `g_freezeRemovePowerupsOnRound` | `1` | -- | Remove dropped powerup items and clamp active powerup timers on round start |
| `g_freezeResetHealthOnRound` | `1` | -- | Reset player health to max on round start |
| `g_freezeResetArmorOnRound` | `1` | -- | Reset armor to `g_startingArmor` on round start |
| `g_freezeResetWeaponsOnRound` | `1` | -- | Reset weapon selection on round start |
| `g_freezeAllowRespawn` | `0` | -- | If 1, allow respawn instead of freeze |
| `g_freezeRoundDelay` | `0` | CVAR_SERVERINFO | Delay between rounds (ms); first round uses `g_roundWarmupDelay` |
| `roundtimelimit` | varies | -- | Per-round time limit in seconds |
| `roundlimit` | varies | -- | Round limit for game end |
| `g_roundWarmupDelay` | varies | -- | Countdown duration before first round (ms) |
| `g_roundDrawLivingCount` | varies | -- | On round timeout, team with more alive players wins |
| `g_roundDrawHealthCount` | varies | -- | On round timeout tiebreaker, team with more total HP+armor wins |
| `g_mercylimit` | `0` | -- | Score difference for mercy rule |
| `g_mercytime` | varies | -- | Minutes before mercy rule activates |
| `g_spawnArmor` | `0` | -- | If nonzero, grants quad powerup at round start (used as spawn armor timer) |

## Round State Machine

```
RS_WARMUP (0) --> RS_COUNTDOWN (1) --> RS_PLAYING (3) --> RS_ROUND_OVER (4)
                      ^                                        |
                      |                                        v
                      +--- (3500ms delay) <--------------------+
                                                               |
                                                          RS_EXIT (5)
                                                      (timelimit/roundlimit/mercy)
```

### RS_WARMUP (0)
- Sets `CS_ROUND_STATUS` to `\time\-1\round\0`.
- Waits until warmup ends and the game state transitions to COUNT_DOWN.

### RS_COUNTDOWN (1)
- Thaws/respawns players from the previous round:
  - Frozen players (`PM_FREEZE`) are thawed with `EV_THAW_PLAYER` event and respawned -- unless they were on the winning team and `g_freezeThawWinningTeam` is 0.
  - Dead/spectating players are respawned normally.
- Optionally clamps powerup timers to 3 seconds (`g_freezeRemovePowerupsOnRound`).
- Optionally resets weapons (`g_freezeResetWeaponsOnRound`).
- Optionally resets health/armor (`g_freezeResetHealthOnRound`, `g_freezeResetArmorOnRound`).
- Sets `PMF_FROZEN` on all players (movement locked).
- Removes dropped powerup entities from the map.
- Updates alive counts and calculates round number: `redScore + blueScore + 1`.
- First round uses `g_roundWarmupDelay`; subsequent rounds use `g_freezeRoundDelay`.
- If countdown is 0, immediately transitions to RS_PLAYING.
- Publishes `CS_ROUND_STATUS` with countdown time and round number.

### RS_PLAYING (3)
- Clears `PMF_FROZEN` on all players -- movement unlocked.
- Resets per-round stats: `round_shots`, `round_hits`, `round_damage`, `killStreak`.
- Optionally grants `g_spawnArmor` as a quad powerup timer.
- Publishes `CS_ROUND_TIME` and `CS_ROUND_STATUS`.
- Per-frame (`Freeze_Think`): checks `Freeze_GameIsOver()` to detect round end.

### RS_ROUND_OVER (4)
- Freezes all non-spectator players (`PMF_FROZEN`).
- Awards team score (+1 to winning team).
- Announces winner with alive count or HP remaining.
- Awards medals:
  - **Accuracy:** >50% hit rate during the round.
  - **Perfect:** On winning team with 0 damage taken during the round.
- Increments round number and calls `CalculateRanks()`.
- Checks game end conditions: timelimit, roundlimit, mercy limit.
- Plays team sound (8=red win, 9=blue win, 18=draw).
- Schedules RS_COUNTDOWN after 3500ms delay.

### RS_EXIT (5)
- Calls `CA_CheckExitRules(1)` to trigger `LogExit()` and intermission.

## Scoring

- **Round win:** +1 to winning team's score.
- **Round timeout tiebreakers:** Checked in order:
  1. `g_roundDrawLivingCount`: team with more alive players wins.
  2. `g_roundDrawHealthCount`: team with more total health+armor wins.
  3. If still tied: draw (`TEAM_FREE`).
- **Win condition:** `roundlimit` reached, `timelimit` expired, or mercy limit exceeded.
- Mercy limit only applies after `g_mercytime` minutes.

## Freeze/Thaw Mechanics

### Freezing (`Freeze_PlayerFrozen`)
- Called when a player takes lethal damage during RS_PLAYING.
- Sets `pm_type` to `PM_FREEZE`, health to 0, `takedamage` to false, entity health to -40.
- Initializes thaw counter: `g_freezeThawTime / 50` ticks.
- Updates team alive counts.

### Thawing (`Freeze_ClientThawCheck`)
- Called per-frame from `ClientEndFrame` for each frozen player.
- Only active during RS_PLAYING.
- Searches within `g_freezeThawRadius` for alive teammates.
- Requires line-of-sight unless `g_freezeThawThroughSurface` is enabled.
- When a teammate is nearby: decrements `freezePlayer` counter each frame.
- When counter reaches 0: thaw completes:
  - Restores `PM_NORMAL`, health to `g_startingHealth + g_startingHealthBonus`, armor to `g_startingArmor`.
  - Fires team sound event (4=red thaw, 5=blue thaw).
  - Re-enables `takedamage`.
- When no teammate nearby: counter resets back toward `g_freezeThawTime / 50`.

### Round End Detection (`Freeze_GameIsOver`)
Checks three conditions:
1. **Round timelimit:** `roundtimelimit` seconds since round start, resolved by living count then health count.
2. **Alive counts:** Both teams at 0 = draw; one team at 0 = other team wins.
3. **Frozen status:** `Freeze_TeamFrozen()` checks if all connected players on a team have health <= 0.

## Spawn Behavior

### `ClientBegin_Freeze`
- **Pre-round (RS_WARMUP):** Unfreeze if needed, normal spawn + weapon select.
- **Countdown (RS_COUNTDOWN):** Unfreeze, spawn, weapon select, set `PMF_TIME_KNOCKBACK`.
- **Round active (RS_PLAYING or later):** Set to `PM_SPECTATOR`, spawn, follow a teammate via `Cmd_FollowCycle_f`.

Late joiners during an active round spectate until the next round.

## Scoreboard

### `FTScoreboardMessage` -- `scores_ft` command
- 28 team item stats (14 per team: armor pickups, mega health, powerup pickups, powerup possession times).
- Team stats for the opposing team are zeroed unless during intermission.
- 17 fields per player: client, team, score, ping, time, frags, deaths, accuracy, bestWeapon, impressive, excellent, gauntlet, assist, teamKills, teamKilled, damageDone, alive.

### `FTScoreboardMessage_impl` -- `ctfstats` command
- 12 fields per player: suicides, damageDone, damageTaken, redArmorPickups, yellowArmorPickups, greenArmorPickups, megaHealthPickups, quadPickups, battleSuitPickups, regenPickups, hastePickups, invisPickups.

## Key Functions

| Function | Description |
|---|---|
| `Freeze_Think()` | Per-frame entry point from `G_RunFrame`; processes state transitions and checks round end |
| `Freeze_RoundStateTransition()` | Main state machine dispatcher |
| `Freeze_GameIsOver()` | Evaluates round end conditions (timeout, alive counts, frozen status) |
| `Freeze_TeamFrozen()` | Returns true if all players on a team have health <= 0 |
| `Freeze_PlayerFrozen()` | Called on player death; sets PM_FREEZE instead of killing |
| `Freeze_ClientThawCheck()` | Per-frame thaw progress for each frozen player |
| `ClientBegin_Freeze()` | Spawn logic for FT: handles pre-round, countdown, and mid-round join |
| `FTScoreboardMessage()` | Sends `scores_ft` command with team stats and 17 per-player fields |
| `FTScoreboardMessage_impl()` | Sends `ctfstats` command with 12 per-player detail fields |
| `TeamCount_Health_FT()` | Sums health+armor totals per team for tiebreaker resolution |

## QL vs Q3 Differences

Freeze Tag did not exist in Quake 3 or Quake 3: Team Arena. It is entirely new to Quake Live.

- **PM_FREEZE (4):** New player movement type added to the pmtype_t enum specifically for Freeze Tag.
- **PMF_FROZEN (0x0004):** New pm_flags bit used during countdown and round-over to lock player movement.
- **EV_THAW_PLAYER (0x57):** New event type for the thaw visual/sound effect.
- **Round-based structure:** FT uses the QL round state machine (`roundState_t`) shared with CA, AD, and RR.
- **Thaw mechanic:** Proximity-based revival is unique to QL; there is no Q3 equivalent.
- **Scoreboard:** Uses `scores_ft` command (distinct from CA's `scores_ca`), with an alive field per player.
