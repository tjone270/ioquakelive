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
#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"
#include "../game/bg_public.h"
#include "cg_public.h"
#include "../ui/ui_shared.h"

// The entire cgame module is unloaded and reloaded on each level change,
// so there is NO persistant data between levels on the client side.
// If you absolutely need something stored, it can either be kept
// by the server in the server stored userinfos, or stashed in a cvar.

#define POWERUP_BLINKS 5

#define POWERUP_BLINK_TIME 1000
#define FADE_TIME 200
#define PULSE_TIME 200
#define DAMAGE_DEFLECT_TIME 100
#define DAMAGE_RETURN_TIME 400
#define DAMAGE_TIME 500
#define LAND_DEFLECT_TIME 150
#define LAND_RETURN_TIME 300
#define STEP_TIME 200
#define DUCK_TIME 100
#define PAIN_TWITCH_TIME 200
#define WEAPON_SELECT_TIME 1400
#define ITEM_SCALEUP_TIME 1000
#define ZOOM_TIME 150
#define ITEM_BLOB_TIME 200
#define MUZZLE_FLASH_TIME 20
#define SINK_TIME 1000  // time for fragments to sink into ground before going away
#define ATTACKER_HEAD_TIME 10000
#define REWARD_TIME 3000

#define PULSE_SCALE 1.5  // amount to scale up the icons when activating

#define MAX_STEP_CHANGE 32

#define MAX_VERTS_ON_POLY 10
#define MAX_MARK_POLYS 256

#define STAT_MINUS 10  // num frame for '-' stats digit

#define ICON_SIZE 48
#define CHARACTER_WIDTH 32
#define CHAR_HEIGHT 48
#define TEXT_ICON_SPACE 4

#define TEAMCHAT_WIDTH 80
#define TEAMCHAT_HEIGHT 8

// very large characters
#define GIANT_WIDTH 32
#define GIANT_HEIGHT 48

#define NUM_CROSSHAIRS 30  // [QL] cgamex86.dll CG_RegisterGraphics @0x10022f40 registers 30; keep in sync with ui_shared.h

#define TEAM_OVERLAY_MAXNAME_WIDTH 12
#define TEAM_OVERLAY_MAXLOCATION_WIDTH 16

#define DEFAULT_MODEL "sarge"

typedef enum {
    FOOTSTEP_NORMAL,
    FOOTSTEP_BOOT,
    FOOTSTEP_FLESH,
    FOOTSTEP_MECH,
    FOOTSTEP_ENERGY,
    FOOTSTEP_METAL,
    FOOTSTEP_SPLASH,
    FOOTSTEP_SNOW,   // [QL]
    FOOTSTEP_WOOD,   // [QL]

    FOOTSTEP_TOTAL
} footstep_t;

typedef enum {
    IMPACTSOUND_DEFAULT,
    IMPACTSOUND_METAL,
    IMPACTSOUND_FLESH
} impactSound_t;

//=================================================

// player entities need to track more information
// than any other type of entity.

// note that not every player entity is a client entity,
// because corpses after respawn are outside the normal
// client numbering range

// when changing animation, set animationTime to frameTime + lerping time
// The current lerp will finish out, then it will lerp to the new animation
typedef struct {
    int oldFrame;
    int oldFrameTime;  // time when ->oldFrame was exactly on

    int frame;
    int frameTime;  // time when ->frame will be exactly on

    float backlerp;

    float yawAngle;
    qboolean yawing;
    float pitchAngle;
    qboolean pitching;

    int animationNumber;  // may include ANIM_TOGGLEBIT
    animation_t* animation;
    int animationTime;  // time when the first frame of the animation will be exact
} lerpFrame_t;

typedef struct {
    lerpFrame_t legs, torso, flag;
    int painTime;
    int painDirection;  // flip from 0 to 1
    int lightningFiring;

    int railFireTime;
    vec3_t railgunTrailStart;  // [QL] cent+0x29c: flash-tag muzzle origin captured at rail fire, used as trail start

    // machinegun spinning
    float barrelAngle;
    int barrelTime;
    qboolean barrelSpinning;
} playerEntity_t;

//=================================================

// centity_t have a direct corespondence with gentity_t in the game, but
// only the entityState_t is directly communicated to the cgame
typedef struct centity_s {
    entityState_t currentState;  // from cg.frame
    entityState_t nextState;     // from cg.nextFrame, if available
    qboolean interpolate;        // true if next is valid to interpolate to
    qboolean currentValid;       // true if cg.frame holds this entity

    int muzzleFlashTime;  // move to playerEntity?
    int previousEvent;
    int teleportFlag;

    int trailTime;  // so missile trails can handle dropped initial packets
    int dustTrailTime;
    int miscTime;

    int snapShotTime;  // last time this entity was found in a snapshot

    playerEntity_t pe;

    int errorTime;  // decay the error from this time
    vec3_t errorOrigin;
    vec3_t errorAngles;

    qboolean extrapolated;  // false if origin / angles is an interpolation
    vec3_t rawOrigin;
    vec3_t rawAngles;

    vec3_t beamEnd;

    // exact interpolated position of entity on this frame
    vec3_t lerpOrigin;
    vec3_t lerpAngles;
} centity_t;

//======================================================================

// local entities are created as a result of events or predicted actions,
// and live independently from all server transmitted entities

typedef struct markPoly_s {
    struct markPoly_s *prevMark, *nextMark;
    int time;
    qhandle_t markShader;
    qboolean alphaFade;  // fade alpha instead of rgb
    float color[4];
    poly_t poly;
    polyVert_t verts[MAX_VERTS_ON_POLY];
} markPoly_t;

typedef enum {
    LE_MARK,
    LE_EXPLOSION,
    LE_SPRITE_EXPLOSION,
    LE_FRAGMENT,
    LE_MOVE_SCALE_FADE,
    LE_FALL_SCALE_FADE,
    LE_FADE_RGB,
    LE_SCALE_FADE,
    LE_SCOREPLUM,
    LE_KAMIKAZE,
    LE_INVULIMPACT,
    LE_INVULJUICED,
    LE_SHOWREFENTITY,
    LE_FREEZE  // [QL] freeze-tag ice effect (set by CG_FreezeEffect). leType is cgame-internal (never serialised) so value is free.
} leType_t;

typedef enum {
    LEF_PUFF_DONT_SCALE = 0x0001,  // do not scale size over time
    LEF_TUMBLE = 0x0002,           // tumble over time, used for ejecting shells
    LEF_SOUND1 = 0x0004,           // sound 1 for kamikaze
    LEF_SOUND2 = 0x0008            // sound 2 for kamikaze
} leFlag_t;

typedef enum {
    LEMT_NONE,
    LEMT_BURN,
    LEMT_BLOOD
} leMarkType_t;  // fragment local entities can leave marks on walls

typedef enum {
    LEBS_NONE,
    LEBS_BLOOD,
    LEBS_BRASS
} leBounceSoundType_t;  // fragment local entities can make sounds on impacts

typedef struct localEntity_s {
    struct localEntity_s *prev, *next;
    leType_t leType;
    int leFlags;

    int startTime;
    int endTime;
    int fadeInTime;

    float lifeRate;  // 1.0 / (endTime - startTime)

    trajectory_t pos;
    trajectory_t angles;

    float bounceFactor;  // 0.0 = no bounce, 1.0 = perfect

    float color[4];

    float radius;

    float light;
    vec3_t lightColor;

    leMarkType_t leMarkType;  // mark to leave on fragment impact
    leBounceSoundType_t leBounceSoundType;

    refEntity_t refEntity;
} localEntity_t;

//======================================================================

// [QL] shared floating-effect pool (cgamex86.dll base DAT_10ab9978, 32 records of 0x80
// bytes each). One pool of screen-projected billboards feeds all of QL's floating
// player/world effects: damage numbers, player outlines, freeze/flag glows and head
// float-sprites. Allocated by CG_AllocFloatingEffect (binary "CG_AllocMark"
// @ 0x1002a0d0 - a mislabelled Ghidra name, not the Q3 wall-mark
// allocator in cg_marks.c), aged and expired each frame by CG_UpdateFloatingEffects
// (binary "CG_UpdateConfigStrings" @ 0x1002a190 - also mislabelled; it ages this pool,
// it is not a config-string updater) and rendered by
// CG_DrawFloatingEffects (binary "CG_DrawDamagePlums" @ 0x10011680 - draws the whole
// pool, not only plums). The struct mirrors the binary's 0x80-byte record field order.
#define MAX_FLOATING_EFFECTS 32

typedef enum {
    FE_DAMAGE_NUMBER = 0,  // floating damage plum (drawn as text)
    FE_OUTLINE       = 1,  // team/enemy player outline billboard
    FE_FREEZE        = 2,  // freeze overlay / flag-carrier glow billboard
    FE_SPRITE        = 3,  // generic world sprite (CG_General/Item/Obituary/EntityEvent)
    FE_FLOAT_SPRITE  = 4   // player head float-sprite (medals, talk, connection icons)
} floatingEffectType_t;

typedef struct {
    int       type;        // 0x00  floatingEffectType_t
    void*     owner;       // 0x04  originating centity_t* (0 if none)
    vec3_t    origin;      // 0x08  world origin
    vec4_t    color;       // 0x14  RGBA; alpha aged by CG_UpdateFloatingEffects
    qhandle_t shader;      // 0x24  billboard pic (0 = none; damage numbers draw text)
    int       clientNum;   // 0x28  FE_SPRITE viewer-cull key
    int       followNum;   // 0x2c  FE_SPRITE viewer-cull key (followed client)
    int       spawnTime;   // 0x30  cg.time at allocation
    float     lifeRate;    // 0x34  lifetime in ms (default 1.0; damage number 1000.0)
    int       pad_38;      // 0x38  unused
    float     worldSize;   // 0x3c  world height used for perspective pixel scaling
    int       pad_40;      // 0x40  unused
    float     minPixels;   // 0x44  minimum on-screen pixel size (floor)
    int       pad_48;      // 0x48  unused
    float     zOffset;     // 0x4c  world Z lift above origin (outline 80, freeze 64, sprite 48)
    float     fadeStart;   // 0x50  ms after spawn when the alpha fade begins
    char      text[16];    // 0x54  damage-number string
    float     velX;        // 0x64  screen X drift, linear in life fraction
    float     velY;        // 0x68  screen Y drift, linear in life fraction
    float     accX;        // 0x6c  screen X drift, * life^2
    float     accY;        // 0x70  screen Y drift, * life^2
    int       doFade;      // 0x74  1 = run the fadeStart alpha fade
    int       pad_78;      // 0x78  unused
    int       active;      // 0x7c  slot in use flag
} floatingEffect_t;

//======================================================================

typedef struct {
    int client;
    int score;
    int roundScore;     // [QL] for RR gametype
    int ping;
    int time;
    int scoreFlags;
    int powerUps;
    int accuracy;
    int impressiveCount;
    int excellentCount;
    int guantletCount;
    int defendCount;
    int assistCount;
    int captures;
    qboolean perfect;
    int team;

    // [QL] extended fields
    qboolean alive;
    int frags;
    int deaths;
    int bestWeapon;
    int bestWeaponAccuracy;
    int damageDone;
    int net;            // frags - deaths
    int tks;            // team kills
    int tkd;            // team kill deaths
    int thaws;          // FT: thaw count

    // [QL] extended team-stats fields written by tdmstats/castats/ctfstats verbs
    // (binary score_t byte offsets in comments; cgame-internal layout so exact offsets need not match)
    int selfKills;               // 0xd0
    int damageReceived;          // 0xf0 (damageDone == 0xec already above)
    int pickups_ra;              // 0xf4  red armour
    int pickups_ya;              // 0xfc  yellow armour
    int pickups_ga;              // 0x104 green armour
    int pickups_mh;              // 0x10c mega health
    int pickups_quad;            // 0x114 quad
    int pickups_bs;              // 0x118 battlesuit
    int pickups_regen;           // 0x11c regen  (CTF verb only)
    int pickups_haste;           // 0x120 haste  (CTF verb only)
    int pickups_invis;           // 0x124 invis  (CTF verb only)
    int weaponKills[MAX_WEAPONS];    // 0x12c 15 ints (index 0=gauntlet..14). CA verb. MAX_WEAPONS must be >= 15.
    int weaponAccuracy[MAX_WEAPONS]; // 0x16c 15 ints. CA verb. HMG uses index 13 for both kills and accuracy.
} score_t;

// [QL] per-weapon stats for duel scoreboard
typedef struct {
    int hits;
    int atts;       // attempts (shots)
    int accuracy;
    int damage;
    int kills;
} duelWeaponStats_t;

// [QL] per-player duel score (populated by "scores" command in duel mode)
typedef struct {
    int clientNum;
    int score;
    int ping;
    int time;
    int kills;
    int deaths;
    int accuracy;
    int bestWeapon;
    int damage;
    int awardExcellent;
    int awardImpressive;
    int awardHumiliation;
    qboolean perfect;
    int redArmorPickups;    float redArmorTime;
    int yellowArmorPickups; float yellowArmorTime;
    int greenArmorPickups;  float greenArmorTime;
    int megaHealthPickups;  float megaHealthTime;
    duelWeaponStats_t weaponStats[MAX_WEAPONS];
} duelScore_t;

// [QL] team stats from castats/tdmstats/ctfstats commands
typedef struct {
    qboolean valid;
    // red team pickups
    int rra, rya, rga, rmh, rquad, rbs;
    int rquadTime, rbsTime;
    // blue team pickups
    int bra, bya, bga, bmh, bquad, bbs;
    int bquadTime, bbsTime;
} teamPickupStats_t;

// [QL] team score header for TDM/FT scores (powerup pickup counts + times)
typedef struct {
    qboolean valid;
    // red team
    int rra, rya, rga, rmh, rquad, rbs, rregen, rhaste, rinvis;
    int rquadTime, rbsTime, rregenTime, rhasteTime, rinvisTime;
    // blue team
    int bra, bya, bga, bmh, bquad, bbs, bregen, bhaste, binvis;
    int bquadTime, bbsTime, bregenTime, bhasteTime, binvisTime;
} tdmScoreHeader_t;

// [QL] team score header for CTF scores (includes flag + medkit)
typedef struct {
    qboolean valid;
    // red team
    int rra, rya, rga, rmh, rquad, rbs, rregen, rhaste, rinvis, rflag, rmedkit;
    int rquadTime, rbsTime, rregenTime, rhasteTime, rinvisTime, rflagTime;
    // blue team
    int bra, bya, bga, bmh, bquad, bbs, bregen, bhaste, binvis, bflag, bmedkit;
    int bquadTime, bbsTime, bregenTime, bhasteTime, binvisTime, bflagTime;
} ctfScoreHeader_t;

// [QL] per-player weapon accuracy (from "acc" command)
typedef struct {
    int accuracy[MAX_WEAPONS];
    int time;       // when received
    int clientNum;  // which client
    qboolean valid;
} accuracyStats_t;

// each client has an associated clientInfo_t
// that contains media references necessary to present the
// client model and other color coded effects
// this is regenerated each time a client's configstring changes,
// usually as a result of a userinfo (name, model, etc) change
#define MAX_CUSTOM_SOUNDS 32

typedef struct {
    qboolean infoValid;

    char name[MAX_QPATH];
    team_t team;
    uint64_t steamId;  // steam id for this client

    int botSkill;  // 0 = not bot, 1-5 = bot

    vec3_t color1;
    vec3_t color2;

    byte c1RGBA[4];
    byte c2RGBA[4];

    int score;     // updated by score servercmds
    int location;  // location index for team mode
    int health;    // you only get this info about your teammates
    int armor;
    int curWeapon;

    int handicap;
    int wins, losses;  // in tourney mode

    int teamTask;         // task in teamplay (offence/defence)
    qboolean teamLeader;  // true when this is a team leader

    int powerups;  // so can display quad/flag status

    int medkitUsageTime;
    int invulnerabilityStartTime;
    int invulnerabilityStopTime;

    // when clientinfo is changed, the loading of models/skins/sounds
    // can be deferred until you are dead, to prevent hitches in
    // gameplay
    char modelName[MAX_QPATH];
    char skinName[MAX_QPATH];
    char headModelName[MAX_QPATH];
    char headSkinName[MAX_QPATH];
    // [QL] forced head/secondary model+skin written by CG_ResolveModelForClient and
    // resolved by CG_ResolveHeadSkinName (binary offsets 0x254 / 0x294). Despite the
    // ql-decompiled header naming them redTeam/blueTeam, these are not team-name strings.
    char forcedHeadModel[MAX_QPATH];  // 0x254
    char forcedHeadSkin[MAX_QPATH];   // 0x294
    qboolean deferred;

    qboolean newAnims;    // true if using the new mission pack animations
    qboolean fixedlegs;   // true if legs yaw is always the same as torso yaw
    qboolean fixedtorso;  // true if torso never changes yaw

    vec3_t headOffset;  // move head in icon views
    footstep_t footsteps;
    gender_t gender;  // from model

    qhandle_t legsModel;
    qhandle_t legsSkin;

    qhandle_t torsoModel;
    qhandle_t torsoSkin;

    qhandle_t headModel;
    qhandle_t headSkin;

    qhandle_t modelIcon;

    float modelScale;  // QL: bounding box scale factor (cg_scalePlayerModelsToBB)

    animation_t animations[MAX_TOTALANIMATIONS];

    sfxHandle_t sounds[MAX_CUSTOM_SOUNDS];

    // [QL] duel queue state from the player configstring (binary clientInfo +0x714/+0x718).
    // specOnly marks spectators who opted out of the queue; queuePosition is 1-based, 0 = none.
    int specOnly;
    int queuePosition;

    // [QL] GeoIP country code from the "c" key (binary clientInfo +0xc8),
    // shown as ui/assets/flags/<code>.tga on the scoreboards
    char country[24];
} clientInfo_t;

// each WP_* weapon enum has an associated weaponInfo_t
// that contains media references necessary to present the
// weapon and its effects
typedef struct weaponInfo_s {
    qboolean registered;
    gitem_t* item;

    qhandle_t handsModel;  // the hands don't actually draw, they just position the weapon
    qhandle_t weaponModel;
    qhandle_t barrelModel;
    qhandle_t flashModel;

    vec3_t weaponMidpoint;  // so it will rotate centered instead of by tag

    float flashDlight;
    vec3_t flashDlightColor;
    sfxHandle_t flashSound[4];  // fast firing weapons randomly choose

    qhandle_t weaponIcon;
    qhandle_t ammoIcon;

    qhandle_t ammoModel;

    qhandle_t missileModel;
    sfxHandle_t missileSound;
    void (*missileTrailFunc)(centity_t*, const struct weaponInfo_s* wi);
    float missileDlight;
    vec3_t missileDlightColor;
    int missileRenderfx;

    void (*ejectBrassFunc)(centity_t*);

    float trailRadius;
    float wiTrailTime;

    sfxHandle_t readySound;
    sfxHandle_t firingSound;
} weaponInfo_t;

// each IT_* item has an associated itemInfo_t
// that constains media references necessary to present the
// item and its effects
typedef struct {
    qboolean registered;
    qhandle_t models[MAX_ITEM_MODELS];
    qhandle_t icon;
} itemInfo_t;

typedef struct {
    int itemNum;
} powerupInfo_t;

#define MAX_SKULLTRAIL 10

typedef struct {
    vec3_t positions[MAX_SKULLTRAIL];
    int numpositions;
} skulltrail_t;

#define MAX_REWARDSTACK 10
#define MAX_SOUNDBUFFER 40

// [QL] announcer sound ring buffer (replaces Q3's single cg.rewardSound[])
#define MAX_ANNOUNCER_QUEUE 32

// [QL] voice chat
#define MAX_VOICEFILES 8         // parsed voice-file slots searched by CG_ParseVoiceChats
#define MAX_VOICECHATBUFFER 32   // voiceChatBuffer ring size (in/out masked modulo 32)

// [QL] maps a client model/skin string -> loaded voice-file slot index.
// Binary voiceChatLists[64] at 0x107d87c0, entry stride 0x44 (68 bytes).
// NOTE: QL's layout differs from ioquake3's voiceChatList_t - verify sizing before reuse.
typedef struct {
    char name[64];
    int  index;
} voiceChatList_t;

// [QL] one buffered incoming voice chat. 0x138 = 312 bytes; matches ioquake3's layout.
typedef struct bufferedVoiceChat_s {
    int         clientNum;
    sfxHandle_t snd;
    int         voiceOnly;
    char        cmd[150];
    char        message[150];
} bufferedVoiceChat_t;

//======================================================================

// all cg.stepTime, cg.duckTime, cg.landTime, etc are set to cg.time when the action
// occurs, and they will have visible effects for #define STEP_TIME or whatever msec after

#define MAX_PREDICTED_EVENTS 16

typedef struct {
    int clientFrame;  // incremented each frame

    int clientNum;

    qboolean demoPlayback;
    qboolean levelShot;  // taking a level menu screenshot
    int deferredPlayerLoading;
    qboolean loading;              // don't defer players at initial startup
    qboolean intermissionStarted;  // don't play voice rewards, because game will end shortly

    // there are only one or two snapshot_t that are relevant at a time
    int latestSnapshotNum;   // the number of snapshots the client system has received
    int latestSnapshotTime;  // the time from latestSnapshotNum, so we don't need to read the snapshot yet

    snapshot_t* snap;      // cg.snap->serverTime <= cg.time
    snapshot_t* nextSnap;  // cg.nextSnap->serverTime > cg.time, or NULL
    snapshot_t activeSnapshots[2];

    float frameInterpolation;  // (float)( cg.time - cg.frame->serverTime ) / (cg.nextFrame->serverTime - cg.frame->serverTime)

    qboolean thisFrameTeleport;
    qboolean nextFrameTeleport;

    int frametime;  // cg.time - cg.oldTime

    int time;  // this is the time value that the client
    // is rendering at.
    int oldTime;  // time at last frame, used for missile trails and prediction checking

    int physicsTime;  // either cg.snap->time or cg.nextSnap->time

    int timelimitWarnings;  // 5 min, 1 min, overtime
    int fraglimitWarnings;

    qboolean mapRestart;  // set on a map restart to set back the weapon

    qboolean renderingThirdPerson;  // during deaths, chasecams, etc

    // prediction state
    qboolean hyperspace;  // true if prediction has hit a trigger_teleport
    playerState_t predictedPlayerState;
    centity_t predictedPlayerEntity;
    qboolean validPPS;  // clear until the first call to CG_PredictPlayerState
    int predictedErrorTime;
    vec3_t predictedError;

    int eventSequence;
    int predictableEvents[MAX_PREDICTED_EVENTS];

    float stepChange;  // for stair up smoothing
    int stepTime;

    float duckChange;  // for duck viewheight smoothing
    int duckTime;

    float landChange;  // for landing hard
    int landTime;

    // input state sent to server
    int weaponSelect;

    // auto rotating items
    vec3_t autoAngles;
    vec3_t autoAxis[3];
    vec3_t autoAnglesFast;
    vec3_t autoAxisFast[3];

    // view rendering
    refdef_t refdef;
    vec3_t refdefViewAngles;  // will be converted to refdef.viewaxis

    // zoom key
    qboolean zoomed;
    int zoomTime;
    float zoomSensitivity;

    // information screen text during loading
    char infoScreenText[MAX_STRING_CHARS];
    int loadingStage;  // [QL] loading progress counter (0-4)

    // scoreboard
    int scoresRequestTime;
    int numScores;
    int selectedScore;
    int teamScores[2];
    score_t scores[MAX_CLIENTS];
    qboolean showScores;
    qboolean scoreBoardShowing;

    // [QL] join-game overlay (ingame_join.menu). Set at CG_Init; the joingame_menu paints
    // instead of the HUD until CG_CloseMenus clears it (player joined or end scoreboard up).
    qboolean joinMenuPending;
    int joinMenuCloseTime;

    // [QL] team-kill complaint prompt (server "complaint" verb). complaintClient is the
    // offender clientNum (or a negative status code); complaintEndTime is when the prompt
    // expires. Consumed by CG_DrawVote (binary DAT_10a5fdc8 / DAT_10a5fdcc).
    int complaintClient;
    int complaintEndTime;

    qboolean statsShowing;     // [QL] +stats overlay active
    qboolean accShowing;       // [QL] +acc overlay active
    qboolean pstatsShowing;    // [QL] +pstats overlay active
    int      accRequestTime;   // [QL] throttle for acc server requests
    int      pstatsRequestTime;// [QL] throttle for pstats server requests
    int scoreFadeTime;
    char killerName[MAX_NAME_LENGTH];

    // skull trails
    skulltrail_t skulltrails[MAX_CLIENTS];

    // centerprinting
    int centerPrintTime;
    int centerPrintCharWidth;
    int centerPrintY;
    char centerPrint[1024];
    int centerPrintLines;

    // low ammo warning state
    int lowAmmoWarning;  // 1 = low, 2 = empty

    // crosshair client ID
    int crosshairClientNum;
    int crosshairClientTime;

    // powerup active flashing
    int powerupActive;
    int powerupTime;

    // attacking player
    int attackerTime;
    int voiceTime;

    // reward medals
    int rewardStack;
    int rewardTime;
    int rewardCount[MAX_REWARDSTACK];
    qhandle_t rewardShader[MAX_REWARDSTACK];
    qhandle_t rewardSound[MAX_REWARDSTACK];

    // sound buffer mainly for announcer sounds
    int soundBufferIn;
    int soundBufferOut;
    int soundTime;
    qhandle_t soundBuffer[MAX_SOUNDBUFFER];

    // for voice chat buffer
    int voiceChatTime;
    int voiceChatBufferIn;
    int voiceChatBufferOut;

    // [QL] deferred spectator auto-follow (CG_CheckAutoFollow, DAT_10ab97c0/4).
    // autoFollowClient == -1 means no pending follow. Distinct from cg_predict.c's
    // CG_SpecAutoFollow.
    int autoFollowClient;
    int autoFollowTime;

    // warmup countdown
    int warmup;
    int warmupCount;
    int warmupGametype;     // [QL] gametype override from CS_WARMUP (-1 = use current)
    int warmupFreezeCount_red;   // [QL] GT_AD per-team count from CS_ROUND_WARMUP
    int warmupFreezeCount_blue;  // [QL] GT_AD per-team count from CS_ROUND_WARMUP
    int lastAutoFireTime;        // [QL] last predicted railgun autofire (cg.time)

    //==========================

    int itemPickup;
    int itemPickupTime;
    int itemPickupBlendTime;  // the pulse around the crosshair is timed separately

    int weaponSelectTime;
    int weaponAnimation;
    int weaponAnimationTime;

    // blend blobs
    float damageTime;
    float damageX, damageY, damageValue;
    int damageType;  // [QL] hit-effect colour selector for CG_DrawDamageBlend (2=green,3=orange,4=blue,else light-blue)

    // status bar head
    float headYaw;
    float headEndPitch;
    float headEndYaw;
    int headEndTime;
    float headStartPitch;
    float headStartYaw;
    int headStartTime;

    // view movement
    float v_dmg_time;
    float v_dmg_pitch;
    float v_dmg_roll;

    // temp working variables for player view
    float bobfracsin;
    int bobcycle;
    float xyspeed;
    int nextOrbitTime;

    // qboolean cameraMode;		// if rendering from a loaded camera

    // development tool
    refEntity_t testModelEntity;
    char testModelName[MAX_QPATH];
    qboolean testGun;

    // [QL] race mode state
    struct {
        qboolean active;            // race in progress
        int      startTime;         // server time of race start
        int      finishTime;        // last finish time
        int      bestTime;          // personal best time
        int      totalCheckpoints;  // total checkpoints on this map
        int      bestSplit;         // best/target split time
        int      checkpointDiff;    // time diff at last checkpoint
        qboolean hasDiff;           // whether diff is valid
        int      checkpointCount;   // current checkpoint number
        int      nextCheckpointEnt;     // entity number of next checkpoint (-1 = none)
        int      nextNextCheckpointEnt; // two-level lookahead (-1 = none)
        int      currentCheckpointEnt; // entity number of current/last checkpoint (-1 = none)
    } race;

    // [QL] point of interest markers (flag carriers, powerup holders, etc.)
    #define MAX_POI_PICS 128
    struct {
        vec3_t   origin;
        int      startTime;
        int      length;     // duration in ms (from es->powerups)
        int      team;       // team association (from es->generic1)
    } poiPics[MAX_POI_PICS];
    int numPoiPics;

    // [QL] spectator item pickup tracking
    #define MAX_SPEC_PICKUPS 10
    struct {
        int      clientNum;
        int      itemIndex;
        int      amount;
        int      time;              // relative time of pickup
        vec3_t   origin;
        qboolean active;
    } specPickups[MAX_SPEC_PICKUPS];

    // [QL] duel scoreboard data
    duelScore_t duelScores[2];
    int duelPlayer1;        // clientNum of 1st duel player (from CS_CLIENTNUM1STPLAYER)
    int duelPlayer2;        // clientNum of 2nd duel player (from CS_CLIENTNUM2NDPLAYER)
    qboolean duelScoresValid;

    // [QL] team pickup stats (from "tdmstats" etc.)
    teamPickupStats_t teamPickups;

    // [QL] team score headers (from scores_tdm/scores_ft/scores_ctf)
    tdmScoreHeader_t tdmScoreHeader;
    ctfScoreHeader_t ctfScoreHeader;

    // [QL] per-player accuracy (from "acc" command)
    accuracyStats_t accuracyStats;

    // [QL] obituary / kill feed ring buffer (binary: 16 entries at 10ab8fec)
    #define MAX_OBITUARIES 16
    struct {
        int         active;
        int         time;           // cg.time when it happened
        char        victimName[40]; // resolved + color-stripped for team games
        int         victimTeam;     // 0=free, 1=red, 2=blue, 3=spec
        char        attackerName[40];
        int         attackerTeam;
        int         hasAttacker;    // 0 for world/suicide
        qhandle_t   weaponIcon;     // shader handle for weapon/skull icon
    } obituaries[MAX_OBITUARIES];
    int obituaryCount;       // number of active entries

    // [QL] speedometer history ring buffer
    #define SPEED_HISTORY_SIZE 128
    float speedHistory[SPEED_HISTORY_SIZE];
    int speedHistoryCount;
    int speedHistoryIndex;
    vec4_t speedBarColor1;   // lower bar color (green)
    vec4_t speedBarColor2;   // upper/overflow bar color (yellow)

    // [QL] chat history and misc client-side flags
    qboolean chatHistoryShowing;
    qboolean killRequested;
    int      disconnectRequest;

    // [QL] chat ring buffer (binary: 24 entries at DAT_10079950)
    #define MAX_CHAT_LINES  24
    #define CHAT_LINE_TEXT  256
    struct {
        int     startTime;
        int     endTime;
        int     teamOnly;
        char    text[CHAT_LINE_TEXT];
    } chatLines[MAX_CHAT_LINES];
    struct {
        int     startTime;
        int     endTime;
        int     teamOnly;
        char    text[CHAT_LINE_TEXT];
    } currentChatLine;
    int chatIndex;

    // [QL] announcer sound ring buffer (replaces Q3's single cg.rewardSound[]).
    // CG_QueueAnnouncement enqueues (delay 0x5dc=1500ms); CG_PlayBufferedVoiceChats/reward path drains.
    // Globals: queue 0x10ab8e3c, delay 0x10ab8ebc, startTime 0x10ab8dbc, in 0x10ab8db4, out 0x10ab8db8, suppress 0x10ab8f3c.
    sfxHandle_t announcerQueue[MAX_ANNOUNCER_QUEUE];
    int         announcerDelay[MAX_ANNOUNCER_QUEUE];
    int         announcerStartTime[MAX_ANNOUNCER_QUEUE];
    int         announcerQueueIn;
    int         announcerQueueOut;
    int         announcerSuppress;

    // [QL] spectator list for the scoreboard CG_SPECTATORS pager. One cleaned display
    // name per entry (duel queue "(n)"/"(s)" prefix included), rebuilt by
    // CG_BuildSpectatorString. Binary: 0x40-byte string stride (0x10aa69b4),
    // count (0x10ab69b4), page offset (0x10ab69bc), 4000ms page timer (0x10ab69b8).
    char spectatorNames[MAX_CLIENTS][64];
    int  spectatorCount;
    int  spectatorPageOffset;
    int  spectatorPageTime;

    // [QL] race-mode flags read by CG_DrawRespawnMessage.
    // raceStarted (DAT_10abaad8) gates the 'Press <key> to respawn' hint;
    // raceActive (DAT_10abaab0) selects 'CURRENT RUN'/'LAST TIME' and which time CG_FormatRaceTime formats.
    // (Distinct from the cg.race sub-struct's own 'active' flag; reconcile in Phase 3.)
    int raceActive;
    int raceStarted;

    // [QL] A&D round-overlay scoreboard, written by CG_InitScores (argv 1..20), read by CG_DrawRoundOverlay.
    // Dedicated array at global 0x10aa67d0 (memset 0x50 = 20 ints); -1 means "no score".
    // not cg.scores[].client - do not overload the score_t path. teamScores[2] above holds argv(21)/argv(22).
    int adScores[20];

    // [QL] chat-recall timers. CG_LastChatCommand sets lastChatTime = cg.time + 3000;
    // CG_LastChatCommand2 sets lastChatTime2 = cg.time + 3000; both re-add cg_lastmsg via CG_AddChat.
    int lastChatTime;
    int lastChatTime2;

    // [QL] cgame key-catcher flag (global 0x10a9c9b4).
    // CG_SetKeyCatcher sets it to 1 if 0; CG_ClearKeyCatcher sets it to 0 if nonzero.
    int keyCatcherActive;

} cg_t;

// all of the model, shader, and sound references that are
// loaded at gamestate time are stored in cgMedia_t
// Other media that can be tied to clients, weapons, or items are
// stored in the clientInfo_t, itemInfo_t, weaponInfo_t, and powerupInfo_t
typedef struct {
    qhandle_t charsetShader;
    qhandle_t charsetProp;
    qhandle_t charsetPropGlow;
    qhandle_t charsetPropB;
    qhandle_t whiteShader;

    // [QL] loading screen assets
    qhandle_t loadingbackShader;
    qhandle_t gtBackgroundShader;
    qhandle_t qlLogoShader;
    qhandle_t logoBackgroundShader;
    qhandle_t backscreenSmokeShader;

    qhandle_t redCubeModel;
    qhandle_t blueCubeModel;
    qhandle_t redCubeIcon;
    qhandle_t blueCubeIcon;

    qhandle_t redFlagModel;
    qhandle_t blueFlagModel;
    qhandle_t neutralFlagModel;
    qhandle_t redFlagShader[3];
    qhandle_t blueFlagShader[3];
    qhandle_t flagShader[4];

    qhandle_t flagPoleModel;
    qhandle_t flagFlapModel;

    qhandle_t redFlagFlapSkin;
    qhandle_t blueFlagFlapSkin;
    qhandle_t neutralFlagFlapSkin;

    qhandle_t redFlagBaseModel;
    qhandle_t blueFlagBaseModel;
    qhandle_t neutralFlagBaseModel;

    qhandle_t overloadBaseModel;
    qhandle_t overloadTargetModel;
    qhandle_t overloadLightsModel;
    qhandle_t overloadEnergyModel;

    qhandle_t harvesterModel;
    qhandle_t harvesterRedSkin;
    qhandle_t harvesterBlueSkin;
    qhandle_t harvesterNeutralModel;

    qhandle_t armorModel;
    qhandle_t armorIcon;

    qhandle_t teamStatusBar;

    qhandle_t deferShader;

    // gib explosions
    qhandle_t gibAbdomen;
    qhandle_t gibArm;
    qhandle_t gibChest;
    qhandle_t gibFist;
    qhandle_t gibFoot;
    qhandle_t gibForearm;
    qhandle_t gibIntestine;
    qhandle_t gibLeg;
    qhandle_t gibSkull;
    qhandle_t gibBrain;

    qhandle_t smoke2;

    qhandle_t machinegunBrassModel;
    qhandle_t shotgunBrassModel;

    qhandle_t railRingsShader;
    qhandle_t railCoreShader;

    qhandle_t lightningShader[5];

    qhandle_t grapplingChainShader;

    qhandle_t balloonShader;
    qhandle_t connectionShader;

    qhandle_t selectShader;
    qhandle_t weaponBarHighlightShader;  // [QL] "ui/assets/hud/weaplit2.tga"
    qhandle_t infiniteAmmoShader;        // [QL] "icons/infinite.tga"
    qhandle_t viewBloodShader;
    qhandle_t tracerShader;
    qhandle_t crosshairShader[NUM_CROSSHAIRS];
    qhandle_t lagometerShader;
    qhandle_t backTileShader;
    qhandle_t noammoShader;

    qhandle_t smokePuffShader;
    qhandle_t smokePuffRageProShader;
    qhandle_t shotgunSmokePuffShader;
    qhandle_t plasmaBallShader;
    qhandle_t waterBubbleShader;
    qhandle_t bloodTrailShader;
    qhandle_t bloodSprayShaders[4];  // [QL] bloodSpray1-4

    qhandle_t sparkParticleShader;  // QL: impact spark particles
    qhandle_t vignetteShader;      // QL: fullscreen vignette overlay

    qhandle_t nailPuffShader;
    qhandle_t blueProxMine;

    // [QL] obituary icons
    qhandle_t fragIconShader;   // "icons/icon_frag" - skull for world/suicide deaths

    qhandle_t numberShaders[11];

    qhandle_t shadowMarkShader;

    qhandle_t botSkillShaders[5];

    // wall mark shaders
    qhandle_t wakeMarkShader;
    qhandle_t bloodMarkShader;
    qhandle_t bulletMarkShader;
    qhandle_t burnMarkShader;
    qhandle_t holeMarkShader;
    qhandle_t energyMarkShader;

    // powerup shaders
    qhandle_t quadShader;
    qhandle_t redQuadShader;
    qhandle_t quadWeaponShader;
    qhandle_t invisShader;
    qhandle_t regenShader;
    qhandle_t battleSuitShader;
    qhandle_t battleWeaponShader;
    qhandle_t hastePuffShader;

    qhandle_t redKamikazeShader;
    qhandle_t blueKamikazeShader;

    // weapon effect models
    qhandle_t bulletFlashModel;
    qhandle_t ringFlashModel;
    qhandle_t dishFlashModel;
    qhandle_t lightningExplosionModel;

    // weapon effect shaders
    qhandle_t railExplosionShader;
    qhandle_t plasmaExplosionShader;
    qhandle_t bulletExplosionShader;
    qhandle_t rocketExplosionShader;
    qhandle_t grenadeExplosionShader;
    qhandle_t bfgExplosionShader;
    qhandle_t bloodExplosionShader;

    // special effects models
    qhandle_t teleportEffectModel;
    qhandle_t teleportEffectShader;

    qhandle_t kamikazeEffectModel;
    qhandle_t kamikazeShockWave;
    qhandle_t kamikazeHeadModel;
    qhandle_t kamikazeHeadTrail;
    qhandle_t guardPowerupModel;
    qhandle_t scoutPowerupModel;
    qhandle_t doublerPowerupModel;
    qhandle_t ammoRegenPowerupModel;
    qhandle_t invulnerabilityImpactModel;
    qhandle_t invulnerabilityJuicedModel;
    qhandle_t medkitUsageModel;
    qhandle_t dustPuffShader;
    qhandle_t heartShader;
    qhandle_t invulnerabilityPowerupModel;

    // scoreboard headers
    qhandle_t scoreboardName;
    qhandle_t scoreboardPing;
    qhandle_t scoreboardScore;
    qhandle_t scoreboardTime;

    // [QL] medals/awards shown during gameplay (16 types)
    qhandle_t medalAccuracy;
    qhandle_t medalAssist;
    qhandle_t medalCapture;
    qhandle_t medalComboKill;
    qhandle_t medalDefend;       // was medalDefend
    qhandle_t medalExcellent;    // was medalExcellent
    qhandle_t medalFirstFrag;
    qhandle_t medalGauntlet;     // was medalGauntlet
    qhandle_t medalHeadshot;
    qhandle_t medalImpressive;   // was medalImpressive
    qhandle_t medalMidair;
    qhandle_t medalPerfect;
    qhandle_t medalPerforated;
    qhandle_t medalQuadGod;
    qhandle_t medalRampage;
    qhandle_t medalRevenge;

    // sounds
    sfxHandle_t quadSound;
    sfxHandle_t tracerSound;
    sfxHandle_t selectSound;
    sfxHandle_t useNothingSound;
    sfxHandle_t wearOffSound;
    sfxHandle_t footsteps[FOOTSTEP_TOTAL][4];
    sfxHandle_t sfx_lghit1;
    sfxHandle_t sfx_lghit2;
    sfxHandle_t sfx_lghit3;
    sfxHandle_t sfx_ric1;
    sfxHandle_t sfx_ric2;
    sfxHandle_t sfx_ric3;
    // sfxHandle_t	sfx_railg;
    sfxHandle_t sfx_rockexp;
    sfxHandle_t sfx_plasmaexp;

    sfxHandle_t sfx_proxexp;
    sfxHandle_t sfx_nghit;
    sfxHandle_t sfx_nghitflesh;
    sfxHandle_t sfx_nghitmetal;
    sfxHandle_t sfx_chghit;
    sfxHandle_t sfx_chghitflesh;
    sfxHandle_t sfx_chghitmetal;
    sfxHandle_t kamikazeExplodeSound;
    sfxHandle_t kamikazeImplodeSound;
    sfxHandle_t kamikazeFarSound;
    sfxHandle_t useInvulnerabilitySound;
    sfxHandle_t invulnerabilityImpactSound1;
    sfxHandle_t invulnerabilityImpactSound2;
    sfxHandle_t invulnerabilityImpactSound3;
    sfxHandle_t invulnerabilityJuicedSound;
    sfxHandle_t obeliskHitSound1;
    sfxHandle_t obeliskHitSound2;
    sfxHandle_t obeliskHitSound3;
    sfxHandle_t obeliskRespawnSound;
    sfxHandle_t winnerSound;
    sfxHandle_t loserSound;
    sfxHandle_t newHighScoreSound;   // [QL] "new_high_score.ogg" (EV_NEW_HIGH_SCORE announce)

    sfxHandle_t gibSound;
    sfxHandle_t gibBounce1Sound;
    sfxHandle_t gibBounce2Sound;
    sfxHandle_t gibBounce3Sound;
    sfxHandle_t teleInSound;
    sfxHandle_t teleOutSound;
    sfxHandle_t noAmmoSound;
    sfxHandle_t respawnSound;
    sfxHandle_t talkSound;
    sfxHandle_t landSound;
    sfxHandle_t fallSound;
    sfxHandle_t jumpPadSound;

    sfxHandle_t oneMinuteSound;
    sfxHandle_t fiveMinuteSound;
    sfxHandle_t suddenDeathSound;

    sfxHandle_t threeFragSound;
    sfxHandle_t twoFragSound;
    sfxHandle_t oneFragSound;

    sfxHandle_t hitSound;
    sfxHandle_t hitSoundHighArmor;
    sfxHandle_t hitSoundLowArmor;
    sfxHandle_t hitTeamSound;
    sfxHandle_t impressiveSound;
    sfxHandle_t excellentSound;
    sfxHandle_t deniedSound;
    sfxHandle_t humiliationSound;
    sfxHandle_t assistSound;
    sfxHandle_t defendSound;
    sfxHandle_t firstImpressiveSound;
    sfxHandle_t firstExcellentSound;
    sfxHandle_t firstHumiliationSound;

    sfxHandle_t takenLeadSound;
    sfxHandle_t tiedLeadSound;
    sfxHandle_t lostLeadSound;

    sfxHandle_t voteNow;
    sfxHandle_t votePassed;
    sfxHandle_t voteFailed;

    sfxHandle_t watrInSound;
    sfxHandle_t watrOutSound;
    sfxHandle_t watrUnSound;

    sfxHandle_t flightSound;
    sfxHandle_t medkitSound;

    sfxHandle_t weaponHoverSound;

    // teamplay sounds
    sfxHandle_t captureAwardSound;
    sfxHandle_t redScoredSound;
    sfxHandle_t blueScoredSound;
    sfxHandle_t redLeadsSound;
    sfxHandle_t blueLeadsSound;
    sfxHandle_t teamsTiedSound;

    // [QL] match/round-win announcer VOs (GTS 14-18)
    sfxHandle_t redWinsSound;
    sfxHandle_t blueWinsSound;
    sfxHandle_t redWinsRoundSound;
    sfxHandle_t blueWinsRoundSound;
    sfxHandle_t roundDrawSound;

    sfxHandle_t captureYourTeamSound;
    sfxHandle_t captureOpponentSound;
    sfxHandle_t returnYourTeamSound;
    sfxHandle_t returnOpponentSound;
    sfxHandle_t takenYourTeamSound;
    sfxHandle_t takenOpponentSound;

    sfxHandle_t redFlagReturnedSound;
    sfxHandle_t blueFlagReturnedSound;

    sfxHandle_t neutralFlagReturnedSound;

    sfxHandle_t enemyTookYourFlagSound;
    sfxHandle_t yourTeamTookEnemyFlagSound;
    sfxHandle_t youHaveFlagSound;

    sfxHandle_t enemyTookTheFlagSound;
    sfxHandle_t yourTeamTookTheFlagSound;
    sfxHandle_t yourBaseIsUnderAttackSound;

    sfxHandle_t holyShitSound;

    // tournament sounds
    sfxHandle_t count3Sound;
    sfxHandle_t count2Sound;
    sfxHandle_t count1Sound;
    sfxHandle_t countFightSound;
    sfxHandle_t countPrepareSound;

    // new stuff
    qhandle_t patrolShader;
    qhandle_t assaultShader;
    qhandle_t campShader;
    qhandle_t followShader;
    qhandle_t defendShader;
    qhandle_t teamLeaderShader;
    qhandle_t retrieveShader;
    qhandle_t escortShader;
    qhandle_t flagShaders[3];
    sfxHandle_t countPrepareTeamSound;

    sfxHandle_t ammoregenSound;
    sfxHandle_t doublerSound;
    sfxHandle_t guardSound;
    sfxHandle_t scoutSound;

    qhandle_t cursor;
    qhandle_t selectCursor;
    qhandle_t sizeCursor;

    sfxHandle_t regenSound;
    sfxHandle_t protectSound;
    sfxHandle_t n_healthSound;
    sfxHandle_t hgrenb1aSound;
    sfxHandle_t hgrenb2aSound;
    sfxHandle_t wstbimplSound;
    sfxHandle_t wstbimpmSound;
    sfxHandle_t wstbimpdSound;
    sfxHandle_t wstbactvSound;

    // [QL] additional sounds
    sfxHandle_t armorRegenSound;
    sfxHandle_t overtimeSound;
    sfxHandle_t pauseSound;  // [QL] klaxon on pause start (CG_DrawTimeout)
    sfxHandle_t thawTickSound;
    sfxHandle_t raceFinishSound;
    sfxHandle_t infectedSound;

    // [QL] Race checkpoint models and shaders (binary-verified)
    qhandle_t raceFlagB;          // models/flag3/b_flag3.md3 (blue)
    qhandle_t raceFlagF;          // models/flag3/f_flag3.md3 (finish)
    qhandle_t raceFlagG;          // models/flag3/g_flag3.md3 (green/start)
    qhandle_t raceFlagD;          // models/flag3/d_flag3.md3 (default/checkpoint)
    qhandle_t raceMarkerStart;    // gfx/2d/race/start
    qhandle_t raceMarkerCheckpoint; // gfx/2d/race/checkpoint
    qhandle_t raceMarkerFinish;   // gfx/2d/race/finish
    qhandle_t domPointModel;      // models/powerups/domination/dompoint.md3 (pedestal)
    qhandle_t domSkinRed;         // models/powerups/domination/domred.skin
    qhandle_t domSkinBlue;        // models/powerups/domination/domblue.skin
    qhandle_t domSkinNeutral;     // models/powerups/domination/domntrl.skin

    // [QL] POI markers
    qhandle_t poiShader;        // sprites/neutralflagcarrier

    // [QL] freeze tag
    qhandle_t iceShardModel;     // models/gibs/sphere.md3
    qhandle_t iceShardShader1;   // powerups/ice1
    qhandle_t iceShardShader2;   // powerups/ice2
    qhandle_t iceShardShader3;   // powerups/ice3
    qhandle_t frozenShader;      // sprites/frozen
    qhandle_t iceMarkShader;     // iceMark

    // [QL] gametype icons for scoreboard/HUD
    qhandle_t gametypeIcon[GT_MAX_GAME_TYPE];

    // [QL] player outline shaders, indexed 0..4 by outline style (= currentState.powerups-1).
    // Used by CG_PlayerOutline. Register order in CG_RegisterGraphics still to be pinned.
    qhandle_t friendlyOutlineShader[5];
    qhandle_t enemyOutlineShader[5];
    qhandle_t friendlyPowerupOutlineShader[5];
    qhandle_t enemyPowerupOutlineShader[5];

    // [QL] freeze-tag effect media (CG_PlayerFreezeEffect / CG_FreezeEffect / CG_ThawPlayer).
    // (frozenShader already declared above.)
    qhandle_t freezeShader;        // freeze overlay shader
    qhandle_t freezeModel;         // freeze-tag ice model (RT_MODEL), global 0x10a5f740
    qhandle_t frozenFlagShader;    // frozen flag overlay
    sfxHandle_t freezeSound;       // played on CHAN_BODY by CG_FreezeEffect, global 0x10a5f944
    qhandle_t iceWhiteModel;       // ice shard model A, global 0x10a5f428 (first/odd shards)
    qhandle_t iceBlueModel;        // ice shard model B, global 0x10a5f42c (alternate shards)

    // [QL] friend marker (CG_PlayerFloatSprite)
    qhandle_t friendShader;
    qhandle_t friendMarkModel;

    // [QL] wallbang / shotgun-kill debris
    qhandle_t debrisPuffShader;    // CG_MissileHitWall_DmgThrough debris
    qhandle_t bloodPuffShader;     // CG_ShotgunKillEffect / CG_ShotgunPellet blood puff

    // [QL] global team-sound extras
    sfxHandle_t survivorSound;     // sound/feedback/survivor_01.ogg, GTS_SURVIVOR (case 0x18)
    sfxHandle_t lastStandingSound; // last_standing.ogg (DAT_10a5fab0), GTS_LAST_STANDING (case 0x13)

    // [QL] CTF flag-status HUD media (CG_DrawFlagStatus / CG_DrawFlagStatusBar).
    // flagStatusHandles indexed as [iconIndex + team*4] (icon 0=atbase,1=taken,2=dropped,3=neutral;
    // team 0=neutral,1=red,2=blue). Distinct from the 4-entry flagShader[] above; base DAT_10a5fc2c.
    qhandle_t flagStatusHandles[12];
    qhandle_t flagStatusBarLeft;   // background left half-shader (DAT_10076354)
    qhandle_t flagStatusBarRight;  // background right half-shader (DAT_10076374)
    qhandle_t flagCarrierIcon;     // shared carrier head/marker (DAT_10a5fc78)

    // [QL] duel player-status plate shaders (CG_DrawPlayerStatusLeft/Right).
    // Globals DAT_10a5f360..0x10a5f384.
    qhandle_t duelStatusDefault, duelStatusLeads, duelStatusTrails, duelStatusTied, duelStatusReady;
    qhandle_t duelStatusDefault_right, duelStatusLeads_right, duelStatusTrails_right, duelStatusTied_right, duelStatusReady_right;

} cgMedia_t;

// The client game static (cgs) structure hold everything
// loaded or calculated from the gamestate.  It will NOT
// be cleared when a tournement restart is done, allowing
// all clients to begin playing instantly
typedef struct {
    gameState_t gameState;  // gamestate from server
    glconfig_t glconfig;    // rendering configuration
    float screenXScale;     // derived from glconfig
    float screenYScale;
    float screenXBias;
    float widescreenBias;   // [QL] half extra width on widescreen (0 if 4:3 or narrower)

    int serverCommandSequence;  // reliable command stream counter
    int processedSnapshotNum;   // the number of snapshots cgame has requested

    qboolean localServer;  // detected on startup by checking sv_running

    // parsed from serverinfo (order matches QL binary's CG_ParseServerinfo)
    gametype_t gametype;
    int teamsize;           // [QL]
    int teamSizeMin;        // [QL] g_teamSizeMin
    int teamForceBalance;   // [QL] g_teamForceBalance
    int dmflags;
    int fraglimit;
    int capturelimit;
    int scorelimit;         // [QL]
    int mercylimit;         // [QL]
    int timelimit;
    int roundlimit;
    int roundtimelimit;
    int roundWarmupDelay;   // [QL] g_roundWarmupDelay
    int freezeRoundDelay;   // [QL] g_freezeRoundDelay
    int maxclients;
    int timeoutCount;       // [QL] g_timeoutCount
    int timeoutsRed;        // [QL] CS_TIMEOUTS_RED (671); empty CS falls back to timeoutCount
    int timeoutsBlue;       // [QL] CS_TIMEOUTS_BLUE (672)
    int timelimit_overtime;  // [QL] g_overtime
    int itemHeight;         // [QL] g_itemHeight
    int gravity;            // [QL] g_gravity
    int weaponRespawn;      // [QL] g_weaponRespawn
    int itemTimers;         // [QL] g_itemTimers
    int quadDamageFactor;   // [QL] g_quadDamageFactor
    int voteFlags;          // [QL] g_voteFlags
    int startingHealth;     // [QL] g_startingHealth
    int adCaptureScoreBonus;  // [QL] g_adCaptureScoreBonus
    int adElimScoreBonus;     // [QL] g_adElimScoreBonus
    int adtouchScoreBonus;    // [QL] g_adtouchScoreBonus
    char mapname[MAX_QPATH];
    char redTeam[MAX_QPATH];
    char blueTeam[MAX_QPATH];

    // [QL] parsed from CS_PLAYERINFO (CG_ParseConfigParams, 0x10048e40). A non-empty
    // override string disables client model/head forcing for that part.
    char playermodelOverride[MAX_QPATH];      // g_playermodelOverride
    char playerheadmodelOverride[MAX_QPATH];  // g_playerheadmodelOverride
    int allowCustomHeadmodels;                // g_allowCustomHeadmodels
    float playerheadScale;                    // g_playerheadScale
    float playerheadScaleOffset;              // g_playerheadScaleOffset
    float playerModelScale;                   // g_playerModelScale

    int voteTime;
    int voteYes;
    int voteNo;
    qboolean voteModified;  // beep whenever changed
    char voteString[MAX_STRING_TOKENS];

    int teamVoteTime[2];
    int teamVoteYes[2];
    int teamVoteNo[2];
    qboolean teamVoteModified[2];  // beep whenever changed
    char teamVoteString[2][MAX_STRING_TOKENS];

    int levelStartTime;

    int scores1, scores2;   // from configstrings
    int redflag, blueflag;  // flag status from configstrings
    int flagStatus;

    qboolean newHud;

    //
    // locally derived information from gamestate
    //
    qhandle_t gameModels[MAX_MODELS];
    sfxHandle_t gameSounds[MAX_SOUNDS];

    int numInlineModels;
    qhandle_t inlineDrawModel[MAX_MODELS];
    vec3_t inlineModelMidpoints[MAX_MODELS];

    clientInfo_t clientinfo[MAX_CLIENTS];

    // teamchat width is *3 because of embedded color codes
    char teamChatMsgs[TEAMCHAT_HEIGHT][TEAMCHAT_WIDTH * 3 + 1];
    int teamChatMsgTimes[TEAMCHAT_HEIGHT];
    int teamChatPos;
    int teamLastChatPos;

    int cursorX;
    int cursorY;
    qboolean eventHandling;
    qboolean mouseCaptured;
    qboolean sizingHud;
    void* capturedItem;
    qhandle_t activeCursor;

    // orders
    int currentOrder;
    qboolean orderPending;
    int orderTime;
    int currentVoiceClient;
    int acceptOrderTime;
    int acceptTask;
    int acceptLeader;
    char acceptVoice[MAX_NAME_LENGTH];

    // media
    cgMedia_t media;

    // ads
    qboolean adsLoaded;
    int numAds;
    float adverts[MAX_MAP_ADVERTISEMENTS * 16];
    char adShaders[MAX_MAP_ADVERTISEMENTS][MAX_QPATH];

    // [QL] round-based mode tracking
    qboolean roundStarted;
    int roundNum;

    // [QL] duel player configstring indices
    int clientNum1stPlayer;
    int clientNum2ndPlayer;

    // [QL] HUD configstrings
    int teamCountRed;    // CS_TEAMCOUNT_RED  (alive red count for round modes)
    int teamCountBlue;   // CS_TEAMCOUNT_BLUE (alive blue count for round modes)
    int armorTiered;     // CS_ARMORINFO tiered-armor flag (mirrors cg_armorTiered)
    int customSettings;  // CS_CUSTOM_SETTINGS bitmask (DAT_10a3ff28); bit 0x4000000 = g_rrInfected
    int playerCylinders; // CS_PLAYER_CYLINDERS (DAT_10a5fd9c); capsule hulls for hit prediction

    // [QL] wallbang surface depth (_DAT_10a5fda8). Parsed from a server configstring,
    // used only by CG_MissileHitWall_DmgThrough. Not a client cvar.
    float dmgThroughDepth;

    // [QL] per-team alive count for CG_DrawTeamAliveCount (CA/FT). Binary indexes int array base DAT_10a404c8.
    int teamAliveCount[TEAM_NUM_TEAMS];

    // [QL] freeze/timeout end time (DAT_10a403e0). Used by CG_GetOvertimeCount /
    // CG_GetLevelTimerMsec; 0 means "use cg.time".
    int freezeEnd;
    // [QL] pause auto-unpause / timein-countdown target (DAT_10a403e4, CS_PAUSE_END_TIME).
    // 0 = indefinite pause; non-zero = level.time the match resumes. Used by CG_DrawTimeout.
    int pauseEnd;

    // [QL] practice-mode flag (CS_PRACTICE 667, DAT_10a3ff30); warmup / weapon-select HUD.
    int practice;
    // [QL] free-spectator camera flag (CS_FREECAM 668, DAT_10a5fd0c); CG_DrawSpectator hint.
    int freecam;
    // [QL] "all ready" countdown target time (CS_ALLREADY_TIME 708, DAT_10a403d8);
    // read by CG_DrawWarmupMessages.
    int allReadyTime;
    // [QL] round start time (CS_ROUND_START_TIME 662, DAT_10a403dc); round timer + sounds.
    int roundStartTime;
} cgs_t;

//==============================================================================

extern cgs_t cgs;
extern cg_t cg;
extern centity_t cg_entities[MAX_GENTITIES];

// [QL] Color wheel arrays (runtime-initialized, 26 entries for 'a'-'z')
extern unsigned int g_colorWheel[26];
extern vec3_t g_colorWheelNormalized[26];
extern weaponInfo_t cg_weapons[MAX_WEAPONS];
extern itemInfo_t cg_items[MAX_ITEMS];
extern markPoly_t cg_markPolys[MAX_MARK_POLYS];

// [QL] packed 0xRRGGBBAA team rail colours (DAT_10a377cc / DAT_10a6efac), parsed from the
// team-rail-colour cvar strings. Bytes: >>24, >>16&0xff, >>8&0xff; core scaled by 0.75.
extern int cg_teamRailColor1[1];
extern int cg_teamRailColor2[1];

// [QL] cgame-only cvar flag selecting the range-registration path in CG_RegisterCvars.
#define CVAR_CG_RANGE 0x1000

extern vmCvar_t cg_centertime;
extern vmCvar_t cg_runpitch;
extern vmCvar_t cg_runroll;
extern vmCvar_t cg_bobup;
extern vmCvar_t cg_bobpitch;
extern vmCvar_t cg_bobroll;
extern vmCvar_t cg_swingSpeed;
extern vmCvar_t cg_shadows;
extern vmCvar_t cg_gibs;
extern vmCvar_t cg_drawTimer;
extern vmCvar_t cg_drawFPS;
extern vmCvar_t cg_drawSnapshot;
extern vmCvar_t cg_draw3dIcons;
extern vmCvar_t cg_drawIcons;
extern vmCvar_t cg_drawAmmoWarning;
extern vmCvar_t cg_drawCrosshair;
extern vmCvar_t cg_drawCrosshairNames;
extern vmCvar_t cg_drawRewards;
extern vmCvar_t cg_flagStyle;
extern vmCvar_t cg_drawTeamOverlay;
extern vmCvar_t cg_teamOverlayUserinfo;
extern vmCvar_t cg_crosshairX;
extern vmCvar_t cg_crosshairY;
extern vmCvar_t cg_crosshairSize;
extern vmCvar_t cg_crosshairHealth;
extern vmCvar_t cg_drawStatus;
extern vmCvar_t cg_drawSpecMessages;
extern vmCvar_t cg_draw2D;
extern vmCvar_t cg_animSpeed;
extern vmCvar_t cg_debugAnim;
extern vmCvar_t cg_debugEvents;
extern vmCvar_t cg_railTrailTime;
extern vmCvar_t cg_errorDecay;
extern vmCvar_t cg_nopredict;
extern vmCvar_t cg_noPlayerAnims;
extern vmCvar_t cg_showmiss;
extern vmCvar_t cg_footsteps;
extern vmCvar_t cg_addMarks;
extern vmCvar_t cg_brassTime;
extern vmCvar_t cg_gun_frame;
extern vmCvar_t cg_gun_x;
extern vmCvar_t cg_gun_y;
extern vmCvar_t cg_gun_z;
extern vmCvar_t cg_drawGun;
extern vmCvar_t cg_viewsize;
extern vmCvar_t cg_tracerChance;
extern vmCvar_t cg_tracerWidth;
extern vmCvar_t cg_tracerLength;
extern vmCvar_t cg_autoswitch;
extern vmCvar_t cg_ignore;
extern vmCvar_t cg_simpleItems;
extern vmCvar_t cg_fov;
extern vmCvar_t cg_zoomFov;
extern vmCvar_t cg_thirdPersonRange;
extern vmCvar_t cg_thirdPersonAngle;
extern vmCvar_t cg_thirdPerson;
extern vmCvar_t cg_lagometer;
extern vmCvar_t cg_drawAttacker;
extern vmCvar_t cg_teamChatTime;
extern vmCvar_t cg_teamChatHeight;
extern vmCvar_t cg_stats;
extern vmCvar_t cg_paused;
extern vmCvar_t cg_blood;
extern vmCvar_t cg_predictItems;
extern vmCvar_t cg_deferPlayers;
extern vmCvar_t cg_teamChatsOnly;
extern vmCvar_t cg_complaintWarning;

extern vmCvar_t cg_scorePlum;
extern vmCvar_t cg_smoothClients;
// pmove_fixed / pmove_msec removed - not in QL binary
extern vmCvar_t cg_cameraOrbit;
extern vmCvar_t cg_cameraOrbitDelay;
extern vmCvar_t cg_timescaleFadeEnd;
extern vmCvar_t cg_timescaleFadeSpeed;
extern vmCvar_t cg_timescale;
extern vmCvar_t cg_cameraMode;

extern vmCvar_t cg_smallFont;
extern vmCvar_t cg_bigFont;
extern vmCvar_t cg_noTaunt;

extern vmCvar_t cg_noProjectileTrail;
extern vmCvar_t cg_bubbleTrail;
extern vmCvar_t cg_oldRail;
extern vmCvar_t cg_oldRocket;
extern vmCvar_t cg_oldPlasma;
extern vmCvar_t cg_trueLightning;

extern vmCvar_t cg_currentSelectedPlayer;
extern vmCvar_t cg_currentSelectedPlayerName;
extern vmCvar_t cg_enableDust;
extern vmCvar_t cg_recordSPDemo;
extern vmCvar_t cg_recordSPDemoName;
extern vmCvar_t cg_obeliskRespawnDelay;
extern vmCvar_t cg_lightningStyle;
extern vmCvar_t cg_screenDamage;
extern vmCvar_t cg_kickScale;

// [QL] additional cvars
extern vmCvar_t cg_damagePlum;
extern vmCvar_t cg_damagePlumColorStyle;
extern vmCvar_t cg_armorTiered;
extern vmCvar_t cg_announcer;
extern vmCvar_t cg_autoHop;
extern vmCvar_t cg_predictLocalRailshots;  // [QL] predicted railgun autofire
extern vmCvar_t cg_blood;
extern vmCvar_t cg_drawCheckpointRemaining;
extern vmCvar_t cg_drawSprites;
extern vmCvar_t cg_drawSpriteSelf;
extern vmCvar_t cg_enemyCrosshairNames;
extern vmCvar_t cg_forceEnemyModel;
extern vmCvar_t cg_forceEnemySkin;
extern vmCvar_t cg_forceTeamModel;
extern vmCvar_t cg_forceTeamSkin;
extern vmCvar_t cg_forceBlueTeamModel;
extern vmCvar_t cg_forceRedTeamModel;
extern vmCvar_t r_colorCorrectActive;  // [QL] renderer cvar mirror, tint scale in CG_PlayerTeamSkins
extern vmCvar_t cg_hitBeep;
extern vmCvar_t cg_killBeep;
extern vmCvar_t cg_lightningImpactCap;
extern vmCvar_t cg_lightningImpact;         // [QL] gate the LG impact flare
extern vmCvar_t cg_rocketTrailRadius;       // [QL] rocket smoke-trail radius (0 disables)
extern vmCvar_t cg_grenadeTrailRadius;      // [QL] grenade smoke-trail radius (0 disables)
extern vmCvar_t cg_nailTrailRadius;         // [QL] nail smoke-trail radius (0 disables)
extern vmCvar_t cg_railReloadTime;          // [QL] railgun refire interval (ms) for view-model tint
extern vmCvar_t cg_loadout;
extern vmCvar_t cg_scalePlayerModelsToBB;
extern vmCvar_t cg_screenDamageAlpha;
extern vmCvar_t cg_screenDamageAlpha_Team;
extern vmCvar_t cg_spectating;
extern vmCvar_t cg_specItemTimers;
extern vmCvar_t cg_speedometer;
extern vmCvar_t cg_flagPOIs;
extern vmCvar_t cg_poiMaxWidth;
extern vmCvar_t cg_poiMinWidth;
extern vmCvar_t cg_powerupPOIs;
extern vmCvar_t cg_teammatePOIs;
extern vmCvar_t cg_teammatePOIsMaxWidth;
extern vmCvar_t cg_teammatePOIsMinWidth;
extern vmCvar_t cg_weaponBar;
extern vmCvar_t cg_weaponPrimary;
extern vmCvar_t cg_drawFullWeaponBar;
extern vmCvar_t cg_lowAmmoWeaponBarWarning;
extern vmCvar_t cg_obituaryRowSize;
extern vmCvar_t s_announcerVolume;
extern vmCvar_t s_killBeepVolume;

// QL cvars (binary-verified from cgamex86.dll)
extern vmCvar_t cg_chatbeep;
extern vmCvar_t cg_deadBodyColor;
extern vmCvar_t cg_deadBodyDarken;
extern vmCvar_t g_training;  // [QL] server training flag; gates model forcing (CG_ResolveModelForClient)
extern vmCvar_t cg_enemyHeadColor;
extern vmCvar_t cg_enemyLowerColor;
extern vmCvar_t cg_enemyUpperColor;
extern vmCvar_t cg_followKiller;
extern vmCvar_t cg_followPowerup;
extern vmCvar_t cg_impactMarkTime;
extern vmCvar_t cg_impactSparks;
extern vmCvar_t cg_impactSparksLifetime;
extern vmCvar_t cg_impactSparksSize;
extern vmCvar_t cg_impactSparksVelocity;
extern vmCvar_t cg_muzzleFlash;
extern vmCvar_t cg_plasmaStyle;
extern vmCvar_t cg_railStyle;
extern vmCvar_t cg_rocketStyle;
extern vmCvar_t cg_smoke_SG;
extern vmCvar_t cg_specFov;
extern vmCvar_t cg_switchOnEmpty;
extern vmCvar_t cg_switchToEmpty;
extern vmCvar_t cg_teamHeadColor;
extern vmCvar_t cg_teamLowerColor;
extern vmCvar_t cg_teamUpperColor;
extern vmCvar_t cg_trueShotgun;
extern vmCvar_t cg_vignette;
extern vmCvar_t cg_zoomOutOnDeath;
extern vmCvar_t cg_zoomScaling;
extern vmCvar_t cg_zoomToggle;

// [QL] team rail-colour gates (DAT_10a658ec / DAT_10a6288c; tested == 1 exactly).
extern vmCvar_t cg_forceTeamRailColor1;
extern vmCvar_t cg_forceTeamRailColor2;
// [QL] positive-sense taunt gate (DAT_10a68eec). EV_TAUNT plays only when non-zero.
extern vmCvar_t cg_allowTaunt;
// [QL] GTS_LAST_STANDING announcer gate (DAT_10b716ec).
extern vmCvar_t cg_announcerLastStanding;

//
// cg_main.c
//
const char* CG_ConfigString(int index);
const char* CG_Argv(int arg);

void QDECL CG_Printf(const char* msg, ...) __attribute__((format(printf, 1, 2)));
void QDECL CG_Error(const char* msg, ...) __attribute__((noreturn, format(printf, 1, 2)));

void CG_StartMusic(void);

void CG_UpdateCvars(void);

int CG_CrosshairPlayer(void);
int CG_LastAttacker(void);
void CG_LoadMenus(const char* menuFile);
void CG_ParseMenu(const char* menuFile);
void CG_KeyEvent(int key, qboolean down);
void CG_MouseEvent(int x, int y);
void CG_EventHandling(int type);
void CG_RankRunFrame(void);
void CG_SetScoreSelection(void* menu);
score_t* CG_GetSelectedScore(void);
void CG_BuildSpectatorString(void);

// [QL] cgameExport_t slots 10-14
void CG_LastChatCommand(void);
void CG_LastChatCommand2(void);
void CG_SetKeyCatcher(void);
void CG_ClearKeyCatcher(void);
int  CG_GetActiveFrame(void);

// [QL] race / match-state helpers
void        CG_RaceInit(int fullReset);
const char* CG_FormatRaceTime(int msec);
int         CG_GetOvertimeCount(void);
const char* CG_DidLocalPlayerWin(void);
const char* CG_TranslateMapName(const char* mapname);
void        CG_CacheCountryFlags(void);

//
// cg_view.c
//
void CG_TestModel_f(void);
void CG_TestGun_f(void);
void CG_TestModelNextFrame_f(void);
void CG_TestModelPrevFrame_f(void);
void CG_TestModelNextSkin_f(void);
void CG_TestModelPrevSkin_f(void);
void CG_ZoomDown_f(void);
void CG_ZoomUp_f(void);
void CG_AddBufferedSound(sfxHandle_t sfx);

void CG_DrawAdvertisements(void);
void CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback);
void CG_AddPOIMarkers(void);
// [QL] CG_CheckAutoFollow, CG_FilterAngles, CG_DrawDuelWeaponStats are file-static in cg_view.c.
qboolean CG_GetLevelTimerMsec(int* msec);

//
// cg_drawtools.c
//
// [QL] server command parsers
void CG_ParseDuelScores(void);
void CG_ParseAccuracy(void);
void CG_ParseTeamStats(void);

void CG_AdjustFrom640(float* x, float* y, float* w, float* h);
void CG_SetWidescreen(int mode);
void CG_FillRect(float x, float y, float width, float height, const float* color);
void CG_DrawPic(float x, float y, float width, float height, qhandle_t hShader);
void CG_DrawString(float x, float y, const char* string, float charWidth, float charHeight, const float* modulate);

void CG_DrawStringExt(int x, int y, const char* string, const float* setColor, qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars);
void CG_DrawBigString(int x, int y, const char* s, float alpha);
void CG_DrawBigStringColor(int x, int y, const char* s, vec4_t color);
void CG_DrawSmallString(int x, int y, const char* s, float alpha);
void CG_DrawSmallStringColor(int x, int y, const char* s, vec4_t color);

int CG_DrawStrlen(const char* str);

float* CG_FadeColor(int startMsec, int totalMsec);
float* CG_TeamColor(int team);
void CG_TileClear(void);
void CG_ColorForHealth(vec4_t hcolor);
void CG_GetColorForHealth(int health, int armor, vec4_t hcolor);

void UI_DrawProportionalString(int x, int y, const char* str, int style, vec4_t color);
void CG_DrawRect(float x, float y, float width, float height, float size, const float* color);
void CG_DrawSides(float x, float y, float w, float h, float size);
void CG_DrawTopBottom(float x, float y, float w, float h, float size);

//
// cg_draw.c, cg_newDraw.c
//
extern int sortedTeamPlayers[TEAM_MAXOVERLAY];
extern int numSortedTeamPlayers;
extern int drawTeamOverlayModificationCount;
extern char systemChat[256];
extern char teamChat1[256];
extern char teamChat2[256];

void CG_AddLagometerFrameInfo(void);
void CG_AddLagometerSnapshotInfo(snapshot_t* snap);
void CG_CenterPrint(const char* str, int y, int charWidth);
void CG_DrawHead(float x, float y, float w, float h, int clientNum, vec3_t headAngles);
void CG_DrawActive(stereoFrame_t stereoView);
void CG_DrawFlagModel(float x, float y, float w, float h, int team, qboolean force2D);
void CG_DrawTeamBackground(int x, int y, int w, int h, float alpha, int team);
void CG_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle, int fontIndex);
void CG_Text_Paint(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style);
int CG_Text_Width(const char* text, float scale, int limit);
int CG_Text_Height(const char* text, float scale, int limit);
void CG_Text_Paint_Font(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style, fontInfo_t* font);
float CG_Text_Width_Font(const char* text, float scale, int limit, fontInfo_t* font);
float CG_Text_Height_Font(const char* text, float scale, int limit, fontInfo_t* font);
void CG_DrawText_DC(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style, int fontIndex);
void CG_DrawText(float x, float y, int fontIndex, float scale, vec4_t color, const char *text, float adjust, int maxChars, int textStyle);
int CG_DrawTextWidth(const char *text, float scale, int limit, int fontIndex);
extern int cg_currentWidescreen;
float CG_TextWidth_DC(const char* text, float scale, int limit, int fontIndex);
float CG_TextHeight_DC(const char* text, float scale, int limit, int fontIndex);
void CG_DrawTextWithCursor_DC(float x, float y, float scale, vec4_t color, const char* text, int cursorPos, char cursor, int limit, int style, int fontIndex);
void CG_SelectPrevPlayer(void);
void CG_SelectNextPlayer(void);
float CG_GetValue(int ownerDraw);
qboolean CG_OwnerDrawVisible(int flags, int flags2);
void CG_RunMenuScript(char** args);
void CG_ExecuteText(int exec_when, const char* text);
void CG_ShowResponseHead(void);
void CG_SetPrintString(int type, const char* p);
void CG_InitTeamChat(void);
void CG_GetTeamColor(vec4_t* color);
const char* CG_GetGameStatusText(void);
const char* CG_GetMatchStatusText(void);
const char* CG_GetKillerText(void);
void CG_Draw3DModel(float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles);
void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader);
void CG_CheckOrderPending(void);
const char* CG_GameTypeString(void);
qboolean CG_YourTeamHasFlag(void);
qboolean CG_OtherTeamHasFlag(void);
qhandle_t CG_StatusHandle(int task);

//
// cg_player.c
//
void CG_Player(centity_t* cent);
void CG_ResetPlayerEntity(centity_t* cent);
void CG_AddRefEntityWithPowerups(refEntity_t* ent, entityState_t* state, int team);
void CG_NewClientInfo(int clientNum);
sfxHandle_t CG_CustomSound(int clientNum, const char* soundName);

// [QL] model/skin forcing + scaling. CG_ResolveModelForClient, CG_CalcModelScale,
// CG_PlayerTeamSkins and CG_PlayerFreezeEffect are file-static in cg_players.c;
// CG_PlayerOutline and CG_Draw3DPlayerModel are cross-file (see below).
qboolean CG_ShouldForceTeamSkin(int playerTeam, int viewerTeam);
// [QL] AD-objective outline decal - the binary calls this from the entity dispatch in
// cg_ents.c (not from CG_Player), so it's declared here.
void CG_PlayerOutline(centity_t* cent);
// [QL] full legs+torso+head player model for the spectator/duel model owner-draws in
// cg_newdraw.c (replaces the head-only CG_DrawHead stand-in).
void CG_Draw3DPlayerModel(float x, float y, float w, float h, int clientNum, int weapon);
void CG_ResolveBodySkinName(clientInfo_t* ci, int forceTeamSkin);
void CG_ResolveHeadSkinName(clientInfo_t* ci, int forceTeamSkin);
void CG_UpdateAllModelScales(void);  // cross-file: called when the model-scale cvar changes

//
// cg_predict.c
//
void CG_BuildSolidList(void);
int CG_PointContents(const vec3_t point, int passEntityNum);
void CG_Trace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, int mask);
void CG_CapsuleTrace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, int mask);
void CG_PredictPlayerState(void);
void CG_LoadDeferredPlayers(void);
void CG_ForceModelChange(void);

//
// cg_events.c
//
void CG_CheckEvents(centity_t* cent);
const char* CG_PlaceString(int rank);
void CG_EntityEvent(centity_t* cent, vec3_t position);
void CG_PainEvent(centity_t* cent, int health);
// [QL] event-driven helpers
void CG_ItemPickupSpec(entityState_t* es);   // spectator item-pickup notify
void CG_PlayIntermissionSound(void);
void CG_PlayMatchStateSound(int state);

//
// cg_ents.c
//
void CG_SetEntitySoundPosition(centity_t* cent);
void CG_AddPacketEntities(void);
void CG_Beam(centity_t* cent);
void CG_AdjustPositionForMover(const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out, vec3_t angles_in, vec3_t angles_out);

void CG_PositionEntityOnTag(refEntity_t* entity, const refEntity_t* parent, qhandle_t parentModel, char* tagName);
void CG_PositionRotatedEntityOnTag(refEntity_t* entity, const refEntity_t* parent, qhandle_t parentModel, char* tagName);

//
// cg_weapons.c
//
void CG_NextWeapon_f(void);
void CG_PrevWeapon_f(void);
void CG_Weapon_f(void);

void CG_RegisterWeapon(int weaponNum);
void CG_RegisterItemVisuals(int itemNum);

void CG_FireWeapon(centity_t* cent);
void CG_MissileHitWall(int weapon, int clientNum, vec3_t origin, vec3_t dir, impactSound_t soundType);
void CG_MissileHitPlayer(int weapon, vec3_t origin, vec3_t dir, int entityNum);
void CG_ShotgunFire(entityState_t* es);
void CG_Bullet(vec3_t origin, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum);

void CG_RailTrail(clientInfo_t* ci, vec3_t start, vec3_t end);
void CG_GrappleTrail(centity_t* ent, const weaponInfo_t* wi);
// [QL] persistent rail-trail spawn (start = pe.railgunTrailStart, end = es->origin2).
// CG_RailTrailCore, CG_GetRailColorFloat, CG_GetRailColorByte are file-static in cg_weapons.c.
void CG_SpawnRailTrail(centity_t* cent);
void CG_MissileHitWall_DmgThrough(vec3_t origin, vec3_t dir, int weapon);  // wallbang (EV_MISSILE_MISS_DMGTHROUGH)
void CG_ShotgunKillEffect(centity_t* cent);  // EV_SHOTGUN_KILL blood-puff burst
void CG_AddViewWeapon(playerState_t* ps);
void CG_AddPlayerWeapon(refEntity_t* parent, playerState_t* ps, centity_t* cent, int team);
void CG_DrawWeaponSelect(void);
void CG_DrawWeaponBar(void);    // [QL] weapon bar (Left/Right/Centered/Classic modes)

void CG_OutOfAmmoChange(void);  // should this be in pmove?

//
// cg_marks.c
//
void CG_InitMarkPolys(void);
void CG_AddMarks(void);
void CG_ImpactMark(qhandle_t markShader,
                   const vec3_t origin,
                   const vec3_t dir,
                   float orientation,
                   float r,
                   float g,
                   float b,
                   float a,
                   qboolean alphaFade,
                   float radius,
                   qboolean temporary);

//
// cg_localents.c
//
void CG_InitLocalEntities(void);
localEntity_t* CG_AllocLocalEntity(void);
void CG_AddLocalEntities(void);

//
// cg_effects.c
//
localEntity_t* CG_SmokePuff(const vec3_t p,
                            const vec3_t vel,
                            float radius,
                            float r,
                            float g,
                            float b,
                            float a,
                            float duration,
                            int startTime,
                            int fadeInTime,
                            int leFlags,
                            qhandle_t hShader);
void CG_BubbleTrail(vec3_t start, vec3_t end, float spacing);
void CG_SpawnEffect(vec3_t org);
void CG_KamikazeEffect(vec3_t org);
void CG_ObeliskExplode(vec3_t org, int entityNum);
void CG_ObeliskPain(vec3_t org);
void CG_InvulnerabilityImpact(vec3_t org, vec3_t angles);
void CG_InvulnerabilityJuiced(vec3_t org);
void CG_LightningBoltBeam(vec3_t start, vec3_t end);

void CG_ScorePlum(int client, vec3_t org, int score);
void CG_DamagePlum(int damage, int weapon, vec3_t org);  // [QL]

// [QL] shared floating-effect pool (cg_effects.c)
floatingEffect_t* CG_AllocFloatingEffect(void);  // [QL] binary "CG_AllocMark" @ 0x1002a0d0
void CG_UpdateFloatingEffects(void);             // [QL] binary "CG_UpdateConfigStrings" @ 0x1002a190
void CG_DrawFloatingEffects(void);               // [QL] binary "CG_DrawDamagePlums" @ 0x10011680

// [QL] freeze-tag / discharge / blood client-side effects (triggered from CG_EntityEvent).
void CG_FreezeEffect(vec3_t origin);
void CG_ThawPlayer(const vec3_t origin);
void CG_SpawnIceShard(const vec3_t origin, const vec3_t velocity, qhandle_t hModel);
void CG_LightningDischargeEffect(int intensity);
void CG_BloodSplat(vec3_t origin);      // EV_INVUL_IMPACT (0x46) handler
void CG_PlayBloodSound(vec3_t origin);  // EV_OBELISKPAIN (0x45) handler

// [QL] announcer sound ring buffer.
void CG_QueueAnnouncement(sfxHandle_t sfx);
void CG_ResetAnnouncements(void);

void CG_GibPlayer(vec3_t playerOrigin);
void CG_BigExplode(vec3_t playerOrigin);

void CG_Bleed(vec3_t origin, int entityNum);
void CG_BloodSplatEffect(vec3_t origin, int entityNum);  // QL: blood splat (replaces CG_Bleed for hit effects)
void CG_SpawnParticleEffect(vec3_t vel, float size, float r, float g, float b, float a, float lifetime, int startTime, int type, qhandle_t shader);  // QL: particle effects
void CG_SpecAutoFollow(int clientNum, int mode);  // QL: auto-follow spectator

localEntity_t* CG_MakeExplosion(vec3_t origin, vec3_t dir, qhandle_t hModel, qhandle_t shader, int msec, qboolean isSprite);

//
// cg_snapshot.c
//
void CG_ProcessSnapshots(void);

//
// cg_info.c
//
void CG_LoadingString(const char* s);
void CG_LoadingItem(int itemNum);
void CG_LoadingClient(int clientNum);
void CG_DrawInformation(void);

//
// cg_scoreboard.c
//
qboolean CG_DrawOldScoreboard(void);

// [QL] scoreboard / spectator hud menu handles, resolved by CG_CacheHudMenuHandles after
// every menu load (CG_LoadHudMenu and the loadhud command). Binary globals 0x10ab9674-98.
extern menuDef_t* menuScoreboard;
extern menuDef_t* menuEndScoreboard;
extern menuDef_t* menuStats;
extern menuDef_t* menuJoinGame;
extern menuDef_t* menuCompSpecHud;
extern menuDef_t* menuCompSpecFollowHud;
extern menuDef_t* menuCompSpecRedTop;
extern menuDef_t* menuCompSpecRedBottom;
extern menuDef_t* menuCompSpecBlueTop;
extern menuDef_t* menuCompSpecBlueBottom;

void CG_SetEndScoreboardMenu(void);
void CG_CacheHudMenuHandles(void);
void CG_CloseMenus(void);
void CG_DrawSpectator(const rectDef_t* rect);

//
// cg_consolecmds.c
//
qboolean CG_ConsoleCommand(void);
void CG_InitConsoleCommands(void);
void CG_ScoresDown_f(void);
void CG_ClearChat(void);
void CG_AddChat(const char *text, int teamOnly, int extraTime);
void CG_DrawChat(const rectDef_t* rect, float scale);
void CG_InitColorWheel(void);

//
// cg_servercmds.c
//
void CG_ExecuteNewServerCommands(int latestSequence);
void CG_ParseServerinfo(void);
void CG_SetConfigValues(void);
void CG_ParsePmoveParams(void);
void CG_ParseConfigParams(void);
void CG_ShaderStateChanged(void);
void CG_LoadVoiceChats(void);
void CG_VoiceChatLocal(int clientNum, const char* cmd);
void CG_PlayBufferedVoiceChats(void);

// [QL] extended team-stats parsers (per-client verbs; arg1 = scoreboard slot into cg.scores[])
void CG_ParseTeamStats_TDM(void);
void CG_ParseTeamStats_CA(void);
void CG_ParseTeamStats_CTF(void);
void CG_InitScores(void);  // scores_ad / adscores parser (22 ints -> cg.adScores + cg.teamScores)

// [QL] voice-chat receive path (entity-event driven via CG_VoiceChatLocal)
void CG_PlayVoiceChat(bufferedVoiceChat_t* vchat);
void CG_AddBufferedVoiceChat(bufferedVoiceChat_t* vchat);
qboolean CG_GetVoiceChat(const char* cmd, sfxHandle_t* snd, const char** chat);
int CG_ParseVoiceChats(const char* filename);
void CG_VoiceChatListForClient(int clientNum);

//
// cg_playerstate.c
//
void CG_Respawn(void);
void CG_TransitionPlayerState(playerState_t* ps, playerState_t* ops);
void CG_CheckChangedPredictableEvents(playerState_t* ps);

//===============================================

//
// system traps
// These functions are how the cgame communicates with the main game system
//

// print message on the local console
void trap_Print(const char* fmt);

// abort the game
void trap_Error(const char* fmt) __attribute__((noreturn));

// milliseconds should only be used for performance tuning, never
// for anything game related.  Get time from the CG_DrawActiveFrame parameter
int trap_Milliseconds(void);

// console variable interaction
void trap_Cvar_Register(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, int flags);
// [QL] 5-arg range cvar register, used by CG_RegisterCvars when CVAR_CG_RANGE is set.
void trap_Cvar_Register_Extended(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, float minValue, float maxValue);
void trap_Cvar_Update(vmCvar_t* vmCvar);
void trap_Cvar_Set(const char* var_name, const char* value);
void trap_Cvar_VariableStringBuffer(const char* var_name, char* buffer, int bufsize);

// ServerCommand and ConsoleCommand parameter access
int trap_Argc(void);
void trap_Argv(int n, char* buffer, int bufferLength);
void trap_Args(char* buffer, int bufferLength);

// filesystem access
// returns length of file
int trap_FS_FOpenFile(const char* qpath, fileHandle_t* f, fsMode_t mode);
void trap_FS_Read(void* buffer, int len, fileHandle_t f);
void trap_FS_Write(const void* buffer, int len, fileHandle_t f);
void trap_FS_FCloseFile(fileHandle_t f);
int trap_FS_Seek(fileHandle_t f, long offset, int origin);  // fsOrigin_t

// add commands to the local console as if they were typed in
// for map changing, etc.  The command is not executed immediately,
// but will be executed in order the next time console commands
// are processed
void trap_SendConsoleCommand(const char* text);

// register a command name so the console can perform command completion.
// FIXME: replace this with a normal console command "defineCommand"?
void trap_AddCommand(const char* cmdName);
void trap_RemoveCommand(const char* cmdName);

// send a string to the server over the network
void trap_SendClientCommand(const char* s);

// force a screen update, only used during gamestate load
void trap_UpdateScreen(void);

// model collision
void trap_CM_LoadMap(const char* mapname);
int trap_CM_NumInlineModels(void);
clipHandle_t trap_CM_InlineModel(int index);  // 0 = world, 1+ = bmodels
clipHandle_t trap_CM_TempBoxModel(const vec3_t mins, const vec3_t maxs);
clipHandle_t trap_CM_TempCapsuleModel(const vec3_t mins, const vec3_t maxs);
int trap_CM_PointContents(const vec3_t p, clipHandle_t model);
int trap_CM_TransformedPointContents(const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles);
void trap_CM_BoxTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask);
void trap_CM_CapsuleTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask);
void trap_CM_TransformedBoxTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles);
void trap_CM_TransformedCapsuleTrace(trace_t* results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles);

// Returns the projection of a polygon onto the solid brushes in the world
int trap_CM_MarkFragments(int numPoints, const vec3_t* points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t* fragmentBuffer);

// normal sounds will have their volume dynamically changed as their entity
// moves and the listener moves
void trap_S_StartSound(vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx);
void trap_S_StopLoopingSound(int entnum);

// a local sound is always played full volume
void trap_S_StartLocalSound(sfxHandle_t sfx, int channelNum);
void trap_S_ClearLoopingSounds(qboolean killall);
void trap_S_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
void trap_S_AddRealLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx);
void trap_S_UpdateEntityPosition(int entityNum, const vec3_t origin);

// respatialize recalculates the volumes of sound as they should be heard by the
// given entityNum and position
void trap_S_Respatialize(int entityNum, const vec3_t origin, vec3_t axis[3], int inwater);
sfxHandle_t trap_S_RegisterSound(const char* sample, qboolean compressed);  // returns buzz if not found
void trap_S_StartBackgroundTrack(const char* intro, const char* loop);      // empty name stops music
void trap_S_StopBackgroundTrack(void);

void trap_R_LoadWorldMap(const char* mapname);

// all media should be registered during level startup to prevent
// hitches during gameplay
qhandle_t trap_R_RegisterModel(const char* name);        // returns rgb axis if not found
qhandle_t trap_R_RegisterSkin(const char* name);         // returns all white if not found
qhandle_t trap_R_RegisterShader(const char* name);       // returns all white if not found
qhandle_t trap_R_RegisterShaderNoMip(const char* name);  // returns all white if not found

// a scene is built up by calls to R_ClearScene and the various R_Add functions.
// Nothing is drawn until R_RenderScene is called.
void trap_R_ClearScene(void);
void trap_R_AddRefEntityToScene(const refEntity_t* re);

// polys are intended for simple wall marks, not really for doing
// significant construction
void trap_R_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts);
void trap_R_AddPolysToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts, int numPolys);
void trap_R_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void trap_R_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b);
int trap_R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
void trap_R_RenderScene(const refdef_t* fd);
void trap_R_SetColor(const float* rgba);  // NULL = 1,1,1,1
void trap_R_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
void trap_R_ModelBounds(clipHandle_t model, vec3_t mins, vec3_t maxs);
int trap_R_LerpTag(orientation_t* tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char* tagName);
void trap_R_RemapShader(const char* oldShader, const char* newShader, const char* timeOffset);
qboolean trap_R_inPVS(const vec3_t p1, const vec3_t p2);

// The glconfig_t will not change during the life of a cgame.
// If it needs to change, the entire cgame will be restarted, because
// all the qhandle_t are then invalid.
void trap_GetGlconfig(glconfig_t* glconfig);

// the gamestate should be grabbed at startup, and whenever a
// configstring changes
void trap_GetGameState(gameState_t* gamestate);

// cgame will poll each frame to see if a newer snapshot has arrived
// that it is interested in.  The time is returned separately so that
// snapshot latency can be calculated.
void trap_GetCurrentSnapshotNumber(int* snapshotNumber, int* serverTime);

// a snapshot get can fail if the snapshot (or the entties it holds) is so
// old that it has fallen out of the client system queue
qboolean trap_GetSnapshot(int snapshotNumber, snapshot_t* snapshot);

// retrieve a text command from the server stream
// the current snapshot will hold the number of the most recent command
// qfalse can be returned if the client system handled the command
// argc() / argv() can be used to examine the parameters of the command
qboolean trap_GetServerCommand(int serverCommandNumber);

// returns the most recent command number that can be passed to GetUserCmd
// this will always be at least one higher than the number in the current
// snapshot, and it may be quite a few higher if it is a fast computer on
// a lagged connection
int trap_GetCurrentCmdNumber(void);

qboolean trap_GetUserCmd(int cmdNumber, usercmd_t* ucmd);

// used for the weapon select and zoom
// [QL] 4-arg variant matches cgamex86.dll CG_DrawActiveFrame syscall:
//   stateValue       -> cmd.weapon         (usercmd_t offset 0x14)
//   weaponPrimary    -> cmd.weaponPrimary  (usercmd_t offset 0x15) - loadout
//   sensitivityScale -> mouse zoom sens (engine cl.cgameSensitivity)
//   fov              -> cmd.fov            (usercmd_t offset 0x16)
void trap_SetUserCmdValue(int stateValue, int weaponPrimary, float sensitivityScale, int fov);

// aids for VM testing
void testPrintInt(char* string, int i);
void testPrintFloat(char* string, float f);

int trap_MemoryRemaining(void);
void trap_R_RegisterFont(const char* fontName, int pointSize, fontInfo_t* font);
void trap_R_Font_DrawString(int x, int y, const char* text, int fontIndex, float scale, int limit, float* maxX, int flags);
void trap_R_Font_TextExtents(const char* text, int start, int limit, float scale, int fontIndex, int* outX, int* outY, int* outW, int* outH);
void trap_R_GetGlyphInfo(int fontIndex, int charValue, glyphInfo_t* glyph);
void trap_IME_SetCompositionFont(int fontIndex, float scale);
qboolean trap_Key_IsDown(int keynum);
int trap_Key_GetCatcher(void);
void trap_Key_SetCatcher(int catcher);
int trap_Key_GetKey(const char* binding);
void trap_Key_KeynumToStringBuf(int keynum, char* buf, int buflen);
void trap_Key_GetBindingBuf(int keynum, char* buf, int buflen);
void trap_S_MuteClient(int clientNum, qboolean mute);

void CG_KeyNameForCommand(const char* command, char* buf, int buflen);

void trap_Get_Advertisements(int* num, float* verts, char shaders[][MAX_QPATH]);

typedef enum {
    SYSTEM_PRINT,
    CHAT_PRINT,
    TEAMCHAT_PRINT
} q3print_t;

int trap_CIN_PlayCinematic(const char* arg0, int xpos, int ypos, int width, int height, int bits);
e_status trap_CIN_StopCinematic(int handle);
e_status trap_CIN_RunCinematic(int handle);
void trap_CIN_DrawCinematic(int handle);
void trap_CIN_SetExtents(int handle, int x, int y, int w, int h);

int trap_RealTime(qtime_t* qtime);
void trap_SnapVector(float* v);

qboolean trap_loadCamera(const char* name);
void trap_startCamera(int time);
qboolean trap_getCameraInfo(int time, vec3_t* origin, vec3_t* angles);

qboolean trap_GetEntityToken(char* buffer, int bufferSize);

void CG_ClearParticles(void);
void CG_AddParticles(void);
void CG_ParticleSnow(qhandle_t pshader, vec3_t origin, vec3_t origin2, int turb, float range, int snum);
void CG_ParticleSmoke(qhandle_t pshader, centity_t* cent);
void CG_AddParticleShrapnel(localEntity_t* le);
void CG_ParticleSnowFlurry(qhandle_t pshader, centity_t* cent);
void CG_ParticleBulletDebris(vec3_t org, vec3_t vel, int duration);
void CG_ParticleSparks(vec3_t org, vec3_t vel, int duration, float x, float y, float speed);
void CG_ParticleDust(centity_t* cent, vec3_t origin, vec3_t dir);
void CG_ParticleMisc(qhandle_t pshader, vec3_t origin, int size, int duration, float alpha);
void CG_ParticleExplosion(char* animStr, vec3_t origin, vec3_t vel, int duration, int sizeStart, int sizeEnd);
extern qboolean initparticles;
int CG_NewParticleArea(int num);
