/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// bg_public.h -- definitions shared by both the server game and client game modules

// because games can change separately from the main system version, we need a
// second version that must match between game and cgame

#define GAME_VERSION BASEGAME

#define DEFAULT_GRAVITY 800
#define GIB_HEALTH -40
#define ARMOR_PROTECTION 0.66

#define MAX_ITEMS 256

#define RANK_TIED_FLAG 0x4000

#define DEFAULT_SHOTGUN_SPREAD 700
#define DEFAULT_SHOTGUN_COUNT 11

#define ITEM_RADIUS 15  // item sizes are needed for client side pickup detection

#define LIGHTNING_RANGE 768

#define SCORE_NOT_PRESENT -9999  // for the CS_SCORES[12] when only one player is present

#define VOTE_TIME 30000  // 30 seconds before vote times out

// [QL] Per-client vote state (binary-verified)
typedef enum {
    VOTE_NONE,
    VOTE_PENDING,
    VOTE_YES,
    VOTE_NO,
    VOTE_PASSED,    // admin/mod force-pass
    VOTE_VETOED     // admin/mod force-fail
} voteState_t;

// [QL] g_voteFlags bitmask - set bit to DISABLE that vote type
#define VF_MAP              0x0001  // bit 0: map change
#define VF_MAP_RESTART      0x0002  // bit 1: map restart
#define VF_NEXTMAP          0x0004  // bit 2: next map
#define VF_GAMETYPE         0x0008  // bit 3: gametype change (via map vote)
#define VF_KICK             0x0010  // bit 4: kick/clientkick
#define VF_TIMELIMIT        0x0020  // bit 5: timelimit
#define VF_FRAGLIMIT        0x0040  // bit 6: fraglimit
#define VF_SHUFFLE          0x0080  // bit 7: shuffle teams
#define VF_TEAMSIZE         0x0100  // bit 8: team size
#define VF_COINTOSS         0x0200  // bit 9: cointoss/random
#define VF_LOADOUTS         0x0400  // bit 10: loadouts
#define VF_ENDMAP_VOTING    0x0800  // bit 11: end-of-match map voting
#define VF_AMMO             0x1000  // bit 12: ammo system
#define VF_TIMERS           0x2000  // bit 13: item timers
#define VF_WEAPRESPAWN      0x4000  // bit 14: weapon respawn time

#define MINS_Z -24
#define DEFAULT_VIEWHEIGHT 26
#define CROUCH_VIEWHEIGHT 12
#define DEAD_VIEWHEIGHT -16

//
// config strings are a general means of communicating variable length strings
// from the server to all connected clients.
//

// CS_SERVERINFO and CS_SYSTEMINFO are defined in q_shared.h
#define CS_MUSIC 2
#define CS_MESSAGE 3  // from the map worldspawn's message field
#define CS_MOTD 4     // g_motd string for server message of the day (not used in QL)
#define CS_WARMUP 5   // server time when the match will be restarted
#define CS_SCORES1 6  // 1st place score
#define CS_SCORES2 7  // 2nd place score
#define CS_VOTE_TIME 8
#define CS_VOTE_STRING 9
#define CS_VOTE_YES 10
#define CS_VOTE_NO 11
#define CS_GAME_VERSION 12
#define CS_LEVEL_START_TIME 13  // so the timer only shows the current level
#define CS_INTERMISSION 14      // when 1, fraglimit/timelimit has been hit and intermission will start in a second or two
#define CS_ITEMS 15             // string of 0's and 1's that tell the client which items are present/to load.
#define CS_BOTINFO 16           // internal debugging stuff (infostring to render bot_hud values on client screen)
#define CS_MODELS 17
#define CS_SOUNDS (CS_MODELS + MAX_MODELS)              // 273
#define CS_PLAYERS (CS_SOUNDS + MAX_SOUNDS)             // 529
#define CS_LOCATIONS (CS_PLAYERS + MAX_CLIENTS)         // 593
#define CS_LAST_GENERIC (CS_LOCATIONS + MAX_LOCATIONS)  // 657
#define CS_FLAGSTATUS 658
#define CS_SCORES1PLAYER 659  // 1st place player's name
#define CS_SCORES2PLAYER 660  // 2nd place player's name
#define CS_ROUND_WARMUP 661
#define CS_ROUND_START_TIME 662
#define CS_TEAMCOUNT_RED 663
#define CS_TEAMCOUNT_BLUE 664
#define CS_SHADERSTATE 665
#define CS_NEXTMAP 666
#define CS_PRACTICE 667
#define CS_FREECAM 668
#define CS_PAUSE_START_TIME 669  // if this is non-zero, the game is paused
#define CS_PAUSE_END_TIME 670    // 0 = pause, !0 = timeout
#define CS_TIMEOUTS_RED 671      // TOs REMAINING
#define CS_TIMEOUTS_BLUE 672
#define CS_MODEL_OVERRIDE 673
#define CS_PLAYER_CYLINDERS 674
#define CS_DEBUGFLAGS 675
#define CS_ENABLEBREATH 676
#define CS_DMGTHROUGHDEPTH 677
#define CS_AUTHOR 678  // from the map worldspawn's author field
#define CS_AUTHOR2 679
#define CS_ADVERT_DELAY 680
#define CS_PMOVEINFO 681
#define CS_ARMORINFO 682
#define CS_WEAPONINFO 683
#define CS_PLAYERINFO 684
#define CS_SCORE1STPLAYER 685      // Score of the duel player on the left
#define CS_SCORE2NDPLAYER 686      // Score of the duel player on the right
#define CS_CLIENTNUM1STPLAYER 687  // ClientNum of the duel player on the left
#define CS_CLIENTNUM2NDPLAYER 688  // ClientNum of the duel player on the right
#define CS_ATMOSEFFECT 689         // unused, was per-map rain/snow effects
#define CS_MOST_DAMAGEDEALT_PLYR 690
#define CS_MOST_ACCURATE_PLYR 691
#define CS_REDTEAMBASE 692
#define CS_BLUETEAMBASE 693
#define CS_BEST_ITEMCONTROL_PLYR 694
#define CS_MOST_VALUABLE_OFFENSIVE_PLYR 695
#define CS_MOST_VALUABLE_DEFENSIVE_PLYR 696
#define CS_MOST_VALUABLE_PLYR 697
#define CS_GENERIC_COUNT_RED 698
#define CS_GENERIC_COUNT_BLUE 699
#define CS_AD_SCORES 700
#define CS_ROUND_WINNER 701
#define CS_CUSTOM_SETTINGS 702
#define CS_ROTATIONMAPS 703
#define CS_ROTATIONVOTES 704
#define CS_DISABLE_VOTE_UI 705
#define CS_ALLREADY_TIME 706
#define CS_INFECTED_SURVIVOR_MINSPEED 707
#define CS_RACE_POINTS 708
#define CS_DISABLE_LOADOUT 709
// 710, 711 reserved/unused
#define CS_MATCH_GUID 712         // also sent in the ZMQ stats
#define CS_STARTING_WEAPONS 713   // bitmask of weapons identical to g_startingWeapons
#define CS_STEAM_ID 714           // the server's steam ID (64)
#define CS_STEAM_WORKSHOP_IDS 715 // space separated list of workshop IDs for the client to load
#define CS_MAX 716

#if (CS_MAX) > MAX_CONFIGSTRINGS
#error overflow: (CS_MAX) > MAX_CONFIGSTRINGS
#endif

// [QL] Game types - completely reordered vs Q3
typedef enum {
    GT_FFA,            // 0  free for all
    GT_DUEL,           // 1  [QL] one on one (was GT_TOURNAMENT)
    GT_RACE,           // 2  [QL] race mode (replaced GT_SINGLE_PLAYER)
    GT_TEAM,           // 3  team deathmatch
    GT_CA,             // 4  [QL] clan arena
    GT_CTF,            // 5  capture the flag
    GT_1FCTF,          // 6  one flag CTF
    GT_OBELISK,        // 7  overload (attack/defend obelisk)
    GT_HARVESTER,      // 8  harvester
    GT_FREEZE,         // 9  [QL] freeze tag
    GT_DOMINATION,     // 10 [QL] domination
    GT_AD,             // 11 [QL] attack & defend
    GT_RR,             // 12 [QL] red rover
    GT_MAX_GAME_TYPE
} gametype_t;

// [QL] Aliases for backward compatibility
#define GT_TOURNAMENT   GT_DUEL

// [QL] gametype name tables (defined in bg_misc.c)
extern const char *gametypeShortNames[];    // short names for entity/config filtering
extern const char *gametypeDisplayNames[];  // full display names for UI

// [QL] Round state for round-based game types (CA, AD, FT, RR)
typedef enum {
    RS_WARMUP,
    RS_COUNTDOWN,
    RS_PLAYING,
    RS_ROUND_OVER
} roundStateState_t;

typedef enum { GENDER_MALE,
               GENDER_FEMALE,
               GENDER_NEUTER } gender_t;

/*
===================================================================================

PMOVE MODULE

The pmove code takes a player_state_t and a usercmd_t and generates a new player_state_t
and some other output data.  Used for local prediction on the client game and true
movement on the server game.
===================================================================================
*/

typedef enum {
    PM_NORMAL,         // can accelerate and turn
    PM_NOCLIP,         // noclip movement
    PM_SPECTATOR,      // still run into walls
    PM_DEAD,           // no acceleration or turning, but free falling
    PM_FREEZE,         // stuck in place with no control
    PM_INTERMISSION,   // no movement or status bar
    PM_TUTORIAL        // [QL] tutorial mode - no movement
} pmtype_t;

typedef enum {
    WEAPON_READY,
    WEAPON_RAISING,
    WEAPON_DROPPING,
    WEAPON_FIRING
} weaponstate_t;

// pmove->pm_flags
// [QL] Verified from qagamex86.dll binary decompilation.
// PMF_TIME_WATERJUMP changed from Q3's 256 to 0x80.
// PMF_ZOOM and PMF_RESPAWNED share 0x0200 (both block weapon fire after respawn).
// Several new flags added (FROZEN, PAUSED, ZOOM, NO_AUTOHOP).
#define PMF_DUCKED          0x0001
#define PMF_JUMP_HELD       0x0002
#define PMF_FROZEN          0x0004   // [QL] freeze tag/CA frozen state
#define PMF_BACKWARDS_JUMP  0x0008   // go into backwards land
#define PMF_BACKWARDS_RUN   0x0010   // coast down to backwards run
#define PMF_TIME_LAND       0x0020   // pm_time is time before rejump
#define PMF_TIME_KNOCKBACK  0x0040   // pm_time is an air-accelerate only time
#define PMF_TIME_WATERJUMP  0x0080   // pm_time is waterjump [QL: 0x80]
#define PMF_PAUSED          0x0100   // [QL] movement pause
#define PMF_ZOOM            0x0200   // [QL] zoom/scope state (shares bit with RESPAWNED)
#define PMF_RESPAWNED       0x0200   // [QL] blocks weapon fire after respawn
#define PMF_USE_ITEM_HELD   0x0400   // [QL] use item button held
#define PMF_GRAPPLE_PULL    0x0800   // pull towards grapple location
#define PMF_FOLLOW          0x1000   // spectate following another player
#define PMF_SCOREBOARD      0x2000   // spectate as a scoreboard
#define PMF_INVULEXPAND     0x4000   // invulnerability sphere set to full size
#define PMF_TIME_GRAPPLE    0x8000   // [QL] one-frame weapon reset after grapple release
#define PMF_PROMODE         0x10000  // [QL] CPM/PQL air control mode active
#define PMF_DOUBLE_JUMPED   0x20000  // [QL] double jump tracking
#define PMF_NO_AUTOHOP      0x40000  // [QL] auto-hop control
#define PMF_CROUCH_SLIDE    0x100000 // [QL] crouch slide active

#define PMF_ALL_TIMES (PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_KNOCKBACK | PMF_TIME_GRAPPLE)

#define MAXTOUCH 32
typedef struct {
    // state (in / out)
    playerState_t* ps;

    // command (in)
    usercmd_t cmd;
    int tracemask;         // collide against these types of surfaces
    int debugLevel;        // if set, diagnostic output will be printed
    int noFootsteps;       // [QL] changed from qboolean
    qboolean gauntletHit;  // true if a gauntlet attack would actually hit something

    // results (out)
    int numtouch;
    int touchents[MAXTOUCH];

    vec3_t mins, maxs;  // bounding box size

    int watertype;
    int waterlevel;

    float xyspeed;

    float stepHeight;   // [QL]
    int stepTime;       // [QL]

    // callbacks to test the world
    // these will be different functions during game and cgame
    void (*trace)(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentMask);
    int (*pointcontents)(const vec3_t point, int passEntityNum);

    qboolean hookEnemy;  // [QL]
} pmove_t;

// if a full pmove isn't done on the client, you can just update the angles
void PM_UpdateViewAngles(playerState_t* ps, const usercmd_t* cmd);
void Pmove(pmove_t* pmove);

//===================================================================================

// player_state->stats[] indexes
// NOTE: may not have more than 16
// [QL] Reordered from Q3 - verified from qagamex64.so binary analysis.
typedef enum {
    STAT_HEALTH,                // 0
    STAT_HOLDABLE_ITEM,         // 1
    STAT_RUNE,                  // 2  [QL moved from 14 in Q3]
    STAT_WEAPONS,               // 3  - 16 bit fields
    STAT_ARMOR,                 // 4
    STAT_BSKILL,                // 5  [QL] bot skill level
    STAT_CLIENTS_READY,         // 6
    STAT_MAX_HEALTH,            // 7
    STAT_SPINUP,                // 8  [QL] chaingun spinup
    STAT_FLIGHT_THRUST,         // 9  [QL]
    STAT_CUR_FLIGHT_FUEL,       // 10 [QL] - also used as regen max
    STAT_MAX_FLIGHT_FUEL,       // 11 [QL] - also used as regen accumulator
    STAT_FLIGHT_REFUEL,         // 12 [QL] - also used as regen rate
    STAT_JUMPTIME,              // 13 [QL]
    STAT_ARMORTYPE,             // 14 [QL]
    STAT_KEY,                   // 15 [QL] bitmask for keys
} statIndex_t;

// Backward compatibility aliases for Q3 Team Arena code.
// In QL these stat slots were repurposed, but existing TA code still compiles.
#define STAT_PERSISTANT_POWERUP STAT_RUNE
#define STAT_DEAD_YAW           STAT_BSKILL

// player_state->persistant[] indexes
// these fields are the only part of player_state that isn't
// cleared on respawn
// NOTE: may not have more than 16
typedef enum {
    PERS_SCORE,           // !!! MUST NOT CHANGE, SERVER AND GAME BOTH REFERENCE !!!
    PERS_HITS,            // total points damage inflicted so damage beeps can sound on change
    PERS_RANK,            // player rank or team rank
    PERS_TEAM,            // player team
    PERS_SPAWN_COUNT,     // incremented every respawn
    PERS_PLAYEREVENTS,    // 16 bits that can be flipped for events
    PERS_ATTACKER,        // clientnum of last damage inflicter
    PERS_ATTACKEE_ARMOR,  // health/armor of last person we attacked
    PERS_KILLED,          // count of the number of times you died
    // player awards tracking
    PERS_IMPRESSIVE_COUNT,     // two railgun hits in a row
    PERS_EXCELLENT_COUNT,      // two successive kills in a short amount of time
    PERS_DEFEND_COUNT,         // defend awards
    PERS_ASSIST_COUNT,         // assist awards
    PERS_GAUNTLET_FRAG_COUNT,  // kills with the guantlet
    PERS_CAPTURES              // captures
} persEnum_t;

// entityState_t->eFlags
#define EF_DEAD 0x00000001             // don't draw a foe marker over players with EF_DEAD
#define EF_TICKING 0x00000002          // used to make players play the prox mine ticking sound
#define EF_TELEPORT_BIT 0x00000004     // toggled every time the origin abruptly changes
#define EF_AWARD_EXCELLENT 0x00000008  // draw an excellent sprite
#define EF_PLAYER_EVENT 0x00000010
#define EF_BOUNCE 0x00000010          // for missiles
#define EF_BOUNCE_HALF 0x00000020     // for missiles
#define EF_AWARD_GAUNTLET 0x00000040  // draw a gauntlet sprite
#define EF_NODRAW 0x00000080          // may have an event, but no model (unspawned items)
#define EF_FIRING 0x00000100          // for lightning gun
#define EF_KAMIKAZE 0x00000200
#define EF_MOVER_STOP 0x00000400        // will push otherwise
#define EF_AWARD_CAP 0x00000800         // draw the capture sprite
#define EF_TALK 0x00001000              // draw a talk balloon
#define EF_CONNECTION 0x00002000        // draw a connection trouble sprite
#define EF_VOTED 0x00004000             // already cast a vote
#define EF_AWARD_IMPRESSIVE 0x00008000  // draw an impressive sprite
#define EF_AWARD_DEFEND 0x00010000      // draw a defend sprite
#define EF_AWARD_ASSIST 0x00020000      // draw a assist sprite
#define EF_AWARD_DENIED 0x00040000      // denied

// NOTE: may not have more than 16
// [QL] Reordered from Q3 - flags come first, then powerups.
// Verified from qagamex64.so binary analysis.
typedef enum {
    PW_NONE,

    PW_REDFLAG,                     // 1  [QL moved from 7]
    PW_BLUEFLAG,                    // 2  [QL moved from 8]
    PW_NEUTRALFLAG,                 // 3  [QL moved from 9]
    PW_QUAD,                        // 4  [QL moved from 1]
    PW_BATTLESUIT,                  // 5  [QL moved from 2]
    PW_HASTE,                       // 6  [QL moved from 3]
    PW_INVIS,                       // 7  [QL moved from 4]
    PW_REGEN,                       // 8  [QL moved from 5]
    PW_FLIGHT,                      // 9  [QL moved from 6]
    PW_INVULNERABILITY,             // 10 [QL moved from 14]
    PW_SCOUT,                       // 11
    PW_GUARD,                       // 12
    PW_DOUBLER,                     // 13
    PW_AMMOREGEN,                   // 14
    PW_FREEZE,                      // 15 [QL] - freeze tag frozen state

    PW_NUM_POWERUPS                 // 16

} powerup_t;

typedef enum {
    HI_NONE,

    HI_TELEPORTER,
    HI_MEDKIT,
    HI_KAMIKAZE,
    HI_PORTAL,
    HI_INVULNERABILITY,

    HI_NUM_HOLDABLE
} holdable_t;

typedef enum {
    WP_NONE,

    WP_GAUNTLET,
    WP_MACHINEGUN,
    WP_SHOTGUN,
    WP_GRENADE_LAUNCHER,
    WP_ROCKET_LAUNCHER,
    WP_LIGHTNING,
    WP_RAILGUN,
    WP_PLASMAGUN,
    WP_BFG,
    WP_GRAPPLING_HOOK,
    WP_NAILGUN,
    WP_PROX_LAUNCHER,
    WP_CHAINGUN,
    WP_HMG,             // [QL] Heavy Machine Gun

    WP_NUM_WEAPONS
} weapon_t;

#define NUM_LIGHTNING_STYLES 5

// reward sounds (stored in ps->persistant[PERS_PLAYEREVENTS])
#define PLAYEREVENT_DENIEDREWARD 0x0001
#define PLAYEREVENT_GAUNTLETREWARD 0x0002
#define PLAYEREVENT_HOLYSHIT 0x0004

// entityState_t->event values
// entity events are for effects that take place relative
// to an existing entities origin.  Very network efficient.

// two bits at the top of the entityState->event field
// will be incremented with each change in the event so
// that an identical event started twice in a row can
// be distinguished.  And off the value with ~EV_EVENT_BITS
// to retrieve the actual event number
#define EV_EVENT_BIT1 0x00000100
#define EV_EVENT_BIT2 0x00000200
#define EV_EVENT_BITS (EV_EVENT_BIT1 | EV_EVENT_BIT2)

#define EVENT_VALID_MSEC 300

// [QL] Entity events - verified from cgamex86.dll + qagamex64.so binary analysis.
// EV_STEP_4/8/12/16 removed from Q3; QL uses playerState_t for step smoothing.
// EV_STOPLOOPINGSOUND removed; many QL-specific events added.
typedef enum {
    EV_NONE,                        // 0x00

    EV_FOOTSTEP,                    // 0x01
    EV_FOOTSTEP_METAL,              // 0x02
    EV_FOOTSPLASH,                  // 0x03
    EV_FOOTWADE,                    // 0x04
    EV_SWIM,                        // 0x05

    EV_FALL_SHORT,                  // 0x06
    EV_FALL_MEDIUM,                 // 0x07
    EV_FALL_FAR,                    // 0x08

    EV_JUMP_PAD,                    // 0x09 - boing sound at origin, jump sound on player
    EV_JUMP,                        // 0x0A

    EV_WATER_TOUCH,                 // 0x0B - foot touches
    EV_WATER_LEAVE,                 // 0x0C - foot leaves
    EV_WATER_UNDER,                 // 0x0D - head touches
    EV_WATER_CLEAR,                 // 0x0E - head leaves

    EV_ITEM_PICKUP,                 // 0x0F - normal item pickups are predictable
    EV_GLOBAL_ITEM_PICKUP,          // 0x10 - powerup / team sounds are broadcast to everyone

    EV_NOAMMO,                      // 0x11
    EV_CHANGE_WEAPON,               // 0x12
    EV_DROP_WEAPON,                 // 0x13 [QL]
    EV_FIRE_WEAPON,                 // 0x14

    EV_USE_ITEM0,                   // 0x15
    EV_USE_ITEM1,                   // 0x16
    EV_USE_ITEM2,                   // 0x17
    EV_USE_ITEM3,                   // 0x18
    EV_USE_ITEM4,                   // 0x19
    EV_USE_ITEM5,                   // 0x1A
    EV_USE_ITEM6,                   // 0x1B
    EV_USE_ITEM7,                   // 0x1C
    EV_USE_ITEM8,                   // 0x1D
    EV_USE_ITEM9,                   // 0x1E
    EV_USE_ITEM10,                  // 0x1F
    EV_USE_ITEM11,                  // 0x20
    EV_USE_ITEM12,                  // 0x21
    EV_USE_ITEM13,                  // 0x22
    EV_USE_ITEM14,                  // 0x23
    EV_USE_ITEM15,                  // 0x24 - unused slot (no handler in binary)

    EV_ITEM_RESPAWN,                // 0x25
    EV_ITEM_POP,                    // 0x26
    EV_PLAYER_TELEPORT_IN,          // 0x27
    EV_PLAYER_TELEPORT_OUT,         // 0x28

    EV_GRENADE_BOUNCE,              // 0x29

    EV_GENERAL_SOUND,               // 0x2A
    EV_GLOBAL_SOUND,                // 0x2B - no attenuation
    EV_GLOBAL_TEAM_SOUND,           // 0x2C

    EV_BULLET_HIT_FLESH,            // 0x2D
    EV_BULLET_HIT_WALL,             // 0x2E

    EV_MISSILE_HIT,                 // 0x2F
    EV_MISSILE_MISS,                // 0x30
    EV_MISSILE_MISS_METAL,          // 0x31
    EV_RAILTRAIL,                   // 0x32
    EV_SHOTGUN,                     // 0x33
    EV_BULLET,                      // 0x34 - no cgame handler (server-side only)

    EV_PAIN,                        // 0x35
    EV_DEATH1,                      // 0x36
    EV_DEATH2,                      // 0x37
    EV_DEATH3,                      // 0x38
    EV_DROWN,                       // 0x39 [QL]
    EV_OBITUARY,                    // 0x3A

    EV_POWERUP_QUAD,                // 0x3B
    EV_POWERUP_BATTLESUIT,          // 0x3C
    EV_POWERUP_REGEN,               // 0x3D
    EV_POWERUP_ARMORREGEN,          // 0x3E [QL]

    EV_GIB_PLAYER,                  // 0x3F
    EV_SCOREPLUM,                   // 0x40

    EV_PROXIMITY_MINE_STICK,        // 0x41
    EV_PROXIMITY_MINE_TRIGGER,      // 0x42
    EV_KAMIKAZE,                    // 0x43
    EV_OBELISKEXPLODE,              // 0x44
    EV_OBELISKPAIN,                 // 0x45
    EV_INVUL_IMPACT,                // 0x46
    EV_JUICED,                      // 0x47
    EV_LIGHTNINGBOLT,               // 0x48

    EV_DEBUG_LINE,                  // 0x49
    EV_TAUNT,                       // 0x4A
    EV_TAUNT_YES,                   // 0x4B
    EV_TAUNT_NO,                    // 0x4C
    EV_TAUNT_FOLLOWME,              // 0x4D
    EV_TAUNT_GETFLAG,               // 0x4E
    EV_TAUNT_GUARDBASE,             // 0x4F
    EV_TAUNT_PATROL,                // 0x50

    EV_FOOTSTEP_SNOW,               // 0x51 [QL]
    EV_FOOTSTEP_WOOD,               // 0x52 [QL]
    EV_ITEM_PICKUP_SPEC,            // 0x53 [QL] - spectator item pickup notification
    EV_OVERTIME,                    // 0x54 [QL]
    EV_GAMEOVER,                    // 0x55 [QL]
    EV_MISSILE_MISS_DMGTHROUGH,     // 0x56 [QL] - damage-through impact (e.g. BFG)
    EV_THAW_PLAYER,                 // 0x57 [QL] - freeze tag thaw complete
    EV_THAW_TICK,                   // 0x58 [QL] - freeze tag thaw tick
    EV_SHOTGUN_KILL,                // 0x59 [QL] - delayed shotgun kill effect
    EV_POI,                         // 0x5A [QL] - point of interest marker
    EV_DEBUG_HITBOX,                // 0x5B [QL] - no cgame handler
    EV_LIGHTNING_DISCHARGE,         // 0x5C [QL] - LG water discharge
    EV_RACE_START,                  // 0x5D [QL]
    EV_RACE_CHECKPOINT,             // 0x5E [QL]
    EV_RACE_FINISH,                 // 0x5F [QL]
    EV_DAMAGEPLUM,                  // 0x60 [QL] - floating damage number
    EV_AWARD,                       // 0x61 [QL] - award notification (10 sub-types)
    EV_INFECTED,                    // 0x62 [QL]
    EV_NEW_HIGH_SCORE,              // 0x63 [QL]

    EV_NUM_ETYPES                   // 0x64 - sentinel
} entity_event_t;

typedef enum {
    GTS_RED_CAPTURE,
    GTS_BLUE_CAPTURE,
    GTS_RED_RETURN,
    GTS_BLUE_RETURN,
    GTS_RED_TAKEN,
    GTS_BLUE_TAKEN,
    GTS_REDOBELISK_ATTACKED,
    GTS_BLUEOBELISK_ATTACKED,
    GTS_REDTEAM_SCORED,
    GTS_BLUETEAM_SCORED,
    GTS_REDTEAM_TOOK_LEAD,
    GTS_BLUETEAM_TOOK_LEAD,
    GTS_TEAMS_ARE_TIED,
    GTS_KAMIKAZE,
    GTS_REDTEAM_WON,       // 14 [QL]
    GTS_BLUETEAM_WON        // 15 [QL]
} global_team_sound_t;

// animations
typedef enum {
    BOTH_DEATH1,
    BOTH_DEAD1,
    BOTH_DEATH2,
    BOTH_DEAD2,
    BOTH_DEATH3,
    BOTH_DEAD3,

    TORSO_GESTURE,

    TORSO_ATTACK,
    TORSO_ATTACK2,

    TORSO_DROP,
    TORSO_RAISE,

    TORSO_STAND,
    TORSO_STAND2,

    LEGS_WALKCR,
    LEGS_WALK,
    LEGS_RUN,
    LEGS_BACK,
    LEGS_SWIM,

    LEGS_JUMP,
    LEGS_LAND,

    LEGS_JUMPB,
    LEGS_LANDB,

    LEGS_IDLE,
    LEGS_IDLECR,

    LEGS_TURN,

    TORSO_GETFLAG,
    TORSO_GUARDBASE,
    TORSO_PATROL,
    TORSO_FOLLOWME,
    TORSO_AFFIRMATIVE,
    TORSO_NEGATIVE,

    MAX_ANIMATIONS,

    LEGS_BACKCR,
    LEGS_BACKWALK,
    FLAG_RUN,
    FLAG_STAND,
    FLAG_STAND2RUN,

    MAX_TOTALANIMATIONS
} animNumber_t;

typedef struct animation_s {
    int firstFrame;
    int numFrames;
    int loopFrames;   // 0 to numFrames
    int frameLerp;    // msec between frames
    int initialLerp;  // msec to get to first frame
    int reversed;     // true if animation is reversed
    int flipflop;     // true if animation should flipflop back to base
} animation_t;

// flip the togglebit every time an animation
// changes so a restart of the same anim can be detected
#define ANIM_TOGGLEBIT 128

typedef enum {
    TEAM_FREE,
    TEAM_RED,
    TEAM_BLUE,
    TEAM_SPECTATOR,

    TEAM_NUM_TEAMS
} team_t;

// Time between location updates
#define TEAM_LOCATION_UPDATE_TIME 1000

// How many players on the overlay
#define TEAM_MAXOVERLAY 32

// team task
typedef enum {
    TEAMTASK_NONE,
    TEAMTASK_OFFENSE,
    TEAMTASK_DEFENSE,
    TEAMTASK_PATROL,
    TEAMTASK_FOLLOW,
    TEAMTASK_RETRIEVE,
    TEAMTASK_ESCORT,
    TEAMTASK_CAMP
} teamtask_t;

// means of death
typedef enum {
    MOD_UNKNOWN,
    MOD_SHOTGUN,
    MOD_GAUNTLET,
    MOD_MACHINEGUN,
    MOD_GRENADE,
    MOD_GRENADE_SPLASH,
    MOD_ROCKET,
    MOD_ROCKET_SPLASH,
    MOD_PLASMA,
    MOD_PLASMA_SPLASH,
    MOD_RAILGUN,
    MOD_LIGHTNING,
    MOD_BFG,
    MOD_BFG_SPLASH,
    MOD_WATER,
    MOD_SLIME,
    MOD_LAVA,
    MOD_CRUSH,
    MOD_TELEFRAG,
    MOD_FALLING,
    MOD_SUICIDE,
    MOD_TARGET_LASER,
    MOD_TRIGGER_HURT,
    MOD_NAIL,
    MOD_CHAINGUN,
    MOD_PROXIMITY_MINE,
    MOD_KAMIKAZE,
    MOD_JUICED,
    MOD_GRAPPLE,
    MOD_SWITCH_TEAMS,       // [QL]
    MOD_THAW,               // [QL]
    MOD_LIGHTNING_DISCHARGE, // [QL]
    MOD_HMG,                // [QL]
    MOD_RAILGUN_HEADSHOT    // [QL]
} meansOfDeath_t;

//---------------------------------------------------------

// gitem_t->type
typedef enum {
    IT_BAD,
    IT_WEAPON,   // EFX: rotate + upscale + minlight
    IT_AMMO,     // EFX: rotate
    IT_ARMOR,    // EFX: rotate + minlight
    IT_HEALTH,   // EFX: static external sphere + rotating internal
    IT_POWERUP,  // instant on, timer based
    // EFX: rotate + external ring that rotates
    IT_HOLDABLE,  // single use, holdable item
    // EFX: rotate + bob
    IT_PERSISTANT_POWERUP,
    IT_TEAM,
    IT_KEY  // [QL] key items (silver, gold, master)
} itemType_t;

// [QL] key giTag bit flags - stored in stats[STAT_KEY]
#define KEY_SILVER  1
#define KEY_GOLD    2
#define KEY_MASTER  4

#define MAX_ITEM_MODELS 4

typedef struct gitem_s {
    char* classname;  // spawning name
    char* pickup_sound;
    char* world_model[MAX_ITEM_MODELS];

    char* icon;
    char* pickup_name;  // for printing on pickup

    int quantity;       // for ammo how much, or duration of powerup
    itemType_t giType;  // IT_* flags

    int giTag;

    char* precaches;  // string of all models and images this item will use
    char* sounds;     // string of all sounds this item will use
} gitem_t;

// included in both the game dll and the client
extern gitem_t bg_itemlist[];
extern int bg_numItems;

// [QL] weapon reload times (fire intervals in ms), indexed by weapon_t
// Updated at runtime by weapon_reload_* cvars via BG_SetWeaponReload()
extern int bg_weaponReloadTime[WP_NUM_WEAPONS];
void BG_SetWeaponReload(int weapon, int reloadTime);

// [QL] PM_Set* - configurable movement physics parameters
void PM_SetAirAccel(float value);
void PM_SetAirControl(float value);
void PM_SetAirStepFriction(float value);
void PM_SetAirSteps(int value);
void PM_SetAirStopAccel(float value);
void PM_SetAutoHop(int value);
void PM_SetBunnyHop(int value);
void PM_SetChainJump(int value);
void PM_SetChainJumpVelocity(float value);
void PM_SetCircleStrafeFriction(float value);
void PM_SetCrouchSlideFriction(float value);
void PM_SetCrouchSlideTime(int value);
void PM_SetCrouchStepJump(int value);
void PM_SetHookPullVelocity(float value);
void PM_SetJumpTimeDeltaMin(float value);
void PM_SetJumpVelocity(float value);
void PM_SetJumpVelocityMax(float value);
void PM_SetJumpVelocityScaleAdd(float value);
void PM_SetJumpVelocityTimeThreshold(float value);
void PM_SetJumpVelocityTimeThresholdOffset(float value);
void PM_SetNoPlayerClip(int value);
void PM_SetRampJump(int value);
void PM_SetRampJumpScale(float value);
void PM_SetStepHeight(float value);
void PM_SetStepJump(int value);
void PM_SetStepJumpVelocity(float value);
void PM_SetStrafeAccel(float value);
void PM_SetWalkAccel(float value);
void PM_SetWalkFriction(float value);
void PM_SetWaterSwimScale(float value);
void PM_SetWaterWadeScale(float value);
void PM_SetWeaponDropTime(int value);
void PM_SetWeaponRaiseTime(int value);
void PM_SetWishSpeed(float value);
void PM_SetDoubleJump(int value);
void PM_SetCrouchSlide(int value);
void PM_SetVelocityGH(float value);
void PM_SetMovementConfig(void);  // sets VQ3/CPM mode based on pm_flags

gitem_t* BG_FindItem(const char* pickupName);
gitem_t* BG_FindItemForWeapon(weapon_t weapon);
gitem_t* BG_FindItemForPowerup(powerup_t pw);
gitem_t* BG_FindItemForHoldable(holdable_t pw);
#define ITEM_INDEX(x) ((x) - bg_itemlist)

qboolean BG_CanItemBeGrabbed(int gametype, const entityState_t* ent, const playerState_t* ps);

// [QL] g_dmflags->integer flags (binary-verified bit assignments)
#define DF_NO_TEAM_DAMAGE           1   // bit 0: no team health damage (CA/FT/DOM)
#define DF_NO_TEAM_ARMOR_DAMAGE     2   // bit 1: no team armor damage (CA/FT/DOM)
#define DF_NO_SELF_DAMAGE           4   // bit 2: no self health damage
#define DF_NO_SELF_ARMOR_DAMAGE     8   // bit 3: no self armor damage
#define DF_NO_FALLING_DAMAGE        16  // bit 4: no fall damage
#define DF_NO_FOOTSTEPS             32  // bit 5: no footstep sounds

// content masks
#define MASK_ALL (-1)
#define MASK_SOLID (CONTENTS_SOLID)
#define MASK_PLAYERSOLID (CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY)
#define MASK_DEADSOLID (CONTENTS_SOLID | CONTENTS_PLAYERCLIP)
#define MASK_WATER (CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME)
#define MASK_OPAQUE (CONTENTS_SOLID | CONTENTS_SLIME | CONTENTS_LAVA)
#define MASK_SHOT (CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_CORPSE)

//
// entityState_t->eType
//
typedef enum {
    ET_GENERAL,
    ET_PLAYER,
    ET_ITEM,
    ET_MISSILE,
    ET_MOVER,
    ET_BEAM,
    ET_PORTAL,
    ET_SPEAKER,
    ET_PUSH_TRIGGER,
    ET_TELEPORT_TRIGGER,
    ET_INVISIBLE,
    ET_GRAPPLE,  // grapple hooked on wall
    ET_TEAM,

    ET_EVENTS  // any of the EV_* events can be added freestanding
               // by setting eType to ET_EVENTS + eventNum
               // this avoids having to set eFlags and eventNum
} entityType_t;

void BG_EvaluateTrajectory(const trajectory_t* tr, int atTime, vec3_t result);
void BG_EvaluateTrajectoryDelta(const trajectory_t* tr, int atTime, vec3_t result);

void BG_AddPredictableEventToPlayerstate(int newEvent, int eventParm, playerState_t* ps);

void BG_TouchJumpPad(playerState_t* ps, entityState_t* jumppad);

void BG_PlayerStateToEntityState(playerState_t* ps, entityState_t* s, qboolean snap);
void BG_PlayerStateToEntityStateExtraPolate(playerState_t* ps, entityState_t* s, int time, qboolean snap);

qboolean BG_PlayerTouchesItem(playerState_t* ps, entityState_t* item, int atTime);

#define MAX_ARENAS 1024
#define MAX_ARENAS_TEXT 16384

#define MAX_BOTS 1024
#define MAX_BOTS_TEXT 8192

// Kamikaze

// 1st shockwave times
#define KAMI_SHOCKWAVE_STARTTIME 0
#define KAMI_SHOCKWAVEFADE_STARTTIME 1500
#define KAMI_SHOCKWAVE_ENDTIME 2000
// explosion/implosion times
#define KAMI_EXPLODE_STARTTIME 250
#define KAMI_IMPLODE_STARTTIME 2000
#define KAMI_IMPLODE_ENDTIME 2250
// 2nd shockwave times
#define KAMI_SHOCKWAVE2_STARTTIME 2000
#define KAMI_SHOCKWAVE2FADE_STARTTIME 2500
#define KAMI_SHOCKWAVE2_ENDTIME 3000
// radius of the models without scaling
#define KAMI_SHOCKWAVEMODEL_RADIUS 88
#define KAMI_BOOMSPHEREMODEL_RADIUS 72
// maximum radius of the models during the effect
#define KAMI_SHOCKWAVE_MAXRADIUS 1320
#define KAMI_BOOMSPHERE_MAXRADIUS 720
#define KAMI_SHOCKWAVE2_MAXRADIUS 704
