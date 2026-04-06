# Harvester (GT_HARVESTER = 8)

## Overview

Harvester is a team-based objective mode. A neutral skull generator (obelisk) spawns skulls at the center of the map. Players collect skulls dropped by killed opponents and deliver them to the enemy team's obelisk to score. The game uses capturelimit/timelimit exit rules. There is no round state machine.

## Source Files

- **Primary:** `code/game/g_gametype_har.c` -- entity validation and item registration (35 lines)
- **Skull/scoring logic:** `code/game/g_team.c` -- cube pickup, carrier tracking, frag bonuses
- **Bot AI:** `code/game/ai_dmnet.c` -- `BotHarvesterCarryingCubes()`, `BotGoHarvest()`, LTG_HARVEST/LTG_RUSHBASE
- **Scoreboard:** `CTFScoreboardMessage()` in `code/game/g_cmds.c` (shared CTF format)

## Map Entities

| Entity | Description |
|---|---|
| `team_redobelisk` | Red team's base obelisk (delivery target for blue team) |
| `team_blueobelisk` | Blue team's base obelisk (delivery target for red team) |
| `team_neutralobelisk` | Central skull generator |

All three entities are required. `Harvester_CheckTeamItems()` prints warnings if any are missing.

## Registered Items

- `Red Cube` -- picked up by blue team players
- `Blue Cube` -- picked up by red team players

These are registered via `Harvester_RegisterItems()` at game init.

## Cvars

| Cvar | Default | Description |
|---|---|---|
| `capturelimit` | varies | Score needed for a team to win |
| `timelimit` | varies | Time limit in minutes |

Harvester inherits the standard team game cvars. No gametype-specific cvars exist.

## Scoring

- **Skull pickup:** When a player is killed, skulls drop. Enemy team members pick up opponent-colored cubes. Each pickup increments `client->ps.generic1` (skull count carried).
- **Delivery:** Delivering skulls to the enemy obelisk awards score. The number of skulls carried determines the capture value.
- **Carrier frag bonus:** Killing an enemy carrying skulls awards `CTF_FRAG_CARRIER_BONUS * tokens * tokens` (quadratic scaling with skull count).
- **Proximity bonuses:** Standard CTF-style carrier defense/recovery bonuses from `g_team.c`.
- **Win condition:** First team to reach `capturelimit`, or highest score when `timelimit` expires.

## Spawn Behavior

Standard team spawn logic. No special spawn modifications. Players can carry skulls across lives -- skulls drop on death for anyone to pick up.

## Key Functions

| Function | Description |
|---|---|
| `Harvester_CheckTeamItems()` | Validates all 3 obelisk entities exist in the map |
| `Harvester_RegisterItems()` | Pre-caches Red Cube and Blue Cube items |

The bulk of the Harvester logic lives in `g_team.c`:

| Function | Description |
|---|---|
| `Team_FragBonuses()` | Awards carrier frag, defense, and assist bonuses (GT_HARVESTER path) |
| `Team_CheckHurtCarrier()` | Tracks attackers who damaged a skull carrier |
| `Pickup_Team()` | GT_HARVESTER branch: increments `generic1` skull count on cube pickup |

## QL vs Q3 Differences

Harvester existed in Quake 3: Team Arena (Q3TA) and was carried forward into Quake Live largely unchanged. The core mechanic -- collect skulls from kills, deliver to enemy base -- is the same. Key differences:

- **Scoreboard format:** Uses the shared `CTFScoreboardMessage` format rather than a dedicated Harvester format.
- **Item registration:** Explicitly registers cubes at game init via `Harvester_RegisterItems()`.
- **Entity validation:** `Harvester_CheckTeamItems()` provides startup warnings for missing obelisks, which Q3TA did not do.
- **No separate game module:** In Q3TA, Harvester was part of the missionpack; in QL it is integrated into the base game module.
