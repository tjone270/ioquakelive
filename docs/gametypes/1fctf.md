# One Flag CTF (GT_1FCTF = 6)

## Overview

One Flag CTF is an objective-based team mode with a single neutral flag. Both teams compete to pick up the neutral flag from the center of the map and carry it to the enemy base to score. The game ends when a team reaches the capturelimit or the timelimit expires.

1FCTF is a continuous (non-round-based) team game. Dead players respawn normally.

## Source Files

- **Primary**: `code/game/g_gametype_1fctf.c` (team item validation only -- 32 lines)
- **Flag logic**: `code/game/g_team.c` (Team_TouchOurFlag, Team_TouchEnemyFlag, Team_FragBonuses, Team_DroppedFlagThink, Pickup_Team)
- **Bonus constants**: `code/game/g_team.h`
- **Flag status enum**: `code/qcommon/q_shared.h` (flagStatus_t)
- **Scoreboard**: Uses `CTFScoreboardMessage` from `code/game/g_gametype_ctf.c` (shared with CTF)

## Cvars

| Cvar | Default | Flags | Description |
|------|---------|-------|-------------|
| `capturelimit` | `8` | SERVERINFO, ARCHIVE, NORESTART | Number of captures to win. |
| `g_timelimit` | varies | SERVERINFO | Match time limit in minutes. |
| `mercylimit` | `0` | SERVERINFO, ARCHIVE, NORESTART | Score difference for early match end. |

## Map Entities

1FCTF requires three flag entities:

| Entity | Description |
|--------|-------------|
| `team_CTF_redflag` | Red team's base (capture destination for blue carriers) |
| `team_CTF_blueflag` | Blue team's base (capture destination for red carriers) |
| `team_CTF_neutralflag` | Neutral flag spawn point (center of map) |

Additionally, the neutral obelisk entity is used for the neutral flag visual:

| Entity | Description |
|--------|-------------|
| `team_neutralobelisk` | Neutral base marker (spawned in `SP_team_neutralobelisk`, active for GT_1FCTF and GT_HARVESTER) |

`OneFCTF_CheckTeamItems()` validates all three flags exist at map load and prints warnings for any missing.

## Flag States

The neutral flag uses extended flag status values (from `q_shared.h`):

| State | Value | Description |
|-------|-------|-------------|
| `FLAG_ATBASE` | 0 | Neutral flag is at the center spawn point |
| `FLAG_TAKEN_RED` | 2 | Red team player is carrying the flag |
| `FLAG_TAKEN_BLUE` | 3 | Blue team player is carrying the flag |
| `FLAG_DROPPED` | 4 | Flag was dropped (carrier died) |

Note: `FLAG_TAKEN` (1) is used by standard CTF. 1FCTF uses `FLAG_TAKEN_RED` and `FLAG_TAKEN_BLUE` to distinguish which team has the flag.

## Scoring

### Team Score
- **+1 team score** per capture (via `AddTeamScore()`).

### Individual Score Bonuses

The same constants from `g_team.h` apply:

| Action | Bonus | Constant |
|--------|-------|----------|
| Capturing the flag | +100 | `CTF_CAPTURE_BONUS` |
| Teammate on capturing team | +25 | `CTF_TEAM_BONUS` |
| Picking up neutral flag | +10 | `CTF_FLAG_BONUS` |
| Fragging enemy flag carrier | +20 | `CTF_FRAG_CARRIER_BONUS` |
| Return-assist | +10 | `CTF_RETURN_FLAG_ASSIST_BONUS` |
| Frag-assist | +10 | `CTF_FRAG_CARRIER_ASSIST_BONUS` |
| Carrier danger protect | +5 | `CTF_CARRIER_DANGER_PROTECT_BONUS` |

### Awards
- **PERS_CAPTURES**: incremented on capture.
- **PERS_DEFEND_COUNT**: incremented for defense frags near own base.
- **PERS_ASSIST_COUNT**: incremented for assists.

## Flag Mechanics

### Picking Up the Neutral Flag (Team_TouchEnemyFlag)

When a player touches the neutral flag (`team_CTF_neutralflag`), `Pickup_Team()` routes it through 1FCTF-specific logic:
- Since the flag is `TEAM_FREE`, `Pickup_Team` calls `Team_TouchEnemyFlag(ent, other, cl->sess.sessionTeam)`.
- The carrier receives `PW_NEUTRALFLAG` powerup set to `INT_MAX`.
- Flag status is set to `FLAG_TAKEN_RED` or `FLAG_TAKEN_BLUE` depending on the carrier's team.
- `CTF_FLAG_BONUS` (+10) is awarded.

### Capturing (Team_TouchOurFlag)

When the carrier touches the enemy base flag:
- `Pickup_Team` detects `team != cl->sess.sessionTeam` and calls `Team_TouchOurFlag(ent, other, cl->sess.sessionTeam)`.
- Inside `Team_TouchOurFlag`, the 1FCTF path sets `enemy_flag = PW_NEUTRALFLAG`.
- Unlike standard CTF, the dropped-flag return path is skipped for 1FCTF (the neutral flag cannot be "returned" by touching it at a base).
- The player must hold `PW_NEUTRALFLAG` to capture.
- On capture: team score +1, `CTF_CAPTURE_BONUS` (+100), all flags reset.
- `Team_ForceGesture()` makes the capturing team taunt.

### Dropped Flag (Team_DroppedFlagThink)

- When the carrier dies, the neutral flag is dropped.
- Flag status changes to `FLAG_DROPPED`.
- After `CTF_FLAG_RETURN_TIME` (40 seconds), the flag auto-returns to the center.
- Any player from either team can pick up a dropped neutral flag.

### Defense Bonuses (Team_FragBonuses)

In 1FCTF, `Team_FragBonuses` sets both `flag_pw` and `enemy_flag_pw` to `PW_NEUTRALFLAG`. Defense and assist bonus logic otherwise works the same as CTF.

## Spawn Behavior

Standard team deathmatch spawning. Dead players respawn normally at team spawn points. No round-based restrictions.

## Key Functions

| Function | Description |
|----------|-------------|
| `OneFCTF_CheckTeamItems()` | Validates that red flag, blue flag, and neutral flag entities all exist. |
| `CTFScoreboardMessage()` | Shared with CTF. Sends `scores_ctf` command. |
| `CTFScoreboardMessage_impl()` | Shared with CTF. Sends `castats` per-player weapon detail. |
| `Pickup_Team()` | Routes neutral flag touches: TEAM_FREE flag goes to `Team_TouchEnemyFlag`; enemy base flag goes to `Team_TouchOurFlag`. (In `g_team.c`.) |
| `Team_TouchEnemyFlag()` | Handles neutral flag pickup. 1FCTF path uses `PW_NEUTRALFLAG` and `FLAG_TAKEN_RED`/`FLAG_TAKEN_BLUE`. (In `g_team.c`.) |
| `Team_TouchOurFlag()` | Handles capture at enemy base. 1FCTF path skips flag-return logic. (In `g_team.c`.) |
| `Team_FragBonuses()` | Awards defense and assist bonuses. 1FCTF sets both flag powerup references to `PW_NEUTRALFLAG`. (In `g_team.c`.) |

## Scoreboard

1FCTF shares the CTF scoreboard format (`scores_ctf` and `castats` commands). See the CTF documentation for the full field breakdown.

## QL vs Q3 Differences

- **GT_1FCTF index changed**: 1FCTF was GT_1FCTF=6 in Q3 (Team Arena). In QL it remains GT_1FCTF=6 but other gametype indices shifted around it (GT_CA=4 was inserted, GT_CTF moved to 5).
- **Q3 Team Arena origin**: 1FCTF was introduced in Quake 3 Team Arena, not the original Q3A. The core flag logic is largely inherited from Team Arena.
- **Scoreboard**: Q3 Team Arena used a simpler scoreboard. QL uses the same extended `scores_ctf` format as CTF with 34 team stat categories and `castats` detail commands.
- **Stat tracking**: QL tracks extended statistics (item pickups, powerup possession times, flag possession time) that Q3 Team Arena did not.
- **mercylimit**: QL adds the mercylimit cvar. Q3 Team Arena had no equivalent.
- **Neutral obelisk entity**: In Q3, the `team_neutralobelisk` entity was shared between 1FCTF and Harvester but served no visual purpose in 1FCTF. QL preserves this behavior.
