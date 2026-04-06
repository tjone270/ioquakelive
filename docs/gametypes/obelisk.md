# Overload (GT_OBELISK = 7)

## Overview

Overload is an objective-based team mode where each team has a destructible obelisk at their base. Teams must attack and destroy the enemy obelisk to score. The obelisk regenerates health over time and fully respawns after being destroyed. The game ends when a team reaches the capturelimit or the timelimit expires.

Overload is a continuous (non-round-based) team game. Dead players respawn normally.

## Source Files

- **Primary**: `code/game/g_gametype_obelisk.c` (team item validation only -- 26 lines)
- **Obelisk logic**: `code/game/g_team.c` (ObeliskDie, ObeliskPain, ObeliskRegen, ObeliskRespawn, ObeliskInit, SpawnObelisk, SP_team_redobelisk, SP_team_blueobelisk)
- **Defense bonuses**: `code/game/g_team.c` (Team_FragBonuses -- obelisk defense path)
- **Bonus constants**: `code/game/g_team.h`

## Cvars

| Cvar | Default | Flags | Description |
|------|---------|-------|-------------|
| `capturelimit` | `8` | SERVERINFO, ARCHIVE, NORESTART | Number of obelisk destructions to win. |
| `g_timelimit` | varies | SERVERINFO | Match time limit in minutes. |
| `mercylimit` | `0` | SERVERINFO, ARCHIVE, NORESTART | Score difference for early match end. |
| `g_obeliskHealth` | `2500` | -- | Starting and maximum health of each obelisk. |
| `g_obeliskRegenPeriod` | `1` | -- | Time in seconds between regeneration ticks. |
| `g_obeliskRegenAmount` | `15` | -- | Health restored per regeneration tick. |
| `g_obeliskRespawnDelay` | `10` | SERVERINFO | Time in seconds before a destroyed obelisk respawns. |

## Map Entities

Overload requires two obelisk entities:

| Entity | Description |
|--------|-------------|
| `team_redobelisk` | Red team's obelisk (blue team attacks this) |
| `team_blueobelisk` | Blue team's obelisk (red team attacks this) |

`Obelisk_CheckTeamItems()` searches for both entities at map load and prints warnings if either is missing.

### Obelisk Entity Structure

Each map-placed obelisk entity (`SP_team_redobelisk` / `SP_team_blueobelisk`) spawns two linked entities:

1. **Visual entity** (the map entity itself): `ET_TEAM` type, linked to renderer. `s.modelindex` stores the team (TEAM_RED or TEAM_BLUE). `s.modelindex2` stores normalized health (0-255). `s.frame` stores visual state (0=normal, 1=under attack, 2=exploding).
2. **Damage entity** (spawned by `SpawnObelisk()`): `ET_GENERAL` type, solid (`CONTENTS_SOLID`), takes damage. Has `die`, `pain`, and `think` callbacks. `activator` points back to the visual entity. `spawnflags` stores the team.

The visual entity's bounding box is (-15, -15, 0) to (15, 15, 87). `ObeliskInit()` handles drop-to-floor and suspended placement.

## Obelisk Lifecycle

### Health and Regeneration (ObeliskRegen)

- Obelisks regenerate health periodically via the `think` callback.
- Every `g_obeliskRegenPeriod` seconds (default: 1), `g_obeliskRegenAmount` HP (default: 15) is restored.
- Health is capped at `g_obeliskHealth` (default: 2500).
- Regeneration fires `EV_POWERUP_REGEN` event for visual feedback.
- The visual entity's `s.modelindex2` is updated to `health * 0xff / g_obeliskHealth` (normalized 0-255 for client rendering).
- During regeneration, `s.frame` is reset to 0 (normal visual state).

### Taking Damage (ObeliskPain)

- When the obelisk takes damage, `ObeliskPain()` is called.
- Actual score awarded to the attacker is `damage / 10` (minimum 1) via `AddScore()`.
- `s.modelindex2` is updated to reflect current health.
- If the obelisk was not already in the "under attack" visual state (`s.frame == 0`), `EV_OBELISKPAIN` event is fired.
- `s.frame` is set to 1 (under attack).

### Destruction (ObeliskDie)

When obelisk health reaches 0:
1. The attacking team (the team opposite the obelisk's owner) gets **+1 team score** via `AddTeamScore()`.
2. `Team_ForceGesture()` makes the scoring team taunt.
3. `CalculateRanks()` is called.
4. The obelisk stops taking damage (`takedamage = qfalse`).
5. Respawn is scheduled after `g_obeliskRespawnDelay` seconds (default: 10).
6. `s.modelindex2` is set to 0xff and `s.frame` to 2 (exploding visual state).
7. `EV_OBELISKEXPLODE` event is fired on the visual entity.
8. The attacker who dealt the killing blow receives `CTF_CAPTURE_BONUS` (+100) individual score.
9. The attacker gets `EF_AWARD_CAP` sprite and `PERS_CAPTURES` is incremented.
10. Both teams' obelisk-attacked timestamps are cleared.

### Respawn (ObeliskRespawn)

After `g_obeliskRespawnDelay` seconds:
- `takedamage` is re-enabled.
- Health is restored to `g_obeliskHealth`.
- Regeneration timer restarts.
- `s.frame` is reset to 0 (normal visual state).

## Scoring

### Team Score
- **+1 team score** per obelisk destruction.

### Individual Score
- **+damage/10** (min 1) for each damage tick dealt to the enemy obelisk (awarded in `ObeliskPain()`).
- **+100** (`CTF_CAPTURE_BONUS`) for dealing the killing blow that destroys the obelisk.

### Awards
- **PERS_CAPTURES**: incremented when the attacker destroys the obelisk.
- **EF_AWARD_CAP**: sprite shown over the player who destroys the obelisk.
- **PERS_DEFEND_COUNT**: incremented for defense frags near your own obelisk (via `Team_FragBonuses()`).

## Defense Bonuses (Team_FragBonuses)

In Overload, `Team_FragBonuses()` uses the team obelisk entity instead of a flag for defense calculations:
- Red team defenders are checked against `team_redobelisk`.
- Blue team defenders are checked against `team_blueobelisk`.
- Frags within `CTF_TARGET_PROTECT_RADIUS` (1000 units) of the obelisk or within `CTF_ATTACKER_PROTECT_RADIUS` (1000 units) award `CTF_FLAG_DEFENSE_BONUS` (+10) and increment `PERS_DEFEND_COUNT`.
- The carrier-related bonuses (carrier protect, carrier danger protect) do not apply in Overload since there is no flag carrier.

## Spawn Behavior

Standard team deathmatch spawning. Dead players respawn normally at team spawn points. No round-based restrictions. The obelisk has `FL_NO_KNOCKBACK` to prevent displacement.

## Key Functions

| Function | Description |
|----------|-------------|
| `Obelisk_CheckTeamItems()` | Validates that both `team_redobelisk` and `team_blueobelisk` exist in the map. |
| `ObeliskRegen()` | Think callback: regenerates obelisk health by `g_obeliskRegenAmount` every `g_obeliskRegenPeriod` seconds. |
| `ObeliskPain()` | Pain callback: updates health display, fires pain event, awards damage/10 score to attacker. |
| `ObeliskDie()` | Die callback: awards team score +1, schedules respawn, fires explosion event, awards capture bonus. |
| `ObeliskRespawn()` | Think callback after destruction: restores health, re-enables damage, restarts regen timer. |
| `SpawnObelisk()` | Creates the invisible damage-taking entity linked to the visual obelisk. Configures callbacks and health. |
| `ObeliskInit()` | Sets up the visual obelisk entity: ET_TEAM type, bounding box, drop-to-floor logic. |
| `SP_team_redobelisk()` | Map entity spawn function for the red obelisk. |
| `SP_team_blueobelisk()` | Map entity spawn function for the blue obelisk. |
| `Team_FragBonuses()` | Awards defense bonuses for frags near the team obelisk (Overload path). (In `g_team.c`.) |

## Scoreboard

Overload uses `DeathmatchScoreboardMessage` (the FFA/TDM format) as a fallback. It does not have a dedicated `scores_obelisk` command.

## Entity Communication to Client

The visual obelisk entity communicates state to the client renderer via:

| Field | Purpose |
|-------|---------|
| `s.eType` | `ET_TEAM` -- identifies this as a team objective entity |
| `s.modelindex` | Team number (TEAM_RED=1 or TEAM_BLUE=2) |
| `s.modelindex2` | Normalized health: `health * 0xff / g_obeliskHealth` (0-255 range) |
| `s.frame` | Visual state: 0=normal, 1=under attack (pain), 2=exploding (destroyed) |

## QL vs Q3 Differences

- **GT_OBELISK index changed**: Overload was GT_OBELISK=6 in Q3 Team Arena. In QL it is GT_OBELISK=7 (shifted by GT_CA and GT_CTF reordering).
- **Q3 Team Arena origin**: Overload was introduced in Quake 3 Team Arena. The core obelisk mechanics (health, regen, destruction, respawn) are inherited from Team Arena.
- **Configurable obelisk stats**: The `g_obeliskHealth`, `g_obeliskRegenPeriod`, `g_obeliskRegenAmount`, and `g_obeliskRespawnDelay` cvars existed in Q3 Team Arena with the same defaults. QL preserves these.
- **Obelisk attack sound timer**: `OVERLOAD_ATTACK_BASE_SOUND_TIME` (20000ms) is defined in `g_team.h` for attack announcement pacing, inherited from Q3.
- **mercylimit**: QL adds the mercylimit cvar for early match termination. Q3 Team Arena had no equivalent.
- **Scoreboard**: Q3 used a simple Team Arena scoreboard. QL falls back to the deathmatch scoreboard format rather than implementing a dedicated obelisk scoreboard.
- **Harvester reuse**: The `SpawnObelisk()` function and obelisk entity infrastructure are shared between GT_OBELISK and GT_HARVESTER. In Overload the entity is solid and takes damage; in Harvester the same entity is a trigger volume for skull delivery (`ObeliskTouch`). This dual-use pattern is inherited from Q3 Team Arena.
