# Race (GT_RACE = 2)

## Overview

Race is a time-trial gametype unique to Quake Live. Players race through a series of checkpoint entities (`race_point`) placed by the map author. The goal is to complete the course from start to finish in the shortest time. There is no fragging -- scoring is purely time-based. Any number of players can race simultaneously on the same map, each running their own independent timer.

GT_RACE occupies index 2, which was GT_SINGLE_PLAYER in Quake 3.


## Source Files

- **Primary:** `code/game/g_gametype_race.c` -- checkpoint system, touch handlers, spawn logic
- **Struct definition:** `code/game/g_local.h` -- `raceInfo_t`

### Binary Addresses (qagamex86.dll)

| Function | Address |
|---|---|
| SP_race_point | 0x10063e47 (model registration verified) |

## Cvars

Race mode does not introduce many unique cvars. It reuses standard server cvars:

| Cvar | Default | Description |
|---|---|---|
| `timelimit` | 0 | Overall match time limit in minutes. 0 = no time limit (typical for race servers). |
| `g_doWarmup` | 1 | Enable warmup phase. Race timing is disabled during warmup (`level.warmupTime > 0`). |
| `g_startingHealth` | 100 | Base spawn health. |
| `g_startingHealthBonus` | 25 | Bonus spawn health. |
| `g_startingWeapons` | 3 | Starting weapons bitmask. |

## Scoring

- **Score = finish time** in milliseconds. Stored in `PERS_SCORE`.
- **Personal best** is tracked in `localPersistant[0]`. Updated when a new finish time is lower than the existing best (or when no best exists yet).
- **Initial score** is set to `0x7FFFFFFF` (max int) as a sentinel indicating no finish yet.
- **Sorting:** `CalculateRanks()` is called after each finish. Players are sorted by best finish time (lower = better).
- There is no fraglimit. The match runs until timelimit or indefinitely.
- Split times are recorded per intermediate checkpoint in the `current_race[]` array. When a new personal best is achieved, the split times are copied to `best_race[]`.

## Checkpoint System

Race maps use `race_point` entities linked in a chain via the standard Quake target/targetname system:

| Checkpoint Type | Has `target` | Has `targetname` | Description |
|---|---|---|---|
| **Start** | Yes | No | Beginning of the course. Touching it starts/restarts the timer. |
| **Intermediate** | Yes | Yes | Mid-course checkpoint. Records a split time and advances to the next point. |
| **Finish** | No | Yes | End of the course. Touching it records the finish time. |

### Chain Linking

Checkpoints form a singly-linked list:
- Start's `target` points to the first intermediate (or directly to finish).
- Each intermediate's `target` points to the next checkpoint.
- Each intermediate's `targetname` is referenced by the previous checkpoint's `target`.
- Finish has a `targetname` but no `target` (end of chain).

### Checkpoint Highlighting

The `s.generic1` field on each checkpoint entity controls client-side highlighting:
- `s.generic1 = 1`: This checkpoint is the player's next target (highlighted/glowing on client).
- `s.generic1 = 0`: Not the active target.

Only one checkpoint is highlighted at a time. When a player touches a checkpoint, the highlight advances to the next one in the chain.

### Visual Models

Each checkpoint type uses a distinct flag model (binary-verified):

| Type | Model |
|---|---|
| Start | `models/flag3/g_flag3.md3` (green flag) |
| Intermediate | `models/flag3/d_flag3.md3` (default flag) |
| Finish | `models/flag3/f_flag3.md3` (finish flag) |

Checkpoints are broadcast to all clients (`SVF_BROADCAST`) regardless of PVS.

## Spawn Behavior

- On `ClientBegin`, race state is fully cleared via `memset(&client->race, 0, ...)`.
- Score is initialized to `0x7FFFFFFF` (sentinel for "no finish yet").
- `Race_ResetCheckpoints()` is called to clear racing state and highlight the first checkpoint.
- A `race_init` server command is sent to the client.
- `ClientSpawn()` is called to actually place the player in the world.
- Standard spawn health/weapons apply.

## raceInfo_t Structure

Stored per-client at `gclient_t.race` (552 bytes):

| Field | Type | Description |
|---|---|---|
| `racingActive` | qboolean | Whether the player is currently in a timed run. |
| `startTime` | int | `level.time` when the player touched the start checkpoint. |
| `lastTime` | int | Finish time of the most recent completed run (milliseconds). |
| `best_race[64]` | int[] | Split times from the personal best run. |
| `current_race[64]` | int[] | Split times from the current run. |
| `currentCheckPoint` | int | Index of the next intermediate checkpoint (0-based). |
| `weaponUsed` | qboolean | Whether a weapon was used during the run. |
| `nextRacePoint` | gentity_t* | Pointer to the next checkpoint entity the player must touch. |
| `nextRacePoint2` | gentity_t* | Two-level lookahead: points to checkpoint after next (for client pre-rendering). |

## Key Functions

| Function | File | Description |
|---|---|---|
| `SP_race_point` | g_gametype_race.c | Spawn function for `race_point` entities. Only active in GT_RACE. Sets bounding box (-40,-40,-15 to 40,40,128), assigns touch handler, registers visual model, links entity. Max 64 race points. |
| `FinishSpawningRacePoint` | g_gametype_race.c | Think function called one frame after spawn. Drops checkpoint to ground (unless spawnflag 1 = suspended), quantizes origin to integer coordinates +1 unit above ground. |
| `Touch_RaceCheckpoint` | g_gametype_race.c | Touch handler for all three checkpoint types. Routes to start/intermediate/finish logic based on presence of `target`/`targetname` fields. Fires EV_RACE_START, EV_RACE_CHECKPOINT, or EV_RACE_FINISH events. |
| `ClientBegin_Race` | g_gametype_race.c | Race-specific client begin. Clears race state, resets checkpoints, sends `race_init`, calls `ClientSpawn`. |
| `Race_ResetCheckpoints` | g_gametype_race.c | Resets a player's race state. Finds the start checkpoint (has target, no targetname), then highlights the first intermediate checkpoint. Clears all other checkpoint highlights. |
| `Race_GetNumPoints` | g_gametype_race.c | Returns the total number of race points spawned on the map. |
| `Cmd_RaceInit_f` | g_gametype_race.c | Client command `raceinit`: resets race state and highlights first checkpoint. |
| `Cmd_RacePoint_f` | g_gametype_race.c | Client command `racepoint list`: lists all race point entities with type and position. |

## Events

| Event | Fields | Description |
|---|---|---|
| `EV_RACE_START` (0x5D) | clientNum, otherEntityNum=startTime, otherEntityNum2=firstCheckpointEntity, eventParm=totalCheckpoints | Broadcast when a player touches the start checkpoint. |
| `EV_RACE_CHECKPOINT` (0x5E) | clientNum, time=splitDelta, otherEntityNum=nextCheckpoint, otherEntityNum2=nextNextCheckpoint, generic1=checkpointIndex | Broadcast when a player touches an intermediate checkpoint. `time` is delta from personal best (0 if no best). Two-level lookahead entity numbers in otherEntityNum/otherEntityNum2. |
| `EV_RACE_FINISH` (0x5F) | clientNum, time=finishTime, powerups=1 (valid finish) | Broadcast when a player touches the finish checkpoint. |

## Limits

| Constant | Value | Description |
|---|---|---|
| `MAX_RACE_POINTS` | 64 | Maximum number of `race_point` entities on a map. Excess entities are freed. |
| `MAX_RACE_CHECKPOINTS` | 64 | Maximum checkpoint splits stored per run. |

## QL vs Q3 Differences

- **Entirely new gametype.** Quake 3 had no race mode. GT_RACE (index 2) replaced GT_SINGLE_PLAYER, which was unused in multiplayer.
- **Checkpoint entity type:** `race_point` is a new entity class not present in Q3.
- **Time-based scoring:** Unlike all other gametypes, Race scores by completion time rather than frags or captures.
- **Race events:** EV_RACE_START, EV_RACE_CHECKPOINT, and EV_RACE_FINISH are new event types added to the entity event enum.
- **Per-client race state:** The `raceInfo_t` struct (552 bytes per client) is entirely new, tracking timing, checkpoints, split times, and personal bests.
- **No fragging:** Race mode is non-combat. There is no fraglimit and kills do not affect the score.
- **Start rate limiting:** The start checkpoint has a 1-second cooldown to prevent rapid restarts when a player is actively racing.
