# Domination (GT_DOMINATION = 10)

## Overview

Domination is a team-based territory control mode. Maps contain up to 5 control points (`team_dom_point` entities). Teams capture points by touching them. Each captured point periodically awards +1 to the controlling team's score. The game uses standard timelimit/capturelimit exit rules. There is no round state machine.

## Source Files

- **Primary:** `code/game/g_gametype_dom.c` -- all Domination logic (~270 lines)
- Binary addresses: DOM_CheckFlagScore (0x1004ac40), DOM_AddTeamScores (0x1004adb0), DOM_FlagTouch (0x1004aea0), DOM_FlagThink (0x1004b790), DOM_FragBonuses (0x1004bb40), SP_team_dom_point (0x1004bcd0)

## Constants

| Constant | Value | Description |
|---|---|---|
| `MAX_DOM_POINTS` | 5 | Maximum number of control points per map |
| `DOM_SCORE_INTERVAL` | 5000 | Scoring interval in milliseconds (every 5 seconds) |

## Map Entities

| Entity | Description |
|---|---|
| `team_dom_point` | A capturable control point. Up to 5 per map. |

## Configstrings

| CS Index | Description |
|---|---|
| 700 | Number of control points held by Red team |
| 701 | Number of control points held by Blue team |

These are updated each time a capture occurs.

## Cvars

Domination uses standard team game cvars:

| Cvar | Description |
|---|---|
| `capturelimit` | Score needed for a team to win |
| `timelimit` | Time limit in minutes |

No gametype-specific cvars exist for Domination.

## Scoring

- **Point capture:** A player touches an unowned or enemy-owned `team_dom_point` to capture it for their team. A capture message is broadcast to all players.
- **Score ticks:** Every `DOM_SCORE_INTERVAL` (5000ms), each captured point awards +1 to the controlling team's score. `CalculateRanks()` is called after each increment.
- **No scoring during intermission/warmup:** `DOM_FlagThink` returns early if `level.intermissionQueued`, `level.intermissionTime`, or `level.warmupTime` is set.
- **Win condition:** First team to reach `capturelimit` or highest score at `timelimit`.

### Individual Player Scoring & Medals

- **CAPTURE medal (+25):** Awarded to the player tracked as `domCapturer` on the point when a score tick occurs. Uses `PERS_CAPTURES` stat and `EF_AWARD_CAP` sprite.
- **ASSIST medal (+15):** Awarded to other same-team players near the point during a score tick. Uses `PERS_ASSIST_COUNT` stat and `EF_AWARD_ASSIST` sprite.
- **DEFENSE medal (+10):** Awarded by `DOM_FragBonuses()` when a player kills an enemy within 500 units of a dom point their team owns (requires PVS line of sight via `trap_InPVS`). Uses `PERS_DEFEND_COUNT` stat and `EF_AWARD_DEFEND` sprite.

### Capturer Tracking (`domCapturer`)

Each dom point entity has a `domCapturer` field (gentity_t pointer) tracking the player who captured it. Determined by `DOM_AddTeamScores()`:
- One team present, single player: that player becomes capturer.
- One team present, multiple players: existing capturer kept if still nearby, otherwise first player.
- Both teams present: contested, capturer set to NULL.
- No players nearby: no change.

## Control Point Lifecycle

### Spawn (`SP_team_dom_point`)
1. Only spawns if `g_gametype.integer == GT_DOMINATION`; otherwise freed.
2. Enforces `MAX_DOM_POINTS` limit.
3. Sets bounding box: mins `(-40, -40, -15)`, maxs `(40, 40, 128)`.
4. Sets `CONTENTS_TRIGGER` for touch detection.
5. Initial owner: `TEAM_FREE` (uncaptured).
6. Registers in the static `domPoints[]` array.
7. Schedules `DOM_FlagFirstThink` for the next frame.

### First Think (`DOM_FlagFirstThink`)
1. Drops the entity to the ground via a downward trace (256 units).
2. Snaps position to the ground contact point.
3. Sets movement type to `TR_STATIONARY`.
4. Assigns `DOM_FlagThink` as the ongoing think function.
5. Assigns `DOM_FlagTouch` as the touch function.
6. Schedules first score tick at `level.time + DOM_SCORE_INTERVAL`.
7. Re-links the entity.

### Touch (`DOM_FlagTouch`)
- Ignores non-client touchers.
- Ignores spectators and free team.
- Ignores if the point is already owned by the toucher's team.
- On capture: sets `ent->count` to the capturing team, broadcasts a capture message, updates CS 700/701 with current red/blue point counts.

### Think (`DOM_FlagThink`)
- Runs every `DOM_SCORE_INTERVAL` milliseconds.
- Skips scoring during intermission or warmup.
- Awards +1 to the controlling team's score and calls `CalculateRanks()`.

## Spawn Behavior

Standard team spawn logic. No special spawn modifications for Domination.

## Key Functions

| Function | Description |
|---|---|
| `SP_team_dom_point()` | Spawn function for `team_dom_point` entities |
| `DOM_FlagFirstThink()` | One-time initialization: drop to ground, set up touch/think |
| `DOM_FlagTouch()` | Capture logic: scans nearby players, determines capturer, awards medals |
| `DOM_FlagThink()` | Periodic scoring for captured points (every 5 seconds) |
| `DOM_AddTeamScores()` | Determines capturer based on team presence near the point |
| `DOM_CheckFlagScore()` | Awards CAPTURE (+25) and ASSIST (+15) medals on score ticks |
| `DOM_FragBonuses()` | Awards DEFENSE (+10) for kills near owned points (<500 units + PVS) |

## Internal State

- `domPoints[MAX_DOM_POINTS]` -- static array of pointers to control point entities.
- `numDomPoints` -- count of spawned control points.
- `ent->count` -- stores the owning team: `TEAM_FREE` (uncaptured), `TEAM_RED`, or `TEAM_BLUE`.
- `ent->domCapturer` -- pointer to the player entity who captured this point (for medal awards).

## QL vs Q3 Differences

Domination did not exist in Quake 3 or Quake 3: Team Arena. It is entirely new to Quake Live.

- **New entity type:** `team_dom_point` is a QL-specific entity with no Q3 equivalent.
- **Simple capture mechanic:** Unlike CTF flags, control points are captured instantly by touch with no carry/return mechanic.
- **Passive scoring:** Points accumulate over time (every 5 seconds per point), rewarding sustained map control rather than individual actions.
- **No dedicated scoreboard:** Domination does not have a custom `scores_dom` command; it uses standard team scoring.
- **No round system:** Unlike FT/AD/RR, Domination is a continuous-play mode like CTF.
