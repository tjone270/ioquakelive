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
// cg_main.c -- initialization and primary entry point for cgame
#include "cg_local.h"

#include "../ui/ui_shared.h"
// display context for new ui stuff
displayContextDef_t cgDC;

// [QL] per-cvar modificationCount cache for the six model/skin-forcing cvars watched by
// CG_UpdateCvars (binary 0x10020CA0 stores six counts individually). QL has no single
// cg_forceModel; forcing is the granular set.
static int cg_forceModelModCounts[6] = { -1, -1, -1, -1, -1, -1 };
static qboolean CG_ForceModelCvarsChanged(void);

void CG_Init(int serverMessageNum, int serverCommandSequence, int clientNum);
void CG_RegisterCvars(void);
void CG_Shutdown(void);

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
Q_EXPORT intptr_t vmMain(int command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10, intptr_t arg11) {
    switch (command) {
        case CG_INIT:
            CG_Init(arg0, arg1, arg2);
            return 0;
        case CG_REGISTER_CVARS:
            CG_RegisterCvars();
            return 0;
        case CG_SHUTDOWN:
            CG_Shutdown();
            return 0;
        case CG_CONSOLE_COMMAND:
            return CG_ConsoleCommand();
        case CG_DRAW_ACTIVE_FRAME:
            CG_DrawActiveFrame(arg0, arg1, arg2);
            return 0;
        case CG_CROSSHAIR_PLAYER:
            return CG_CrosshairPlayer();
        case CG_LAST_ATTACKER:
            return CG_LastAttacker();
        case CG_KEY_EVENT:
            CG_KeyEvent(arg0, arg1);
            return 0;
        case CG_MOUSE_EVENT:
            cgDC.cursorx = cgs.cursorX;
            cgDC.cursory = cgs.cursorY;
            CG_MouseEvent(arg0, arg1);
            return 0;
        case CG_EVENT_HANDLING:
            CG_EventHandling(arg0);
            return 0;
        // [QL] cgamex86.dll exports 15 entries (table at 0x100769a8); slots 10-14 below.
        case CG_LAST_CHAT_COMMAND:
            CG_LastChatCommand();
            return 0;
        case CG_LAST_CHAT_COMMAND2:
            CG_LastChatCommand2();
            return 0;
        case CG_SET_KEY_CATCHER:
            CG_SetKeyCatcher();
            return 0;
        case CG_CLEAR_KEY_CATCHER:
            CG_ClearKeyCatcher();
            return 0;
        case CG_GET_ACTIVE_FRAME:
            return CG_GetActiveFrame();
        default:
            CG_Error("vmMain: unknown command %i", command);
            break;
    }
    return -1;
}

cg_t cg;
cgs_t cgs;
centity_t cg_entities[MAX_GENTITIES];
weaponInfo_t cg_weapons[MAX_WEAPONS];
itemInfo_t cg_items[MAX_ITEMS];

vmCvar_t cg_railTrailTime;
vmCvar_t cg_centertime;
vmCvar_t cg_runpitch;
vmCvar_t cg_runroll;
vmCvar_t cg_bobup;
vmCvar_t cg_bobpitch;
vmCvar_t cg_bobroll;
vmCvar_t cg_swingSpeed;
vmCvar_t cg_shadows;
vmCvar_t cg_gibs;
vmCvar_t cg_drawTimer;
vmCvar_t cg_drawFPS;
vmCvar_t cg_drawSnapshot;
vmCvar_t cg_draw3dIcons;
vmCvar_t cg_drawIcons;
vmCvar_t cg_drawAmmoWarning;
vmCvar_t cg_drawCrosshair;
vmCvar_t cg_drawCrosshairNames;
vmCvar_t cg_drawRewards;
vmCvar_t cg_crosshairSize;
vmCvar_t cg_crosshairX;
vmCvar_t cg_crosshairY;
vmCvar_t cg_crosshairHealth;
vmCvar_t cg_draw2D;
vmCvar_t cg_drawStatus;
vmCvar_t cg_animSpeed;
vmCvar_t cg_debugAnim;
vmCvar_t cg_debugEvents;
vmCvar_t cg_errorDecay;
vmCvar_t cg_nopredict;
vmCvar_t cg_noPlayerAnims;
vmCvar_t cg_showmiss;
vmCvar_t cg_footsteps;
vmCvar_t cg_addMarks;
vmCvar_t cg_brassTime;
vmCvar_t cg_viewsize;
vmCvar_t cg_drawGun;
vmCvar_t cg_gun_frame;
vmCvar_t cg_gun_x;
vmCvar_t cg_gun_y;
vmCvar_t cg_gun_z;
vmCvar_t cg_tracerChance;
vmCvar_t cg_tracerWidth;
vmCvar_t cg_tracerLength;
vmCvar_t cg_autoswitch;
vmCvar_t cg_ignore;
vmCvar_t cg_simpleItems;
vmCvar_t cg_fov;
vmCvar_t cg_zoomFov;
vmCvar_t cg_thirdPerson;
vmCvar_t cg_thirdPersonRange;
vmCvar_t cg_thirdPersonAngle;
vmCvar_t cg_lagometer;
vmCvar_t cg_drawAttacker;
vmCvar_t cg_teamChatTime;
vmCvar_t cg_teamChatHeight;
vmCvar_t cg_stats;
vmCvar_t cg_paused;
vmCvar_t cg_blood;
vmCvar_t cg_predictItems;
vmCvar_t cg_deferPlayers;
vmCvar_t cg_drawTeamOverlay;
vmCvar_t cg_teamOverlayUserinfo;
vmCvar_t cg_teamChatsOnly;

vmCvar_t cg_hudFiles;
vmCvar_t cg_scorePlum;
vmCvar_t cg_smoothClients;
vmCvar_t cg_pmove_msec;
vmCvar_t cg_cameraMode;
vmCvar_t cg_cameraOrbit;
vmCvar_t cg_cameraOrbitDelay;
vmCvar_t cg_timescaleFadeEnd;
vmCvar_t cg_timescaleFadeSpeed;
vmCvar_t cg_timescale;

vmCvar_t cg_smallFont;
vmCvar_t cg_bigFont;
vmCvar_t cg_noTaunt;

vmCvar_t cg_noProjectileTrail;
vmCvar_t cg_oldRail;
vmCvar_t cg_oldRocket;
vmCvar_t cg_oldPlasma;
vmCvar_t cg_trueLightning;

vmCvar_t cg_currentSelectedPlayer;
vmCvar_t cg_currentSelectedPlayerName;
vmCvar_t cg_enableDust;
vmCvar_t cg_recordSPDemo;
vmCvar_t cg_recordSPDemoName;
vmCvar_t ui_singlePlayerActive;  // [QL] missing from binary parity audit
vmCvar_t cg_obeliskRespawnDelay;
vmCvar_t cg_lightningStyle;
vmCvar_t cg_screenDamage;
vmCvar_t cg_kickScale;

// [QL] additional cvars
vmCvar_t cg_armorTiered;
vmCvar_t cg_allowTaunt;
vmCvar_t cg_announcer;
vmCvar_t cg_autoHop;
vmCvar_t cg_predictLocalRailshots;  // [QL] predicted railgun autofire
vmCvar_t cg_bob;
vmCvar_t cg_bubbleTrail;
vmCvar_t cg_buzzerSound;
vmCvar_t cg_crosshairBrightness;
vmCvar_t cg_crosshairColor;
vmCvar_t cg_crosshairHitColor;
vmCvar_t cg_crosshairHitStyle;
vmCvar_t cg_crosshairHitTime;
vmCvar_t cg_crosshairPulse;
vmCvar_t cg_damagePlum;
vmCvar_t cg_damagePlumColorStyle;
vmCvar_t cg_deadBodyColor;
vmCvar_t cg_deadBodyDarken;
vmCvar_t cg_drawCrosshairTeamHealth;
vmCvar_t cg_drawFragMessages;
vmCvar_t cg_drawFullWeaponBar;
vmCvar_t cg_drawItemPickups;
vmCvar_t cg_drawCheckpointRemaining;
vmCvar_t cg_drawSpecMessages;
vmCvar_t cg_drawPregameMessages;
vmCvar_t cg_drawTieredArmorAvailability;
vmCvar_t cg_enemyHeadColor;
vmCvar_t cg_enemyLowerColor;
vmCvar_t cg_enemyUpperColor;
vmCvar_t r_colorCorrectActive;  // [QL] renderer cvar, mirrored so CG_PlayerTeamSkins can scale the tint
vmCvar_t cg_flagStyle;
vmCvar_t cg_forceEnemyModel;
vmCvar_t cg_forceTeamModel;
vmCvar_t cg_hitBeep;
vmCvar_t cg_impactSparks;
vmCvar_t cg_impactSparksLifetime;
vmCvar_t cg_impactSparksSize;
vmCvar_t cg_impactSparksVelocity;
vmCvar_t cg_itemFx;
vmCvar_t cg_itemTimers;
vmCvar_t cg_killBeep;
vmCvar_t cg_levelTimerDirection;
vmCvar_t cg_lightningImpact;
vmCvar_t cg_rocketTrailRadius;
vmCvar_t cg_grenadeTrailRadius;
vmCvar_t cg_nailTrailRadius;
vmCvar_t cg_railReloadTime;
vmCvar_t cg_lowAmmoWarningPercentile;
vmCvar_t cg_lowAmmoWarningSound;
vmCvar_t cg_muzzleFlash;
vmCvar_t cg_obituaryRowSize;
vmCvar_t cg_playerLean;
vmCvar_t cg_plasmaStyle;
vmCvar_t cg_railStyle;
vmCvar_t cg_rocketStyle;
vmCvar_t cg_raceBeep;
vmCvar_t cg_screenDamage_Self;
vmCvar_t cg_screenDamage_Team;
vmCvar_t cg_simpleItemsBob;
vmCvar_t cg_simpleItemsHeightOffset;
vmCvar_t cg_simpleItemsRadius;
vmCvar_t cg_speedometer;
vmCvar_t cg_switchOnEmpty;
vmCvar_t cg_switchToEmpty;
vmCvar_t cg_teamHeadColor;
vmCvar_t cg_teamLowerColor;
vmCvar_t cg_teamUpperColor;
vmCvar_t cg_thirdPersonPitch;
vmCvar_t cg_trueShotgun;
vmCvar_t cg_vignette;
vmCvar_t cg_waterWarp;
vmCvar_t cg_weaponBar;
vmCvar_t cg_zoomOutOnDeath;
vmCvar_t cg_zoomScaling;
vmCvar_t cg_zoomSensitivity;
vmCvar_t cg_zoomToggle;
vmCvar_t cg_drawInputCmds;
vmCvar_t cg_drawTeamOverlayOpacity;
vmCvar_t cg_drawTeamOverlaySize;
vmCvar_t cg_drawTeamOverlayX;
vmCvar_t cg_drawTeamOverlayY;
vmCvar_t cg_impactMarkTime;
vmCvar_t cg_chatbeep;
vmCvar_t cg_teamChatBeep;
vmCvar_t cg_gametype;
vmCvar_t cg_svFps;
vmCvar_t cg_comMaxfps;

// [QL] announcer cvars
vmCvar_t cg_announcerLastStandingVO;
vmCvar_t cg_announcerLeadsVO;
vmCvar_t cg_announcerRewardsVO;
vmCvar_t cg_announcerTiesVO;

// [QL] sound cvars
vmCvar_t s_ambient;
vmCvar_t s_announcerVolume;
vmCvar_t s_killBeepVolume;

// [QL] weapon loadout/config cvars
vmCvar_t cg_weaponConfig;
vmCvar_t cg_weaponConfig_g;
vmCvar_t cg_weaponConfig_mg;
vmCvar_t cg_weaponConfig_sg;
vmCvar_t cg_weaponConfig_gl;
vmCvar_t cg_weaponConfig_rl;
vmCvar_t cg_weaponConfig_lg;
vmCvar_t cg_weaponConfig_rg;
vmCvar_t cg_weaponConfig_pg;
vmCvar_t cg_weaponConfig_bfg;
vmCvar_t cg_weaponConfig_gh;
vmCvar_t cg_weaponConfig_ng;
vmCvar_t cg_weaponConfig_pl;
vmCvar_t cg_weaponConfig_cg;
vmCvar_t cg_weaponConfig_hmg;
vmCvar_t cg_weaponColor_grenade;
vmCvar_t cg_weaponPrimary;

// [QL] spectator/demo cvars
vmCvar_t cg_drawDemoHUD;
vmCvar_t cg_followKiller;
vmCvar_t cg_followPowerup;
vmCvar_t cg_specDuelHealthArmor;
vmCvar_t cg_specDuelHealthColor;
vmCvar_t cg_specFov;
vmCvar_t cg_specItemTimers;
vmCvar_t cg_specItemTimersSize;
vmCvar_t cg_specItemTimersX;
vmCvar_t cg_specItemTimersY;
vmCvar_t cg_specNames;
vmCvar_t cg_specTeamVitals;
vmCvar_t cg_specTeamVitalsHealthColor;
vmCvar_t cg_specTeamVitalsWidth;
vmCvar_t cg_specTeamVitalsY;

// [QL] HUD/UI cvars
vmCvar_t cg_chatHistoryLength;
vmCvar_t cg_complaintWarning;
vmCvar_t cg_drawCrosshairTeamHealthSize;
vmCvar_t cg_drawInputCmdsSize;
vmCvar_t cg_drawInputCmdsX;
vmCvar_t cg_drawInputCmdsY;
vmCvar_t cg_drawRewardsRowSize;
vmCvar_t cg_drawSprites;
vmCvar_t cg_drawSpriteSelf;
vmCvar_t cg_enemyCrosshairNames;
vmCvar_t cg_enemyCrosshairNamesOpacity;
vmCvar_t cg_lowAmmoWeaponBarWarning;
vmCvar_t cg_selfOnTeamOverlay;
vmCvar_t cg_teammateCrosshairNames;
vmCvar_t cg_teammateCrosshairNamesOpacity;
vmCvar_t cg_teammateNames;
vmCvar_t cg_useItemMessage;
vmCvar_t cg_useItemWarning;
vmCvar_t cg_voiceChatIndicator;

// [QL] POI cvars
vmCvar_t cg_flagPOIs;
vmCvar_t cg_poiMaxWidth;
vmCvar_t cg_poiMinWidth;
vmCvar_t cg_powerupPOIs;
vmCvar_t cg_teammatePOIs;
vmCvar_t cg_teammatePOIsMaxWidth;
vmCvar_t cg_teammatePOIsMinWidth;

// [QL] smoke/particle cvars
vmCvar_t cg_smoke_SG;
vmCvar_t cg_smokeRadius_dust;
vmCvar_t cg_smokeRadius_flight;
vmCvar_t cg_smokeRadius_GL;
vmCvar_t cg_smokeRadius_haste;
vmCvar_t cg_smokeRadius_NG;
vmCvar_t cg_smokeRadius_RL;

// [QL] visual/rendering cvars
vmCvar_t cg_drawDeadFriendTime;
vmCvar_t cg_drawHitFriendTime;
vmCvar_t cg_forceBlueTeamModel;
vmCvar_t cg_forceDrawCrosshair;
vmCvar_t cg_forceEnemySkin;
vmCvar_t cg_forceEnemyWeaponColor;
vmCvar_t cg_forceRedTeamModel;
vmCvar_t cg_forceTeamSkin;
vmCvar_t cg_forceTeamWeaponColor;
vmCvar_t cg_lightningImpactCap;
vmCvar_t cg_screenDamageAlpha;
vmCvar_t cg_screenDamageAlpha_Team;
vmCvar_t cg_scalePlayerModelsToBB;

// [QL] gameplay cvars
vmCvar_t cg_autoAction;
vmCvar_t cg_autoProjectileNudge;
vmCvar_t cg_predictLocalRailshots;

// [QL] gameplay cvars (continued)
vmCvar_t cg_latchedHookOffset;
vmCvar_t cg_overheadNamesWidth;
vmCvar_t cg_preferredStartingWeapons;
vmCvar_t cg_projectileNudge;
vmCvar_t cg_weaponPrimaryQueued;

// [QL] camera/view cvars
vmCvar_t cg_cameraSmartMode;
vmCvar_t cg_cameraThirdPersonSmartMode;
vmCvar_t cg_filter_angles;

// [QL] loadout disable cvars
vmCvar_t cg_disableLoadout_bfg;
vmCvar_t cg_disableLoadout_cg;
vmCvar_t cg_disableLoadout_gl;
vmCvar_t cg_disableLoadout_hmg;
vmCvar_t cg_disableLoadout_lg;
vmCvar_t cg_disableLoadout_mg;
vmCvar_t cg_disableLoadout_ng;
vmCvar_t cg_disableLoadout_pg;
vmCvar_t cg_disableLoadout_pm;
vmCvar_t cg_disableLoadout_rg;
vmCvar_t cg_disableLoadout_rl;
vmCvar_t cg_disableLoadout_sg;
vmCvar_t cg_disableLoadout_gh;
vmCvar_t cg_disableLoadout_gt;

// [QL] game info cvars (server-to-client state)
vmCvar_t cg_gameInfo1;
vmCvar_t cg_gameInfo2;
vmCvar_t cg_gameInfo3;
vmCvar_t cg_gameInfo4;
vmCvar_t cg_gameInfo5;
vmCvar_t cg_gameInfo6;

// [QL] team name cvars
vmCvar_t cg_blueTeamName;
vmCvar_t cg_redTeamName;
vmCvar_t cg_playerTeam;

// [QL] voice/chat cvars
vmCvar_t cg_playVoiceChats;
vmCvar_t cg_showVoiceText;

// [QL] misc cvars
vmCvar_t cg_debugFlags;
vmCvar_t cg_drawProfileImages;
vmCvar_t cg_ignoreMouseInput;
vmCvar_t cg_lastmsg;
vmCvar_t cg_stereoSeparation;
vmCvar_t cg_trackPlayer;

// [QL] UI state cvars (shared with UI module)
vmCvar_t ui_endMapVotingDisabled;
vmCvar_t ui_gameTypeVotingDisabled;
vmCvar_t ui_intermission;
vmCvar_t ui_mainmenu;
vmCvar_t ui_mapVotingDisabled;
vmCvar_t ui_voteactive;
vmCvar_t ui_votestring;
vmCvar_t ui_warmup;

// [QL] g_ cvars read by cgame
vmCvar_t g_skipTrainingEnable;
vmCvar_t g_training;

// [QL] internal/ROM cvars
vmCvar_t cg_loadout;
vmCvar_t cg_spectating;

// [QL] Internal rail-forcing globals. These are NOT registered cvars in QL - the binary
// populates them (cg_forceTeamRailColor1 DAT_10a658ec, cg_forceTeamRailColor2 DAT_10a6288c,
// cg_teamRailColor1/2 packed colours) from server enforcement / the cvar-update path.
// Zero-initialised here, so rail forcing is inactive on a fresh client. Model/skin forcing
// is now driven by the real cg_force* cvars (see CG_ResolveModelForClient).
vmCvar_t cg_forceTeamRailColor1;
vmCvar_t cg_forceTeamRailColor2;
int cg_teamRailColor1[1];
int cg_teamRailColor2[1];

// [QL] Last-standing announcer gate (DAT_10b716ec). Registered below so the announcer
// defaults on; distinct from the cg_announcerLastStandingVO voice-set cvar.
vmCvar_t cg_announcerLastStanding;

typedef struct {
    vmCvar_t* vmCvar;
    char* cvarName;
    char* defaultString;
    int cvarFlags;
    // [QL] cgamex86.dll cvarTable_t is 6 fields (24 bytes): entries whose cvarFlags
    // carries CVAR_CG_RANGE (0x1000) are registered through the 5-arg range syscall.
    float minValue;
    float maxValue;
} cvarTable_t;

static cvarTable_t cvarTable[] = {
    {&cg_ignore, "cg_ignore", "0", 0},  // used for debugging
    {&cg_autoswitch, "cg_autoswitch", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] default 0 (was Q3 "1")
    {&cg_drawGun, "cg_drawGun", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_zoomFov, "cg_zoomfov", "30", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_fov, "cg_fov", "100", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_viewsize, "cg_viewsize", "100", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_shadows, "cg_shadows", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_gibs, "cg_gibs", "1", CVAR_ARCHIVE},
    {&cg_draw2D, "cg_draw2D", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawStatus, "cg_drawStatus", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawTimer, "cg_drawTimer", "0", CVAR_ARCHIVE},
    {&cg_drawFPS, "cg_drawFPS", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawSnapshot, "cg_drawSnapshot", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_draw3dIcons, "cg_draw3dIcons", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawIcons, "cg_drawIcons", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawAmmoWarning, "cg_drawAmmoWarning", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] default 2 (was Q3 "1")
    {&cg_drawAttacker, "cg_drawAttacker", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawCrosshair, "cg_drawCrosshair", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawCrosshairNames, "cg_drawCrosshairNames", "1", CVAR_ARCHIVE},
    {&cg_drawRewards, "cg_drawRewards", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_crosshairSize, "cg_crosshairSize", "32", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_crosshairHealth, "cg_crosshairHealth", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] default 0 (was Q3 "1")
    {&cg_crosshairX, "cg_crosshairX", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_crosshairY, "cg_crosshairY", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_brassTime, "cg_brassTime", "2500", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_simpleItems, "cg_simpleItems", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_addMarks, "cg_marks", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_lagometer, "cg_lagometer", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] default 0 (was Q3 "1")
    {&cg_railTrailTime, "cg_railTrailTime", "400", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_gun_x, "cg_gunX", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] binary: CVAR_ARCHIVE (player viewmodel offset, not a cheat)
    {&cg_gun_y, "cg_gunY", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_gun_z, "cg_gunZ", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_centertime, "cg_centertime", "3", CVAR_CHEAT},
    {&cg_runpitch, "cg_runpitch", "0.002", CVAR_ARCHIVE},
    {&cg_runroll, "cg_runroll", "0.005", CVAR_ARCHIVE},
    {&cg_bobup, "cg_bobup", "0.005", CVAR_CHEAT},
    {&cg_bobpitch, "cg_bobpitch", "0.002", CVAR_ARCHIVE},
    {&cg_bobroll, "cg_bobroll", "0.002", CVAR_ARCHIVE},
    {&cg_swingSpeed, "cg_swingSpeed", "0.3", CVAR_CHEAT},
    {&cg_animSpeed, "cg_animspeed", "1", CVAR_CHEAT},
    {&cg_debugAnim, "cg_debuganim", "0", CVAR_CHEAT},
    {&cg_debugEvents, "cg_debugevents", "0", CVAR_CHEAT},
    {&cg_errorDecay, "cg_errordecay", "100", CVAR_USERSAVE | CVAR_VM_CREATED},
    {&cg_nopredict, "cg_nopredict", "0", CVAR_VM_CREATED | CVAR_REPLICATE},
    {&cg_noPlayerAnims, "cg_noplayeranims", "0", CVAR_CHEAT},
    {&cg_showmiss, "cg_showmiss", "0", 0},
    {&cg_footsteps, "cg_footsteps", "1", CVAR_CHEAT},
    {&cg_tracerChance, "cg_tracerchance", "0.4", CVAR_CHEAT},
    {&cg_tracerWidth, "cg_tracerwidth", "1", CVAR_CHEAT},
    {&cg_tracerLength, "cg_tracerlength", "100", CVAR_CHEAT},
    {&cg_thirdPersonRange, "cg_thirdPersonRange", "80", CVAR_CHEAT},  // [QL] default 80 (was Q3 "40")
    {&cg_thirdPersonAngle, "cg_thirdPersonAngle", "0", CVAR_CHEAT},
    {&cg_thirdPerson, "cg_thirdPerson", "0", CVAR_CHEAT},  // [QL] cheat-protected (binary: CVAR_CHEAT)
    {&cg_teamChatTime, "cg_teamChatTime", "3000", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teamChatHeight, "cg_teamChatHeight", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_predictItems, "cg_predictItems", "1", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_USERINFO | CVAR_ARCHIVE},  // [QL] binary: CVAR_ARCHIVE | CVAR_USERINFO
    {&cg_deferPlayers, "cg_deferPlayers", "1", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] default 1 (was Q3 "0")
    {&cg_drawTeamOverlay, "cg_drawTeamOverlay", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] default 1 (was Q3 "0")
    {&cg_teamOverlayUserinfo, "teamoverlay", "0", CVAR_ROM | CVAR_USERINFO},
    {&cg_stats, "cg_stats", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_teamChatsOnly, "cg_teamChatsOnly", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    // the following variables are created in other parts of the system,
    // but we also reference them here
    {&cg_paused, "cl_paused", "0", CVAR_ROM},
    {&cg_blood, "com_blood", "1", CVAR_ARCHIVE},

    {&cg_currentSelectedPlayer, "cg_currentSelectedPlayer", "0", 0},
    {&cg_currentSelectedPlayerName, "cg_currentSelectedPlayerName", "", 0},
    {&cg_enableDust, "g_enableDust", "0", CVAR_SERVERINFO},
    {&cg_recordSPDemo, "ui_recordSPDemo", "0", 0},
    {&cg_recordSPDemoName, "ui_recordSPDemoName", "", 0},
    {&ui_singlePlayerActive, "ui_singlePlayerActive", "0", CVAR_USERINFO},  // [QL] binary: CVAR_USERINFO
    {&cg_obeliskRespawnDelay, "g_obeliskRespawnDelay", "10", 0},
    {&cg_hudFiles, "cg_hudFiles", "ui/hud.txt", CVAR_USERSAVE | CVAR_REPLICATE},

    {&cg_cameraOrbit, "cg_cameraOrbit", "0", CVAR_CHEAT},
    {&cg_cameraOrbitDelay, "cg_cameraOrbitDelay", "50", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_timescaleFadeEnd, "cg_timescaleFadeEnd", "1", 0},
    {&cg_timescaleFadeSpeed, "cg_timescaleFadeSpeed", "0", 0},
    {&cg_timescale, "timescale", "1", CVAR_VM_CREATED},
    {&cg_scorePlum, "cg_scorePlums", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] default 0 (was Q3 "1")
    {&cg_smoothClients, "cg_smoothClients", "0", CVAR_USERINFO | CVAR_ARCHIVE},
    {&cg_cameraMode, "com_cameraMode", "0", CVAR_CHEAT},

    {&cg_smallFont, "ui_smallFont", "0.25", CVAR_ROM},
    {&cg_bigFont, "ui_bigFont", "0.4", CVAR_ROM},
    {&cg_noTaunt, "cg_noTaunt", "0", CVAR_ARCHIVE},

    {&cg_noProjectileTrail, "cg_noProjectileTrail", "0", CVAR_ARCHIVE},
    {&cg_oldRail, "cg_oldRail", "1", CVAR_ARCHIVE},
    {&cg_oldRocket, "cg_oldRocket", "1", CVAR_ARCHIVE},
    {&cg_oldPlasma, "cg_oldPlasma", "1", CVAR_ARCHIVE},
    {&cg_trueLightning, "cg_trueLightning", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] default 1 (was Q3 "0.0")
    {&cg_lightningStyle, "cg_lightningStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_screenDamage, "cg_screenDamage", "0x700000C8", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_kickScale, "cg_kickScale", "0.25", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},

    // [QL] additional cvars from cgamex86.dll binary (build 1069)
    {&cg_armorTiered, "cg_armorTiered", "0", CVAR_ROM},
    {&cg_allowTaunt, "cg_allowTaunt", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_announcer, "cg_announcer", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_autoHop, "cg_autoHop", "1", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_USERINFO | CVAR_ARCHIVE},  // [QL] binary: CVAR_ARCHIVE | CVAR_USERINFO (server reads via userinfo)
    {&cg_predictLocalRailshots, "cg_predictLocalRailshots", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},  // [QL] predicted railgun autofire trail
    {&cg_bob, "cg_bob", "0.25", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_bubbleTrail, "cg_bubbleTrail", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_buzzerSound, "cg_buzzerSound", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_crosshairBrightness, "cg_crosshairBrightness", "1.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_crosshairColor, "cg_crosshairColor", "25", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_crosshairHitColor, "cg_crosshairHitColor", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_crosshairHitStyle, "cg_crosshairHitStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_crosshairHitTime, "cg_crosshairHitTime", "200.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_crosshairPulse, "cg_crosshairPulse", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_damagePlum, "cg_damagePlum", "g mg sg gl rl lg rg pg bfg gh cg ng pl hmg", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_damagePlumColorStyle, "cg_damagePlumColorStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_deadBodyColor, "cg_deadBodyColor", "0x101010FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_deadBodyDarken, "cg_deadBodyDarken", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawCrosshairTeamHealth, "cg_drawCrosshairTeamHealth", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawFragMessages, "cg_drawFragMessages", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawFullWeaponBar, "cg_drawFullWeaponBar", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawItemPickups, "cg_drawItemPickups", "3", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawCheckpointRemaining, "cg_drawCheckpointRemaining", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawSpecMessages, "cg_drawSpecMessages", "1", CVAR_USERSAVE | CVAR_VM_CREATED},
    {&cg_drawPregameMessages, "cg_drawPregameMessages", "1", CVAR_USERSAVE | CVAR_VM_CREATED},
    {&cg_drawTieredArmorAvailability, "cg_drawTieredArmorAvailability", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_enemyHeadColor, "cg_enemyHeadColor", "0x2a8000FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_enemyLowerColor, "cg_enemyLowerColor", "0x2a8000FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_enemyUpperColor, "cg_enemyUpperColor", "0x2a8000FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&r_colorCorrectActive, "r_colorCorrectActive", "0", 0},  // [QL] mirror of the renderer cvar (tint scale)
    {&cg_flagStyle, "cg_flagStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceEnemyModel, "cg_forceEnemyModel", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceTeamModel, "cg_forceTeamModel", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_hitBeep, "cg_hitBeep", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_impactSparks, "cg_impactSparks", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_impactSparksLifetime, "cg_impactSparksLifetime", "250", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_impactSparksSize, "cg_impactSparksSize", "8", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_impactSparksVelocity, "cg_impactSparksVelocity", "128", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_itemFx, "cg_itemFx", "7", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_itemTimers, "cg_itemTimers", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_killBeep, "cg_killBeep", "7", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_levelTimerDirection, "cg_levelTimerDirection", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_lightningImpact, "cg_lightningImpact", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_rocketTrailRadius, "cg_rocketTrailRadius", "64", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_grenadeTrailRadius, "cg_grenadeTrailRadius", "32", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_nailTrailRadius, "cg_nailTrailRadius", "16", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_railReloadTime, "cg_railReloadTime", "1500", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_lowAmmoWarningPercentile, "cg_lowAmmoWarningPercentile", "0.20", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_lowAmmoWarningSound, "cg_lowAmmoWarningSound", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_muzzleFlash, "cg_muzzleFlash", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_obituaryRowSize, "cg_obituaryRowSize", "5", CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_playerLean, "cg_playerLean", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_ARCHIVE},
    {&cg_plasmaStyle, "cg_plasmaStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_railStyle, "cg_railStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_rocketStyle, "cg_rocketStyle", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_raceBeep, "cg_raceBeep", "7", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_screenDamage_Self, "cg_screenDamage_Self", "0x00000000", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_screenDamage_Team, "cg_screenDamage_Team", "0x700000C8", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_simpleItemsBob, "cg_simpleItemsBob", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_simpleItemsHeightOffset, "cg_simpleItemsHeightOffset", "8", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_simpleItemsRadius, "cg_simpleItemsRadius", "15", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_speedometer, "cg_speedometer", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_switchOnEmpty, "cg_switchOnEmpty", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_switchToEmpty, "cg_switchToEmpty", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teamHeadColor, "cg_teamHeadColor", "0x808080FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teamLowerColor, "cg_teamLowerColor", "0x808080FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teamUpperColor, "cg_teamUpperColor", "0x808080FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_thirdPersonPitch, "cg_thirdPersonPitch", "-1", CVAR_CHEAT},
    {&cg_trueShotgun, "cg_trueShotgun", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_vignette, "cg_vignette", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_waterWarp, "cg_waterWarp", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponBar, "cg_weaponBar", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_zoomOutOnDeath, "cg_zoomOutOnDeath", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_zoomScaling, "cg_zoomScaling", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_zoomSensitivity, "cg_zoomSensitivity", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_zoomToggle, "cg_zoomToggle", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawInputCmds, "cg_drawInputCmds", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawTeamOverlayOpacity, "cg_drawTeamOverlayOpacity", "0.75", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawTeamOverlaySize, "cg_drawTeamOverlaySize", "0.16", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawTeamOverlayX, "cg_drawTeamOverlayX", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawTeamOverlayY, "cg_drawTeamOverlayY", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_impactMarkTime, "cg_impactMarkTime", "10000", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_chatbeep, "cg_chatbeep", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teamChatBeep, "cg_teamChatBeep", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_gametype, "cg_gametype", "0", CVAR_ROM},
    {&cg_svFps, "sv_fps", "40", CVAR_ROM},
    {&cg_comMaxfps, "com_maxfps", "125", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},

    // [QL] announcer cvars
    {&cg_announcerLastStandingVO, "cg_announcerLastStandingVO", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_announcerLeadsVO, "cg_announcerLeadsVO", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_announcerRewardsVO, "cg_announcerRewardsVO", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_announcerTiesVO, "cg_announcerTiesVO", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},

    // [QL] sound cvars
    {&s_ambient, "s_ambient", "1", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_LATCH | CVAR_ARCHIVE},
    {&s_announcerVolume, "s_announcerVolume", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&s_killBeepVolume, "s_killBeepVolume", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},

    // [QL] weapon config cvars
    {&cg_weaponConfig, "cg_weaponConfig", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_g, "cg_weaponConfig_g", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_mg, "cg_weaponConfig_mg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_sg, "cg_weaponConfig_sg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_gl, "cg_weaponConfig_gl", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_rl, "cg_weaponConfig_rl", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_lg, "cg_weaponConfig_lg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_rg, "cg_weaponConfig_rg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_pg, "cg_weaponConfig_pg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_bfg, "cg_weaponConfig_bfg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_gh, "cg_weaponConfig_gh", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_ng, "cg_weaponConfig_ng", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_pl, "cg_weaponConfig_pl", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_cg, "cg_weaponConfig_cg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponConfig_hmg, "cg_weaponConfig_hmg", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponColor_grenade, "cg_weaponColor_grenade", "0x007000FF", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_weaponPrimary, "cg_weaponPrimary", "", CVAR_ROM},

    // [QL] spectator/demo cvars
    {&cg_drawDemoHUD, "cg_drawDemoHUD", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_followKiller, "cg_followKiller", "0", CVAR_VM_CREATED},
    {&cg_followPowerup, "cg_followPowerup", "0", CVAR_VM_CREATED},
    {&cg_specDuelHealthArmor, "cg_specDuelHealthArmor", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specDuelHealthColor, "cg_specDuelHealthColor", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specFov, "cg_specFov", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specItemTimers, "cg_specItemTimers", "7", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specItemTimersSize, "cg_specItemTimersSize", "0.24", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specItemTimersX, "cg_specItemTimersX", "10", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specItemTimersY, "cg_specItemTimersY", "200", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specNames, "cg_specNames", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specTeamVitals, "cg_specTeamVitals", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specTeamVitalsHealthColor, "cg_specTeamVitalsHealthColor", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specTeamVitalsWidth, "cg_specTeamVitalsWidth", "100", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_specTeamVitalsY, "cg_specTeamVitalsY", "85", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},

    // [QL] HUD/UI cvars
    {&cg_chatHistoryLength, "cg_chatHistoryLength", "6", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_complaintWarning, "cg_complaintWarning", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawCrosshairTeamHealthSize, "cg_drawCrosshairTeamHealthSize", "0.12", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawInputCmdsSize, "cg_drawInputCmdsSize", "24", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawInputCmdsX, "cg_drawInputCmdsX", "320", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawInputCmdsY, "cg_drawInputCmdsY", "240", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawRewardsRowSize, "cg_drawRewardsRowSize", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawSprites, "cg_drawSprites", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawSpriteSelf, "cg_drawSpriteSelf", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_enemyCrosshairNames, "cg_enemyCrosshairNames", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_enemyCrosshairNamesOpacity, "cg_enemyCrosshairNamesOpacity", "0.75", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_lowAmmoWeaponBarWarning, "cg_lowAmmoWeaponBarWarning", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_selfOnTeamOverlay, "cg_selfOnTeamOverlay", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teammateCrosshairNames, "cg_teammateCrosshairNames", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teammateCrosshairNamesOpacity, "cg_teammateCrosshairNamesOpacity", "0.75", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teammateNames, "cg_teammateNames", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_useItemMessage, "cg_useItemMessage", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_useItemWarning, "cg_useItemWarning", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_voiceChatIndicator, "cg_voiceChatIndicator", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},

    // [QL] POI cvars
    {&cg_flagPOIs, "cg_flagPOIs", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_poiMaxWidth, "cg_poiMaxWidth", "32.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_poiMinWidth, "cg_poiMinWidth", "16.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_powerupPOIs, "cg_powerupPOIs", "2", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teammatePOIs, "cg_teammatePOIs", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teammatePOIsMaxWidth, "cg_teammatePOIsMaxWidth", "24.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_teammatePOIsMinWidth, "cg_teammatePOIsMinWidth", "4.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},

    // [QL] smoke/particle cvars
    {&cg_smoke_SG, "cg_smoke_SG", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_smokeRadius_dust, "cg_smokeRadius_dust", "24", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_smokeRadius_flight, "cg_smokeRadius_flight", "8", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_smokeRadius_GL, "cg_smokeRadius_GL", "64", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_smokeRadius_haste, "cg_smokeRadius_haste", "8", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_smokeRadius_NG, "cg_smokeRadius_NG", "16", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_smokeRadius_RL, "cg_smokeRadius_RL", "32", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},

    // [QL] visual/rendering cvars
    {&cg_blood, "cg_blood", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawDeadFriendTime, "cg_drawDeadFriendTime", "3000.0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_drawHitFriendTime, "cg_drawHitFriendTime", "100", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceBlueTeamModel, "cg_forceBlueTeamModel", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceDrawCrosshair, "cg_forceDrawCrosshair", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceEnemySkin, "cg_forceEnemySkin", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceEnemyWeaponColor, "cg_forceEnemyWeaponColor", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceRedTeamModel, "cg_forceRedTeamModel", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceTeamSkin, "cg_forceTeamSkin", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_forceTeamWeaponColor, "cg_forceTeamWeaponColor", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_lightningImpactCap, "cg_lightningImpactCap", "192", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_screenDamageAlpha, "cg_screenDamageAlpha", "200", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_screenDamageAlpha_Team, "cg_screenDamageAlpha_Team", "200", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_scalePlayerModelsToBB, "cg_scalePlayerModelsToBB", "1", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT

    // [QL] gameplay cvars
    {&cg_autoAction, "cg_autoAction", "3", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_USERINFO | CVAR_ARCHIVE},  // [QL] binary: CVAR_ARCHIVE | CVAR_USERINFO
    {&cg_autoProjectileNudge, "cg_autoProjectileNudge", "0", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_latchedHookOffset, "cg_latchedHookOffset", "1.0", CVAR_ROM},
    {&cg_overheadNamesWidth, "cg_overheadNamesWidth", "75", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_preferredStartingWeapons, "cg_preferredStartingWeapons", "", CVAR_USERSAVE | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_predictLocalRailshots, "cg_predictLocalRailshots", "1", 0},
    {&cg_projectileNudge, "cg_projectileNudge", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT

    // [QL] camera/view cvars
    {&cg_cameraSmartMode, "cg_cameraSmartMode", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_cameraThirdPersonSmartMode, "cg_cameraThirdPersonSmartMode", "1", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_filter_angles, "cg_filter_angles", "0", 0},
    {&cg_stereoSeparation, "cg_stereoSeparation", "0.4", CVAR_ARCHIVE},

    // [QL] loadout disable cvars - CVAR_ROM (set by server via CS_DISABLE_LOADOUT, must not persist)
    {&cg_disableLoadout_bfg, "cg_disableLoadout_bfg", "0", CVAR_ROM},
    {&cg_disableLoadout_cg, "cg_disableLoadout_cg", "0", CVAR_ROM},
    {&cg_disableLoadout_gh, "cg_disableLoadout_gh", "0", CVAR_ROM},
    {&cg_disableLoadout_gl, "cg_disableLoadout_gl", "0", CVAR_ROM},
    {&cg_disableLoadout_gt, "cg_disableLoadout_g",  "0", CVAR_ROM},  // [QL] cvar name matches cgamex86.dll string table
    {&cg_disableLoadout_hmg, "cg_disableLoadout_hmg", "0", CVAR_ROM},
    {&cg_disableLoadout_lg, "cg_disableLoadout_lg", "0", CVAR_ROM},
    {&cg_disableLoadout_mg, "cg_disableLoadout_mg", "0", CVAR_ROM},
    {&cg_disableLoadout_ng, "cg_disableLoadout_ng", "0", CVAR_ROM},
    {&cg_disableLoadout_pg, "cg_disableLoadout_pg", "0", CVAR_ROM},
    {&cg_disableLoadout_pm, "cg_disableLoadout_pl", "0", CVAR_ROM},  // [QL] cvar name matches cgamex86.dll string table
    {&cg_disableLoadout_rg, "cg_disableLoadout_rg", "0", CVAR_ROM},
    {&cg_disableLoadout_rl, "cg_disableLoadout_rl", "0", CVAR_ROM},
    {&cg_disableLoadout_sg, "cg_disableLoadout_sg", "0", CVAR_ROM},

    // [QL] game info cvars (server-to-client state)
    {&cg_gameInfo1, "cg_gameInfo1", "0", CVAR_ROM},
    {&cg_gameInfo2, "cg_gameInfo2", "0", CVAR_ROM},
    {&cg_gameInfo3, "cg_gameInfo3", "0", CVAR_ROM},
    {&cg_gameInfo4, "cg_gameInfo4", "0", CVAR_ROM},
    {&cg_gameInfo5, "cg_gameInfo5", "0", CVAR_ROM},
    {&cg_gameInfo6, "cg_gameInfo6", "0", CVAR_ROM},

    // [QL] team name cvars
    {&cg_blueTeamName, "cg_blueTeamName", "", 0},
    {&cg_redTeamName, "cg_redTeamName", "", 0},
    {&cg_playerTeam, "cg_playerTeam", "", CVAR_ROM},

    // [QL] voice/chat cvars
    {&cg_playVoiceChats, "cg_playVoiceChats", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_showVoiceText, "cg_showVoiceText", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT

    // [QL] misc cvars
    {&cg_debugFlags, "cg_debugFlags", "0", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_drawProfileImages, "cg_drawProfileImages", "1", CVAR_USERSAVE | CVAR_VM_CREATED | CVAR_REPLICATE | CVAR_ARCHIVE},
    {&cg_ignoreMouseInput, "cg_ignoreMouseInput", "0", CVAR_ROM},
    {&cg_lastmsg, "cg_lastmsg", "", 0},
    {&cg_trackPlayer, "cg_trackPlayer", "-1", CVAR_CHEAT},  // [QL] binary: CVAR_CHEAT
    {&cg_weaponPrimaryQueued, "cg_weaponPrimaryQueued", "", CVAR_TEMP},  // [QL] menu writes via exec; binary uses CVAR_TEMP

    // [QL] UI state cvars (shared with UI module)
    {&ui_endMapVotingDisabled, "ui_endMapVotingDisabled", "0", CVAR_ROM},
    {&ui_gameTypeVotingDisabled, "ui_gameTypeVotingDisabled", "0", CVAR_ROM},
    {&ui_intermission, "ui_intermission", "0", CVAR_ROM},
    {&ui_mainmenu, "ui_mainmenu", "1", CVAR_ROM},
    {&ui_mapVotingDisabled, "ui_mapVotingDisabled", "0", CVAR_ROM},
    {&ui_voteactive, "ui_voteactive", "0", CVAR_ROM},
    {&ui_votestring, "ui_votestring", "", CVAR_CHEAT},
    {&ui_warmup, "ui_warmup", "0", CVAR_ROM},

    // [QL] g_ cvars read by cgame
    {&g_skipTrainingEnable, "g_skipTrainingEnable", "0", 0},
    {&g_training, "g_training", "0", 0},

    // [QL] internal/ROM cvars
    {&cg_loadout, "cg_loadout", "0", CVAR_ROM},
    {&cg_spectating, "cg_spectating", "0", CVAR_ROM},

    // [QL] last-standing announcer gate (defaults on)
    {&cg_announcerLastStanding, "cg_announcerLastStanding", "1", CVAR_ARCHIVE},
};

static int cvarTableSize = ARRAY_LEN(cvarTable);

/*
=================
CG_QueueAnnouncement / CG_ResetAnnouncements

[QL] cgamex86.dll announcer queue (CG_QueueAnnouncement @ 0x1004e110,
CG_ResetAnnouncements @ 0x1004e180). In the binary these share the same ring the QL
CG_PlayBufferedSounds drains, with a 1500ms (0x5dc) stagger delay per entry.
ioquakelive has that buffered-sound ring (CG_AddBufferedSound /
CG_PlayBufferedSounds in cg_view.c, drained each frame on CHAN_ANNOUNCER), so a queued
announcement is routed straight through it. The per-entry stagger delay is not
reproduced (minor - announcements drain in order).
=================
*/
void CG_QueueAnnouncement(sfxHandle_t sound) {
    if (sound) {
        CG_AddBufferedSound(sound);
    }
}

void CG_ResetAnnouncements(void) {
    memset(cg.soundBuffer, 0, sizeof(cg.soundBuffer));
    cg.soundBufferIn = 0;
    cg.soundBufferOut = 0;
}

/*
=================
CG_RegisterCvars
=================
*/
void CG_RegisterCvars(void) {
    int i;
    cvarTable_t* cv;
    char var[MAX_TOKEN_CHARS];

    for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++) {
        // [QL] QL registers CVAR_CG_RANGE cvars through a 5-arg range-register syscall
        // that the ioquake3 engine does not provide; fall back to standard
        // registration (no cvarTable entry currently sets CVAR_CG_RANGE).
        trap_Cvar_Register(cv->vmCvar, cv->cvarName,
                           cv->defaultString, cv->cvarFlags & ~CVAR_CG_RANGE);
    }

    // [QL] ROM build-version cvar reported by cgamex86.dll build 1069.
    trap_Cvar_Register(NULL, "cg_version", "1069 win-x86 May 25 2016 15:18:10", CVAR_ROM);

    // [QL] clear any stale vote-active flag on (re)load.
    trap_Cvar_Set("ui_voteactive", "0");

    // see if we are also running the server on this machine
    trap_Cvar_VariableStringBuffer("sv_running", var, sizeof(var));
    cgs.localServer = atoi(var);

    (void)CG_ForceModelCvarsChanged();  // seed the force-model modcount cache

    trap_Cvar_Register(NULL, "model", DEFAULT_MODEL, CVAR_USERINFO | CVAR_ARCHIVE);
    trap_Cvar_Register(NULL, "headmodel", DEFAULT_MODEL, CVAR_USERINFO | CVAR_ARCHIVE);
}

/*
===================
CG_ForceModelCvarsChanged

[QL] Have any of the six model/skin-forcing cvars changed since last checked? Mirrors the six
modificationCount comparisons in binary CG_UpdateCvars (0x10020CA0): all six are compared, and if
any differ the whole cache is refreshed. QL has no single cg_forceModel; forcing is the granular set.
===================
*/
static qboolean CG_ForceModelCvarsChanged(void) {
    vmCvar_t* const cvars[6] = {
        &cg_forceEnemyModel, &cg_forceEnemySkin,
        &cg_forceTeamModel, &cg_forceTeamSkin,
        &cg_forceRedTeamModel, &cg_forceBlueTeamModel,
    };
    qboolean changed = qfalse;
    int i;

    for (i = 0; i < 6; i++) {
        if (cg_forceModelModCounts[i] != cvars[i]->modificationCount) {
            changed = qtrue;
        }
    }
    if (changed) {
        for (i = 0; i < 6; i++) {
            cg_forceModelModCounts[i] = cvars[i]->modificationCount;
        }
    }
    return changed;
}

/*
===================
CG_ForceModelChange
===================
*/
void CG_ForceModelChange(void) {
    int i;

    for (i = 0; i < cgs.maxclients; i++) {
        // never force the local client's own model (binary CG_ForceModelChange 0x1003e440 skips
        // cg.clientNum; this also keeps it safe to call from CG_NewClientInfo without recursing).
        if (i == cg.clientNum) {
            continue;
        }
        // CG_NewClientInfo handles an empty configstring itself (memset + return), so the binary
        // calls it unconditionally - no empty-skip here.
        CG_NewClientInfo(i);
    }

    // binary CG_ForceModelChange re-derives each re-registered client's model scale inline; that
    // is the same per-client step as CG_UpdateAllModelScales (0x1003e4b0), so run it once here.
    CG_UpdateAllModelScales();
}

/*
=================
CG_UpdateCvars
=================
*/
void CG_UpdateCvars(void) {
    int i;
    cvarTable_t* cv;

    for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++) {
        trap_Cvar_Update(cv->vmCvar);
    }

    // check for modications here

    // If team overlay is on, ask for updates from the server.  If it's off,
    // let the server know so we don't receive it
    if (drawTeamOverlayModificationCount != cg_drawTeamOverlay.modificationCount) {
        drawTeamOverlayModificationCount = cg_drawTeamOverlay.modificationCount;

        if (cg_drawTeamOverlay.integer > 0) {
            trap_Cvar_Set("teamoverlay", "1");
        } else {
            trap_Cvar_Set("teamoverlay", "0");
        }
    }

    // [QL] if any of the six model/skin-forcing cvars changed, re-register the LOCAL client.
    // Binary CG_UpdateCvars (0x10020CA0), when !cg.demoPlayback, calls CG_NewClientInfo(cg.clientNum);
    // its tail then cascades into CG_ForceModelChange + CG_LoadDeferredPlayers for every other
    // client (CG_NewClientInfo 0x1003e640). Not run during demo playback.
    if (!cg.demoPlayback && CG_ForceModelCvarsChanged()) {
        CG_NewClientInfo(cg.clientNum);
    }
}

int CG_CrosshairPlayer(void) {
    if (cg.time > (cg.crosshairClientTime + 1000)) {
        return -1;
    }
    return cg.crosshairClientNum;
}

int CG_LastAttacker(void) {
    if (!cg.attackerTime) {
        return -1;
    }
    return cg.snap->ps.persistant[PERS_ATTACKER];
}

// ============================================================================
// [QL] vmMain export slots 10-14 (cgamex86.dll table at 0x100769a8).
// ============================================================================

// Address: 0x10029ff0
// Re-adds the last outgoing chat line to the chat display and arms a 3s recall
// timer. Matches: ioquakelive (QL-only export).
void CG_LastChatCommand(void) {
    char msg[256];

    cg.lastChatTime = cg.time + 3000;
    trap_Cvar_VariableStringBuffer("cg_lastmsg", msg, sizeof(msg));
    CG_AddChat(msg, 0, 0);
}

// Address: 0x1002a060
// Same as CG_LastChatCommand but arms the second recall timer.
void CG_LastChatCommand2(void) {
    char msg[256];

    cg.lastChatTime2 = cg.time + 3000;
    trap_Cvar_VariableStringBuffer("cg_lastmsg", msg, sizeof(msg));
    CG_AddChat(msg, 0, 0);
}

// Address: 0x10007cd0
// Sets the cgame key-catcher flag (global 0x10a9c9b4) if not already set.
void CG_SetKeyCatcher(void) {
    if (cg.keyCatcherActive == 0) {
        cg.keyCatcherActive = 1;
    }
}

// Address: 0x10007cf0
// Clears the cgame key-catcher flag if set.
void CG_ClearKeyCatcher(void) {
    if (cg.keyCatcherActive != 0) {
        cg.keyCatcherActive = 0;
    }
}

// Address: 0x1004e4d0
// Returns the client frame counter (dword at 0x10a9c1f4, cg.clientFrame),
// bumped once per rendered scene in CG_DrawActiveFrame.
int CG_GetActiveFrame(void) {
    return cg.clientFrame;
}

// ============================================================================
// [QL] match / race / map-name helpers
// ============================================================================

// Address: 0x10001320
// Formats a millisecond race time as "[-]M:0S.mmm".
const char* CG_FormatRaceTime(int msec) {
    int absMsec = (msec < 0) ? -msec : msec;
    float seconds = (float)absMsec / 1000.0f;
    int minutes = (int)seconds / 60;
    float secs = seconds - (float)minutes * 60.0f;
    const char* pad = (secs < 10.0f) ? "0" : "";
    const char* sign = (msec < 0) ? "-" : "";

    return va("%s%d:%s%.03f", sign, minutes, pad, (double)secs);
}

// Address: 0x100253f0
// Reads ui/country.txt and pre-caches every country flag shader.
void CG_CacheCountryFlags(void) {
    fileHandle_t f;
    int len;
    char buf[4096];
    char code[32 + 1024];  // code[0..31] name + code[32..] path scratch
    char* p;
    int i;

    code[0] = '\0';

    trap_R_RegisterShaderNoMip("ui/assets/flags/none.tga");

    len = trap_FS_FOpenFile("ui/country.txt", &f, FS_READ);
    if (len <= 0) {
        Com_Printf("ERROR: CG_CacheCountryFlags: %s too small\n", "ui/country.txt");
        return;
    }
    if (len >= (int)sizeof(buf)) {
        Com_Printf("ERROR: CG_CacheCountryFlags: %s too large. Size is %d, limit is %d\n",
                   "ui/country.txt", len, (int)sizeof(buf));
        return;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = '\0';
    trap_FS_FCloseFile(f);

    p = code;
    for (i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '\r' || c == '\t' || c == ' ') {
            continue;
        }
        if (c == '\n') {
            *p = '\0';
            if (code[0] != '\0') {
                Com_sprintf(code + 32, 1024, "ui/assets/flags/%s.tga", code);
                trap_R_RegisterShaderNoMip(code + 32);
            }
            code[0] = '\0';
            p = code;
        } else {
            *p++ = c;
        }
    }
}

// Address: 0x1002a4a0
// Resets race-mode tracking state; on a full reset also clears the personal
// best. Only acts in GT_RACE.
// NOTE: the QL race globals (0x10abaab0..0x10abaad4) map onto ioquakelive's
// cg.race sub-struct + cg.raceActive; the exact field pairing is a Phase-3
// reconciliation item (see cg_local.h race notes).
void CG_RaceInit(int fullReset) {
    if (cgs.gametype != GT_RACE) {
        return;
    }

    cg.raceActive = 0;                    // 0x10abaab0
    cg.race.startTime = 0;                // 0x10abaab4
    cg.race.finishTime = 0;               // 0x10abaac0
    cg.race.bestSplit = -1;               // 0x10abaac4
    cg.race.checkpointDiff = -1;          // 0x10abaac8
    cg.race.hasDiff = qfalse;             // 0x10abaacc
    cg.race.checkpointCount = 0;          // 0x10abaad0
    cg.race.currentCheckpointEnt = 0;     // 0x10abaad4

    if (fullReset) {
        cg.race.bestTime = 0;             // 0x10abaab8
        cg.race.totalCheckpoints = 0;     // 0x10abaabc
    }

    trap_SendConsoleCommand("race_reset");
}

// Address: 0x1002a500
// Returns the local player's name (colour codes kept) for end-of-match display.
const char* CG_DidLocalPlayerWin(void) {
    static char name[40];
    int clientNum = cg.snap->ps.clientNum;

    Q_strncpyz(name, cgs.clientinfo[clientNum].name, sizeof(name));
    return name;
}

// Address: 0x1002a5f0
// Returns the number of overtime periods elapsed past the time limit.
int CG_GetOvertimeCount(void) {
    int timeUsed;
    int elapsed;
    int overtimeCount;
    int i;

    // [QL] 0x10a403e0: freeze/timeout end time, else current time.
    timeUsed = cgs.freezeEnd;
    if (timeUsed == 0) {
        timeUsed = cg.time;
    }

    elapsed = (cgs.timelimit * 60000 - timeUsed + cgs.levelStartTime) / 1000;

    if (elapsed <= 0) {
        return 0;
    }

    overtimeCount = 1;
    for (i = 1; i <= elapsed; i++) {
        // [QL] 0x10a3ffa8 = overtime period length (g_overtime == cgs.timelimit_overtime).
        if (cgs.timelimit_overtime != 0 && (i % cgs.timelimit_overtime) == 0) {
            overtimeCount++;
        }
    }
    return overtimeCount;
}

// Address: 0x10056f90
// Translates an internal map filename to its display name via a fixed table
// (49 entries, extracted from cgamex86.dll 0x10075df0). Falls back to the
// input name when no prefix match is found.
typedef struct {
    const char* filename;
    const char* displayName;
} mapTranslation_t;

static const mapTranslation_t mapTranslations[] = {
    { "qzca1", "asylum" },
    { "qzca2", "trinity" },
    { "qzca3", "quarantine" },
    { "qzctf1", "duelingkeeps" },
    { "qzctf2", "troubledwaters" },
    { "qzctf3", "stronghold" },
    { "qzctf4", "spacectf" },
    { "qzctf5", "falloutbunker" },
    { "qzctf6", "beyondreality" },
    { "qzctf7", "ironworks" },
    { "qzctf8", "siberia" },
    { "qzctf9", "bloodlust" },
    { "qzctf10", "courtyard" },
    { "qzdm1", "arenagate" },
    { "qzdm2", "spillway" },
    { "qzdm3", "hearth" },
    { "qzdm4", "eviscerated" },
    { "qzdm5", "forgotten" },
    { "qzdm6", "campgrounds" },
    { "qzdm7", "retribution" },
    { "qzdm8", "brimstoneabbey" },
    { "qzdm9", "heroskeep" },
    { "qzdm10", "namelessplace" },
    { "qzdm11", "chemicalreaction" },
    { "qzdm12", "dredwerkz" },
    { "qzdm13", "lostworld" },
    { "qzdm14", "grimdungeons" },
    { "qzdm15", "demonkeep" },
    { "qzdm16", "cobaltstation" },
    { "qzdm17", "longestyard" },
    { "qzdm18", "spacechamber" },
    { "qzdm19", "terminalheights" },
    { "qzdm20", "hiddenfortress" },
    { "qzteam1", "basesiege" },
    { "qzteam2", "falloutbunker" },
    { "qzteam3", "innersanctums" },
    { "qzteam4", "scornforge" },
    { "qzteam6", "vortexportal" },
    { "qzteam7", "rebound" },
    { "qztourney1", "powerstation" },
    { "qztourney2", "provinggrounds" },
    { "qztourney3", "hellsgate" },
    { "qztourney4", "verticalvengeance" },
    { "qztourney5", "hellsgateredux" },
    { "qztourney6", "almostlost" },
    { "qztourney7", "furiousheights" },
    { "qztourney8", "sacellum" },
    { "qztourney9", "houseofdecay" },
    { "ztntourney1", "bloodrun" },
};

const char* CG_TranslateMapName(const char* mapname) {
    int i;

    if (!mapname) {
        return NULL;
    }

    for (i = 0; i < (int)ARRAY_LEN(mapTranslations); i++) {
        if (mapTranslations[i].filename &&
            !Q_stricmpn(mapname, mapTranslations[i].filename, strlen(mapname))) {
            return mapTranslations[i].displayName;
        }
    }
    return mapname;
}

void QDECL CG_Printf(const char* msg, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, msg);
    Q_vsnprintf(text, sizeof(text), msg, argptr);
    va_end(argptr);

    trap_Print(text);
}

void QDECL CG_Error(const char* msg, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, msg);
    Q_vsnprintf(text, sizeof(text), msg, argptr);
    va_end(argptr);

    trap_Error(text);
}

void QDECL Com_Error(int level, const char* error, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, error);
    Q_vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

    trap_Error(text);
}

void QDECL Com_Printf(const char* msg, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, msg);
    Q_vsnprintf(text, sizeof(text), msg, argptr);
    va_end(argptr);

    trap_Print(text);
}

/*
================
CG_Argv
================
*/
const char* CG_Argv(int arg) {
    static char buffer[MAX_STRING_CHARS];

    trap_Argv(arg, buffer, sizeof(buffer));

    return buffer;
}

//========================================================================

/*
=================
CG_RegisterItemSounds

The server says this item is used on this level
=================
*/
static void CG_RegisterItemSounds(int itemNum) {
    gitem_t* item;
    char data[MAX_QPATH];
    char *s, *start;
    int len;

    item = &bg_itemlist[itemNum];

    if (item->pickup_sound) {
        trap_S_RegisterSound(item->pickup_sound, qfalse);
    }

    // parse the space separated precache string for other media
    s = item->sounds;
    if (!s || !s[0])
        return;

    while (*s) {
        start = s;
        while (*s && *s != ' ') {
            s++;
        }

        len = s - start;
        if (len >= MAX_QPATH || len < 5) {
            CG_Error("PrecacheItem: %s has bad precache string",
                     item->classname);
            return;
        }
        memcpy(data, start, len);
        data[len] = 0;
        if (*s) {
            s++;
        }

        if (!strcmp(data + len - 3, "wav")) {
            trap_S_RegisterSound(data, qfalse);
        }
    }
}

/*
=================
CG_RegisterSounds

called during a precache command
=================
*/
static void CG_RegisterSounds(void) {
    int i;
    char items[MAX_ITEMS + 1];
    char name[MAX_QPATH];
    const char* soundName;

    // [QL] vo sounds use announcer voice prefix (vo/vo_evil/vo_female)
    cgs.media.oneMinuteSound = trap_S_RegisterSound("sound/vo/1_minute.ogg", qtrue);
    cgs.media.fiveMinuteSound = trap_S_RegisterSound("sound/vo/5_minute.ogg", qtrue);
    cgs.media.suddenDeathSound = trap_S_RegisterSound("sound/vo/sudden_death.ogg", qtrue);
    cgs.media.oneFragSound = trap_S_RegisterSound("sound/vo/1_frag.ogg", qtrue);
    cgs.media.twoFragSound = trap_S_RegisterSound("sound/vo/2_frags.ogg", qtrue);
    cgs.media.threeFragSound = trap_S_RegisterSound("sound/vo/3_frags.ogg", qtrue);
    cgs.media.count3Sound = trap_S_RegisterSound("sound/vo/three.ogg", qtrue);
    cgs.media.count2Sound = trap_S_RegisterSound("sound/vo/two.ogg", qtrue);
    cgs.media.count1Sound = trap_S_RegisterSound("sound/vo/one.ogg", qtrue);
    cgs.media.countFightSound = trap_S_RegisterSound("sound/vo/fight.ogg", qtrue);
    cgs.media.countPrepareSound = trap_S_RegisterSound("sound/vo/prepare_to_fight.ogg", qtrue);
    cgs.media.countPrepareTeamSound = trap_S_RegisterSound("sound/vo/prepare_your_team.ogg", qtrue);

    if (cgs.gametype >= GT_TEAM) {
        cgs.media.captureAwardSound = trap_S_RegisterSound("sound/teamplay/flagcapture_yourteam", qtrue);
        cgs.media.redLeadsSound = trap_S_RegisterSound("sound/vo/red_leads", qtrue);
        cgs.media.blueLeadsSound = trap_S_RegisterSound("sound/vo/blue_leads", qtrue);
        cgs.media.teamsTiedSound = trap_S_RegisterSound("sound/vo/teams_tied", qtrue);
        cgs.media.hitTeamSound = trap_S_RegisterSound("sound/feedback/hit_teammate", qtrue);

        cgs.media.redScoredSound = trap_S_RegisterSound("sound/vo/red_scores", qtrue);
        cgs.media.blueScoredSound = trap_S_RegisterSound("sound/vo/blue_scores", qtrue);

        // [QL] match/round-win announcer VOs (GTS 14-18; binary CG_RegisterSounds
        // registers red_wins.ogg/blue_wins.ogg/red_wins_round.ogg/blue_wins_round.ogg/
        // round_draw.ogg). Queued via CG_QueueAnnouncement in the GTS event handler.
        cgs.media.redWinsSound = trap_S_RegisterSound("sound/vo/red_wins", qtrue);
        cgs.media.blueWinsSound = trap_S_RegisterSound("sound/vo/blue_wins", qtrue);
        cgs.media.redWinsRoundSound = trap_S_RegisterSound("sound/vo/red_wins_round", qtrue);
        cgs.media.blueWinsRoundSound = trap_S_RegisterSound("sound/vo/blue_wins_round", qtrue);
        cgs.media.roundDrawSound = trap_S_RegisterSound("sound/vo/round_draw", qtrue);

        cgs.media.captureYourTeamSound = trap_S_RegisterSound("sound/teamplay/flagcapture_yourteam", qtrue);
        cgs.media.captureOpponentSound = trap_S_RegisterSound("sound/teamplay/flagcapture_opponent", qtrue);

        cgs.media.returnYourTeamSound = trap_S_RegisterSound("sound/teamplay/flagreturn_yourteam", qtrue);
        cgs.media.returnOpponentSound = trap_S_RegisterSound("sound/teamplay/flagreturn_opponent", qtrue);

        cgs.media.takenYourTeamSound = trap_S_RegisterSound("sound/teamplay/flagtaken_yourteam", qtrue);
        cgs.media.takenOpponentSound = trap_S_RegisterSound("sound/teamplay/flagtaken_opponent", qtrue);

        if (cgs.gametype == GT_CTF) {
            cgs.media.redFlagReturnedSound = trap_S_RegisterSound("sound/vo/red_flag_returned", qtrue);
            cgs.media.blueFlagReturnedSound = trap_S_RegisterSound("sound/vo/blue_flag_returned", qtrue);
            cgs.media.enemyTookYourFlagSound = trap_S_RegisterSound("sound/vo/your_flag_taken", qtrue);
            cgs.media.yourTeamTookEnemyFlagSound = trap_S_RegisterSound("sound/vo/enemy_flag_taken", qtrue);
        }

        if (cgs.gametype == GT_1FCTF) {
            cgs.media.neutralFlagReturnedSound = trap_S_RegisterSound("sound/teamplay/flagreturn_opponent.ogg", qtrue);
            cgs.media.yourTeamTookTheFlagSound = trap_S_RegisterSound("sound/vo/your_team_has_flag", qtrue);
            cgs.media.enemyTookTheFlagSound = trap_S_RegisterSound("sound/vo/the_enemy_has_flag", qtrue);
        }

        if (cgs.gametype == GT_1FCTF || cgs.gametype == GT_CTF) {
            cgs.media.youHaveFlagSound = trap_S_RegisterSound("sound/vo/you_have_flag", qtrue);
            cgs.media.holyShitSound = trap_S_RegisterSound("sound/vo/holy_shit", qtrue);
        }

        if (cgs.gametype == GT_OBELISK) {
            cgs.media.yourBaseIsUnderAttackSound = trap_S_RegisterSound("sound/teamplay/voc_base_attack.ogg", qtrue);
        }
    }

    cgs.media.tracerSound = trap_S_RegisterSound("sound/weapons/machinegun/buletby1.ogg", qfalse);
    cgs.media.selectSound = trap_S_RegisterSound("sound/weapons/change.ogg", qfalse);
    cgs.media.wearOffSound = trap_S_RegisterSound("sound/items/wearoff.ogg", qfalse);
    cgs.media.useNothingSound = trap_S_RegisterSound("sound/items/use_nothing.ogg", qfalse);
    cgs.media.gibSound = trap_S_RegisterSound("dlc_gibs/gibsplt1.ogg", qfalse);
    cgs.media.gibBounce1Sound = trap_S_RegisterSound("dlc_gibs/gibimp1.ogg", qfalse);
    cgs.media.gibBounce2Sound = trap_S_RegisterSound("dlc_gibs/gibimp2.ogg", qfalse);
    cgs.media.gibBounce3Sound = trap_S_RegisterSound("dlc_gibs/gibimp3.ogg", qfalse);

    cgs.media.useInvulnerabilitySound = trap_S_RegisterSound("sound/items/invul_activate.ogg", qfalse);
    cgs.media.invulnerabilityImpactSound1 = trap_S_RegisterSound("sound/items/invul_impact_01.ogg", qfalse);
    cgs.media.invulnerabilityImpactSound2 = trap_S_RegisterSound("sound/items/invul_impact_02.ogg", qfalse);
    cgs.media.invulnerabilityImpactSound3 = trap_S_RegisterSound("sound/items/invul_impact_03.ogg", qfalse);
    cgs.media.invulnerabilityJuicedSound = trap_S_RegisterSound("dlc_gibs/invul_juiced.ogg", qfalse);
    cgs.media.obeliskHitSound1 = trap_S_RegisterSound("sound/items/obelisk_hit_01.ogg", qfalse);
    cgs.media.obeliskHitSound2 = trap_S_RegisterSound("sound/items/obelisk_hit_02.ogg", qfalse);
    cgs.media.obeliskHitSound3 = trap_S_RegisterSound("sound/items/obelisk_hit_03.ogg", qfalse);
    cgs.media.obeliskRespawnSound = trap_S_RegisterSound("sound/items/obelisk_respawn.ogg", qfalse);

    // [QL] renamed from cl_* prefix
    cgs.media.ammoregenSound = trap_S_RegisterSound("sound/items/armorregen.ogg", qfalse);
    cgs.media.doublerSound = trap_S_RegisterSound("sound/items/damage.ogg", qfalse);  // no dedicated doubler sound in QL
    cgs.media.guardSound = trap_S_RegisterSound("sound/items/guard.ogg", qfalse);
    cgs.media.scoutSound = trap_S_RegisterSound("sound/items/scout.ogg", qfalse);

    cgs.media.teleInSound = trap_S_RegisterSound("sound/world/telein.ogg", qfalse);
    cgs.media.teleOutSound = trap_S_RegisterSound("sound/world/teleout.ogg", qfalse);
    cgs.media.respawnSound = trap_S_RegisterSound("sound/items/respawn1.ogg", qfalse);

    cgs.media.noAmmoSound = trap_S_RegisterSound("sound/weapons/noammo.ogg", qfalse);

    cgs.media.talkSound = trap_S_RegisterSound("sound/player/talk.ogg", qfalse);
    cgs.media.landSound = trap_S_RegisterSound("sound/player/land1.ogg", qfalse);

    cgs.media.hitSound = trap_S_RegisterSound("sound/feedback/hit0.ogg", qfalse);

    // [QL] hit tier sounds use hit0-hit3 instead of hithi/hitlo
    cgs.media.hitSoundHighArmor = trap_S_RegisterSound("sound/feedback/hit0.ogg", qfalse);
    cgs.media.hitSoundLowArmor = trap_S_RegisterSound("sound/feedback/hit3.ogg", qfalse);

    // [QL] announcements moved from sound/feedback/ to sound/vo/
    cgs.media.impressiveSound = trap_S_RegisterSound("sound/vo/impressive1.ogg", qtrue);
    cgs.media.excellentSound = trap_S_RegisterSound("sound/vo/excellent1.ogg", qtrue);
    cgs.media.deniedSound = trap_S_RegisterSound("sound/vo/denied.ogg", qtrue);
    cgs.media.humiliationSound = trap_S_RegisterSound("sound/vo/humiliation1.ogg", qtrue);
    cgs.media.assistSound = trap_S_RegisterSound("sound/vo/assist.ogg", qtrue);
    cgs.media.defendSound = trap_S_RegisterSound("sound/vo/defense.ogg", qtrue);

    cgs.media.firstImpressiveSound = trap_S_RegisterSound("sound/vo/first_impressive.ogg", qtrue);
    cgs.media.firstExcellentSound = trap_S_RegisterSound("sound/vo/first_excellent.ogg", qtrue);
    cgs.media.firstHumiliationSound = trap_S_RegisterSound("sound/vo/first_gauntlet.ogg", qtrue);

    cgs.media.takenLeadSound = trap_S_RegisterSound("sound/vo/lead_taken.ogg", qtrue);
    cgs.media.tiedLeadSound = trap_S_RegisterSound("sound/vo/lead_tied.ogg", qtrue);
    cgs.media.lostLeadSound = trap_S_RegisterSound("sound/vo/lead_lost.ogg", qtrue);

    cgs.media.voteNow = trap_S_RegisterSound("sound/vo/vote_now.ogg", qtrue);
    cgs.media.votePassed = trap_S_RegisterSound("sound/vo/vote_passed.ogg", qtrue);
    cgs.media.voteFailed = trap_S_RegisterSound("sound/vo/vote_failed.ogg", qtrue);

    cgs.media.watrInSound = trap_S_RegisterSound("sound/player/watr_in.ogg", qfalse);
    cgs.media.watrOutSound = trap_S_RegisterSound("sound/player/watr_out.ogg", qfalse);
    cgs.media.watrUnSound = trap_S_RegisterSound("sound/player/watr_un.ogg", qfalse);

    cgs.media.jumpPadSound = trap_S_RegisterSound("sound/world/jumppad.ogg", qfalse);

    for (i = 0; i < 4; i++) {
        Com_sprintf(name, sizeof(name), "sound/player/footsteps/step%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_NORMAL][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/boot%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_BOOT][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/flesh%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_FLESH][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/mech%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_MECH][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/energy%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_ENERGY][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/splash%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_SPLASH][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/clank%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_METAL][i] = trap_S_RegisterSound(name, qfalse);

        // [QL] snow and wood surface footsteps
        Com_sprintf(name, sizeof(name), "sound/player/footsteps/snow%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_SNOW][i] = trap_S_RegisterSound(name, qfalse);

        Com_sprintf(name, sizeof(name), "sound/player/footsteps/wood%i.ogg", i + 1);
        cgs.media.footsteps[FOOTSTEP_WOOD][i] = trap_S_RegisterSound(name, qfalse);
    }

    // only register the items that the server says we need
    Q_strncpyz(items, CG_ConfigString(CS_ITEMS), sizeof(items));

    for (i = 1; i < bg_numItems; i++) {
        CG_RegisterItemSounds(i);
    }

    for (i = 1; i < MAX_SOUNDS; i++) {
        soundName = CG_ConfigString(CS_SOUNDS + i);
        if (!soundName[0]) {
            break;
        }
        if (soundName[0] == '*') {
            continue;  // custom sound
        }
        cgs.gameSounds[i] = trap_S_RegisterSound(soundName, qfalse);
    }

    // FIXME: only needed with item
    cgs.media.flightSound = trap_S_RegisterSound("sound/items/flight.ogg", qfalse);
    cgs.media.medkitSound = trap_S_RegisterSound("sound/items/use_medkit.ogg", qfalse);
    cgs.media.quadSound = trap_S_RegisterSound("sound/items/damage3.ogg", qfalse);
    cgs.media.sfx_ric1 = trap_S_RegisterSound("sound/weapons/machinegun/ric1.ogg", qfalse);
    cgs.media.sfx_ric2 = trap_S_RegisterSound("sound/weapons/machinegun/ric2.ogg", qfalse);
    cgs.media.sfx_ric3 = trap_S_RegisterSound("sound/weapons/machinegun/ric3.ogg", qfalse);
    cgs.media.sfx_rockexp = trap_S_RegisterSound("sound/weapons/rocket/rocklx1a.ogg", qfalse);
    cgs.media.sfx_plasmaexp = trap_S_RegisterSound("sound/weapons/plasma/plasmx1a.ogg", qfalse);

    cgs.media.sfx_proxexp = trap_S_RegisterSound("sound/weapons/proxmine/wstbexpl.ogg", qfalse);
    cgs.media.sfx_nghit = trap_S_RegisterSound("sound/weapons/nailgun/wnalimpd.ogg", qfalse);
    cgs.media.sfx_nghitflesh = trap_S_RegisterSound("sound/weapons/nailgun/wnalimpl.ogg", qfalse);
    cgs.media.sfx_nghitmetal = trap_S_RegisterSound("sound/weapons/nailgun/wnalimpm.ogg", qfalse);
    cgs.media.sfx_chghit = trap_S_RegisterSound("sound/weapons/vulcan/wvulimpd.ogg", qfalse);
    cgs.media.sfx_chghitflesh = trap_S_RegisterSound("sound/weapons/vulcan/wvulimpl.ogg", qfalse);
    cgs.media.sfx_chghitmetal = trap_S_RegisterSound("sound/weapons/vulcan/wvulimpm.ogg", qfalse);
    cgs.media.weaponHoverSound = trap_S_RegisterSound("sound/weapons/weapon_hover.ogg", qfalse);
    cgs.media.kamikazeExplodeSound = trap_S_RegisterSound("sound/items/kam_explode.ogg", qfalse);
    cgs.media.kamikazeImplodeSound = trap_S_RegisterSound("sound/items/kam_implode.ogg", qfalse);
    cgs.media.kamikazeFarSound = trap_S_RegisterSound("sound/items/kam_explode_far.ogg", qfalse);
    // [QL] moved from sound/feedback/voc_youwin to sound/vo/you_win
    cgs.media.winnerSound = trap_S_RegisterSound("sound/vo/you_win.ogg", qfalse);
    cgs.media.loserSound = trap_S_RegisterSound("sound/vo/you_lose.ogg", qfalse);
    cgs.media.newHighScoreSound = trap_S_RegisterSound("new_high_score.ogg", qfalse);  // [QL] binary path string 0x1006cfa4

    cgs.media.wstbimplSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbimpl.ogg", qfalse);
    cgs.media.wstbimpmSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbimpm.ogg", qfalse);
    cgs.media.wstbimpdSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbimpd.ogg", qfalse);
    cgs.media.wstbactvSound = trap_S_RegisterSound("sound/weapons/proxmine/wstbactv.ogg", qfalse);

    cgs.media.regenSound = trap_S_RegisterSound("sound/items/regen.ogg", qfalse);
    cgs.media.protectSound = trap_S_RegisterSound("sound/items/protect3.ogg", qfalse);
    cgs.media.n_healthSound = trap_S_RegisterSound("sound/items/n_health.ogg", qfalse);
    cgs.media.hgrenb1aSound = trap_S_RegisterSound("sound/weapons/grenade/hgrenb1a.ogg", qfalse);
    cgs.media.hgrenb2aSound = trap_S_RegisterSound("sound/weapons/grenade/hgrenb2a.ogg", qfalse);

    // [QL] additional sounds
    cgs.media.armorRegenSound = trap_S_RegisterSound("sound/misc/ar1_pkup.ogg", qfalse);
    cgs.media.overtimeSound = trap_S_RegisterSound("sound/world/klaxon2.ogg", qfalse);
    cgs.media.thawTickSound = trap_S_RegisterSound("sound/misc/tim_pump.ogg", qfalse);
    cgs.media.raceFinishSound = trap_S_RegisterSound("sound/world/klaxon1.ogg", qfalse);
    cgs.media.pauseSound = trap_S_RegisterSound("sound/world/klaxon1.ogg", qfalse);  // [QL] pause klaxon (DAT_10a5fcf4)
    cgs.media.infectedSound = trap_S_RegisterSound("sound/vo/infected.ogg", qfalse);

    // [QL] Race checkpoint assets (binary-verified from cgamex86.dll CG_RegisterGraphics)
    cgs.media.raceFlagB = trap_R_RegisterModel("models/flag3/b_flag3.md3");
    cgs.media.raceFlagF = trap_R_RegisterModel("models/flag3/f_flag3.md3");
    cgs.media.raceFlagG = trap_R_RegisterModel("models/flag3/g_flag3.md3");
    cgs.media.raceFlagD = trap_R_RegisterModel("models/flag3/d_flag3.md3");
    cgs.media.raceMarkerStart = trap_R_RegisterShader("gfx/2d/race/start");
    cgs.media.raceMarkerCheckpoint = trap_R_RegisterShader("gfx/2d/race/checkpoint");
    cgs.media.raceMarkerFinish = trap_R_RegisterShader("gfx/2d/race/finish");
    cgs.media.domPointModel = trap_R_RegisterModel("models/powerups/domination/dompoint.md3");
    cgs.media.domSkinRed = trap_R_RegisterSkin("models/powerups/domination/domred.skin");
    cgs.media.domSkinBlue = trap_R_RegisterSkin("models/powerups/domination/domblue.skin");
    cgs.media.domSkinNeutral = trap_R_RegisterSkin("models/powerups/domination/domntrl.skin");
}

//===================================================================================

/*
=================
CG_RegisterGraphics

This function may execute for a couple of minutes with a slow disk.
=================
*/
static void CG_RegisterGraphics(void) {
    int i;
    char items[MAX_ITEMS + 1];
    static char* sb_nums[11] = {
        "gfx/2d/numbers/zero_32b",
        "gfx/2d/numbers/one_32b",
        "gfx/2d/numbers/two_32b",
        "gfx/2d/numbers/three_32b",
        "gfx/2d/numbers/four_32b",
        "gfx/2d/numbers/five_32b",
        "gfx/2d/numbers/six_32b",
        "gfx/2d/numbers/seven_32b",
        "gfx/2d/numbers/eight_32b",
        "gfx/2d/numbers/nine_32b",
        "gfx/2d/numbers/minus_32b",
    };

    // clear any references to old media
    memset(&cg.refdef, 0, sizeof(cg.refdef));
    trap_R_ClearScene();

    CG_LoadingString(cgs.mapname);

    trap_R_LoadWorldMap(cgs.mapname);

    // precache status bar pics
    CG_LoadingString("game media");

    for (i = 0; i < 11; i++) {
        cgs.media.numberShaders[i] = trap_R_RegisterShader(sb_nums[i]);
    }

    cgs.media.botSkillShaders[0] = trap_R_RegisterShader("menu/art/skill1.png");
    cgs.media.botSkillShaders[1] = trap_R_RegisterShader("menu/art/skill2.png");
    cgs.media.botSkillShaders[2] = trap_R_RegisterShader("menu/art/skill3.png");
    cgs.media.botSkillShaders[3] = trap_R_RegisterShader("menu/art/skill4.png");
    cgs.media.botSkillShaders[4] = trap_R_RegisterShader("menu/art/skill5.png");

    cgs.media.viewBloodShader = trap_R_RegisterShader("viewBloodBlend");

    cgs.media.deferShader = trap_R_RegisterShaderNoMip("gfx/2d/defer.png");

    cgs.media.scoreboardName = trap_R_RegisterShaderNoMip("menu/tab/name.tga");
    cgs.media.scoreboardPing = trap_R_RegisterShaderNoMip("menu/tab/ping.tga");
    cgs.media.scoreboardScore = trap_R_RegisterShaderNoMip("menu/tab/score.tga");
    cgs.media.scoreboardTime = trap_R_RegisterShaderNoMip("menu/tab/time.tga");

    cgs.media.smokePuffShader = trap_R_RegisterShader("smokePuff");
    cgs.media.smokePuffRageProShader = trap_R_RegisterShader("smokePuffRagePro");
    cgs.media.shotgunSmokePuffShader = trap_R_RegisterShader("shotgunSmokePuff");

    cgs.media.nailPuffShader = trap_R_RegisterShader("nailtrail");
    cgs.media.blueProxMine = trap_R_RegisterModel("models/weaphits/proxmineb.md3");

    cgs.media.plasmaBallShader = trap_R_RegisterShader("sprites/plasma1");
    // [QL] blood spray shaders (translucent sprites with proper alpha blend)
    cgs.media.bloodSprayShaders[0] = trap_R_RegisterShader("bloodSpray1");
    cgs.media.bloodSprayShaders[1] = trap_R_RegisterShader("bloodSpray2");
    cgs.media.bloodSprayShaders[2] = trap_R_RegisterShader("bloodSpray3");
    cgs.media.bloodSprayShaders[3] = trap_R_RegisterShader("bloodSpray4");
    cgs.media.bloodTrailShader = cgs.media.bloodSprayShaders[0];
    cgs.media.lagometerShader = trap_R_RegisterShader("lagometer");
    cgs.media.connectionShader = trap_R_RegisterShader("disconnected");

    cgs.media.waterBubbleShader = trap_R_RegisterShader("waterBubble");

    cgs.media.tracerShader = trap_R_RegisterShader("gfx/misc/tracer");
    cgs.media.selectShader = trap_R_RegisterShader("gfx/2d/select");
    cgs.media.weaponBarHighlightShader = trap_R_RegisterShader("ui/assets/hud/weaplit2.tga");
    cgs.media.infiniteAmmoShader = trap_R_RegisterShader("icons/infinite.tga");

    for (i = 1; i < NUM_CROSSHAIRS; i++) {
        cgs.media.crosshairShader[i] = trap_R_RegisterShader(va("gfx/2d/crosshair%i", i));
    }

    cgs.media.backTileShader = trap_R_RegisterShader("gfx/2d/backtile");
    cgs.media.noammoShader = trap_R_RegisterShader("icons/noammo");
    cgs.media.fragIconShader = trap_R_RegisterShader("icons/icon_frag");

    // powerup shaders
    cgs.media.quadShader = trap_R_RegisterShader("powerups/quad");
    cgs.media.quadWeaponShader = trap_R_RegisterShader("powerups/quadWeapon");
    cgs.media.battleSuitShader = trap_R_RegisterShader("powerups/battleSuit");
    cgs.media.battleWeaponShader = trap_R_RegisterShader("powerups/battleWeapon");
    cgs.media.invisShader = trap_R_RegisterShader("powerups/invisibility");
    cgs.media.regenShader = trap_R_RegisterShader("powerups/regen");
    cgs.media.hastePuffShader = trap_R_RegisterShader("hasteSmokePuff");

    // [QL] gametype icons for scoreboard/HUD (from pak00: ui/assets/hud/*.png)
    cgs.media.gametypeIcon[GT_FFA]        = trap_R_RegisterShaderNoMip("ui/assets/hud/ffa");
    cgs.media.gametypeIcon[GT_DUEL]       = trap_R_RegisterShaderNoMip("ui/assets/hud/duel");
    cgs.media.gametypeIcon[GT_RACE]       = trap_R_RegisterShaderNoMip("ui/assets/hud/race");
    cgs.media.gametypeIcon[GT_TEAM]       = trap_R_RegisterShaderNoMip("ui/assets/hud/tdm");
    cgs.media.gametypeIcon[GT_CA]         = trap_R_RegisterShaderNoMip("ui/assets/hud/ca");
    cgs.media.gametypeIcon[GT_CTF]        = trap_R_RegisterShaderNoMip("ui/assets/hud/ctf");
    cgs.media.gametypeIcon[GT_1FCTF]      = trap_R_RegisterShaderNoMip("ui/assets/hud/1f");
    cgs.media.gametypeIcon[GT_OBELISK]    = trap_R_RegisterShaderNoMip("ui/assets/hud/ob");
    cgs.media.gametypeIcon[GT_HARVESTER]  = trap_R_RegisterShaderNoMip("ui/assets/hud/har");
    cgs.media.gametypeIcon[GT_FREEZE]     = trap_R_RegisterShaderNoMip("ui/assets/hud/ft");
    cgs.media.gametypeIcon[GT_DOMINATION] = trap_R_RegisterShaderNoMip("ui/assets/hud/dom");
    cgs.media.gametypeIcon[GT_AD]         = trap_R_RegisterShaderNoMip("ui/assets/hud/ad");
    cgs.media.gametypeIcon[GT_RR]         = trap_R_RegisterShaderNoMip("ui/assets/hud/rr");

    if (cgs.gametype == GT_HARVESTER) {
        cgs.media.redCubeModel = trap_R_RegisterModel("models/powerups/orb/r_orb.md3");
        cgs.media.blueCubeModel = trap_R_RegisterModel("models/powerups/orb/b_orb.md3");
        cgs.media.redCubeIcon = trap_R_RegisterShader("icons/skull_red");
        cgs.media.blueCubeIcon = trap_R_RegisterShader("icons/skull_blue");
    }

    if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF || cgs.gametype == GT_HARVESTER) {
        cgs.media.redFlagModel = trap_R_RegisterModel("models/flags/r_flag.md3");
        cgs.media.blueFlagModel = trap_R_RegisterModel("models/flags/b_flag.md3");
        cgs.media.redFlagShader[0] = trap_R_RegisterShaderNoMip("icons/iconf_red1");
        cgs.media.redFlagShader[1] = trap_R_RegisterShaderNoMip("icons/iconf_red2");
        cgs.media.redFlagShader[2] = trap_R_RegisterShaderNoMip("icons/iconf_red3");
        cgs.media.blueFlagShader[0] = trap_R_RegisterShaderNoMip("icons/iconf_blu1");
        cgs.media.blueFlagShader[1] = trap_R_RegisterShaderNoMip("icons/iconf_blu2");
        cgs.media.blueFlagShader[2] = trap_R_RegisterShaderNoMip("icons/iconf_blu3");

        cgs.media.flagPoleModel = trap_R_RegisterModel("models/flag2/flagpole.md3");
        cgs.media.flagFlapModel = trap_R_RegisterModel("models/flag2/flagflap3.md3");

        cgs.media.redFlagFlapSkin = trap_R_RegisterSkin("models/flag2/red.skin");
        cgs.media.blueFlagFlapSkin = trap_R_RegisterSkin("models/flag2/blue.skin");
        cgs.media.neutralFlagFlapSkin = trap_R_RegisterSkin("models/flag2/white.skin");

        cgs.media.redFlagBaseModel = trap_R_RegisterModel("models/mapobjects/flagbase/red_base.md3");
        cgs.media.blueFlagBaseModel = trap_R_RegisterModel("models/mapobjects/flagbase/blue_base.md3");
        cgs.media.neutralFlagBaseModel = trap_R_RegisterModel("models/mapobjects/flagbase/ntrl_base.md3");
    }

    if (cgs.gametype == GT_1FCTF) {
        cgs.media.neutralFlagModel = trap_R_RegisterModel("models/flags/n_flag.md3");
        cgs.media.flagShader[0] = trap_R_RegisterShaderNoMip("icons/iconf_neutral1");
        cgs.media.flagShader[1] = trap_R_RegisterShaderNoMip("icons/iconf_red2");
        cgs.media.flagShader[2] = trap_R_RegisterShaderNoMip("icons/iconf_blu2");
        cgs.media.flagShader[3] = trap_R_RegisterShaderNoMip("icons/iconf_neutral3");
    }

    if (cgs.gametype == GT_OBELISK) {
        cgs.media.rocketExplosionShader = trap_R_RegisterShader("rocketExplosion");
        cgs.media.overloadBaseModel = trap_R_RegisterModel("models/powerups/overload_base.md3");
        cgs.media.overloadTargetModel = trap_R_RegisterModel("models/powerups/overload_target.md3");
        cgs.media.overloadLightsModel = trap_R_RegisterModel("models/powerups/overload_lights.md3");
        cgs.media.overloadEnergyModel = trap_R_RegisterModel("models/powerups/overload_energy.md3");
    }

    if (cgs.gametype == GT_HARVESTER) {
        cgs.media.harvesterModel = trap_R_RegisterModel("models/powerups/harvester/harvester.md3");
        cgs.media.harvesterRedSkin = trap_R_RegisterSkin("models/powerups/harvester/red.skin");
        cgs.media.harvesterBlueSkin = trap_R_RegisterSkin("models/powerups/harvester/blue.skin");
        cgs.media.harvesterNeutralModel = trap_R_RegisterModel("models/powerups/obelisk/obelisk.md3");
    }

    cgs.media.redKamikazeShader = trap_R_RegisterShader("models/weaphits/kamikred");
    cgs.media.dustPuffShader = trap_R_RegisterShader("hasteSmokePuff");

    if (cgs.gametype >= GT_TEAM) {
        cgs.media.redQuadShader = trap_R_RegisterShader("powerups/blueflag");
        cgs.media.teamStatusBar = trap_R_RegisterShader("gfx/2d/colorbar.tga");
        cgs.media.blueKamikazeShader = trap_R_RegisterShader("models/weaphits/kamikblu");
        cgs.media.poiShader = trap_R_RegisterShader("sprites/neutralflagcarrier");
    }

    cgs.media.armorModel = trap_R_RegisterModel("models/powerups/armor/armor_yel.md3");
    cgs.media.armorIcon = trap_R_RegisterShaderNoMip("icons/iconr_yellow");

    cgs.media.machinegunBrassModel = trap_R_RegisterModel("models/weapons2/shells/m_shell.md3");
    cgs.media.shotgunBrassModel = trap_R_RegisterModel("models/weapons2/shells/s_shell.md3");

    cgs.media.gibAbdomen = trap_R_RegisterModel("dlc_gibs/abdomen.md3");
    cgs.media.gibArm = trap_R_RegisterModel("dlc_gibs/arm.md3");
    cgs.media.gibChest = trap_R_RegisterModel("dlc_gibs/chest.md3");
    cgs.media.gibFist = trap_R_RegisterModel("dlc_gibs/fist.md3");
    cgs.media.gibFoot = trap_R_RegisterModel("dlc_gibs/foot.md3");
    cgs.media.gibForearm = trap_R_RegisterModel("dlc_gibs/forearm.md3");
    cgs.media.gibIntestine = trap_R_RegisterModel("dlc_gibs/intestine.md3");
    cgs.media.gibLeg = trap_R_RegisterModel("dlc_gibs/leg.md3");
    cgs.media.gibSkull = trap_R_RegisterModel("dlc_gibs/skull.md3");
    cgs.media.gibBrain = trap_R_RegisterModel("dlc_gibs/brain.md3");

    cgs.media.smoke2 = trap_R_RegisterModel("models/weapons2/shells/s_shell.md3");

    cgs.media.balloonShader = trap_R_RegisterShader("sprites/balloon3");

    cgs.media.bloodExplosionShader = cgs.media.bloodSprayShaders[0];

    cgs.media.bulletFlashModel = trap_R_RegisterModel("models/weaphits/bullet.md3");
    cgs.media.ringFlashModel = trap_R_RegisterModel("models/weaphits/ring02.md3");
    cgs.media.dishFlashModel = trap_R_RegisterModel("models/weaphits/boom01.md3");
    cgs.media.teleportEffectModel = trap_R_RegisterModel("models/powerups/pop.md3");

    cgs.media.kamikazeEffectModel = trap_R_RegisterModel("models/weaphits/kamboom2.md3");
    cgs.media.kamikazeShockWave = trap_R_RegisterModel("models/weaphits/kamwave.md3");
    cgs.media.kamikazeHeadModel = trap_R_RegisterModel("models/powerups/kamikazi.md3");
    cgs.media.kamikazeHeadTrail = trap_R_RegisterModel("models/powerups/trailtest.md3");
    cgs.media.guardPowerupModel = trap_R_RegisterModel("models/powerups/guard_player.md3");
    cgs.media.scoutPowerupModel = trap_R_RegisterModel("models/powerups/scout_player.md3");
    cgs.media.doublerPowerupModel = trap_R_RegisterModel("models/powerups/doubler_player.md3");
    cgs.media.ammoRegenPowerupModel = trap_R_RegisterModel("models/powerups/ammo_player.md3");
    cgs.media.invulnerabilityImpactModel = trap_R_RegisterModel("models/powerups/shield/impact.md3");
    cgs.media.invulnerabilityJuicedModel = trap_R_RegisterModel("dlc_gibs/juicer.md3");  // [QL] moved from models/powerups/shield/
    cgs.media.medkitUsageModel = trap_R_RegisterModel("models/powerups/regen.md3");
    cgs.media.heartShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/selectedhealth.tga");
    cgs.media.invulnerabilityPowerupModel = trap_R_RegisterModel("models/powerups/shield/shield.md3");

    // [QL] 16 medal types (verified from binary CG_RegisterGraphics)
    cgs.media.medalAccuracy = trap_R_RegisterShaderNoMip("medal_accuracy");
    cgs.media.medalAssist = trap_R_RegisterShaderNoMip("medal_assist");
    cgs.media.medalCapture = trap_R_RegisterShaderNoMip("medal_capture");
    cgs.media.medalComboKill = trap_R_RegisterShaderNoMip("medal_combokill");
    cgs.media.medalDefend = trap_R_RegisterShaderNoMip("medal_defense");
    cgs.media.medalExcellent = trap_R_RegisterShaderNoMip("medal_excellent");
    cgs.media.medalFirstFrag = trap_R_RegisterShaderNoMip("medal_firstfrag");
    cgs.media.medalGauntlet = trap_R_RegisterShaderNoMip("medal_gauntlet");
    cgs.media.medalHeadshot = trap_R_RegisterShaderNoMip("medal_headshot");
    cgs.media.medalImpressive = trap_R_RegisterShaderNoMip("medal_impressive");
    cgs.media.medalMidair = trap_R_RegisterShaderNoMip("medal_midair");
    cgs.media.medalPerfect = trap_R_RegisterShaderNoMip("medal_perfect");
    cgs.media.medalPerforated = trap_R_RegisterShaderNoMip("medal_perforated");
    cgs.media.medalQuadGod = trap_R_RegisterShaderNoMip("medal_quadgod");
    cgs.media.medalRampage = trap_R_RegisterShaderNoMip("medal_rampage");
    cgs.media.medalRevenge = trap_R_RegisterShaderNoMip("medal_revenge");

    // [QL] freeze tag
    cgs.media.iceShardModel = trap_R_RegisterModel("models/gibs/sphere.md3");
    cgs.media.iceShardShader1 = trap_R_RegisterShader("powerups/ice1");
    cgs.media.iceShardShader2 = trap_R_RegisterShader("powerups/ice2");
    cgs.media.iceShardShader3 = trap_R_RegisterShader("powerups/ice3");
    cgs.media.frozenShader = trap_R_RegisterShader("sprites/frozen");
    cgs.media.iceMarkShader = trap_R_RegisterShader("iceMark");

    memset(cg_items, 0, sizeof(cg_items));
    memset(cg_weapons, 0, sizeof(cg_weapons));

    // only register the items that the server says we need
    Q_strncpyz(items, CG_ConfigString(CS_ITEMS), sizeof(items));

    for (i = 1; i < bg_numItems; i++) {
        if (items[i] == '1') {
            CG_LoadingItem(i);
            CG_RegisterItemVisuals(i);
        }
    }

    // wall marks
    cgs.media.bulletMarkShader = trap_R_RegisterShader("gfx/damage/bullet_mrk");
    cgs.media.burnMarkShader = trap_R_RegisterShader("gfx/damage/burn_med_mrk");
    cgs.media.holeMarkShader = trap_R_RegisterShader("gfx/damage/hole_lg_mrk");
    cgs.media.energyMarkShader = trap_R_RegisterShader("gfx/damage/plasma_mrk");
    cgs.media.shadowMarkShader = trap_R_RegisterShader("markShadow");
    cgs.media.wakeMarkShader = trap_R_RegisterShader("wake");
    cgs.media.bloodMarkShader = cgs.media.bloodSprayShaders[0];

    // register the inline models
    cgs.numInlineModels = trap_CM_NumInlineModels();
    for (i = 1; i < cgs.numInlineModels; i++) {
        char name[10];
        vec3_t mins, maxs;
        int j;

        Com_sprintf(name, sizeof(name), "*%i", i);
        cgs.inlineDrawModel[i] = trap_R_RegisterModel(name);
        trap_R_ModelBounds(cgs.inlineDrawModel[i], mins, maxs);
        for (j = 0; j < 3; j++) {
            cgs.inlineModelMidpoints[i][j] = mins[j] + 0.5 * (maxs[j] - mins[j]);
        }
    }

    // register all the server specified models
    for (i = 1; i < MAX_MODELS; i++) {
        const char* modelName;

        modelName = CG_ConfigString(CS_MODELS + i);
        if (!modelName[0]) {
            break;
        }
        cgs.gameModels[i] = trap_R_RegisterModel(modelName);
    }

    // new stuff
    cgs.media.patrolShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/patrol.png");
    cgs.media.assaultShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/assault.png");
    cgs.media.campShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/camp.png");
    cgs.media.followShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/follow.png");
    cgs.media.defendShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/defend.png");
    cgs.media.teamLeaderShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/team_leader.png");
    cgs.media.retrieveShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/retrieve.png");
    cgs.media.escortShader = trap_R_RegisterShaderNoMip("ui/assets/statusbar/escort.png");
    cgs.media.cursor = trap_R_RegisterShaderNoMip("menu/art/3_cursor2");
    cgs.media.sizeCursor = trap_R_RegisterShaderNoMip("ui/assets/sizecursor.png");
    cgs.media.selectCursor = trap_R_RegisterShaderNoMip("ui/assets/selectcursor.png");
    cgs.media.flagShaders[0] = trap_R_RegisterShaderNoMip("ui/assets/statusbar/flag_in_base.tga");
    cgs.media.flagShaders[1] = trap_R_RegisterShaderNoMip("ui/assets/statusbar/flag_capture.tga");
    cgs.media.flagShaders[2] = trap_R_RegisterShaderNoMip("ui/assets/statusbar/flag_missing.tga");

    // [QL] flag-status HUD icon set for CG_DrawFlagStatus / CG_DrawOneFlagStatus.
    // Indexed [icon + team*4]: team 0 neutral, 1 red, 2 blue; icon 0 at-base, 1 taken,
    // 2 dropped, 3 stolen. (binary CG_RegisterGraphics, store base DAT_10a5fc2c @0x10023a4f.)
    {
        static const char* flagStatusShaders[12] = {
            "gfx/2d/flag_status/flag_at_base.tga",
            "gfx/2d/flag_status/flag_taken.tga",
            "gfx/2d/flag_status/flag_dropped.tga",
            "gfx/2d/flag_status/flag_stolen.tga",
            "gfx/2d/flag_status/red_flag_at_base.tga",
            "gfx/2d/flag_status/red_flag_taken.tga",
            "gfx/2d/flag_status/red_flag_dropped.tga",
            "gfx/2d/flag_status/red_flag_stolen.tga",
            "gfx/2d/flag_status/blue_flag_at_base.tga",
            "gfx/2d/flag_status/blue_flag_taken.tga",
            "gfx/2d/flag_status/blue_flag_dropped.tga",
            "gfx/2d/flag_status/blue_flag_stolen.tga",
        };
        int fi;
        for (fi = 0; fi < 12; fi++) {
            cgs.media.flagStatusHandles[fi] = trap_R_RegisterShaderNoMip(flagStatusShaders[fi]);
        }
    }

    // [QL] head models are in the player directory, not a separate heads/ directory
    trap_R_RegisterModel("models/players/james/lower.md3");
    trap_R_RegisterModel("models/players/james/upper.md3");
    trap_R_RegisterModel("models/players/james/head.md3");

    trap_R_RegisterModel("models/players/janet/lower.md3");
    trap_R_RegisterModel("models/players/janet/upper.md3");
    trap_R_RegisterModel("models/players/janet/head.md3");

    CG_ClearParticles();
    /*
        for (i=1; i<MAX_PARTICLES_AREAS; i++)
        {
            {
                int rval;

                rval = CG_NewParticleArea ( CS_PARTICLES + i);
                if (!rval)
                    break;
            }
        }
    */
}

// [QL] sort collected spectators: queue members ahead of spec-only clients, then by
// queue position (binary comparator 0x10024f90)
static int QDECL CG_CompareSpectatorClients(const void* a, const void* b) {
    int ca = *(const int*)a;
    int cb = *(const int*)b;

    if (cgs.clientinfo[ca].specOnly != cgs.clientinfo[cb].specOnly) {
        return cgs.clientinfo[ca].specOnly - cgs.clientinfo[cb].specOnly;
    }
    return cgs.clientinfo[ca].queuePosition - cgs.clientinfo[cb].queuePosition;
}

/*
=======================
CG_BuildSpectatorString

[QL] Rebuilds cg.spectatorNames for the scoreboard pager (binary 0x10024ff0): one
cleaned name per entry, sorted, with the duel queue position "(n)" or the spec-only
"(s)" marker prefixed. Colour codes and control characters are stripped from the
displayed name. Rebuilt on +scores, client registration and player configstring
changes.
=======================
*/
void CG_BuildSpectatorString(void) {
    int list[MAX_CLIENTS];
    char cleanName[MAX_QPATH];
    const char* prefix;
    const char* in;
    char* out;
    int count, queued, clientNum;
    int i;

    count = 0;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_SPECTATOR) {
            list[count++] = i;
        }
    }
    qsort(list, count, sizeof(list[0]), CG_CompareSpectatorClients);

    memset(cg.spectatorNames, 0, sizeof(cg.spectatorNames));

    queued = 0;
    for (i = 0; i < count; i++) {
        clientNum = list[i];
        if (!cgs.clientinfo[clientNum].infoValid) {
            continue;
        }

        if (cgs.clientinfo[clientNum].team == TEAM_SPECTATOR) {
            if (cgs.gametype != GT_DUEL) {
                prefix = "";
                goto append;
            }
        } else if (cgs.gametype != GT_DUEL || !cgs.clientinfo[clientNum].queuePosition ||
                   cgs.clientinfo[clientNum].team != TEAM_FREE) {
            continue;
        }

        if (cgs.clientinfo[clientNum].specOnly) {
            prefix = "^7(^5s^7)";
        } else if (!cgs.clientinfo[clientNum].queuePosition) {
            prefix = "";
        } else {
            int pos = cgs.clientinfo[clientNum].queuePosition;
            // renumber gaps in the server-side queue so the display stays sequential
            if (queued != pos - 1) {
                pos = queued + 1;
            }
            prefix = va("(%d)", pos);
            queued++;
        }

append:
        // strip colour codes and control characters from the display name
        // (into a copy; the binary cleans clientinfo in place)
        in = cgs.clientinfo[clientNum].name;
        out = cleanName;
        while (*in && out < cleanName + sizeof(cleanName) - 1) {
            if (in[0] == Q_COLOR_ESCAPE && in[1] >= '0' && in[1] <= '7') {
                in += 2;
                continue;
            }
            if (*in >= 0x20) {
                *out++ = *in;
            }
            in++;
        }
        *out = '\0';

        Q_strncpyz(cg.spectatorNames[i], va("%s%s", prefix, cleanName),
                   sizeof(cg.spectatorNames[i]));
    }

    if (count != cg.spectatorCount) {
        cg.spectatorCount = count;
        if (count < cg.spectatorPageOffset) {
            cg.spectatorPageOffset = 0;
        }
    }
}

/*
===================
CG_RegisterClients
===================
*/
static void CG_RegisterClients(void) {
    int i;

    CG_LoadingClient(cg.clientNum);
    CG_NewClientInfo(cg.clientNum);

    for (i = 0; i < MAX_CLIENTS; i++) {
        const char* clientInfo;

        if (cg.clientNum == i) {
            continue;
        }

        clientInfo = CG_ConfigString(CS_PLAYERS + i);
        if (!clientInfo[0]) {
            continue;
        }
        CG_LoadingClient(i);
        CG_NewClientInfo(i);
    }
    CG_BuildSpectatorString();
}

//===========================================================================

/*
=================
CG_ConfigString
=================
*/
const char* CG_ConfigString(int index) {
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        CG_Error("CG_ConfigString: bad index: %i", index);
    }
    return cgs.gameState.stringData + cgs.gameState.stringOffsets[index];
}

//==================================================================

/*
======================
CG_StartMusic

======================
*/
void CG_StartMusic(void) {
    char* s;
    char parm1[MAX_QPATH], parm2[MAX_QPATH];

    // start the background music
    s = (char*)CG_ConfigString(CS_MUSIC);
    Q_strncpyz(parm1, COM_Parse(&s), sizeof(parm1));
    Q_strncpyz(parm2, COM_Parse(&s), sizeof(parm2));

    trap_S_StartBackgroundTrack(parm1, parm2);
}

char* CG_GetMenuBuffer(const char* filename) {
    int len;
    fileHandle_t f;
    static char buf[MAX_MENUFILE];

    len = trap_FS_FOpenFile(filename, &f, FS_READ);
    if (!f) {
        trap_Print(va(S_COLOR_RED "menu file not found: %s, using default\n", filename));
        return NULL;
    }
    if (len >= MAX_MENUFILE) {
        trap_Print(va(S_COLOR_RED "menu file too large: %s is %i, max allowed is %i\n", filename, len, MAX_MENUFILE));
        trap_FS_FCloseFile(f);
        return NULL;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = 0;
    trap_FS_FCloseFile(f);

    return buf;
}

//
// ==============================
// new hud stuff ( mission pack )
// ==============================
//
qboolean CG_Asset_Parse(int handle) {
    pc_token_t token;
    const char* tempStr;

    if (!trap_PC_ReadToken(handle, &token))
        return qfalse;
    if (Q_stricmp(token.string, "{") != 0) {
        return qfalse;
    }

    while (1) {
        if (!trap_PC_ReadToken(handle, &token))
            return qfalse;

        if (Q_stricmp(token.string, "}") == 0) {
            return qtrue;
        }

        // font - parse but skip registerFont if already registered by CG_Init
        if (Q_stricmp(token.string, "font") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            if (!cgDC.Assets.fontRegistered) {
                cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.textFont);
            }
            continue;
        }

        // smallFont
        if (Q_stricmp(token.string, "smallFont") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            if (!cgDC.Assets.fontRegistered) {
                cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.smallFont);
            }
            continue;
        }

        // bigFont
        if (Q_stricmp(token.string, "bigfont") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            if (!cgDC.Assets.fontRegistered) {
                cgDC.registerFont(tempStr, pointSize, &cgDC.Assets.bigFont);
            }
            continue;
        }

        // gradientbar
        if (Q_stricmp(token.string, "gradientbar") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            cgDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(tempStr);
            continue;
        }

        // enterMenuSound
        if (Q_stricmp(token.string, "menuEnterSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            cgDC.Assets.menuEnterSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        // exitMenuSound
        if (Q_stricmp(token.string, "menuExitSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            cgDC.Assets.menuExitSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        // itemFocusSound
        if (Q_stricmp(token.string, "itemFocusSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            cgDC.Assets.itemFocusSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        // menuBuzzSound
        if (Q_stricmp(token.string, "menuBuzzSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            cgDC.Assets.menuBuzzSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        if (Q_stricmp(token.string, "cursor") == 0) {
            if (!PC_String_Parse(handle, &cgDC.Assets.cursorStr)) {
                return qfalse;
            }
            cgDC.Assets.cursor = trap_R_RegisterShaderNoMip(cgDC.Assets.cursorStr);
            continue;
        }

        if (Q_stricmp(token.string, "fadeClamp") == 0) {
            if (!PC_Float_Parse(handle, &cgDC.Assets.fadeClamp)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "fadeCycle") == 0) {
            if (!PC_Int_Parse(handle, &cgDC.Assets.fadeCycle)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "fadeAmount") == 0) {
            if (!PC_Float_Parse(handle, &cgDC.Assets.fadeAmount)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowX") == 0) {
            if (!PC_Float_Parse(handle, &cgDC.Assets.shadowX)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowY") == 0) {
            if (!PC_Float_Parse(handle, &cgDC.Assets.shadowY)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowColor") == 0) {
            if (!PC_Color_Parse(handle, &cgDC.Assets.shadowColor)) {
                return qfalse;
            }
            cgDC.Assets.shadowFadeClamp = cgDC.Assets.shadowColor[3];
            continue;
        }
    }
    return qfalse;
}

void CG_ParseMenu(const char* menuFile) {
    pc_token_t token;
    int handle;

    handle = trap_PC_LoadSource(menuFile);
    if (!handle)
        handle = trap_PC_LoadSource("ui/testhud.menu");
    if (!handle)
        return;

    while (1) {
        if (!trap_PC_ReadToken(handle, &token)) {
            break;
        }

        // if ( Q_stricmp( token, "{" ) ) {
        //	Com_Printf( "Missing { in menu file\n" );
        //	break;
        // }

        // if ( menuCount == MAX_MENUS ) {
        //	Com_Printf( "Too many menus!\n" );
        //	break;
        // }

        if (token.string[0] == '}') {
            break;
        }

        if (Q_stricmp(token.string, "assetGlobalDef") == 0) {
            if (CG_Asset_Parse(handle)) {
                continue;
            } else {
                break;
            }
        }

        if (Q_stricmp(token.string, "menudef") == 0) {
            // start a new menu
            Menu_New(handle);
        }
    }
    trap_PC_FreeSource(handle);
}

qboolean CG_Load_Menu(char** p) {
    char* token;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        CG_ParseMenu(token);
    }
    return qfalse;
}

void CG_LoadMenus(const char* menuFile) {
    char* token;
    char* p;
    int len, start;
    fileHandle_t f;
    static char buf[MAX_MENUDEFFILE];

    start = trap_Milliseconds();

    len = trap_FS_FOpenFile(menuFile, &f, FS_READ);
    if (!f) {
        Com_Printf(S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile);
        len = trap_FS_FOpenFile("ui/hud.txt", &f, FS_READ);
        if (!f) {
            CG_Error(S_COLOR_RED "default menu file not found: ui/hud.txt, unable to continue!");
        }
    }

    if (len >= MAX_MENUDEFFILE) {
        trap_FS_FCloseFile(f);
        CG_Error(S_COLOR_RED "menu file too large: %s is %i, max allowed is %i", menuFile, len, MAX_MENUDEFFILE);
        return;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = 0;
    trap_FS_FCloseFile(f);

    COM_Compress(buf);

    Menu_Reset();

    p = buf;

    while (1) {
        token = COM_ParseExt(&p, qtrue);
        if (!token[0]) {
            break;
        }

        // if ( Q_stricmp( token, "{" ) ) {
        //	Com_Printf( "Missing { in menu file\n" );
        //	break;
        // }

        // if ( menuCount == MAX_MENUS ) {
        //	Com_Printf( "Too many menus!\n" );
        //	break;
        // }

        if (Q_stricmp(token, "}") == 0) {
            break;
        }

        if (Q_stricmp(token, "loadmenu") == 0) {
            if (CG_Load_Menu(&p)) {
                continue;
            } else {
                break;
            }
        }
    }

    Com_Printf("UI menu load time = %d milliseconds\n", trap_Milliseconds() - start);
}

static qboolean CG_OwnerDrawHandleKey(int ownerDraw, int flags, float* special, int key) {
    return qfalse;
}

static int CG_FeederCount(float feederID) {
    int i, count;
    count = 0;
    if (feederID == FEEDER_REDTEAM_LIST) {
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team == TEAM_RED) {
                count++;
            }
        }
    } else if (feederID == FEEDER_BLUETEAM_LIST) {
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team == TEAM_BLUE) {
                count++;
            }
        }
    } else if (feederID == FEEDER_SCOREBOARD) {
        // binary 0x10025e70 counts only feeders 5/6/11/17/18; FEEDER_ENDSCOREBOARD
        // is dead in 1069 (no menu uses it) and yields 0 like everything else
        return cg.numScores;
    } else if (feederID == FEEDER_REDTEAM_STATS) {
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team == TEAM_RED) count++;
        }
    } else if (feederID == FEEDER_BLUETEAM_STATS) {
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team == TEAM_BLUE) count++;
        }
    }
    return count;
}

/*
=================
CG_SetScoreSelection

[QL] binary 0x10025f60. Spectators deselect everything; players select their own
score row. Team gametypes push the GLOBAL selected index through the primary team
list's cursor and the feeder selection callback (which converts it to a team row),
then deselect the other team's list.
=================
*/
void CG_SetScoreSelection(void* p) {
    menuDef_t* menu = (menuDef_t*)p;
    playerState_t* ps = &cg.snap->ps;
    int i;

    if (ps->persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        cg.selectedScore = -1;
    } else {
        for (i = 0; i < cg.numScores; i++) {
            if (ps->clientNum == cg.scores[i].client) {
                cg.selectedScore = i;
            }
        }
    }

    if (menu == NULL) {
        // just interested in setting the selected score
        return;
    }

    if (cgs.gametype >= GT_TEAM) {
        int primary = FEEDER_REDTEAM_LIST;
        int secondary = FEEDER_BLUETEAM_LIST;
        itemDef_t* item = NULL;

        if (cg.selectedScore >= 0 && cg.scores[cg.selectedScore].team == TEAM_BLUE) {
            primary = FEEDER_BLUETEAM_LIST;
            secondary = FEEDER_REDTEAM_LIST;
        }

        for (i = 0; i < menu->itemCount; i++) {
            if (menu->items[i]->special == primary) {
                item = menu->items[i];
                break;
            }
        }
        if (item) {
            if (cg.selectedScore == 0) {
                listBoxDef_t* listPtr = (listBoxDef_t*)item->typeData;
                if (listPtr) {
                    listPtr->startPos = 0;
                    listPtr->cursorPos = 0;
                }
            }
            item->cursorPos = cg.selectedScore;
            cgDC.feederSelection(item->special, item->cursorPos);
        }
        Menu_SetFeederSelection(menu, secondary, -1, NULL);
    } else {
        Menu_SetFeederSelection(menu, FEEDER_SCOREBOARD, cg.selectedScore, NULL);
    }
}

// FIXME: might need to cache this info
static clientInfo_t* CG_InfoFromScoreIndex(int index, int team, int* scoreIndex) {
    int i, count;
    if (cgs.gametype >= GT_TEAM) {
        count = 0;
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team == team) {
                if (count == index) {
                    *scoreIndex = i;
                    return &cgs.clientinfo[cg.scores[i].client];
                }
                count++;
            }
        }
    }
    *scoreIndex = index;
    return &cgs.clientinfo[cg.scores[index].client];
}

// [QL] Format damage with "k" suffix for scoreboard display (binary-verified)
static const char* CG_FormatDamage(int dmg) {
    if (dmg >= 10000) return va("%.0fk", (float)dmg / 1000.0f);
    if (dmg >= 1000) return va("%.1fk", (float)dmg / 1000.0f);
    return va("%d", dmg);
}

// [QL] country flag shader for a GeoIP code; falls back to the blank flag
// (shaders are pre-cached by CG_CacheCountryFlags)
static qhandle_t CG_CountryFlagShader(const char* code) {
    qhandle_t h = 0;

    if (code && code[0]) {
        h = trap_R_RegisterShaderNoMip(va("ui/assets/flags/%s.tga", code));
    }
    if (!h) {
        h = trap_R_RegisterShaderNoMip("ui/assets/flags/none.tga");
    }
    return h;
}

// [QL] the team lists and stats pages shorten long names to the column width,
// measured at the binary's fixed 0.16 scale, and mark the cut with "..."
static const char* CG_TruncateListName(const char* name, int maxWidth, int shrinkWidth) {
    static char buf[64];
    int len;

    if (CG_DrawTextWidth(name, 0.16f, 0, 0) <= maxWidth) {
        return name;
    }
    Q_strncpyz(buf, name, sizeof(buf));
    len = (int)strlen(buf);
    while (len > 0 && CG_DrawTextWidth(buf, 0.16f, 0, 0) >= shrinkWidth) {
        buf[--len] = '\0';
    }
    Q_strcat(buf, sizeof(buf), "...");
    return buf;
}

// [QL] per-gametype name column widths on the team lists (binary feeders each
// carry their own pair)
static const char* CG_ListNameForGametype(const char* name) {
    switch (cgs.gametype) {
        case GT_TEAM:
        case GT_FREEZE:
            return CG_TruncateListName(name, 100, 92);
        case GT_CA:
            return CG_TruncateListName(name, 69, 62);
        case GT_CTF:
        case GT_1FCTF:
        case GT_HARVESTER:
        case GT_DOMINATION:
        case GT_AD:
            return CG_TruncateListName(name, 73, 66);
    }
    return name;
}

// [QL] icon/status columns 0-5 shared by the FFA and team list feeders (each binary
// feeder inlines this block). Returns NULL when the column is gametype-specific.
static const char* CG_FeederColumnsCommon(score_t* sp, clientInfo_t* info, int column,
                                          qhandle_t* handle, qboolean teamFeeder) {
    int pw;
    gitem_t* item;

    switch (column) {
        case 0:
            // the binary prefers the Steam profile image (cg_drawProfileImages)
            // when one is cached; without Steam avatars the model icon is the
            // permanent fallback
            *handle = info->modelIcon;
            return "";
        case 1:
            // voice state column: muted / speaking icons; no voice backend feeds it
            return "";
        case 2:
            if (info->handicap < 100 && !info->botSkill) {
                return va("%d", info->handicap);
            }
            return " ";
        case 3:
            if (cg.warmup || cg.snap->ps.pm_type == PM_INTERMISSION) {
                *handle = (cg.snap->ps.stats[STAT_CLIENTS_READY] & (1 << sp->client))
                    ? trap_R_RegisterShaderNoMip("ui/assets/score/arrowg.tga")
                    : trap_R_RegisterShaderNoMip("ui/assets/score/arrowr.tga");
                return "";
            }
            // first held powerup, skipping flight and the freeze marker
            for (pw = 1; pw < PW_NUM_POWERUPS; pw++) {
                if (pw == PW_FLIGHT || pw == PW_FREEZE) {
                    continue;
                }
                if (cg_entities[sp->client].currentState.powerups & (1 << pw)) {
                    item = BG_FindItemForPowerup(pw);
                    if (item) {
                        *handle = trap_R_RegisterShader(item->icon);
                    }
                    return "";
                }
            }
            if (teamFeeder && BG_IsRoundBasedGameType(cgs.gametype) && sp->alive) {
                *handle = (info->team == TEAM_RED)
                    ? trap_R_RegisterShaderNoMip("ui/assets/score/ca_arrow_red.tga")
                    : trap_R_RegisterShaderNoMip("ui/assets/score/ca_arrow_blue.tga");
            }
            return "";
        case 4:
            *handle = CG_CountryFlagShader(info->country);
            return "";
        case 5:
            if (teamFeeder) {
                return CG_ListNameForGametype(info->name);
            }
            return info->name;
    }
    return NULL;
}

// [QL] FFA/Duel/RR scoreboard columns 6-13 (14 total, binary 0x10026160)
static const char* CG_FeederColumnsFfa(score_t* sp, clientInfo_t* info, int column, qhandle_t* handle) {
    switch (column) {
        case 6:
            if (cgs.gametype == GT_DUEL) {
                return va("%d/%d", info->wins, info->losses);
            }
            return " ";
        case 7:
            if (sp->score == -9999 || sp->score == -999) return "-";
            return va("%d", sp->score);
        case 8:
            return va("%d/%d", sp->frags, sp->deaths);
        case 9:
            return CG_FormatDamage(sp->damageDone);
        case 10:
            if (sp->damageDone > 0 && sp->bestWeapon > 0 && sp->bestWeapon < WP_NUM_WEAPONS)
                *handle = cg_weapons[sp->bestWeapon].weaponIcon;
            return "";
        case 11:
            if (sp->damageDone > 0 && sp->bestWeapon > 0)
                return va("%d%%", sp->bestWeaponAccuracy);
            return "";
        case 12:
            return va("%4d", sp->time);
        case 13:
            return va("%4d", sp->ping);
    }
    return "";
}

// [QL] TDM columns 6-9 (10 total), FT columns 6-9 (10 total) - binary FUN_10027e30
static const char* CG_FeederColumnsTdm(score_t* sp, clientInfo_t* info, int column, qhandle_t* handle) {
    switch (column) {
        case 6:
            if (sp->score == -9999 || sp->score == -999) return "";
            return va("%d", sp->score);
        case 7:
            if (cgs.gametype == GT_FREEZE)
                return va("%d/%d", sp->frags, sp->deaths);
            // TDM net includes the team-kill adjustment (binary 0x10027e30)
            return va("%d", sp->frags + sp->tkd - sp->deaths - sp->tks);
        case 8:
            if (cgs.gametype == GT_FREEZE)
                return va("%d", sp->thaws);
            // instagib shows overall accuracy instead of damage
            if (cgs.customSettings & 0x20000)
                return va("%d%%", sp->accuracy);
            return CG_FormatDamage(sp->damageDone);
        case 9:
            return va("%d", sp->ping);
    }
    return "";
}

// [QL] CA columns 6-11 (12 total) - binary FUN_10027240
static const char* CG_FeederColumnsCa(score_t* sp, clientInfo_t* info, int column, qhandle_t* handle) {
    switch (column) {
        case 6:
            if (sp->score == -9999 || sp->score == -999) return "";
            return va("%d", sp->score);
        case 7:
            return va("%d/%d", sp->frags, sp->deaths);
        case 8:
            return CG_FormatDamage(sp->damageDone);
        case 9:
            if (sp->damageDone > 0 && sp->bestWeapon > 0 && sp->bestWeapon < WP_NUM_WEAPONS)
                *handle = cg_weapons[sp->bestWeapon].weaponIcon;
            return "";
        case 10:
            if (sp->damageDone > 0 && sp->bestWeapon > 0)
                return va("%d%%", sp->bestWeaponAccuracy);
            return "";
        case 11:
            return va("%d", sp->ping);
    }
    return "";
}

// [QL] CTF/1FCTF/HAR/DOM/AD columns 6-11 (12 total) - binary FUN_100268b0
static const char* CG_FeederColumnsCtf(score_t* sp, clientInfo_t* info, int column, qhandle_t* handle) {
    switch (column) {
        case 6:
            if (sp->score == -9999 || sp->score == -999) return "";
            return va("%d", sp->score);
        case 7:
            return va("%d/%d", sp->frags, sp->deaths);
        case 8:
            return va("%d", sp->captures);
        case 9:
            return va("%d", sp->assistCount);
        case 10:
            return va("%d", sp->defendCount);
        case 11:
            return va("%d", sp->ping);
    }
    return "";
}

// [QL] Race scoreboard columns 0-6 (7 total, different layout) - binary FUN_10027c40
static const char* CG_FeederItemTextRace(clientInfo_t* info, score_t* sp, int column, qhandle_t* handle) {
    switch (column) {
        case 0:
            *handle = info->modelIcon;
            return "";
        case 1:
            return "";
        case 2:
            if (cg.warmup || cg.snap->ps.pm_type == PM_INTERMISSION) {
                *handle = (cg.snap->ps.stats[STAT_CLIENTS_READY] & (1 << sp->client))
                    ? trap_R_RegisterShaderNoMip("ui/assets/score/arrowg.tga")
                    : trap_R_RegisterShaderNoMip("ui/assets/score/arrowr.tga");
            }
            return " ";
        case 3:
            return info->name;
        case 4:
            if (sp->score > 0 && sp->score != 0x7FFFFFFF)
                return va("%d:%02d.%03d", sp->score / 60000, (sp->score / 1000) % 60, sp->score % 1000);
            return "-";
        case 5:
            return va("%4d", sp->time);
        case 6:
            return va("%4d", sp->ping);
    }
    return "";
}

// [QL] fallback team feeder for gametypes without a dedicated layout (GT 7 and RR
// team lists; 9 columns) - binary 0x100265a0
static const char* CG_FeederColumnsDefault(score_t* sp, clientInfo_t* info, int column, qhandle_t* handle) {
    switch (column) {
        case 0:
            *handle = info->modelIcon;
            return "";
        case 1:
            if (info->handicap < 100 && !info->botSkill) {
                return va("%d", info->handicap);
            }
            return " ";
        case 2:
            if (cg.warmup || cg.snap->ps.pm_type == PM_INTERMISSION) {
                *handle = (cg.snap->ps.stats[STAT_CLIENTS_READY] & (1 << sp->client))
                    ? trap_R_RegisterShaderNoMip("ui/assets/score/arrowg.tga")
                    : trap_R_RegisterShaderNoMip("ui/assets/score/arrowr.tga");
            } else if (BG_IsRoundBasedGameType(cgs.gametype) && sp->alive) {
                *handle = (info->team == TEAM_RED)
                    ? trap_R_RegisterShaderNoMip("ui/assets/score/ca_arrow_red.tga")
                    : trap_R_RegisterShaderNoMip("ui/assets/score/ca_arrow_blue.tga");
            }
            return "";
        case 3:
            return info->name;
        case 4:
            if (sp->score == -9999 || sp->score == -999) return "-";
            return va("%d", sp->score);
        case 5:
            return va("%d", sp->frags);
        case 6:
            return va("%d", sp->deaths);
        case 7:
            return va("%d", sp->ping);
    }
    return "";
}

// [QL] stats score column: yellow number with a dash for the not-playing sentinels
static const char* CG_StatsScoreString(int score) {
    if (score == -9999 || score == -999) {
        return "^3-";
    }
    return va("^3%d", score);
}

// [QL] weapon column on the CA stats page: kills in yellow, accuracy in white
static const char* CG_StatsWeaponString(score_t* sp, int weapon) {
    return va("^3%d ^7%d%%", sp->weaponKills[weapon], sp->weaponAccuracy[weapon]);
}

// [QL] end-scoreboard stats pages. The binary has one function per gametype group:
// 0x10028300 (TDM/FT, 18 columns), 0x10027720 (CA, 17), 0x10026cf0 (CTF group, 19).
// Base fields come from the scores_* verbs, the extended fields from the
// tdmstats/castats/ctfstats verbs, all in the same cg.scores rows. Deaths are
// displayed net of team-kill deaths and suicides.
static const char* CG_FeederItemTextStats(float feederID, int index, int column, qhandle_t* handle) {
    int scoreIndex = 0;
    int team = (feederID == FEEDER_REDTEAM_STATS) ? TEAM_RED : TEAM_BLUE;
    clientInfo_t* info = CG_InfoFromScoreIndex(index, team, &scoreIndex);
    score_t* sp = &cg.scores[scoreIndex];

    if (!info || !info->infoValid || info->team == TEAM_SPECTATOR) return "";

    switch (cgs.gametype) {
        case GT_CA:
            // 17 columns: player, score, frags, deaths, DmgD, DmgR, Acc,
            // 8 weapon pairs (gauntlet..RG..PG, HMG), time
            switch (column) {
                case 0: return CG_TruncateListName(info->name, 100, 92);
                case 1: return CG_StatsScoreString(sp->score);
                case 2: return va("%d", sp->frags);
                case 3: return va("%d", sp->deaths - sp->tkd - sp->selfKills);
                case 4: return va("%d", sp->damageDone);
                case 5: return va("%d", sp->damageReceived);
                case 6: return va("%d%%", sp->accuracy);
                case 7: return va("%d", sp->weaponKills[WP_GAUNTLET - 1]);
                case 8: return CG_StatsWeaponString(sp, WP_MACHINEGUN - 1);
                case 9: return CG_StatsWeaponString(sp, WP_SHOTGUN - 1);
                case 10: return CG_StatsWeaponString(sp, WP_GRENADE_LAUNCHER - 1);
                case 11: return CG_StatsWeaponString(sp, WP_ROCKET_LAUNCHER - 1);
                case 12: return CG_StatsWeaponString(sp, WP_LIGHTNING - 1);
                case 13: return CG_StatsWeaponString(sp, WP_RAILGUN - 1);
                case 14: return CG_StatsWeaponString(sp, WP_PLASMAGUN - 1);
                case 15: return CG_StatsWeaponString(sp, WP_HMG - 1);
                case 16: return va("%4d", sp->time);
            }
            return "";
        case GT_TEAM:
        case GT_FREEZE:
            // 18 columns: player, score, frags, deaths, suicides, TK, TKd, net,
            // DmgD, DmgR, Acc, six pickups, time
            switch (column) {
                case 0: return CG_TruncateListName(info->name, 100, 92);
                case 1: return CG_StatsScoreString(sp->score);
                case 2: return va("%d", sp->frags);
                case 3: return va("%d", sp->deaths - sp->tkd - sp->selfKills);
                case 4: return va("%d", sp->selfKills);
                case 5: return va("%d", sp->tks);
                case 6: return va("%d", sp->tkd);
                case 7: return va("%d", sp->frags + sp->tkd - sp->deaths - sp->tks);
                case 8: return va("%d", sp->damageDone);
                case 9: return va("%d", sp->damageReceived);
                case 10: return va("%d%%", sp->accuracy);
                case 11: return va("%d", sp->pickups_ra);
                case 12: return va("%d", sp->pickups_ya);
                case 13: return va("%d", sp->pickups_ga);
                case 14: return va("%d", sp->pickups_mh);
                case 15: return va("%d", sp->pickups_quad);
                case 16: return va("%d", sp->pickups_bs);
                case 17: return va("%4d", sp->time);
            }
            return "";
        case GT_CTF:
        case GT_1FCTF:
        case GT_HARVESTER:
        case GT_DOMINATION:
        case GT_AD:
            // 19 columns: player, score, frags, deaths, suicides, net, DmgD, DmgR,
            // Acc, nine pickups, time
            switch (column) {
                case 0: return CG_TruncateListName(info->name, 100, 92);
                case 1: return CG_StatsScoreString(sp->score);
                case 2: return va("%d", sp->frags);
                case 3: return va("%d", sp->deaths - sp->tkd - sp->selfKills);
                case 4: return va("%d", sp->selfKills);
                case 5: return va("%d", sp->frags - sp->deaths);
                case 6: return va("%d", sp->damageDone);
                case 7: return va("%d", sp->damageReceived);
                case 8: return va("%d%%", sp->accuracy);
                case 9: return va("%d", sp->pickups_ra);
                case 10: return va("%d", sp->pickups_ya);
                case 11: return va("%d", sp->pickups_ga);
                case 12: return va("%d", sp->pickups_mh);
                case 13: return va("%d", sp->pickups_quad);
                case 14: return va("%d", sp->pickups_bs);
                case 15: return va("%d", sp->pickups_regen);
                case 16: return va("%d", sp->pickups_haste);
                case 17: return va("%d", sp->pickups_invis);
                case 18: return va("%4d", sp->time);
            }
            return "";
        default:
            // the binary dispatcher serves no stats pages outside these gametypes
            return "";
    }
}

// [QL] Main feeder dispatcher - dispatches to per-gametype functions
// Binary: FUN_10028830 dispatches by feederID + gametype
static const char* CG_FeederItemText(float feederID, int index, int column, qhandle_t* handle) {
    int scoreIndex = 0;
    clientInfo_t* info = NULL;
    int team = -1;
    score_t* sp = NULL;
    const char* common;

    *handle = -1;

    // Stats feeders have completely separate column layouts
    if (feederID == FEEDER_REDTEAM_STATS || feederID == FEEDER_BLUETEAM_STATS) {
        return CG_FeederItemTextStats(feederID, index, column, handle);
    }

    if (feederID == FEEDER_REDTEAM_LIST) {
        team = TEAM_RED;
    } else if (feederID == FEEDER_BLUETEAM_LIST) {
        team = TEAM_BLUE;
    }

    info = CG_InfoFromScoreIndex(index, team, &scoreIndex);
    sp = &cg.scores[scoreIndex];

    if (!info || !info->infoValid || info->team == TEAM_SPECTATOR) return "";

    // Race scoreboard has unique column layout (7 columns, name at col 3)
    if (cgs.gametype == GT_RACE && team == -1) {
        return CG_FeederItemTextRace(info, sp, column, handle);
    }

    // Gametype-specific columns for the team feeders. GT 7 and RR fall back to the
    // 9-column default layout, which replaces the common block entirely.
    if (team != -1) {
        switch (cgs.gametype) {
            case GT_TEAM:
            case GT_FREEZE:
                common = CG_FeederColumnsCommon(sp, info, column, handle, qtrue);
                if (common) return common;
                return CG_FeederColumnsTdm(sp, info, column, handle);
            case GT_CA:
                common = CG_FeederColumnsCommon(sp, info, column, handle, qtrue);
                if (common) return common;
                return CG_FeederColumnsCa(sp, info, column, handle);
            case GT_CTF:
            case GT_1FCTF:
            case GT_HARVESTER:
            case GT_DOMINATION:
            case GT_AD:
                common = CG_FeederColumnsCommon(sp, info, column, handle, qtrue);
                if (common) return common;
                return CG_FeederColumnsCtf(sp, info, column, handle);
            default:
                return CG_FeederColumnsDefault(sp, info, column, handle);
        }
    }

    // Non-team feeders use the FFA layout
    common = CG_FeederColumnsCommon(sp, info, column, handle, qfalse);
    if (common) return common;
    return CG_FeederColumnsFfa(sp, info, column, handle);
}

static qhandle_t CG_FeederItemImage(float feederID, int index) {
    // [QL] the binary's feederItemImage slot is a ret-0 stub; every icon flows
    // through the per-column handle out-param of CG_FeederItemText
    return 0;
}

/*
=================
CG_FeederSelection

[QL] binary 0x10028b10. In team gametypes the index is a team-relative row at
intermission (user clicks on the end scoreboard) but a global score index during
play (pushed by CG_SetScoreSelection). The playerlistRED/playerlistBLUE items on
the in-game menu get their cursor synced, and the other team's list on the end
menu is deselected.
=================
*/
static void CG_FeederSelection(float feederID, int index) {
    int i, count, row;
    int team;
    itemDef_t* item;

    if (index == -1) {
        return;
    }
    if (cgs.gametype < GT_TEAM) {
        cg.selectedScore = index;
        return;
    }

    team = (feederID == FEEDER_REDTEAM_LIST) ? TEAM_RED : TEAM_BLUE;
    count = 0;
    row = -1;

    if (cg.snap->ps.pm_type == PM_INTERMISSION) {
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].team != team) {
                continue;
            }
            if (count == index) {
                cg.selectedScore = i;
                break;
            }
            count++;
        }
        return;
    }

    for (i = 0; i < cg.numScores; i++) {
        if (cg.scores[i].team != team) {
            continue;
        }
        if (index >= 0 && index < cg.numScores &&
            cg.scores[i].client == cg.scores[index].client) {
            row = count;
        }
        count++;
    }

    if (menuScoreboard) {
        item = Menu_FindItemByName(menuScoreboard,
                                   (team == TEAM_RED) ? "playerlistRED" : "playerlistBLUE");
        if (item) {
            item->cursorPos = row;
        }
    }
    if (menuEndScoreboard) {
        item = Menu_FindItemByName(menuEndScoreboard,
                                   (team == TEAM_RED) ? "playerlistBLUE" : "playerlistRED");
        if (item) {
            item->cursorPos = -1;
        }
    }
}

static float CG_Cvar_Get(const char* cvar) {
    char buff[128];
    memset(buff, 0, sizeof(buff));
    trap_Cvar_VariableStringBuffer(cvar, buff, sizeof(buff));
    return atof(buff);
}

void CG_Text_PaintWithCursor(float x, float y, float scale, vec4_t color, const char* text, int cursorPos, char cursor, int limit, int style) {
    CG_Text_Paint(x, y, scale, color, text, 0, limit, style);
}

static int CG_OwnerDrawWidth(int ownerDraw, float scale) {
    switch (ownerDraw) {
        case CG_GAME_TYPE:
            return CG_Text_Width(CG_GameTypeString(), scale, 0);
        case CG_GAME_STATUS:
            return CG_Text_Width(CG_GetGameStatusText(), scale, 0);
        case CG_MATCH_STATUS:
            return CG_Text_Width(CG_GetMatchStatusText(), scale, 0);
        case CG_KILLER:
            return CG_Text_Width(CG_GetKillerText(), scale, 0);
    }
    return 0;
}

static int CG_PlayCinematic(const char* name, float x, float y, float w, float h) {
    return trap_CIN_PlayCinematic(name, x, y, w, h, CIN_loop);
}

static void CG_StopCinematic(int handle) {
    trap_CIN_StopCinematic(handle);
}

static void CG_DrawCinematic(int handle, float x, float y, float w, float h) {
    trap_CIN_SetExtents(handle, x, y, w, h);
    trap_CIN_DrawCinematic(handle);
}

static void CG_RunCinematicFrame(int handle) {
    trap_CIN_RunCinematic(handle);
}

/*
=================
CG_LoadHudMenu();

=================
*/
void CG_LoadHudMenu(void) {
    char buff[1024];
    const char* hudSet;

    cgDC.registerShaderNoMip = &trap_R_RegisterShaderNoMip;
    cgDC.setColor = &trap_R_SetColor;
    cgDC.drawHandlePic = &CG_DrawPic;
    cgDC.drawStretchPic = &trap_R_DrawStretchPic;
    cgDC.drawText = &CG_DrawText_DC;
    cgDC.textWidth = &CG_TextWidth_DC;
    cgDC.textHeight = &CG_TextHeight_DC;
    cgDC.registerModel = &trap_R_RegisterModel;
    cgDC.modelBounds = &trap_R_ModelBounds;
    cgDC.fillRect = &CG_FillRect;
    cgDC.drawRect = &CG_DrawRect;
    cgDC.drawSides = &CG_DrawSides;
    cgDC.drawTopBottom = &CG_DrawTopBottom;
    cgDC.clearScene = &trap_R_ClearScene;
    cgDC.addRefEntityToScene = &trap_R_AddRefEntityToScene;
    cgDC.renderScene = &trap_R_RenderScene;
    cgDC.registerFont = &trap_R_RegisterFont;
    cgDC.ownerDrawItem = &CG_OwnerDraw;
    cgDC.getValue = &CG_GetValue;
    cgDC.ownerDrawVisible = &CG_OwnerDrawVisible;
    cgDC.runScript = &CG_RunMenuScript;
    cgDC.getTeamColor = &CG_GetTeamColor;
    cgDC.setCVar = trap_Cvar_Set;
    cgDC.getCVarString = trap_Cvar_VariableStringBuffer;
    cgDC.getCVarValue = CG_Cvar_Get;
    cgDC.drawTextWithCursor = &CG_DrawTextWithCursor_DC;
    // cgDC.setOverstrikeMode = &trap_Key_SetOverstrikeMode;
    // cgDC.getOverstrikeMode = &trap_Key_GetOverstrikeMode;
    cgDC.startLocalSound = &trap_S_StartLocalSound;
    cgDC.ownerDrawHandleKey = &CG_OwnerDrawHandleKey;
    cgDC.feederCount = &CG_FeederCount;
    cgDC.feederItemImage = &CG_FeederItemImage;
    cgDC.feederItemText = &CG_FeederItemText;
    cgDC.feederSelection = &CG_FeederSelection;
    // cgDC.setBinding = &trap_Key_SetBinding;
    // cgDC.getBindingBuf = &trap_Key_GetBindingBuf;
    cgDC.keynumToStringBuf = &trap_Key_KeynumToStringBuf;
    // cgDC.executeText = &trap_Cmd_ExecuteText;
    cgDC.Error = &Com_Error;
    cgDC.Print = &Com_Printf;
    cgDC.ownerDrawWidth = &CG_OwnerDrawWidth;
    // cgDC.Pause = &CG_Pause;
    cgDC.registerSound = &trap_S_RegisterSound;
    cgDC.startBackgroundTrack = &trap_S_StartBackgroundTrack;
    cgDC.stopBackgroundTrack = &trap_S_StopBackgroundTrack;
    cgDC.playCinematic = &CG_PlayCinematic;
    cgDC.stopCinematic = &CG_StopCinematic;
    cgDC.drawCinematic = &CG_DrawCinematic;
    cgDC.runCinematicFrame = &CG_RunCinematicFrame;
    cgDC.setWidescreen = &CG_SetWidescreen;

    Init_Display(&cgDC);

    Menu_Reset();

    trap_Cvar_VariableStringBuffer("cg_hudFiles", buff, sizeof(buff));
    hudSet = buff;
    if (hudSet[0] == '\0') {
        hudSet = "ui/hud.txt";
    }

    // [QL] Load hud.txt first - CG_LoadMenus calls Menu_Reset() internally,
    // so all CG_ParseMenu calls must come AFTER to avoid being wiped.
    CG_LoadMenus(hudSet);

    // [QL] Static table menus (binary-verified order)
    CG_ParseMenu("ui/intro.menu");
    CG_ParseMenu("ui/ingamescoreteam.menu");
    CG_ParseMenu("ui/ingamescorenoteam.menu");
    CG_ParseMenu("ui/endscoreteam.menu");
    CG_ParseMenu("ui/endscorenoteam.menu");
    CG_ParseMenu("ui/spectator.menu");
    CG_ParseMenu("ui/spectator_follow.menu");
    CG_ParseMenu("ui/comp_spectator.menu");
    CG_ParseMenu("ui/comp_spectator_follow.menu");
    CG_ParseMenu("ui/ingamestats.menu");

    // [QL] Per-gametype in-game scoreboard menus
    CG_ParseMenu("ui/ingame_scoreboard_ffa.menu");
    CG_ParseMenu("ui/ingame_scoreboard_duel.menu");
    CG_ParseMenu("ui/ingame_scoreboard_race.menu");
    CG_ParseMenu("ui/ingame_scoreboard_tdm.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ca.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ctf.menu");
    CG_ParseMenu("ui/ingame_scoreboard_1fctf.menu");
    CG_ParseMenu("ui/ingame_scoreboard_har.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ft.menu");
    CG_ParseMenu("ui/ingame_scoreboard_dom.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ad.menu");
    CG_ParseMenu("ui/ingame_scoreboard_rr.menu");

    // [QL] Per-gametype end-of-game scoreboard menus
    CG_ParseMenu("ui/end_scoreboard_ffa.menu");
    CG_ParseMenu("ui/end_scoreboard_duel.menu");
    CG_ParseMenu("ui/end_scoreboard_race.menu");
    CG_ParseMenu("ui/end_scoreboard_tdm.menu");
    CG_ParseMenu("ui/end_scoreboard_ca.menu");
    CG_ParseMenu("ui/end_scoreboard_ctf.menu");
    CG_ParseMenu("ui/end_scoreboard_1fctf.menu");
    CG_ParseMenu("ui/end_scoreboard_har.menu");
    CG_ParseMenu("ui/end_scoreboard_ft.menu");
    CG_ParseMenu("ui/end_scoreboard_dom.menu");
    CG_ParseMenu("ui/end_scoreboard_ad.menu");
    CG_ParseMenu("ui/end_scoreboard_rr.menu");

    CG_CacheHudMenuHandles();
}

void CG_AssetCache(void) {
    cgDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(ASSET_GRADIENTBAR);
    cgDC.Assets.fxBasePic = trap_R_RegisterShaderNoMip(ART_FX_BASE);
    cgDC.Assets.fxPic[0] = trap_R_RegisterShaderNoMip(ART_FX_RED);
    cgDC.Assets.fxPic[1] = trap_R_RegisterShaderNoMip(ART_FX_YELLOW);
    cgDC.Assets.fxPic[2] = trap_R_RegisterShaderNoMip(ART_FX_GREEN);
    cgDC.Assets.fxPic[3] = trap_R_RegisterShaderNoMip(ART_FX_TEAL);
    cgDC.Assets.fxPic[4] = trap_R_RegisterShaderNoMip(ART_FX_BLUE);
    cgDC.Assets.fxPic[5] = trap_R_RegisterShaderNoMip(ART_FX_CYAN);
    cgDC.Assets.fxPic[6] = trap_R_RegisterShaderNoMip(ART_FX_WHITE);
    cgDC.Assets.scrollBar = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR);
    cgDC.Assets.scrollBarArrowDown = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWDOWN);
    cgDC.Assets.scrollBarArrowUp = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWUP);
    cgDC.Assets.scrollBarArrowLeft = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWLEFT);
    cgDC.Assets.scrollBarArrowRight = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWRIGHT);
    cgDC.Assets.scrollBarThumb = trap_R_RegisterShaderNoMip(ASSET_SCROLL_THUMB);
    cgDC.Assets.sliderBar = trap_R_RegisterShaderNoMip(ASSET_SLIDER_BAR);
    cgDC.Assets.sliderThumb = trap_R_RegisterShaderNoMip(ASSET_SLIDER_THUMB);
}

/*
=================
CG_Init

Called after every level change or subsystem restart
Will perform callbacks to make the loading info screen update.
=================
*/
void CG_Init(int serverMessageNum, int serverCommandSequence, int clientNum) {
    const char* s;

    // clear everything
    memset(&cgs, 0, sizeof(cgs));
    memset(&cg, 0, sizeof(cg));
    memset(cg_entities, 0, sizeof(cg_entities));
    memset(cg_weapons, 0, sizeof(cg_weapons));
    memset(cg_items, 0, sizeof(cg_items));

    cg.clientNum = clientNum;
    cg.race.nextCheckpointEnt = -1;
    cg.race.nextNextCheckpointEnt = -1;
    cg.race.currentCheckpointEnt = -1;
    cg.autoFollowClient = -1;  // [QL] deferred auto-follow idle (CG_CheckAutoFollow)
    CG_ClearChat();

    cgs.processedSnapshotNum = serverMessageNum;
    cgs.serverCommandSequence = serverCommandSequence;

    // load a few needed things before we do any screen updates
    cgs.media.charsetShader = trap_R_RegisterShader("gfx/2d/bigchars");
    cgs.media.whiteShader = trap_R_RegisterShader("white");
    cgs.media.charsetProp = trap_R_RegisterShaderNoMip("menu/art/font1_prop.png");
    cgs.media.charsetPropGlow = trap_R_RegisterShaderNoMip("menu/art/font1_prop_glo.png");
    cgs.media.charsetPropB = trap_R_RegisterShaderNoMip("menu/art/font2_prop.png");

    // [QL] Register TTF fonts early so CG_Text_Paint works during loading screen.
    // Register at 2x point sizes for sharper rendering at high resolutions -
    // ioq3's font atlas is fixed at registration time; higher point size = more glyph detail.
    // Override glyphScale so text draws at the correct visual size.
    // CG_LoadHudMenu() later tries to re-register with abstract names like "fonts/font"
    // which don't exist in the pk3 - RE_RegisterFont returns early on file-not-found
    // WITHOUT modifying the output struct, so our early registration is preserved.
    trap_R_RegisterFont("fonts/notosans-regular.ttf", 48, &cgDC.Assets.textFont);
    trap_R_RegisterFont("fonts/droidsansmono.ttf", 32, &cgDC.Assets.smallFont);
    trap_R_RegisterFont("fonts/handelgothic.ttf", 96, &cgDC.Assets.bigFont);
    cgDC.Assets.fontRegistered = qtrue;

    // [QL] Populate extraFonts[] for fontIndex-based selection:
    // 0 = textFont (NotoSans), 1 = bigFont (handelgothic), 2 = smallFont (DroidSansMono)
    memcpy(&cgDC.Assets.extraFonts[0], &cgDC.Assets.textFont, sizeof(fontInfo_t));
    memcpy(&cgDC.Assets.extraFonts[1], &cgDC.Assets.bigFont, sizeof(fontInfo_t));
    memcpy(&cgDC.Assets.extraFonts[2], &cgDC.Assets.smallFont, sizeof(fontInfo_t));

    // [QL] loading screen assets
    cgs.media.loadingbackShader = trap_R_RegisterShaderNoMip("levelshots/loadingback.jpg");
    cgs.media.gtBackgroundShader = trap_R_RegisterShaderNoMip("ui/assets/main_menu/gt_background.png");
    cgs.media.qlLogoShader = trap_R_RegisterShaderNoMip("ui/assets/main_menu/ql_logo.png");
    cgs.media.logoBackgroundShader = trap_R_RegisterShaderNoMip("ui/assets/main_menu/logo_background.png");
    cgs.media.backscreenSmokeShader = trap_R_RegisterShaderNoMip("ui/assets/backscreen_smoke.jpg");

    // [QL] initialize speedometer bar colors (green / yellow, alpha set per-mode)
    cg.speedBarColor1[0] = 0.0f; cg.speedBarColor1[1] = 1.0f;
    cg.speedBarColor1[2] = 0.0f; cg.speedBarColor1[3] = 1.0f;
    cg.speedBarColor2[0] = 1.0f; cg.speedBarColor2[1] = 1.0f;
    cg.speedBarColor2[2] = 0.0f; cg.speedBarColor2[3] = 1.0f;

    CG_RegisterCvars();
    CG_InitColorWheel();

    // [QL] reset state cvars on map load
    trap_Cvar_Set("ui_mainmenu", "0");
    trap_Cvar_Set("ui_intermission", "0");

    CG_InitConsoleCommands();

    cg.weaponSelect = WP_MACHINEGUN;

    cgs.redflag = cgs.blueflag = -1;  // For compatibily, default to unset for
    cgs.flagStatus = -1;              // old servers

    // get the rendering configuration from the client system
    trap_GetGlconfig(&cgs.glconfig);
    cgs.screenXScale = cgs.glconfig.vidWidth / (float)SCREEN_WIDTH;
    cgs.screenYScale = cgs.glconfig.vidHeight / (float)SCREEN_HEIGHT;

    // [QL] compute widescreen bias: half the extra width beyond 4:3
    {
        float width43 = 4.0f * cgs.glconfig.vidHeight / 3.0f;
        if ((float)cgs.glconfig.vidWidth > width43) {
            cgs.widescreenBias = ((float)cgs.glconfig.vidWidth - width43) * 0.5f;
        } else {
            cgs.widescreenBias = 0.0f;
        }
    }

    // get the gamestate from the client system
    trap_GetGameState(&cgs.gameState);

    // check version
    s = CG_ConfigString(CS_GAME_VERSION);
    if (strcmp(s, GAME_VERSION)) {
        CG_Error("Client/Server game mismatch: %s/%s", GAME_VERSION, s);
    }

    s = CG_ConfigString(CS_LEVEL_START_TIME);
    cgs.levelStartTime = atoi(s);

    CG_ParseServerinfo();
    // [QL] parse player model/head overrides before clients are registered so the
    // g_allowCustomHeadmodels / model-override rules apply on the initial load.
    CG_ParseConfigParams();
    // [QL] custom-settings bitmask must be set before CG_RegisterClients so the RR-infected
    // bones model (customSettings bit 0x4000000) applies to clients on the initial load.
    cgs.customSettings = atoi(CG_ConfigString(CS_CUSTOM_SETTINGS));

    // load the new map
    CG_LoadingString("collision map");

    trap_CM_LoadMap(cgs.mapname);

    String_Init();

    cg.loading = qtrue;  // force players to load instead of defer
    cg.loadingStage++;

    CG_LoadingString("sounds");

    CG_RegisterSounds();
    cg.loadingStage++;

    CG_LoadingString("graphics");

    CG_RegisterGraphics();
    cg.loadingStage++;

    CG_LoadingString("clients");

    CG_RegisterClients();  // if low on memory, some clients will be deferred
    cg.loadingStage++;

    CG_AssetCache();
    CG_LoadHudMenu();  // load new hud stuff

    cg.loading = qfalse;  // future players will be deferred

    CG_InitLocalEntities();

    CG_InitMarkPolys();

    // remove the last loading update
    cg.infoScreenText[0] = 0;

    // Make sure we have update values (scores)
    CG_SetConfigValues();

    // [QL] the join-game overlay owns the screen until the client joins a team
    // or the end scoreboard comes up (binary CG_Init 0x10029fb2)
    cg.joinMenuPending = qtrue;

    // [QL] pre-cache country flag shaders for the scoreboard/profile UI.
    CG_CacheCountryFlags();

    CG_StartMusic();

    CG_LoadingString("");

    CG_InitTeamChat();

    CG_ShaderStateChanged();

    trap_S_ClearLoopingSounds(qtrue);
}

/*
=================
CG_Shutdown

Called before every level change or subsystem restart
=================
*/
void CG_Shutdown(void) {
    // some mods may need to do cleanup work here,
    // like closing files or archiving session data
}