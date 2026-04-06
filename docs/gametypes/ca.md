# Clan Arena (GT_CA = 4)

## Overview

Clan Arena is a round-based team elimination mode. Two teams (Red and Blue) fight until one team is completely eliminated. Dead players do not respawn until the next round. The winning team receives +1 to their team score. The game ends when a team reaches the roundlimit, the timelimit expires, or the mercylimit is reached.

No items spawn on the map. Players spawn with full loadout and are governed by `g_spawnArmor` for initial protection time.

## Source Files

- **Primary**: `code/game/g_gametype_ca.c` (round state machine, accuracy/damage filter, scoreboard)
- **Shared helpers**: `code/game/g_gametype_common.c` (UpdateTeamAliveCount, dead-player spectator logic)
- **Binary addresses** (qagamex86.dll build 1069):
  - `CA_CheckTimer` -- `0x10038080`
  - `CA_AccuracyMessage` -- `0x100380d0`
  - `CA_CheckExitRules` -- `0x100382e0`
  - `CA_RoundStateTransition` -- `0x10038420`
  - `CA_RunFrame` -- `0x10038be0`
  - `TeamCount_Health` -- `0x1006b100`
  - `LastManStanding` -- `0x1006b200`
  - `CAScoreboardMessage` -- `0x1003e4f0`

## Cvars

| Cvar | Default | Flags | Description |
|------|---------|-------|-------------|
| `roundlimit` | `0` | SERVERINFO, ARCHIVE, NORESTART | Rounds to win for match victory. 0 = no limit. |
| `g_roundWarmupDelay` | `10000` | SERVERINFO | Countdown duration (ms) between rounds before unfreeze. |
| `g_accuracyFlags` | `0` | -- | Bitfield controlling damage/knockback filtering. Bit 1: suppress team damage. Bit 2: suppress team knockback. Bit 4: suppress self-damage. Bit 8: suppress self-knockback. |
| `g_roundDrawLivingCount` | `1` | -- | If nonzero, round tiebreaker uses surviving player count. |
| `g_roundDrawHealthCount` | `1` | -- | If nonzero, round tiebreaker uses total team health+armor when alive counts are equal. |
| `g_lastManStandingWarning` | `0` | -- | If nonzero, enable last-man-standing announcements. |
| `g_lastManStandingMessage` | `"You are the last standing"` | -- | Centerprint message sent to the sole survivor. |
| `g_spawnArmor` | `0` | -- | If nonzero, grants PW_QUAD for this duration (ms) at round start as spawn protection. |
| `g_timelimit` | varies | SERVERINFO | Overall match time limit in minutes. |
| `mercylimit` | `0` | SERVERINFO, ARCHIVE, NORESTART | Score difference at which the match ends early (after `g_mercytime`). |
| `g_mercytime` | `0` | -- | Minutes before mercylimit can take effect. |

## Round State Machine

CA uses a `roundStateState_t` enum defined in `bg_public.h`:

```
RS_WARMUP (0) --> RS_COUNTDOWN (1) --> RS_PLAYING (3) --> RS_ROUND_OVER (4) --+
                      ^                                                        |
                      +--------------------------------------------------------+
                                                                               |
                                                              RS_EXIT (5) <----+ (game over)
```

State 2 (`RS_SHUFFLE`) is unused by CA (it is RR-only). Entering state 2 triggers `G_Error`.

### State Transitions

**RS_WARMUP (0)**
- Entry state during warmup. Sets `CS_ROUND_STATUS` to `\time\-1\round\0`.
- Remains here until warmup ends and the first round begins.

**RS_COUNTDOWN (1)**
- All non-spectator players are respawned via `ClientSpawn()` and frozen (`PMF_FROZEN` set).
- `UpdateTeamAliveCount()` is called to initialize alive counts.
- If `g_roundWarmupDelay` is 0, immediately transitions to RS_PLAYING.
- Otherwise, schedules RS_PLAYING after `g_roundWarmupDelay` milliseconds.
- Round number is calculated as `redScore + blueScore + 1`.
- `CS_ROUND_STATUS` is set with the countdown end time and round number.

**RS_PLAYING (3)**
- `PMF_FROZEN` is cleared on all connected players.
- Per-round tracking is reset: `round_shots`, `round_hits`, `round_damage`, `killStreak`.
- If `g_spawnArmor` is nonzero, `PW_QUAD` is granted for that duration.
- `CS_ROUND_TIME` is set to the current time. `CS_ROUND_STATUS` shows the round number.
- While in this state, `CA_RunFrame()` checks alive counts each frame.
- When one team reaches 0 alive players, transitions to RS_ROUND_OVER.

**RS_ROUND_OVER (4)**
- All non-spectator players are frozen (`PMF_FROZEN`).
- Alive counts and health totals are computed via `UpdateTeamAliveCount()` and `TeamCount_Health()`.
- Winner determination:
  1. If one team has 0 alive and the other does not, the surviving team wins.
  2. If both teams have survivors, `g_roundDrawLivingCount` breaks the tie by alive count.
  3. If alive counts are equal (or `g_roundDrawLivingCount` is 0), `g_roundDrawHealthCount` breaks the tie by total health+armor.
  4. If health is also equal, the round is a draw (no score awarded).
- Winning team gets +1 to `level.teamScores`. `CalculateRanks()` is called.
- Awards are checked: ACCURACY (>50% hit rate), PERFECT (on winning team with 0 damage taken).
- Round-end sounds: GTS_RED_WINS_ROUND (16), GTS_BLUE_WINS_ROUND (17), or GTS_DRAW_ROUND (19). Note: GT_RR uses different sound indices (18/20).
- Exit conditions checked: timelimit, roundlimit, mercylimit. If met, schedules RS_EXIT after 1500ms.
- Otherwise, schedules RS_COUNTDOWN after 3500ms for the next round.
- `CS_ROUND_TIME` is set to -1 (no active round timer).

**RS_EXIT (5)**
- Calls `CA_CheckExitRules(1)` which triggers `LogExit()` and ends the match.

## Scoring

- **Team score**: +1 per round won.
- **Individual score**: CA implements a score-per-damage mechanic in `CA_AccuracyMessage()`. Every 100 damage dealt to enemies accumulates +1 to the attacker's `PERS_SCORE`. Damage is clamped to the target's current armor (for the damage component) and max health (for the knockback component) to prevent over-counting.
- **Awards**: ACCURACY (>50% round hit rate), PERFECT (round won with 0 damage taken).

## Damage Filtering (CA_AccuracyMessage)

`CA_AccuracyMessage()` is called from the damage path and acts as both a damage filter and the score-per-damage accumulator.

- During warmup, all damage passes through.
- Outside RS_PLAYING state, all damage is suppressed (returns `qfalse`).
- `g_accuracyFlags` bitfield controls self-damage/knockback and team-damage/knockback suppression.
- Score accumulation: damage to enemies is accumulated in `dmgAccumulator`. When it reaches 100, +1 score is awarded and `CalculateRanks()` is called.

## Spawn Behavior

- During RS_COUNTDOWN, all non-spectator players are respawned with `ClientSpawn()` and immediately frozen (`PMF_FROZEN`).
- When RS_PLAYING begins, `PMF_FROZEN` is cleared and players can move.
- Dead players during a round become spectators (handled in `g_gametype_common.c`) and do not respawn until the next round.
- When a player dies during RS_PLAYING, `CA_RunFrame()` is called from the death path to check if the round should end.

## Last Man Standing

When `g_lastManStandingWarning` is nonzero and a team is reduced to exactly 1 alive player:
- `EV_GLOBAL_TEAM_SOUND` with `GTS_LAST_STANDING` (19) is broadcast.
- The sole survivor receives a centerprint with the `g_lastManStandingMessage` string.
- This check runs every frame in `CA_RunFrame()`.

## Key Functions

| Function | Description |
|----------|-------------|
| `CA_RoundStateTransition()` | Core state machine. Handles all state transitions and round logic. |
| `CA_RunFrame()` | Per-frame update. Advances timers, checks alive counts, triggers round-over or last-man-standing. Also called from `player_die`. |
| `CA_CheckTimer()` | Checks if a pending timer has elapsed and advances state if so. Returns current state or -1 if still waiting. |
| `CA_CheckExitRules()` | Checks timelimit, roundlimit, and mercylimit. If `doExit` is true, calls `LogExit()`. |
| `CA_AccuracyMessage()` | Damage/knockback filter and score-per-damage accumulator. Returns whether damage should be applied. |
| `TeamCount_Health()` | Fills a 4-element array with total health+armor per team (alive players only). Used for tiebreaking. |
| `LastManStanding()` | Broadcasts GTS_LAST_STANDING sound and centerprint to the sole survivor on a team. |
| `CAScoreboardMessage()` | Builds and sends the `scores_ca` server command with 16 fields per player. |

## Scoreboard

The CA scoreboard uses the `scores_ca` command format:

```
scores_ca <numPlayers> <redScore> <blueScore> <playerData...>
```

16 fields per player: `client team score ping time kills deaths accuracy bestWeapon bestWeaponAccuracy damageDone impressive excellent gauntlet perfect alive`

## QL vs Q3 Differences

- **Clan Arena did not exist in Q3.** It is entirely a Quake Live addition (GT_CA = 4, occupying the slot that was unused in Q3).
- The round state machine (`roundStateState_t`) is QL-specific infrastructure shared with Freeze Tag (GT_FT), Attack & Defend (GT_AD), and Red Rover (GT_RR).
- Score-per-damage mechanic (100 dmg = +1 individual score) is QL-specific.
- `g_accuracyFlags` damage filtering system is QL-specific.
- Round tiebreakers by alive count and health totals are QL-specific.
- Last-man-standing announcements are QL-specific.
- The `CA_AccuracyMessage` function is also used by GT_RR (Red Rover) for its round-end sound indices.
- Awards (ACCURACY, PERFECT) with the `EV_AWARD` broadcast system are QL additions.
