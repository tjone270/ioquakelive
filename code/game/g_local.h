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
// g_local.h -- local definitions for game module

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "g_public.h"

//==================================================================

// the "gameversion" client command will print this plus compile date
#define GAMEVERSION BASEGAME

#define BODY_QUEUE_SIZE 8

#define INFINITE 1000000

#define FRAMETIME 100  // msec
#define CARNAGE_REWARD_TIME 3000
#define REWARD_SPRITE_TIME 2000

#define INTERMISSION_DELAY_TIME 200  // [QL] binary uses 200ms (was 1000 in Q3)
#define SP_INTERMISSION_DELAY_TIME 5000

// g_playerCylinders: use capsule trace for player hits when enabled
#define G_TracePlayerHit(results, start, mins, maxs, end, passEnt, mask) \
    do { \
        if (g_playerCylinders.integer) \
            trap_TraceCapsule(results, start, mins, maxs, end, passEnt, mask); \
        else \
            trap_Trace(results, start, mins, maxs, end, passEnt, mask); \
    } while (0)

// gentity->flags
#define FL_GODMODE 0x00000010
#define FL_NOTARGET 0x00000020
#define FL_TEAMSLAVE 0x00000400  // not the first on the team
#define FL_NO_KNOCKBACK 0x00000800
#define FL_DROPPED_ITEM 0x00001000
#define FL_NO_BOTS 0x00002000        // spawn point not for bot use
#define FL_NO_HUMANS 0x00004000      // spawn point just for bots
#define FL_FORCE_GESTURE 0x00008000  // force gesture on client

// movers are things like doors, plats, buttons, etc
typedef enum {
    MOVER_POS1,
    MOVER_POS2,
    MOVER_1TO2,
    MOVER_2TO1
} moverState_t;

#define SP_PODIUM_MODEL "models/mapobjects/podium/podium4.md3"

//============================================================================

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

// [QL] gentity_s - game entity (1056 bytes)
struct gentity_s {
    entityState_t s;   // communicated by server to clients
    entityShared_t r;  // shared by both the server system and game

    // DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
    // EXPECTS THE FIELDS IN THAT ORDER!
    //================================

    struct gclient_s* client;  // NULL if not a client

    qboolean inuse;

    char* classname;  // set in QuakeEd
    int spawnflags;   // set in QuakeEd

    qboolean neverFree;  // if true, FreeEntity will only unlink

    int flags;  // FL_* variables

    char* model;
    char* model2;
    int freetime;  // level.time when the object was freed

    int eventTime;  // events will be cleared EVENT_VALID_MSEC after set
    qboolean freeAfterEvent;
    qboolean unlinkAfterEvent;

    qboolean physicsObject;
    float physicsBounce;  // 1.0 = continuous bounce, 0.0 = no bounce
    int clipmask;

    // movers
    moverState_t moverState;
    int soundPos1;
    int sound1to2;
    int sound2to1;
    int soundPos2;
    int soundLoop;
    gentity_t* parent;
    gentity_t* nextTrain;
    gentity_t* prevTrain;
    vec3_t pos1, pos2;

    char* message;
    char* cvar;              // [QL]
    char* tourPointTarget;   // [QL]
    char* tourPointTargetName; // [QL]
    char* noise;             // [QL]

    int timestamp;  // body queue sinking, etc
    float angle;    // [QL]

    char* target;
    char* target2;      // [QL] secondary target (race checkpoint chaining)
    char* targetname;
    char* targetShaderName;
    char* targetShaderNewName;
    gentity_t* target_ent;

    float speed;
    vec3_t movedir;

    int nextthink;
    void (*think)(gentity_t* self);
    void (*framethink)(gentity_t* self);  // [QL]
    void (*reached)(gentity_t* self);
    void (*blocked)(gentity_t* self, gentity_t* other);
    void (*touch)(gentity_t* self, gentity_t* other, trace_t* trace);
    void (*use)(gentity_t* self, gentity_t* other, gentity_t* activator);
    void (*pain)(gentity_t* self, gentity_t* attacker, int damage);
    void (*die)(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod);

    int pain_debounce_time;
    int fly_sound_debounce_time;  // wind tunnel

    int health;

    qboolean takedamage;

    int damage;
    int damageFactor;  // [QL]
    int splashDamage;
    int splashRadius;
    int methodOfDeath;
    int splashMethodOfDeath;

    int count;

    gentity_t* chain;
    gentity_t* enemy;
    gentity_t* activator;
    char* team;
    gentity_t* teammaster;
    gentity_t* teamchain;

    int kamikazeTime;
    int kamikazeShockTime;

    int watertype;
    int waterlevel;

    int noise_index;
    int bouncecount;  // [QL]

    // timing variables
    float wait;
    float random;

    int spawnTime;  // [QL]

    gitem_t* item;  // for bonus items
    int pickupCount;  // [QL]
};

typedef enum {
    CON_DISCONNECTED,
    CON_CONNECTING,
    CON_CONNECTED
} clientConnected_t;

typedef enum {
    SPECTATOR_NOT,
    SPECTATOR_FREE,
    SPECTATOR_FOLLOW,
    SPECTATOR_SCOREBOARD
} spectatorState_t;

typedef enum {
    TEAM_BEGIN,  // Beginning a team game, spawn at base
    TEAM_ACTIVE  // Now actively playing
} playerTeamStateState_t;

typedef struct {
    playerTeamStateState_t state;

    int location;

    int captures;
    int basedefense;
    int carrierdefense;
    int flagrecovery;
    int fragcarrier;
    int assists;

    float lasthurtcarrier;
    float lastreturnedflag;
    float flagsince;
    float lastfraggedcarrier;

    int flagruntime;        // [QL] time carrying flag
    int flagrunrelays;      // [QL] relay count
    int lastkilltime;       // [QL] last kill timestamp
} playerTeamState_t;

// client data that stays across multiple levels or tournament restarts
// this is achieved by writing all the data to cvar strings at game shutdown
// time and reading them back at connection time.  Anything added here
// MUST be dealt with in G_InitSessionData() / G_ReadSessionData() / G_WriteSessionData()
// [QL] Expanded session data
typedef struct {
    team_t sessionTeam;
    int spectatorTime;          // [QL] renamed from spectatorNum
    spectatorState_t spectatorState;
    int spectatorClient;  // for chasecam and follow mode
    int weaponPrimary;    // [QL] primary weapon selection
    int wins, losses;     // tournament stats
    qboolean teamLeader;  // true when this client is a team leader
    int privileges;        // [QL] 0=none, 1=referee, 2=admin
    int specOnly;          // [QL] spectate-only flag
    int playQueue;         // [QL] queue position for play
    qboolean updatePlayQueue; // [QL]
    int muted;             // [QL] chat mute flag
    int prevScore;         // [QL] previous score (for round tracking)
    int joinTime;          // [QL] time() when bot joined (duel)
} clientSession_t;

//
#define MAX_NETNAME 36
#define MAX_VOTE_COUNT 3

// [QL] Expanded persistent data (248 bytes)
typedef struct {
    clientConnected_t connected;
    usercmd_t cmd;               // we would lose angles if not persistant
    qboolean localClient;        // true if "ip" info key is "localhost"
    qboolean initialSpawn;       // the first spawn should be at a cool location
    qboolean predictItemPickup;  // based on cg_predictItems userinfo
    char netname[40];            // [QL] was MAX_NETNAME (36), now 40
    char country[24];            // [QL] GeoIP country code
    uint64_t steamId;            // [QL] Steam ID
    int maxHealth;                // for handicapping
    int voteCount;                // to prevent people from constantly calling votes
    int voteState;                // [QL] current vote state
    int complaints;               // [QL]
    int complaintClient;          // [QL]
    int complaintEndTime;         // [QL]
    int damageFromTeammates;      // [QL]
    int damageToTeammates;        // [QL]
    qboolean ready;               // [QL] ready-up state
    int autoaction;               // [QL]
    int timeouts;                 // [QL]
    int enterTime;                // level.time the client entered the game
    playerTeamState_t teamState;  // status in teamplay games
    int damageResidual;           // [QL]
    int inactivityTime;           // [QL] moved from gclient_s
    int inactivityWarning;        // [QL] moved from gclient_s
    int lastUserinfoUpdate;       // [QL]
    int userInfoFloodInfractions; // [QL]
    int lastMapVoteTime;          // [QL]
    int lastMapVoteIndex;         // [QL]
    int roundDamageTaken;         // [QL]
    int roundDamageDealt;         // [QL]
    int roundShotsHit;            // [QL]
    int roundKillCount;           // [QL]
    int localPlayerSpawnTime;     // [QL]
    int caDamageAccum;            // [QL] CA: accumulated damage for +1 score per 100
    qboolean teamInfo;            // send team overlay updates?
} clientPersistant_t;

// [QL] Per-player expanded statistics (812 bytes)
typedef struct {
    uint32_t    statId;
    int         lastThinkTime;
    int         teamJoinTime;
    int         totalPlayTime;
    int         serverRank;
    qboolean    serverRankIsTied;
    int         teamRank;
    qboolean    teamRankIsTied;
    int         numKills;
    int         numDeaths;
    int         numSuicides;
    int         numTeamKills;
    int         numTeamKilled;
    int         numWeaponKills[16];
    int         numWeaponDeaths[16];
    int         shotsFired[16];
    int         shotsHit[16];
    int         damageDealt[16];
    int         damageTaken[16];
    int         powerups[16];
    int         holdablePickups[7];
    int         weaponPickups[16];
    int         weaponUsageTime[16];
    int         numCaptures;
    int         numAssists;
    int         numDefends;
    int         numHolyShits;
    int         totalDamageDealt;
    int         totalDamageTaken;
    int         previousHealth;
    int         previousArmor;
    int         numAmmoPickups;
    int         numFirstMegaHealthPickups;
    int         numMegaHealthPickups;
    int         megaHealthPickupTime;
    int         numHealthPickups;
    int         numFirstRedArmorPickups;
    int         numRedArmorPickups;
    int         redArmorPickupTime;
    int         numFirstYellowArmorPickups;
    int         numYellowArmorPickups;
    int         yellowArmorPickupTime;
    int         numFirstGreenArmorPickups;
    int         numGreenArmorPickups;
    int         greenArmorPickupTime;
    int         numQuadDamagePickups;
    int         numQuadDamageKills;
    int         numBattleSuitPickups;
    int         numRegenerationPickups;
    int         numHastePickups;
    int         numInvisibilityPickups;
    int         numRedFlagPickups;
    int         numBlueFlagPickups;
    int         numNeutralFlagPickups;
    int         numMedkitPickups;
    int         numArmorPickups;
    int         numDenials;
    int         killStreak;
    int         maxKillStreak;
    int         xp;
    int         domThreeFlagsTime;
    int         numMidairShotgunKills;
    int         roundHits;
} expandedStatObj_t;

// [QL] Per-client race data (552 bytes)
typedef struct {
    qboolean    racingActive;
    int         startTime;
    int         lastTime;
    int         best_race[64];
    int         current_race[64];
    int         currentCheckPoint;
    qboolean    weaponUsed;
    int         _pad;
    gentity_t   *nextRacePoint;
    gentity_t   *nextRacePoint2;
} raceInfo_t;

// [QL] this structure is cleared on each ClientSpawn(),
// except for 'client->pers' and 'client->sess'
struct gclient_s {
    // ps MUST be the first element, because the server expects it
    playerState_t ps;  // communicated by server to clients

    // the rest of the structure is private to game
    clientPersistant_t pers;
    clientSession_t sess;

    qboolean noclip;

    int lastCmdTime;  // level.time of last usercmd_t, for EF_CONNECTION
    int buttons;
    int oldbuttons;

    // sum up damage over an entire frame
    int damage_armor;           // damage absorbed by armor
    int damage_blood;           // damage taken out of health
    vec3_t damage_from;         // origin for vector calculation
    qboolean damage_fromWorld;  // if true, don't use the damage_from vector

    int impressiveCount;        // [QL]
    int accurateCount;          // for "impressive" reward sound

    int accuracy_shots;  // total number of shots
    int accuracy_hits;   // total number of hits

    int lastClientKilled;       // [QL]
    int lastkilled_client;      // last client that this client killed
    int lasthurt_client[2];     // [QL] expanded to 2-element array
    int lasthurt_mod[2];        // [QL] expanded to 2-element array
    int lasthurt_time[2];       // [QL]

    int lastKillTime;           // for multiple kill rewards
    int lastGibTime;            // [QL]
    int rampageCounter;         // [QL]
    int revengeCounter[64];     // [QL] per-client revenge tracking

    // timers
    int respawnTime;             // can respawn when time > this
    int rewardTime;              // clear the EF_AWARD_IMPRESSIVE, etc when time > this
    int airOutTime;

    qboolean fireHeld;  // used for hook
    gentity_t* hook;     // grapple hook if out

    int switchTeamTime;  // time the player switched teams

    // timeResidual is used to handle events that happen every second
    int timeResidual;
    int timeResidualScout;      // [QL]
    int timeResidualArmor;      // [QL]
    int timeResidualHealth;     // [QL]
    int timeResidualPingPOI;    // [QL]
    int timeResidualSpecInfo;   // [QL]
    qboolean healthRegenActive; // [QL]
    qboolean armorRegenActive;  // [QL]

    gentity_t* persistantPowerup;
    int portalID;
    int ammoTimes[WP_NUM_WEAPONS];
    int invulnerabilityTime;

    expandedStatObj_t expandedStats;  // [QL] (812 bytes)

    int ignoreChatsTime;        // [QL]
    int lastUserCmdTime;        // [QL]
    qboolean freezePlayer;      // [QL]
    int deferredSpawnTime;      // [QL]
    int deferredSpawnCount;     // [QL]
    raceInfo_t race;            // [QL] (552 bytes)
    int damagePlum[MAX_CLIENTS]; // [QL] accumulated SG damage per target for EV_DAMAGEPLUM
    int shotgunDmg[64];         // [QL]
    int round_shots;            // [QL]
    int round_hits;             // [QL]
    int round_damage;           // [QL]
    qboolean queuedSpectatorFollow; // [QL]
    int queuedSpectatorClient;  // [QL]

    char* areabits;
};

//
// this structure is cleared as each map is entered
//
#define MAX_SPAWN_VARS 64
#define MAX_SPAWN_VARS_CHARS 4096

// [QL] round state (36 bytes)
typedef struct {
    roundStateState_t   eCurrent;
    roundStateState_t   eNext;
    int                 tNext;
    int                 startTime;
    int                 turn;
    int                 round;
    team_t              prevRoundWinningTeam;
    qboolean            touch;
    qboolean            capture;
} roundState_t;

typedef struct {
    gclient_t       *clients;
    gentity_t       *gentities;
    int             gentitySize;
    int             num_entities;
    int             warmupTime;
    fileHandle_t    logFile;
    int             maxclients;
    int             time;
    int             frametime;              /* [QL] */
    int             startTime;
    int             teamScores[4];
    int             nextTeamInfoTime;       /* [QL] */
    qboolean        newSession;
    qboolean        restarted;
    qboolean        shufflePending;         /* [QL] */
    int             shuffleReadyTime;       /* [QL] */
    int             numConnectedClients;
    int             numNonSpectatorClients;
    int             numPlayingClients;
    int             numReadyClients;        /* [QL] */
    int             numReadyHumans;         /* [QL] */
    int             numStandardClients;     /* [QL] */
    int             sortedClients[64];
    int             follow1;
    int             follow2;
    int             snd_fry;
    int             warmupModificationCount;
    char            voteString[1024];
    char            voteDisplayString[1024];
    int             voteExecuteTime;
    int             voteTime;
    int             voteYes;
    int             voteNo;
    int             pendingVoteCaller;      /* [QL] */
    qboolean        spawning;
    int             numSpawnVars;
    char            *spawnVars[64][2];
    int             numSpawnVarChars;
    char            spawnVarChars[4096];
    int             intermissionQueued;
    int             intermissionTime;
    qboolean        readyToExit;
    qboolean        votingEnded;            /* [QL] */
    int             exitTime;
    vec3_t          intermission_origin;
    vec3_t          intermission_angle;
    qboolean        locationLinked;
    /* 4 bytes padding */
    gentity_t       *locationHead;
    int             timePauseBegin;         /* [QL] */
    int             timeOvertime;           /* [QL] */
    int             timeInitialPowerupSpawn; /* [QL] */
    int             bodyQueIndex;
    gentity_t       *bodyQue[BODY_QUEUE_SIZE];
    int             portalSequence;
    qboolean        gameStatsReported;      /* [QL] */
    qboolean        scoringDisabled;        /* [QL] round-based: scoring off between rounds */
    qboolean        mapIsTrainingMap;       /* [QL] */
    int             clientNum1stPlayer;     /* [QL] */
    int             clientNum2ndPlayer;     /* [QL] */
    char            scoreboardArchive1[1024]; /* [QL] */
    char            scoreboardArchive2[1024]; /* [QL] */
    char            firstScorer[40];        /* [QL] */
    char            lastScorer[40];         /* [QL] */
    char            lastTeamScorer[40];     /* [QL] */
    char            firstFrag[40];          /* [QL] */
    vec3_t          red_flag_origin;        /* [QL] */
    vec3_t          blue_flag_origin;       /* [QL] */
    int             spawnCount[4];          /* [QL] */
    int             runeSpawns[5];          /* [QL] */
    int             itemCount[60];          /* [QL] */
    int             suddenDeathRespawnDelay; /* [QL] */
    int             suddenDeathRespawnDelayLastAnnounced; /* [QL] */
    int             numRedArmorPickups[4];
    int             numYellowArmorPickups[4];
    int             numGreenArmorPickups[4];
    int             numMegaHealthPickups[4];
    int             numQuadDamagePickups[4];
    int             numBattleSuitPickups[4];
    int             numRegenerationPickups[4];
    int             numHastePickups[4];
    int             numInvisibilityPickups[4];
    int             quadDamagePossessionTime[4];
    int             battleSuitPossessionTime[4];
    int             regenerationPossessionTime[4];
    int             hastePossessionTime[4];
    int             invisibilityPossessionTime[4];
    int             numFlagPickups[4];
    int             numMedkitPickups[4];
    int             flagPossessionTime[4];
    /* [QL] Domination */
    gentity_t       *dominationPoints[5];
    int             dominationPointCount;
    int             dominationPointsTallied;
    /* [QL] Race */
    int             racePointCount;
    /* [QL] Misc */
    qboolean        disableDropWeapon;
    qboolean        teamShuffleActive;
    int             lastTeamScores[4];
    int             lastTeamRoundScores[4];
    /* [QL] Attack & Defend */
    team_t          attackingTeam;
    roundState_t    roundState;             /* 36 bytes */
    int             lastTeamCountSent;
    /* [QL] Red Rover */
    int             infectedConscript;
    int             lastZombieSurvivor;
    int             zombieScoreTime;
    int             lastInfectionTime;
    /* [QL] Intermission map voting */
    char            intermissionMapNames[3][1024];
    char            intermissionMapTitles[3][1024];
    char            intermissionMapConfigs[3][1024];
    int             intermissionMapVotes[3];
    /* [QL] Match state */
    qboolean        matchForfeited;
    int             allReadyTime;
    qboolean        notifyCvarChange;
    int             notifyCvarChangeTime;
    int             lastLeadChangeTime;
    int             lastLeadChangeClient;
    int             lastLeadChangeElapsedTime;
    int             rrInfectedCounters[4];  /* [QL] Red Rover infection tracking */
    int             rrInfectedSpreadStart;  /* [QL] */
    int             rrSurvivorScoreTimer;   /* [QL] */
} level_locals_t;

//
// g_spawn.c
//
qboolean G_SpawnString(const char* key, const char* defaultString, char** out);
// spawn string returns a temporary reference, you must CopyString() if you want to keep it
qboolean G_SpawnFloat(const char* key, const char* defaultString, float* out);
qboolean G_SpawnInt(const char* key, const char* defaultString, int* out);
qboolean G_SpawnVector(const char* key, const char* defaultString, float* out);
void G_SpawnEntitiesFromString(void);
char* G_NewString(const char* string);

//
// g_cmds.c
//
void Cmd_Score_f(gentity_t* ent);
void StopFollowing(gentity_t* ent);
void BroadcastTeamChange(gclient_t* client, int oldTeam);
void SetTeam(gentity_t* ent, const char* s);
void Cmd_FollowCycle_f(gentity_t* ent, int dir);
void ClearVote(void);

//
// g_items.c
//
void G_CheckTeamItems(void);
void G_RunItem(gentity_t* ent);
void RespawnItem(gentity_t* ent);
void G_RespawnKey(int keyTag);

void UseHoldableItem(gentity_t* ent);
void PrecacheItem(gitem_t* it);
gentity_t* Drop_Item(gentity_t* ent, gitem_t* item, float angle);
gentity_t* LaunchItem(gitem_t* item, vec3_t origin, vec3_t velocity);
void SetRespawn(gentity_t* ent, float delay);
void G_SpawnItem(gentity_t* ent, gitem_t* item);
void FinishSpawningItem(gentity_t* ent);
void Think_Weapon(gentity_t* ent);
int ArmorIndex(gentity_t* ent);
void Add_Ammo(gentity_t* ent, int weapon, int count);
void Touch_Item(gentity_t* ent, gentity_t* other, trace_t* trace);

void ClearRegisteredItems(void);
void RegisterItem(gitem_t* item);
void SaveRegisteredItems(void);

//
// g_utils.c
//
int G_ModelIndex(char* name);
int G_SoundIndex(char* name);
void G_TeamCommand(team_t team, char* cmd);
void G_KillBox(gentity_t* ent);
gentity_t* G_Find(gentity_t* from, int fieldofs, const char* match);
gentity_t* G_PickTarget(char* targetname);
void G_UseTargets(gentity_t* ent, gentity_t* activator);
void G_SetMovedir(vec3_t angles, vec3_t movedir);

void G_InitGentity(gentity_t* e);
gentity_t* G_Spawn(void);
gentity_t* G_TempEntity(vec3_t origin, int event);
void G_Sound(gentity_t* ent, int channel, int soundIndex);
void G_FreeEntity(gentity_t* e);
qboolean G_EntitiesFree(void);

void G_TouchTriggers(gentity_t* ent);

float* tv(float x, float y, float z);
char* vtos(const vec3_t v);

float vectoyaw(const vec3_t vec);

void G_AddPredictableEvent(gentity_t* ent, int event, int eventParm);
void G_AddEvent(gentity_t* ent, int event, int eventParm);
void G_SetOrigin(gentity_t* ent, vec3_t origin);
void AddRemap(const char* oldShader, const char* newShader, float timeOffset);
const char* BuildShaderStateConfig(void);

//
// g_combat.c
//
qboolean CanDamage(gentity_t* targ, vec3_t origin);
void G_Damage(gentity_t* targ, gentity_t* inflictor, gentity_t* attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod);
qboolean G_RadiusDamage(vec3_t origin, gentity_t* inflictor, gentity_t* attacker, float damage, float radius, gentity_t* ignore, int dflags, int mod);
qboolean G_RadiusDamageThrough(vec3_t origin, gentity_t* inflictor, gentity_t* attacker, float damage, float radius, gentity_t* ignore, int dflags, int mod);
qboolean G_WaterRadiusDamage(vec3_t origin, gentity_t *attacker, float damage, float radius);
int G_InvulnerabilityEffect(gentity_t* targ, vec3_t dir, vec3_t point, vec3_t impactpoint, vec3_t bouncedir);
void body_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int meansOfDeath);
void TossClientItems(gentity_t* self);
void TossClientPersistantPowerups(gentity_t* self);
void TossClientCubes(gentity_t* self);

// damage flags
#define DAMAGE_RADIUS 0x00000001              // damage was indirect
#define DAMAGE_NO_ARMOR 0x00000002            // armour does not protect from this damage
#define DAMAGE_NO_KNOCKBACK 0x00000004        // do not affect velocity, just view angles
#define DAMAGE_NO_PROTECTION 0x00000008       // armor, shields, invulnerability, and godmode have no effect
#define DAMAGE_NO_TEAM_PROTECTION 0x00000010  // armor, shields, invulnerability, and godmode have no effect

//
// g_missile.c
//
void G_RunMissile(gentity_t* ent);

gentity_t* fire_plasma(gentity_t* self, vec3_t start, vec3_t aimdir);
gentity_t* fire_grenade(gentity_t* self, vec3_t start, vec3_t aimdir);
gentity_t* fire_rocket(gentity_t* self, vec3_t start, vec3_t dir);
gentity_t* fire_bfg(gentity_t* self, vec3_t start, vec3_t dir);
gentity_t* fire_grapple(gentity_t* self, vec3_t start, vec3_t dir);
gentity_t* fire_nail(gentity_t* self, vec3_t start, vec3_t forward, vec3_t right, vec3_t up);
gentity_t* fire_prox(gentity_t* self, vec3_t start, vec3_t aimdir);

//
// g_mover.c
//
void G_RunMover(gentity_t* ent);
void Touch_DoorTrigger(gentity_t* ent, gentity_t* other, trace_t* trace);

//
// g_trigger.c
//
void trigger_teleporter_touch(gentity_t* self, gentity_t* other, trace_t* trace);

//
// g_misc.c
//
void TeleportPlayer(gentity_t* player, vec3_t origin, vec3_t angles);
void DropPortalSource(gentity_t* ent);
void DropPortalDestination(gentity_t* ent);

//
// g_weapon.c
//
qboolean LogAccuracyHit(gentity_t* target, gentity_t* attacker);
void CalcMuzzlePoint(gentity_t* ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint);
void SnapVectorTowards(vec3_t v, vec3_t to);
qboolean CheckGauntletAttack(gentity_t* ent);
void Weapon_HookFree(gentity_t* ent);
void Weapon_HookThink(gentity_t* ent);

//
// g_client.c
//
int TeamCount(int ignoreClientNum, team_t team);
int TeamLeader(int team);
team_t PickTeam(int ignoreClientNum);
void SetClientViewAngle(gentity_t* ent, vec3_t angle);
gentity_t* SelectSpawnPoint(vec3_t avoidPoint, vec3_t origin, vec3_t angles, qboolean isbot);
void CopyToBodyQue(gentity_t* ent);
void ClientRespawn(gentity_t* ent);
void BeginIntermission(void);
void InitBodyQue(void);
void ClientSpawn(gentity_t* ent);
void player_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int mod);
void AddScore(gentity_t* ent, vec3_t origin, int score);
void CalculateRanks(void);
qboolean SpotWouldTelefrag(gentity_t* spot);

//
// g_svcmds.c
//
qboolean ConsoleCommand(void);
void G_ProcessIPBans(void);
qboolean G_FilterPacket(char* from);

//
// g_weapon.c
//
void FireWeapon(gentity_t* ent);
void G_StartKamikaze(gentity_t* ent);

//
// g_cmds.c
//
void DeathmatchScoreboardMessage(gentity_t* ent);
int STAT_GetBestWeapon(gclient_t *cl);
void FFAScoreboardMessage_impl(void);
void DuelScoreboardMessage_impl(void);
void DuelScoreboardMessage(gentity_t *ent);
void RaceScoreboardMessage(gentity_t *ent);
void TDMScoreboardMessage(gentity_t *ent);
void TDMScoreboardMessage_impl(gentity_t *ent);
void CAScoreboardMessage(gentity_t *ent);
void CTFScoreboardMessage(gentity_t *ent);
void CTFScoreboardMessage_impl(gentity_t *ent);
void FTScoreboardMessage(gentity_t *ent);
void FTScoreboardMessage_impl(gentity_t *ent);
void RRScoreboardMessage(gentity_t *ent);
void ADScoreboardMessage(gentity_t *ent);
void SendScoreboardMessageToTeam(int team);
void AddDuelScore(void);

//
// g_main.c
//
void MoveClientToIntermission(gentity_t* ent);
void FindIntermissionPoint(void);
void SetLeader(int team, int client);
void CheckTeamLeader(int team);
void G_RunThink(gentity_t* ent);
void AddTournamentQueue(gclient_t* client);
void SetWarmupState(int warmupTime);
void QDECL G_LogPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void LogExit(int isShutdown, int restart, const char* string);
void SendScoreboardMessageToAllClients(void);
void QDECL G_Printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void QDECL G_Error(const char* fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void G_ShutdownGame(int restart);
void G_InitGame(int levelTime, int randomSeed, int restart);
void G_RunFrame(int levelTime);

// g_gametype_common.c - Shared gametype helpers
int STAT_GetBestWeapon(gclient_t *cl);

// g_gametype_duel.c - Duel / Tournament
void CheckTournament(void);
void AddDuelScore(void);
void AddTournamentPlayer(void);

// g_gametype_ctf.c - Capture The Flag
void CTF_CheckTeamItems(void);

// g_gametype_1fctf.c - One Flag CTF
void OneFCTF_CheckTeamItems(void);

// g_gametype_obelisk.c - Overload
void Obelisk_CheckTeamItems(void);

// g_gametype_har.c - Harvester
void Harvester_CheckTeamItems(void);
void Harvester_RegisterItems(void);

// g_gametype_race.c - Race mode checkpoint system
void SP_race_point(gentity_t *ent);
void Touch_RaceCheckpoint(gentity_t *self, gentity_t *other, trace_t *trace);
int Race_GetNumPoints(void);
void Race_ResetCheckpoints(gentity_t *playerEnt);

// g_gametype_ad.c - Attack & Defend round state machine
void AD_RoundStateTransition(void);
qboolean AD_CheckExitRules(int doExit);
void AD_RunFrame(void);
int AD_IsInPlayState(void);
int AD_CanScore(void);

// [QL] RR infection cvars
extern vmCvar_t g_rrInfectedSpreadTime;
extern vmCvar_t g_rrInfectedSpreadWarningTime;
extern vmCvar_t g_rrInfectedSurvivorScoreMethod;
extern vmCvar_t g_rrInfectedSurvivorScoreRate;
extern vmCvar_t g_rrInfectedSurvivorScoreBonus;

// g_gametype_rr.c - Red Rover round state machine
void RR_RoundStateTransition(void);
void RR_RunFrame(void);
void RR_OnPlayerDeath(gentity_t *victim);
void RR_CheckInfection(void);
void RR_SurvivalBonus(int mode);

// g_gametype_ft.c - Freeze Tag round state machine
void Freeze_RoundStateTransition(void);
void Freeze_Think(void);
void Freeze_ClientThawCheck(gentity_t *ent);
void Freeze_PlayerFrozen(gentity_t *self);
qboolean Freeze_TeamFrozen(int team);
qboolean Freeze_GameIsOver(int *aliveCounts, int *healthTotals);

// g_gametype_ca.c - Clan Arena round state machine
void CA_RoundStateTransition(void);
qboolean CA_CheckExitRules(int doExit);
qboolean CA_AccuracyMessage(gentity_t *target, gentity_t *attacker, int *damage, int *knockback);
void CA_RunFrame(void);
void CA_PlayerDied(gentity_t *self);
void G_RegisterCvars(void);

//
// g_client.c
//
char* ClientConnect(int clientNum, qboolean firstTime, qboolean isBot);
void ClientUserinfoChanged(int clientNum);
void ClientDisconnect(int clientNum);
void ClientBegin(int clientNum);
void ClientCommand(int clientNum);
void ClientBegin_Race(gentity_t* ent);
void ClientBegin_RoundBased(gentity_t* ent);
void ClientBegin_Freeze(gentity_t* ent);
void ClientBegin_RedRover(gentity_t* ent);
void SelectSpawnWeapon(gentity_t* ent);
void UpdateTeamAliveCount(int *outRed, int *outBlue);
int G_GetAccessLevel(const char* ip);
void G_ReleaseGrapple(gentity_t* ent);
void STAT_InitClient(gentity_t* ent);
void STAT_PublishClientDisconnect(gclient_t* client, int reason);
void STAT_SubscribeClient(gentity_t* ent);
void STAT_LogDisconnect(int clientNum);
void STAT_MatchEnd(void);
qboolean G_IsTeamLocked(team_t team);

//
// g_active.c
//
void ClientThink(int clientNum);
void ClientEndFrame(gentity_t* ent);
void G_RunClient(gentity_t* ent);
qboolean ClientIsReadyToExit(int clientNum);  // [QL] readyToExit tracking
void ClearClientReadyToExit(void);            // [QL] reset at intermission start

//
// g_team.c
//
qboolean OnSameTeam(gentity_t* ent1, gentity_t* ent2);
void Team_CheckDroppedItem(gentity_t* dropped);
qboolean CheckObeliskAttack(gentity_t* obelisk, gentity_t* attacker);

//
// g_mem.c
//
void* G_Alloc(int size);
void G_InitMemory(void);
void Svcmd_GameMem_f(void);

//
// g_session.c
//
void G_ReadSessionData(gclient_t* client);
void G_InitSessionData(gclient_t* client, char* userinfo);

void G_InitWorldSession(void);
void G_WriteSessionData(void);

//
// g_arenas.c
//
void UpdateTournamentInfo(void);
void SpawnModelsOnVictoryPads(void);
void Svcmd_AbortPodium_f(void);

//
// g_bot.c
//
void G_InitBots(qboolean restart);
char* G_GetBotInfoByNumber(int num);
char* G_GetBotInfoByName(const char* name);
void G_CheckBotSpawn(void);
void G_RemoveQueuedBotBegin(int clientNum);
qboolean G_BotConnect(int clientNum, qboolean restart);
void Svcmd_AddBot_f(void);
void Svcmd_BotList_f(void);
void BotInterbreedEndMatch(void);

// ai_main.c
#define MAX_FILEPATH 144

// bot settings
typedef struct bot_settings_s {
    char characterfile[MAX_FILEPATH];
    float skill;
} bot_settings_t;

int BotAISetup(int restart);
int BotAIShutdown(int restart);
int BotAILoadMap(int restart);
int BotAISetupClient(int client, struct bot_settings_s* settings, qboolean restart);
int BotAIShutdownClient(int client, qboolean restart);
int BotAIStartFrame(int time);
void BotTestAAS(vec3_t origin);

#include "g_team.h"  // teamplay specific stuff

extern level_locals_t level;
extern gentity_t g_entities[MAX_GENTITIES];
extern gclient_t g_clients[MAX_CLIENTS];
extern gameImport_t *imports;  // [QL] engine syscall table

#define FOFS(x) ((size_t)&(((gentity_t*)0)->x))

extern vmCvar_t g_gametype;
extern vmCvar_t g_dedicated;
extern vmCvar_t g_cheats;
extern vmCvar_t g_maxclients;      // allow this many total, including spectators
extern vmCvar_t g_maxGameClients;  // allow this many active
extern vmCvar_t g_restarted;

extern vmCvar_t g_dmflags;
extern vmCvar_t g_fraglimit;
extern vmCvar_t g_timelimit;
extern vmCvar_t g_capturelimit;
extern vmCvar_t g_friendlyFire;
extern vmCvar_t g_password;
extern vmCvar_t g_needpass;
extern vmCvar_t g_gravity;
extern vmCvar_t g_speed;
extern vmCvar_t g_knockback;
extern vmCvar_t g_quadfactor;
extern vmCvar_t g_forcerespawn;
extern vmCvar_t g_inactivity;
extern vmCvar_t g_debugMove;
extern vmCvar_t g_debugAlloc;
extern vmCvar_t g_debugDamage;
extern vmCvar_t g_weaponRespawn;
extern vmCvar_t g_weaponTeamRespawn;
extern vmCvar_t g_synchronousClients;
extern vmCvar_t g_motd;
extern vmCvar_t g_warmup;
extern vmCvar_t g_doWarmup;
extern vmCvar_t g_gameState;
extern vmCvar_t g_warmupReadyPercentage;
extern vmCvar_t g_forfeit;
extern vmCvar_t g_blood;
extern vmCvar_t g_allowVote;
extern vmCvar_t g_allowVoteMidGame;
extern vmCvar_t g_allowSpecVote;
extern vmCvar_t g_voteFlags;
extern vmCvar_t g_voteDelay;
extern vmCvar_t g_voteLimit;
extern vmCvar_t g_teamAutoJoin;
extern vmCvar_t g_teamForceBalance;
extern vmCvar_t g_banIPs;
extern vmCvar_t g_filterBan;
extern vmCvar_t g_obeliskHealth;
extern vmCvar_t g_obeliskRegenPeriod;
extern vmCvar_t g_obeliskRegenAmount;
extern vmCvar_t g_obeliskRespawnDelay;
extern vmCvar_t g_cubeTimeout;
extern vmCvar_t g_smoothClients;
extern vmCvar_t g_knockback_z;
extern vmCvar_t g_knockback_z_self;
extern vmCvar_t g_knockback_cripple;
extern vmCvar_t g_enableDust;
extern vmCvar_t g_enableBreath;
extern vmCvar_t g_proxMineTimeout;
extern vmCvar_t g_localTeamPref;
extern vmCvar_t g_infiniteAmmo;
extern vmCvar_t g_dropFlag;
extern vmCvar_t g_runes;

// [QL] per-weapon damage cvars
extern vmCvar_t g_damage_g;
extern vmCvar_t g_damage_mg;
extern vmCvar_t g_damage_sg;
extern vmCvar_t g_damage_gl;
extern vmCvar_t g_damage_rl;
extern vmCvar_t g_damage_lg;
extern vmCvar_t g_damage_rg;
extern vmCvar_t g_damage_pg;
extern vmCvar_t g_damage_bfg;
extern vmCvar_t g_damage_gh;
extern vmCvar_t g_damage_ng;
extern vmCvar_t g_damage_cg;
extern vmCvar_t g_damage_hmg;
extern vmCvar_t g_damage_pl;

// [QL] per-weapon advanced cvars
extern vmCvar_t g_damage_sg_outer;
extern vmCvar_t g_damage_sg_falloff;
extern vmCvar_t g_range_sg_falloff;
extern vmCvar_t g_damage_lg_falloff;
extern vmCvar_t g_range_lg_falloff;
extern vmCvar_t g_headShotDamage_rg;
extern vmCvar_t g_railJump;

// [QL] per-weapon knockback multiplier cvars
extern vmCvar_t g_knockback_g;
extern vmCvar_t g_knockback_mg;
extern vmCvar_t g_knockback_sg;
extern vmCvar_t g_knockback_gl;
extern vmCvar_t g_knockback_rl;
extern vmCvar_t g_knockback_rl_self;
extern vmCvar_t g_knockback_lg;
extern vmCvar_t g_knockback_rg;
extern vmCvar_t g_knockback_pg;
extern vmCvar_t g_knockback_pg_self;
extern vmCvar_t g_knockback_bfg;
extern vmCvar_t g_knockback_gh;
extern vmCvar_t g_knockback_ng;
extern vmCvar_t g_knockback_pl;
extern vmCvar_t g_knockback_cg;
extern vmCvar_t g_knockback_hmg;

// [QL] weapon reload (fire interval) cvars
extern vmCvar_t weapon_reload_mg;
extern vmCvar_t weapon_reload_sg;
extern vmCvar_t weapon_reload_gl;
extern vmCvar_t weapon_reload_rl;
extern vmCvar_t weapon_reload_lg;
extern vmCvar_t weapon_reload_rg;
extern vmCvar_t weapon_reload_pg;
extern vmCvar_t weapon_reload_bfg;
extern vmCvar_t weapon_reload_gh;
extern vmCvar_t weapon_reload_ng;
extern vmCvar_t weapon_reload_prox;
extern vmCvar_t weapon_reload_cg;
extern vmCvar_t weapon_reload_hmg;

// [QL] per-weapon splash damage cvars
extern vmCvar_t g_splashdamage_gl;
extern vmCvar_t g_splashdamage_rl;
extern vmCvar_t g_splashdamage_pg;
extern vmCvar_t g_splashdamage_bfg;
extern vmCvar_t g_splashdamage_pl;
extern vmCvar_t g_splashdamageOffset;
extern vmCvar_t g_rocketsplashOffset;

// [QL] knockback cap cvar
extern vmCvar_t g_max_knockback;

// [QL] per-weapon splash radius cvars
extern vmCvar_t g_splashradius_gl;
extern vmCvar_t g_splashradius_rl;
extern vmCvar_t g_splashradius_pg;
extern vmCvar_t g_splashradius_bfg;
extern vmCvar_t g_splashradius_pl;

// [QL] projectile velocity cvars
extern vmCvar_t g_velocity_gl;
extern vmCvar_t g_velocity_rl;
extern vmCvar_t g_velocity_pg;
extern vmCvar_t g_velocity_bfg;
extern vmCvar_t g_velocity_gh;

// [QL] projectile acceleration cvars
extern vmCvar_t g_accelFactor_rl;
extern vmCvar_t g_accelFactor_pg;
extern vmCvar_t g_accelFactor_bfg;
extern vmCvar_t g_accelRate_rl;
extern vmCvar_t g_accelRate_pg;
extern vmCvar_t g_accelRate_bfg;

// [QL] projectile gravity cvars
extern vmCvar_t weapon_gravity_rl;
extern vmCvar_t weapon_gravity_pg;
extern vmCvar_t weapon_gravity_bfg;
extern vmCvar_t weapon_gravity_ng;

// [QL] nailgun cvars
extern vmCvar_t g_nailspeed;
extern vmCvar_t g_nailcount;
extern vmCvar_t g_nailspread;
extern vmCvar_t g_nailbounce;
extern vmCvar_t g_nailbouncepercentage;

// [QL] guided rocket cvar
extern vmCvar_t g_guidedRocket;

// [QL] pmove cvars (movement physics)
extern vmCvar_t pmove_AirControl;
extern vmCvar_t pmove_AutoHop;
extern vmCvar_t pmove_CrouchSlide;
extern vmCvar_t pmove_DoubleJump;
extern vmCvar_t pmove_NoPlayerClip;

// [QL] loadout system
extern vmCvar_t g_loadout;

// [QL] ammo system
extern vmCvar_t g_ammoPack;
extern vmCvar_t g_ammoRespawn;
extern vmCvar_t g_spawnItemAmmo;

// [QL] serverinfo cvars
extern vmCvar_t g_teamsize;
extern vmCvar_t g_teamSizeMin;
extern vmCvar_t g_overtime;
extern vmCvar_t g_scorelimit;
extern vmCvar_t g_mercylimit;
extern vmCvar_t g_mercytime;
extern vmCvar_t g_rrAllowNegativeScores;
extern vmCvar_t g_spawnItems;
extern vmCvar_t sv_quitOnExitLevel;
extern vmCvar_t g_isBotOnly;
extern vmCvar_t g_training;
extern vmCvar_t g_itemHeight;
extern vmCvar_t g_itemTimers;
extern vmCvar_t g_quadDamageFactor;
extern vmCvar_t g_freezeRoundDelay;
extern vmCvar_t g_timeoutCount;

// [QL] damage-through-surface
extern vmCvar_t g_forceDmgThroughSurface;
extern vmCvar_t g_dmgThroughSurfaceDistance;
extern vmCvar_t g_dmgThroughSurfaceDampening;
extern vmCvar_t g_dmgThroughSurfaceAngularThreshold;

// [QL] player cylinder hitbox
extern vmCvar_t g_playerCylinders;

// [QL] server framerate
extern vmCvar_t sv_fps;

// [QL] round-based game mode cvars
extern vmCvar_t roundtimelimit;
extern vmCvar_t g_roundWarmupDelay;
extern vmCvar_t roundlimit;
extern vmCvar_t g_accuracyFlags;
extern vmCvar_t g_lastManStandingWarning;
extern vmCvar_t g_roundDrawLivingCount;
extern vmCvar_t g_roundDrawHealthCount;
extern vmCvar_t g_spawnArmor;
extern vmCvar_t g_adElimScoreBonus;

// [QL] freeze tag cvars
extern vmCvar_t g_freezeThawTick;
extern vmCvar_t g_freezeProtectedSpawnTime;
extern vmCvar_t g_freezeThawTime;
extern vmCvar_t g_freezeAutoThawTime;
extern vmCvar_t g_freezeThawRadius;
extern vmCvar_t g_freezeThawThroughSurface;
extern vmCvar_t g_freezeThawWinningTeam;
extern vmCvar_t g_freezeRemovePowerupsOnRound;
extern vmCvar_t g_freezeResetHealthOnRound;
extern vmCvar_t g_freezeResetArmorOnRound;
extern vmCvar_t g_freezeResetWeaponsOnRound;
extern vmCvar_t g_freezeAllowRespawn;

// [QL] lag compensation
extern vmCvar_t g_lagHaxHistory;
extern vmCvar_t g_lagHaxMs;

// g_unlagged.c - lag compensation (HAX_* system)
void HAX_Init(void);
void HAX_Clear(gentity_t *ent);
void HAX_Update(gentity_t *ent);
void HAX_Begin(gentity_t *shooter, int commandTime);
void HAX_End(gentity_t *shooter);

// [QL] weapon modifiers
extern vmCvar_t g_ironsights_mg;

// [QL] Quad Hog
extern vmCvar_t g_quadHog;
extern vmCvar_t g_quadHogTime;
extern vmCvar_t g_damagePlums;
void G_QG_Touch_Item(gentity_t *ent, gentity_t *other, trace_t *trace);
gentity_t *G_QG_RespawnQuad(vec3_t origin);
void G_QG_Think(void);

// [QL] starting loadout cvars
extern vmCvar_t g_startingWeapons;
extern vmCvar_t g_startingHealth;
extern vmCvar_t g_startingHealthBonus;
extern vmCvar_t g_startingArmor;
extern vmCvar_t armor_tiered;
extern vmCvar_t g_startingAmmo_g;
extern vmCvar_t g_startingAmmo_mg;
extern vmCvar_t g_startingAmmo_sg;
extern vmCvar_t g_startingAmmo_gl;
extern vmCvar_t g_startingAmmo_rl;
extern vmCvar_t g_startingAmmo_lg;
extern vmCvar_t g_startingAmmo_rg;
extern vmCvar_t g_startingAmmo_pg;
extern vmCvar_t g_startingAmmo_bfg;
extern vmCvar_t g_startingAmmo_gh;
extern vmCvar_t g_startingAmmo_ng;
extern vmCvar_t g_startingAmmo_pl;
extern vmCvar_t g_startingAmmo_cg;
extern vmCvar_t g_startingAmmo_hmg;

void trap_Print(const char* text);
void trap_Error(const char* fmt, ...) __attribute__((noreturn));
int trap_Milliseconds(void);
int trap_RealTime(qtime_t* qtime);
int trap_Argc(void);
void trap_Argv(int n, char* buffer, int bufferLength);
void trap_Args(char* buffer, int bufferLength);
int trap_FS_FOpenFile(const char* qpath, fileHandle_t* f, fsMode_t mode);
void trap_FS_Read(void* buffer, int len, fileHandle_t f);
void trap_FS_Write(const void* buffer, int len, fileHandle_t f);
void trap_FS_FCloseFile(fileHandle_t f);
int trap_FS_GetFileList(const char* path, const char* extension, char* listbuf, int bufsize);
int trap_FS_Seek(fileHandle_t f, long offset, int origin);  // fsOrigin_t
void trap_SendConsoleCommand(int exec_when, const char* text);
void trap_Cvar_Register(vmCvar_t* cvar, const char* var_name, const char* value, int flags);
void trap_Cvar_Update(vmCvar_t* cvar);
void trap_Cvar_Set(const char* var_name, const char* value);
int trap_Cvar_VariableIntegerValue(const char* var_name);
float trap_Cvar_VariableValue(const char* var_name);
void trap_Cvar_VariableStringBuffer(const char* var_name, char* buffer, int bufsize);
void trap_LocateGameData(gentity_t* gEnts, int numGEntities, int sizeofGEntity_t, playerState_t* gameClients, int sizeofGameClient);
void trap_DropClient(int clientNum, const char* reason);
void trap_SendServerCommand(int clientNum, const char* text);
void trap_SetConfigstring(int num, const char* string);
void trap_GetConfigstring(int num, char* buffer, int bufferSize);
void trap_GetUserinfo(int num, char* buffer, int bufferSize);
void trap_SetUserinfo(int num, const char* buffer);
void trap_GetServerinfo(char* buffer, int bufferSize);
void trap_SetBrushModel(gentity_t* ent, const char* name);
void trap_Trace(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask);
void trap_TraceCapsule(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask);
int trap_PointContents(const vec3_t point, int passEntityNum);
qboolean trap_InPVS(const vec3_t p1, const vec3_t p2);
qboolean trap_InPVSIgnorePortals(const vec3_t p1, const vec3_t p2);
void trap_AdjustAreaPortalState(gentity_t* ent, qboolean open);
qboolean trap_AreasConnected(int area1, int area2);
void trap_LinkEntity(gentity_t* ent);
void trap_UnlinkEntity(gentity_t* ent);
int trap_EntitiesInBox(const vec3_t mins, const vec3_t maxs, int* entityList, int maxcount);
qboolean trap_EntityContact(const vec3_t mins, const vec3_t maxs, const gentity_t* ent);
int trap_BotAllocateClient(void);
void trap_BotFreeClient(int clientNum);
void trap_GetUsercmd(int clientNum, usercmd_t* cmd);
qboolean trap_GetEntityToken(char* buffer, int bufferSize);

int trap_DebugPolygonCreate(int color, int numPoints, vec3_t* points);
void trap_DebugPolygonDelete(int id);

int trap_BotLibSetup(void);
int trap_BotLibShutdown(void);
int trap_BotLibVarSet(char* var_name, char* value);
int trap_BotLibVarGet(char* var_name, char* value, int size);
int trap_BotLibDefine(char* string);
int trap_BotLibStartFrame(float time);
int trap_BotLibLoadMap(const char* mapname);
int trap_BotLibUpdateEntity(int ent, void /* struct bot_updateentity_s */* bue);
int trap_BotLibTest(int parm0, char* parm1, vec3_t parm2, vec3_t parm3);

int trap_BotGetSnapshotEntity(int clientNum, int sequence);
int trap_BotGetServerCommand(int clientNum, char* message, int size);
void trap_BotUserCommand(int client, usercmd_t* ucmd);

int trap_AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int* areas, int maxareas);
int trap_AAS_AreaInfo(int areanum, void /* struct aas_areainfo_s */* info);
void trap_AAS_EntityInfo(int entnum, void /* struct aas_entityinfo_s */* info);

int trap_AAS_Initialized(void);
void trap_AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs);
float trap_AAS_Time(void);

int trap_AAS_PointAreaNum(vec3_t point);
int trap_AAS_PointReachabilityAreaIndex(vec3_t point);
int trap_AAS_TraceAreas(vec3_t start, vec3_t end, int* areas, vec3_t* points, int maxareas);

int trap_AAS_PointContents(vec3_t point);
int trap_AAS_NextBSPEntity(int ent);
int trap_AAS_ValueForBSPEpairKey(int ent, char* key, char* value, int size);
int trap_AAS_VectorForBSPEpairKey(int ent, char* key, vec3_t v);
int trap_AAS_FloatForBSPEpairKey(int ent, char* key, float* value);
int trap_AAS_IntForBSPEpairKey(int ent, char* key, int* value);

int trap_AAS_AreaReachability(int areanum);

int trap_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags);
int trap_AAS_EnableRoutingArea(int areanum, int enable);
int trap_AAS_PredictRoute(void /*struct aas_predictroute_s*/* route, int areanum, vec3_t origin, int goalareanum, int travelflags, int maxareas, int maxtime, int stopevent, int stopcontents, int stoptfl, int stopareanum);

int trap_AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags, void /*struct aas_altroutegoal_s*/* altroutegoals, int maxaltroutegoals, int type);
int trap_AAS_Swimming(vec3_t origin);
int trap_AAS_PredictClientMovement(void /* aas_clientmove_s */* move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize);

void trap_EA_Say(int client, char* str);
void trap_EA_SayTeam(int client, char* str);
void trap_EA_Command(int client, char* command);

void trap_EA_Action(int client, int action);
void trap_EA_Gesture(int client);
void trap_EA_Talk(int client);
void trap_EA_Attack(int client);
void trap_EA_Use(int client);
void trap_EA_Respawn(int client);
void trap_EA_Crouch(int client);
void trap_EA_MoveUp(int client);
void trap_EA_MoveDown(int client);
void trap_EA_MoveForward(int client);
void trap_EA_MoveBack(int client);
void trap_EA_MoveLeft(int client);
void trap_EA_MoveRight(int client);
void trap_EA_SelectWeapon(int client, int weapon);
void trap_EA_Jump(int client);
void trap_EA_DelayedJump(int client);
void trap_EA_Move(int client, vec3_t dir, float speed);
void trap_EA_View(int client, vec3_t viewangles);

void trap_EA_EndRegular(int client, float thinktime);
void trap_EA_GetInput(int client, float thinktime, void /* struct bot_input_s */* input);
void trap_EA_ResetInput(int client);

int trap_BotLoadCharacter(char* charfile, float skill);
void trap_BotFreeCharacter(int character);
float trap_Characteristic_Float(int character, int index);
float trap_Characteristic_BFloat(int character, int index, float min, float max);
int trap_Characteristic_Integer(int character, int index);
int trap_Characteristic_BInteger(int character, int index, int min, int max);
void trap_Characteristic_String(int character, int index, char* buf, int size);

int trap_BotAllocChatState(void);
void trap_BotFreeChatState(int handle);
void trap_BotQueueConsoleMessage(int chatstate, int type, char* message);
void trap_BotRemoveConsoleMessage(int chatstate, int handle);
int trap_BotNextConsoleMessage(int chatstate, void /* struct bot_consolemessage_s */* cm);
int trap_BotNumConsoleMessages(int chatstate);
void trap_BotInitialChat(int chatstate, char* type, int mcontext, char* var0, char* var1, char* var2, char* var3, char* var4, char* var5, char* var6, char* var7);
int trap_BotNumInitialChats(int chatstate, char* type);
int trap_BotReplyChat(int chatstate, char* message, int mcontext, int vcontext, char* var0, char* var1, char* var2, char* var3, char* var4, char* var5, char* var6, char* var7);
int trap_BotChatLength(int chatstate);
void trap_BotEnterChat(int chatstate, int client, int sendto);
void trap_BotGetChatMessage(int chatstate, char* buf, int size);
int trap_StringContains(char* str1, char* str2, int casesensitive);
int trap_BotFindMatch(char* str, void /* struct bot_match_s */* match, unsigned long int context);
void trap_BotMatchVariable(void /* struct bot_match_s */* match, int variable, char* buf, int size);
void trap_UnifyWhiteSpaces(char* string);
void trap_BotReplaceSynonyms(char* string, unsigned long int context);
int trap_BotLoadChatFile(int chatstate, char* chatfile, char* chatname);
void trap_BotSetChatGender(int chatstate, int gender);
void trap_BotSetChatName(int chatstate, char* name, int client);
void trap_BotResetGoalState(int goalstate);
void trap_BotRemoveFromAvoidGoals(int goalstate, int number);
void trap_BotResetAvoidGoals(int goalstate);
void trap_BotPushGoal(int goalstate, void /* struct bot_goal_s */* goal);
void trap_BotPopGoal(int goalstate);
void trap_BotEmptyGoalStack(int goalstate);
void trap_BotDumpAvoidGoals(int goalstate);
void trap_BotDumpGoalStack(int goalstate);
void trap_BotGoalName(int number, char* name, int size);
int trap_BotGetTopGoal(int goalstate, void /* struct bot_goal_s */* goal);
int trap_BotGetSecondGoal(int goalstate, void /* struct bot_goal_s */* goal);
int trap_BotChooseLTGItem(int goalstate, vec3_t origin, int* inventory, int travelflags);
int trap_BotChooseNBGItem(int goalstate, vec3_t origin, int* inventory, int travelflags, void /* struct bot_goal_s */* ltg, float maxtime);
int trap_BotTouchingGoal(vec3_t origin, void /* struct bot_goal_s */* goal);
int trap_BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, void /* struct bot_goal_s */* goal);
int trap_BotGetNextCampSpotGoal(int num, void /* struct bot_goal_s */* goal);
int trap_BotGetMapLocationGoal(char* name, void /* struct bot_goal_s */* goal);
int trap_BotGetLevelItemGoal(int index, char* classname, void /* struct bot_goal_s */* goal);
float trap_BotAvoidGoalTime(int goalstate, int number);
void trap_BotSetAvoidGoalTime(int goalstate, int number, float avoidtime);
void trap_BotInitLevelItems(void);
void trap_BotUpdateEntityItems(void);
int trap_BotLoadItemWeights(int goalstate, char* filename);
void trap_BotFreeItemWeights(int goalstate);
void trap_BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child);
void trap_BotSaveGoalFuzzyLogic(int goalstate, char* filename);
void trap_BotMutateGoalFuzzyLogic(int goalstate, float range);
int trap_BotAllocGoalState(int state);
void trap_BotFreeGoalState(int handle);

void trap_BotResetMoveState(int movestate);
void trap_BotMoveToGoal(void /* struct bot_moveresult_s */* result, int movestate, void /* struct bot_goal_s */* goal, int travelflags);
int trap_BotMoveInDirection(int movestate, vec3_t dir, float speed, int type);
void trap_BotResetAvoidReach(int movestate);
void trap_BotResetLastAvoidReach(int movestate);
int trap_BotReachabilityArea(vec3_t origin, int testground);
int trap_BotMovementViewTarget(int movestate, void /* struct bot_goal_s */* goal, int travelflags, float lookahead, vec3_t target);
int trap_BotPredictVisiblePosition(vec3_t origin, int areanum, void /* struct bot_goal_s */* goal, int travelflags, vec3_t target);
int trap_BotAllocMoveState(void);
void trap_BotFreeMoveState(int handle);
void trap_BotInitMoveState(int handle, void /* struct bot_initmove_s */* initmove);
void trap_BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type);

int trap_BotChooseBestFightWeapon(int weaponstate, int* inventory);
void trap_BotGetWeaponInfo(int weaponstate, int weapon, void /* struct weaponinfo_s */* weaponinfo);
int trap_BotLoadWeaponWeights(int weaponstate, char* filename);
int trap_BotAllocWeaponState(void);
void trap_BotFreeWeaponState(int weaponstate);
void trap_BotResetWeaponState(int weaponstate);

int trap_GeneticParentsAndChildSelection(int numranks, float* ranks, int* parent1, int* parent2, int* child);

void trap_SnapVector(float* v);
