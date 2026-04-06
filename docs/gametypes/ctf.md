# Capture The Flag (GT_CTF = 5)

## Overview

Capture The Flag is an objective-based team mode. Each team has a flag at their base. Players must steal the enemy flag and return it to their own base (where their flag must be present) to score a capture. The game ends when a team reaches the capturelimit or the timelimit expires.

CTF is a continuous (non-round-based) team game. Dead players respawn normally.

## Source Files

- **Primary**: `code/game/g_gametype_ctf.c` (team item validation, scoreboards)
- **Flag logic**: `code/game/g_team.c` (Team_TouchOurFlag, Team_TouchEnemyFlag, Team_ReturnFlag, Team_FragBonuses, Team_DroppedFlagThink)
- **Bonus constants**: `code/game/g_team.h`
- **Flag status enum**: `code/qcommon/q_shared.h` (flagStatus_t)
- **Binary addresses** (qagamex86.dll build 1069):
  - `CTFScoreboardMessage` -- `0x1003e8c0`
  - `CTFScoreboardMessage_impl` -- `0x1003e700`

## Cvars

| Cvar | Default | Flags | Description |
|------|---------|-------|-------------|
| `capturelimit` | `8` | SERVERINFO, ARCHIVE, NORESTART | Number of captures to win. |
| `g_timelimit` | varies | SERVERINFO | Match time limit in minutes. |
| `mercylimit` | `0` | SERVERINFO, ARCHIVE, NORESTART | Score difference for early match end. |

## Map Entities

CTF requires two flag entities placed in the map:

| Entity | Description |
|--------|-------------|
| `team_CTF_redflag` | Red team's flag base position |
| `team_CTF_blueflag` | Blue team's flag base position |

`CTF_CheckTeamItems()` is called at map load to verify both flags exist. A warning is printed if either is missing.

## Flag States

Flags track their status via `flagStatus_t` (from `q_shared.h`):

| State | Value | Description |
|-------|-------|-------------|
| `FLAG_ATBASE` | 0 | Flag is at its home base |
| `FLAG_TAKEN` | 1 | Flag is being carried by an enemy |
| `FLAG_DROPPED` | 4 | Flag was dropped on the ground (carrier died) |

Flag status is communicated to clients via configstrings and the `Team_SetFlagStatus()` function.

## Scoring

### Team Score
- **+1 team score** per capture (via `AddTeamScore()`).

### Individual Score Bonuses

All bonus values are defined in `g_team.h`:

| Action | Bonus | Constant |
|--------|-------|----------|
| Capturing the flag | +100 | `CTF_CAPTURE_BONUS` |
| Teammate on capturing team | +25 | `CTF_TEAM_BONUS` |
| Picking up enemy flag | +10 | `CTF_FLAG_BONUS` |
| Returning own dropped flag | +10 | `CTF_RECOVERY_BONUS` |
| Fragging enemy flag carrier | +20 | `CTF_FRAG_CARRIER_BONUS` |
| Protecting flag carrier (frag someone who hurt carrier within 8s) | +5 | `CTF_CARRIER_DANGER_PROTECT_BONUS` |
| Flag defense / carrier defense (frag near flag or carrier, 1000u radius) | +10 | `CTF_FLAG_DEFENSE_BONUS` |
| Return-assist (returned flag, then teammate captures within 10s) | +10 | `CTF_RETURN_FLAG_ASSIST_BONUS` |
| Frag-assist (fragged carrier, then teammate captures within 10s) | +10 | `CTF_FRAG_CARRIER_ASSIST_BONUS` |

### Awards
- **PERS_CAPTURES**: incremented on capture, shown on scoreboard.
- **PERS_DEFEND_COUNT**: incremented for flag/carrier defense frags.
- **PERS_ASSIST_COUNT**: incremented for return-assists and frag-carrier-assists.
- **EF_AWARD_CAP**: sprite shown over player's head on capture.
- **EF_AWARD_DEFEND**: sprite for defense frags.
- **EF_AWARD_ASSIST**: sprite for assists.

## Flag Mechanics

### Picking Up Enemy Flag (Team_TouchEnemyFlag)
- The carrier receives `PW_REDFLAG` or `PW_BLUEFLAG` powerup set to `INT_MAX` (flags never expire).
- Flag status changes to `FLAG_TAKEN`.
- `CTF_FLAG_BONUS` (+10) is awarded.

### Capturing (Team_TouchOurFlag)
- Carrier must touch their own flag while it is at base (`FLAG_ATBASE`).
- Carrier must be holding the enemy flag powerup.
- On capture: enemy flag powerup is cleared, team score +1, `CTF_CAPTURE_BONUS` (+100).
- All flags are reset to base via `Team_ResetFlags()`.
- All teammates receive `CTF_TEAM_BONUS` (+25).
- Assist bonuses are checked for teammates who recently returned the flag or fragged the enemy carrier.
- `Team_ForceGesture()` makes all capturing team members taunt.

### Returning Own Flag (Team_TouchOurFlag -- dropped flag case)
- If a player touches their own team's dropped flag, it is returned to base.
- `CTF_RECOVERY_BONUS` (+10) is awarded.
- Return sound is played.

### Dropped Flag (Team_DroppedFlagThink)
- When a flag carrier dies, the flag is dropped at their position.
- Flag status changes to `FLAG_DROPPED`.
- After `CTF_FLAG_RETURN_TIME` (40 seconds), the flag auto-returns to base.

### Defense Bonuses (Team_FragBonuses)
- Fragging an enemy near your own flag (within `CTF_TARGET_PROTECT_RADIUS` = 1000 units) or near your flag carrier awards defense bonuses.
- Fragging someone who recently hurt your carrier (within `CTF_CARRIER_DANGER_PROTECT_TIMEOUT` = 8 seconds) awards `CTF_CARRIER_DANGER_PROTECT_BONUS` (+5).

## Spawn Behavior

Standard team deathmatch spawning. Dead players respawn normally at team spawn points. No round-based restrictions.

## Key Functions

| Function | Description |
|----------|-------------|
| `CTF_CheckTeamItems()` | Validates that both red and blue flags exist in the map. |
| `CTFScoreboardMessage()` | Builds and sends the `scores_ctf` command with 17 team stat categories per team and 17 fields per player. |
| `CTFScoreboardMessage_impl()` | Sends detailed per-player weapon stats via `castats` command: damageDone, damageTaken, plus per-weapon accuracy and kills (15 weapons). |
| `Team_TouchOurFlag()` | Handles touching own flag: return if dropped, capture if carrying enemy flag. (In `g_team.c`.) |
| `Team_TouchEnemyFlag()` | Handles picking up enemy flag. (In `g_team.c`.) |
| `Team_ReturnFlag()` | Resets a flag to base and plays return sound. (In `g_team.c`.) |
| `Team_FragBonuses()` | Awards defense, carrier-protect, and assist bonuses on frags. (In `g_team.c`.) |
| `Team_DroppedFlagThink()` | Auto-return timer for dropped flags. (In `g_team.c`.) |
| `Pickup_Team()` | Dispatch for touching team items: routes to TouchOurFlag or TouchEnemyFlag. (In `g_team.c`.) |

## Scoreboard

### Main Scoreboard (scores_ctf)

The header contains 34 team stat values (17 categories per team) plus 3 metadata values:

**17 per-team categories**: Red Armor pickups, Yellow Armor pickups, Green Armor pickups, Mega Health pickups, Quad Damage pickups, Battle Suit pickups, Regeneration pickups, Haste pickups, Invisibility pickups, Flag pickups, Medkit pickups, Quad possession time, Battle Suit possession time, Regeneration possession time, Haste possession time, Invisibility possession time, Flag possession time.

These are sent as: `redStats[0..16] blueStats[0..16] numPlayers redScore blueScore`.

Opponent team stats are zeroed out during gameplay (only visible during intermission).

**17 fields per player**: `client team score ping time kills deaths accuracy bestWeapon impressive excellent gauntlet defend assist captures perfect alive`

### Detail Stats (castats)

Sent per-player via `CTFScoreboardMessage_impl()`: `castats <playerIndex> <damageDone> <damageTaken> [<weapAcc> <weapKills>] * 15`

## QL vs Q3 Differences

- **GT_CTF index changed**: CTF was GT_CTF=4 in Q3. In QL it is GT_CTF=5 (shifted by the insertion of GT_CA=4).
- **Scoreboard format**: Q3 used a simpler `scores` command. QL uses `scores_ctf` with 34 team-level stat categories (item pickups, powerup possession times, flag possession times) that Q3 did not track.
- **Detail stats**: The `castats` command with per-weapon accuracy and kill breakdowns is QL-specific.
- **Stat tracking**: QL tracks extended stats (numRedArmorPickups, flagPossessionTime, etc.) at the level struct level. Q3 had no equivalent.
- **mercylimit**: QL adds the mercylimit cvar for early match termination by score difference, which Q3 CTF did not have.
- **Flag possession time**: QL tracks per-team flag possession time for the scoreboard. Q3 did not.
- **Viewer privacy**: QL hides opponent team stats on the scoreboard during gameplay (only shown at intermission). Q3 showed all stats to everyone.
