# ioquakelive

An implementation of the **Quake Live** game, built on the open-source [ioquake3](https://ioquake3.org/) engine. The goal is to produce a functionally equivalent open-source client/server to the final release of Quake Live (v1069).

#### Why?
It's really important to me that the Quake Live community prosper - it's such a great game; this is why I have put so much time and effort into working on this fork.

## Project Status

**Huge work in progress.** The codebase is being incrementally updated from ioquake3's baseline to match the behaviour/architecture of Quake Live.

### What works

- Quake Live clients can connect to the ioquakelive server.
- QL network protocol (custom netchan, entity/player state field tables, usercmd layout)
- QL game module (`qagame`) - 40%-50% complete-ish.
- QL UI module - new exports, ~78 new cvars, font/widescreen support
- QL cgame module - updated event system, physics, HUD owner-draws, scoreboard rendering
- Font rendering via FreeType (though might move to `stb_truetype` in future as this is what Quake Live uses)
- Widescreen coordinate system (4:3 content area with left/center/right bias)
- All base game types: Free for All, Duel, Team Deathmatch, Clan Arena, CTF, 1F CTF, Harvester, Freeze Tag, Domination, A&D, Race, Red Rover (most have basic implementation)

### What's required to run the game
You must have an original `pak00.pk3` file acquired from a legitimate purchased copy of [Quake Live](https://steampowered.com/app/282440) from Steam. ioquakelive will not run without a legitimate `pak00.pk3` file. Quake Live and its assets remain the property of *id Software*.

I do not have any rights to use any Quake Live game assets and cannot/**will not** redistribute these from my own legitimate copy.



### Key differences from upstream ioquake3

| Area | ioquake3 (Q3) | ioquakelive (QL) |
|------|---------------|-------------------|
| Protocol | Q3 1.32 / ioq3 71 | QL build 1069's protocol 91 |
| Base game dir | `baseq3` | `baseq3` filesystem, `baseqz` protocol name |
| Game types | FFA, Tourney, TDM, CTF | + CA, FT, Race, DOM, A&D, Harvester, 1FCTF, RR (and all TA modes) |
| Physics | Q3 air control | + CPM air control, double jump, crouch slide, auto-hop |
| Font system | Scale-based (textFont/bigFont) | fontIndex-based (extraFonts[3]) |
| Widescreen | Stretch only | 4:3 constrained with left/center/right modes for anchoring elements (compatible with QL `.menu` files) |
| Key/door system | N/A | IT_KEY items, key-locked func_door |
| Stats | Basic scoreboard | Detailed per-weapon accuracy, 'premium' scoreboards from QL |

### Implementation progress

#### Network protocol

| Area | Status | Notes |
|------|--------|-------|
| Netchan format | Done | GENCHECKSUM removed to match QL packet header |
| Client message parsing | Done | Extra byte after 3 header longs in `SV_ExecuteClientMessage` |
| entityStateFields | Done | 58 entries, reordered, gravity as int32 |
| playerStateFields | Done | 58 entries, `pm_flags` 24-bit, `weaponPrimary`, 9 new QL fields |
| usercmd_t deltas | Done | Offsets 21-25 reordered, 2 generic byte fields added |

#### Client engine

| Area | Status | Notes |
|------|--------|-------|
| CG_REGISTER_CVARS call | Done | `VM_Call` before `CG_INIT` in `cl_cgame.c` |
| Sound system | Pending | QL sound system changes not yet audited |
| Demo recording/playback/journalling | Pending | Not yet tested with QL protocol |
| Download system | Pending | HTTP redirect and pk3 downloads not yet audited |

#### Server engine

| Area | Status | Notes |
|------|--------|-------|
| Snapshot system | Done | Entity/player state serialisation matches QL |
| Bot management | Done | `sv_bot.c` functional |
| ZMQ stats/rcon | Pending | QL uses ZeroMQ for remote console and stats publishing |
| Server browser | Pending | Valve's Server Query Protocol planned |

#### cgame (client-side game module)

| Area | Status | Notes |
|------|--------|-------|
| Event system | Done | 100 `EV_` event handlers including race, infection, awards |
| Widescreen rendering | Done | `CG_AdjustFrom640` with `STRETCH`/`LEFT`/`CENTER`/`RIGHT` modes |
| Owner-draw text | Done | All ~70 owner-draw functions route through `CG_DrawText` |
| Serverinfo parsing | Done | 29 fields matching binary `CG_ParseServerinfo` |
| Scoreboard owner-draws | Done | Team scores, player counts, match state, round/overtime |
| Duel scoreboard | Done | Per-weapon stats for both players (frags/hits/shots/dmg/acc) |
| Voting display | Done | `ui_voteactive`, `ui_votestring`, end-of-match map voting |
| Weapon rendering | Done | All QL weapons including nailgun, chaingun, HMG, prox launcher |
| Grapple hook chain | Done | `RT_RAIL_CORE` rendering (single tiled quad) |
| Spectator tracking | Done | `cg_spectating` cvar follows `PM_SPECTATOR` transitions |
| Prediction/pmove | Done | 9 binary-verified fixes (freeze, dead float, hookEnemy, etc.) |
| Obituary feed | Done | Attacker/victim name rendering with weapon icons |
| Team overlay | Done | Scrolling spectator list, team info |
| Crosshair | Pending | QL crosshair set not fully verified |
| Awards/medals display | Pending | Rendering present but QL-specific award set not fully audited |
| Damage direction indicator | Pending | Not yet verified against binary |

#### UI (user interface module)

| Area | Status | Notes |
|------|--------|-------|
| New exports | Done | `UI_REGISTER_CVARS`, `UI_CHECK_ACTIVE_MENU`, `UI_WALK_MENUS`, `UI_DRAW_ADVERTISEMENT` |
| fontIndex support | Done | All 26+ DC call sites pass `item->fontIndex` |
| Cvars | Done | ~78 new cvars registered in `ui_main.c` |
| Menu file loading | Done | `pak01.pk3` override system for custom menus/other assets |
| Non-team scoreboards | Done | Implemented and fixed up visuals to ensure associated `.menu` files process accurately. |
| Server browser | Pending | Not yet adapted for QL master server protocol |
| Team scoreboards | Pending | Still a lot of stuff not correctly drawing here. |

#### Game module (server-side game logic)

| Area | Status | Notes |
|------|--------|-------|
| Game types | Done | FFA, Duel, TDM, CA, CTF, 1FCTF, Harvester, FT, DOM, A&D, Race, RR |
| Physics (bg_pmove) | Done | CPM air control, double jump, crouch slide, auto-hop, ladder move |
| Grapple hook | Done | Wall/enemy pull, close-range slowdown, water damping |
| Weapon definitions | Done | All QL weapons with correct reload times |
| Key/door system | Done | `IT_KEY` type, silver/gold/master keys, `func_door` spawnflags |
| Warmup state machine | Done | `g_gameState` cvar, ready percentage, forfeit logic |
| Starting health/armor | Done | `g_startingHealth`, `g_startingHealthBonus`, `g_startingArmor` |
| Voting system | Done | `VF_ENDMAP_VOTING`, `IntermissionVote`, `nextmaps` parsing |
| Serverinfo cvars | Done | 8 new `CVAR_SERVERINFO` cvars |
| Bot loading | Done | `G_LoadBots`, `G_LoadBotsFromFile`, `G_ParseInfos` |
| CVAR table | Done | ~390 cvars with 56 `OnChanged` callbacks |
| JSON stats reporting | Partial | Original uses C++ jsoncpp; JSON functions stubbed, non-JSON helpers preserved |
| Race checkpoints | Pending | Race gametype init/checkpoint logic not yet implemented |
| Loadout system | Pending | `g_loadout` cvar propagated but full loadout logic not audited |
| Factories | Pending | Similar to the JSON stats reporting point.
| Unlagged (`lagHax`)| Pending | Have to understand it first!
#### Renderer

| Area | Status | Notes |
|------|--------|-------|
| RT_RAIL_CORE | Done | Rendering case for grapple hook chain |
| QL-specific shaders | Pending | Shader keywords not fully audited |
| Additional render effects | Pending | Freeze/thaw effects, infection visuals not yet implemented |
| Post-processing (bloom, etc) | Pending | IDK what I'm doing when it comes to OpenGL so it's on the backburner |

## Building

### Windows (Visual Studio)

```
cd misc\msvc142
MSBuild ioquakelive.sln -p:Configuration=Debug -p:Platform=Win32
```

Individual projects: `cgame.vcxproj`, `ui.vcxproj`, `qagame.vcxproj`, `quakelive.vcxproj`, `opengl2.vcxproj`

**Toolset:** v145 (VS 2022+), **Platform:** Win32/x64

### Linux / macOS

Standard ioquake3 Makefile build - not yet tested with QL-specific changes.

## Directory Layout

```
code/
  cgame/          Client-side game module (HUD, scoreboard, effects, prediction)
  game/           Server-side game module (game logic, entities, physics)
  ui/             User interface module (menus, server browser)
  client/         Engine client code
  server/         Engine server code
  qcommon/        Shared engine code (networking, filesystem, commands)
  renderercommon/  Shared renderer code
  renderer_opengl2/ OpenGL 2 renderer
misc/
  msvc142/        Visual Studio solution and project files
pak01_content/    Asset overrides (menus, shaders, models) → pak01.pk3
```

## Ancestry

This project is a fork of [ioquake3](https://github.com/ioquake/ioq3) and retains its GPL v2 license. The original ioquake3 README is preserved below.

---

<details>
<summary>Original ioquake3 README</summary>

![Build](https://github.com/ioquake/ioq3/workflows/Build/badge.svg)

                   ,---------------------------------------.
                   |   _                     _       ____  |
                   |  (_)___  __ _ _  _ __ _| |_____|__ /  |
                   |  | / _ \/ _` | || / _` | / / -_)|_ \  |
                   |  |_\___/\__, |\_,_\__,_|_\_\___|___/  |
                   |            |_|                        |
                   |                                       |
                   `--------- https://ioquake3.org --------'

The intent of this project is to provide a baseline Quake 3 which may be used
for further development and baseq3 fun.
Some of the major features currently implemented are:

  * SDL 2 backend
  * OpenAL sound API support (multiple speaker support and better sound
    quality)
  * Full x86_64 support on Linux
  * VoIP support, both in-game and external support through Mumble.
  * MinGW compilation support on Windows and cross compilation support on Linux
  * AVI video capture of demos
  * Much improved console autocompletion
  * Persistent console history
  * Colorized terminal output
  * Optional Ogg Vorbis support
  * Much improved QVM tools
  * Support for various esoteric operating systems
  * cl_guid support
  * HTTP/FTP download redirection (using cURL)
  * Multiuser support on Windows systems (user specific game data
    is stored in "%APPDATA%\Quake3")
  * PNG support
  * Web support via Emscripten
  * Many, many bug fixes

The map editor and associated compiling tools are not included. We suggest you
use a modern copy from http://icculus.org/gtkradiant/.

The original id software readme that accompanied the Q3 source release has been
renamed to id-readme.txt so as to prevent confusion. Please refer to the
website for updated status.

More documentation including a Player's Guide and Sysadmin Guide are on:
https://ioquake3.org/help/

If you've got issues that you aren't sure are worth filing as bugs, or just
want to chat, please visit our forums:
https://discourse.ioquake.org

# Credits

Maintainers

  * James Canete <use.less01@gmail.com>
  * Ludwig Nussel <ludwig.nussel@suse.de>
  * Thilo Schulz <arny@ats.s.bawue.de>
  * Tim Angus <tim@ngus.net>
  * Tony J. White <tjw@tjw.org>
  * Jack Slater <jack@ioquake.org>
  * Zack Middleton <zturtleman@gmail.com>

Significant contributions from

  * Ryan C. Gordon <icculus@icculus.org>
  * Andreas Kohn <andreas@syndrom23.de>
  * Joerg Dietrich <Dietrich_Joerg@t-online.de>
  * Stuart Dalton <badcdev@gmail.com>
  * Vincent S. Cojot <vincent at cojot dot name>
  * optical <alex@rigbo.se>
  * Aaron Gyes <floam@aaron.gy>

</details>
