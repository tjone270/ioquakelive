# Attack & Defend (GT_AD = 11)

## Overview

Attack & Defend is a round-based team mode with asymmetric objectives. Each round has two turns: in turn 0, Red attacks and Blue defends; in turn 1, the roles swap. The attacking team scores by capturing the flag; the defending team scores by eliminating all attackers or running out the clock. Exit rules are only evaluated after both turns complete. This ensures fairness -- both teams get equal opportunity on offense and defense.

## Source Files

- **Primary:** `code/game/g_gametype_ad.c` -- round state machine, score history, scoreboard (427 lines)
- **Binary addresses:**
  - `ADScoreboardMessage`: `0x100365a0`
- **Shared infrastructure:** Uses flag capture/return logic from `code/game/g_team.c`

## Cvars

| Cvar | Default | Flags | Description |
|---|---|---|---|
| `g_adElimScoreBonus` | `1` | CVAR_SERVERINFO | Bonus score awarded when the defending team eliminates all attackers |
| `g_scorelimit` | varies | -- | Score needed (after both turns) for a team to win |
| `g_timelimit` | varies | -- | Time limit in minutes |
| `g_mercylimit` | `0` | -- | Score difference for mercy rule |
| `g_mercytime` | varies | -- | Minutes before mercy rule activates |
| `g_roundWarmupDelay` | varies | -- | Countdown duration before each round (ms) |
| `roundtimelimit` | varies | -- | Per-turn time limit in seconds |
| `g_spawnArmor` | `0` | -- | If nonzero, grants quad powerup timer at round start |

## Round State Machine

AD uses a two-turn round structure. Each "round" consists of turn 0 (Red attacks) and turn 1 (Blue attacks). The turn toggles after each RS_ROUND_OVER.

```
RS_WARMUP (0) --> RS_COUNTDOWN (1) --> RS_PLAYING (3) --> RS_ROUND_OVER (4)
                      ^                                        |
                      |                                        |  turn ^= 1
                      +--- (3500ms delay) <--------------------+  round++ if turn wraps to 0
                                                               |
                                                          RS_EXIT (5)
                                                  (only after turn 1, scores unequal)
```

### RS_WARMUP (0)
- Initializes the AD score history ring buffer (all slots set to -1).
- Sends initial `scores_ad` command with all -1 slots.
- Sets `CS_ROUND_STATUS` to `\time\-1\round\0\turn\0\state\0`.

### RS_COUNTDOWN (1)
- Spawns all non-spectator players and sets `PMF_FROZEN` (movement locked).
- Updates alive counts.
- If `g_roundWarmupDelay` is 0, immediately transitions to RS_PLAYING.
- Otherwise, schedules RS_PLAYING after `g_roundWarmupDelay` ms.
- Publishes `CS_ROUND_STATUS` with time, round, turn, and `state\1`.

### RS_PLAYING (3)
- Clears `PMF_FROZEN` on all players -- movement unlocked.
- Resets per-round stats: `round_shots`, `round_hits`, `round_damage`, `killStreak`.
- Resets `capture` and `touch` flags to 0.
- Optionally grants `g_spawnArmor` quad timer.
- Publishes `CS_ROUND_TIME` and `CS_ROUND_STATUS` with `turn` and `state\2`.

### RS_ROUND_OVER (4)
- Freezes all non-spectators (`PMF_FROZEN`), clears quad and battlesuit powerups.
- Determines winner:
  - **Capture (capture == 1):** Attacking team (turn 0 = Red, turn 1 = Blue) wins.
  - **Attacker elimination (all attackers dead):** Defending team wins and receives `g_adElimScoreBonus` bonus score.
  - **Defender elimination (all defenders dead):** Attacking team wins.
- Awards medals:
  - **Accuracy:** >50% hit rate during the turn.
  - **Perfect:** 0 damage taken during the turn.
- Calls `CalculateRanks()` and `AD_UpdateScoreHistory()`.
- Returns both flags via `Team_ReturnFlag()`.
- Checks game end (only after turn 1 with unequal scores):
  - Timelimit, scorelimit, and mercy limit.
- Plays team sound (16=red win, 17=blue win, 18=draw).
- **Turn toggle:** `turn ^= 1`. If turn wraps back to 0, `round++`.
- Schedules RS_COUNTDOWN after 3500ms delay.

### RS_EXIT (5)
- Calls `AD_CheckExitRules(1)` which triggers `LogExit()` for timelimit, scorelimit, or mercy limit.

## Two-Turn Structure

The `level.roundState.turn` field alternates between 0 and 1:

| Turn | Attacking Team | Defending Team |
|---|---|---|
| 0 | Red | Blue |
| 1 | Blue | Red |

- Turn toggles (`turn ^= 1`) after every RS_ROUND_OVER.
- The round number only increments when turn wraps back to 0 (after both teams have attacked).
- Exit rules (`AD_CheckExitRules`) only fire when `turn == 1` -- ensuring both teams get equal turns before the game can end.

## Score History

AD maintains a ring buffer of per-turn score deltas for the scoreboard:

- `ad_scoreRaw[20]` -- raw score deltas per turn.
- `ad_scoreSorted[20]` -- reordered for display (chronological, interleaved turns).
- `ad_scoreHistoryIndex` -- current write position.
- `ad_prevScore[2]` -- previous cumulative score per team (index 0=Red, 1=Blue).
- `ad_turnDelta[2]` -- score gained during the current turn per team.

`AD_UpdateScoreHistory()` computes the delta, writes to the ring buffer, reorders for display, and sends the `scores_ad` command.

## Scoring

- **Flag capture:** The attacking team captures the defending team's flag to score points.
- **Elimination bonus:** If the defending team kills all attackers, they receive `g_adElimScoreBonus` (default 1) extra points.
- **Round timeout:** If `roundtimelimit` expires, the turn ends (no capture = defender wins).
- **Win condition:** After both turns, if scores are unequal: scorelimit, timelimit, or mercy limit triggers game end.

## Round End Detection (`AD_RunFrame`)

Per-frame during RS_PLAYING, checks:
1. Both teams' alive counts (`UpdateTeamAliveCount`).
2. If either team has 0 alive players, or `capture == 1`: transition to RS_ROUND_OVER.
3. If `roundtimelimit` seconds have elapsed since round start: transition to RS_ROUND_OVER.

## Helper Functions

| Function | Description |
|---|---|
| `AD_IsInPlayState()` | Returns attacking team number (1=Red, 2=Blue) during RS_PLAYING, 0 otherwise |
| `AD_CanScore()` | Returns defending team number during RS_PLAYING, 0 otherwise |

These are used by flag capture logic in `g_team.c` to enforce asymmetric scoring -- only the attacking team can capture.

## Spawn Behavior

- All players are spawned and frozen (`PMF_FROZEN`) during RS_COUNTDOWN.
- Standard team spawn logic; no asymmetric spawn points.
- Late joiners during an active round follow CA-style spectating.

## Scoreboard

### `ADScoreboardMessage` -- `scores_ad` command
- 22 values: 20 score history slots (ring buffer, -1 for empty) + `teamScoreRed` + `teamScoreBlue`.
- Sent to all clients (`-1` target).
- Updated via `AD_UpdateScoreHistory()` after each turn ends.

## Key Functions

| Function | Description |
|---|---|
| `AD_RoundStateTransition()` | Main state machine dispatcher for all round states |
| `AD_RunFrame()` | Per-frame entry point from `G_RunFrame`; checks round end conditions |
| `AD_CheckExitRules()` | Evaluates timelimit, scorelimit, and mercy limit (only after turn 1) |
| `AD_UpdateScoreHistory()` | Updates the 20-slot score ring buffer and sends `scores_ad` |
| `AD_InitScoreboard()` | Resets score history to all -1 and sends initial `scores_ad` |
| `AD_IsInPlayState()` | Returns attacking team number or 0 |
| `AD_CanScore()` | Returns defending team number or 0 |
| `ADScoreboardMessage()` | Sends `scores_ad` command with current state |

## QL vs Q3 Differences

Attack & Defend did not exist in Quake 3 or Quake 3: Team Arena. It is entirely new to Quake Live.

- **Asymmetric rounds:** The two-turn structure with alternating attack/defend roles has no Q3 precedent.
- **Score history ring buffer:** The 20-slot AD score history is unique to this gametype.
- **Flag reuse:** AD reuses CTF flag entities and capture logic but restricts who can score via `AD_IsInPlayState()` / `AD_CanScore()`.
- **Elimination bonus:** `g_adElimScoreBonus` rewards the defending team for a full wipe of attackers.
- **Deferred exit rules:** Game end is only evaluated after both turns complete, preventing unfair early exits.
