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

#include "g_local.h"

level_locals_t level;

typedef struct {
    vmCvar_t* vmCvar;
    char* cvarName;
    char* defaultString;
    int cvarFlags;
    int modCount;           // [QL] tracks vmCvar->modificationCount (was modificationCount)
    void (*onChanged)(void); // [QL] callback when value changes (was trackChange/teamShader)
} cvarTable_t;

gentity_t g_entities[MAX_GENTITIES];
gclient_t g_clients[MAX_CLIENTS];

gameImport_t *imports;   // [QL] engine syscall table

vmCvar_t g_gametype;
vmCvar_t g_dmflags;
vmCvar_t g_fraglimit;
vmCvar_t g_timelimit;
vmCvar_t g_capturelimit;
vmCvar_t g_friendlyFire;
vmCvar_t g_password;
vmCvar_t g_needpass;
vmCvar_t g_maxclients;
vmCvar_t g_maxGameClients;
vmCvar_t g_dedicated;
vmCvar_t g_speed;
vmCvar_t g_gravity;
vmCvar_t g_cheats;
vmCvar_t g_knockback;
vmCvar_t g_quadfactor;
vmCvar_t g_forcerespawn;
vmCvar_t g_inactivity;
vmCvar_t g_debugMove;
vmCvar_t g_debugDamage;
vmCvar_t g_debugAlloc;
vmCvar_t g_weaponRespawn;
vmCvar_t g_weaponTeamRespawn;
vmCvar_t g_motd;
vmCvar_t g_synchronousClients;
vmCvar_t g_warmup;
vmCvar_t g_doWarmup;
vmCvar_t g_restarted;
vmCvar_t g_logfile;
vmCvar_t g_logfileSync;
vmCvar_t g_blood;
vmCvar_t g_podiumDist;
vmCvar_t g_podiumDrop;
vmCvar_t g_allowVote;
vmCvar_t g_allowVoteMidGame;
vmCvar_t g_allowSpecVote;
vmCvar_t g_voteFlags;
vmCvar_t g_voteDelay;
vmCvar_t g_voteLimit;
vmCvar_t g_teamAutoJoin;
vmCvar_t g_teamForceBalance;
vmCvar_t g_banIPs;
vmCvar_t g_filterBan;
vmCvar_t g_smoothClients;
vmCvar_t g_knockback_z;
vmCvar_t g_knockback_z_self;
vmCvar_t g_knockback_cripple;
vmCvar_t g_listEntity;
vmCvar_t g_localTeamPref;
vmCvar_t g_obeliskHealth;
vmCvar_t g_obeliskRegenPeriod;
vmCvar_t g_obeliskRegenAmount;
vmCvar_t g_obeliskRespawnDelay;
vmCvar_t g_cubeTimeout;
vmCvar_t g_redteam;
vmCvar_t g_blueteam;
vmCvar_t g_enableDust;
vmCvar_t g_enableBreath;
vmCvar_t g_proxMineTimeout;
vmCvar_t g_infiniteAmmo;
vmCvar_t g_dropFlag;
vmCvar_t g_runes;

// [QL] per-weapon damage cvars
vmCvar_t g_damage_g;
vmCvar_t g_damage_mg;
vmCvar_t g_damage_sg;
vmCvar_t g_damage_gl;
vmCvar_t g_damage_rl;
vmCvar_t g_damage_lg;
vmCvar_t g_damage_rg;
vmCvar_t g_damage_pg;
vmCvar_t g_damage_bfg;
vmCvar_t g_damage_gh;
vmCvar_t g_damage_ng;
vmCvar_t g_damage_cg;
vmCvar_t g_damage_hmg;
vmCvar_t g_damage_pl;

// [QL] per-weapon knockback multiplier cvars
vmCvar_t g_knockback_g;
vmCvar_t g_knockback_mg;
vmCvar_t g_knockback_sg;
vmCvar_t g_knockback_gl;
vmCvar_t g_knockback_rl;
vmCvar_t g_knockback_rl_self;
vmCvar_t g_knockback_lg;
vmCvar_t g_knockback_rg;
vmCvar_t g_knockback_pg;
vmCvar_t g_knockback_pg_self;
vmCvar_t g_knockback_bfg;
vmCvar_t g_knockback_gh;
vmCvar_t g_knockback_ng;
vmCvar_t g_knockback_pl;
vmCvar_t g_knockback_cg;
vmCvar_t g_knockback_hmg;

// [QL] weapon reload (fire interval) cvars
vmCvar_t weapon_reload_mg;
vmCvar_t weapon_reload_sg;
vmCvar_t weapon_reload_gl;
vmCvar_t weapon_reload_rl;
vmCvar_t weapon_reload_lg;
vmCvar_t weapon_reload_rg;
vmCvar_t weapon_reload_pg;
vmCvar_t weapon_reload_bfg;
vmCvar_t weapon_reload_gh;
vmCvar_t weapon_reload_ng;
vmCvar_t weapon_reload_prox;
vmCvar_t weapon_reload_cg;
vmCvar_t weapon_reload_hmg;

// [QL] per-weapon splash damage cvars
vmCvar_t g_splashdamage_gl;
vmCvar_t g_splashdamage_rl;
vmCvar_t g_splashdamage_pg;
vmCvar_t g_splashdamage_bfg;
vmCvar_t g_splashdamage_pl;
vmCvar_t g_splashdamageOffset;
vmCvar_t g_rocketsplashOffset;

// [QL] knockback cap cvar
vmCvar_t g_max_knockback;

// [QL] per-weapon splash radius cvars
vmCvar_t g_splashradius_gl;
vmCvar_t g_splashradius_rl;
vmCvar_t g_splashradius_pg;
vmCvar_t g_splashradius_bfg;
vmCvar_t g_splashradius_pl;

// [QL] projectile velocity cvars
vmCvar_t g_velocity_gl;
vmCvar_t g_velocity_rl;
vmCvar_t g_velocity_pg;
vmCvar_t g_velocity_bfg;
vmCvar_t g_velocity_gh;

// [QL] projectile acceleration cvars
vmCvar_t g_accelFactor_rl;
vmCvar_t g_accelFactor_pg;
vmCvar_t g_accelFactor_bfg;
vmCvar_t g_accelRate_rl;
vmCvar_t g_accelRate_pg;
vmCvar_t g_accelRate_bfg;

// [QL] projectile gravity cvars
vmCvar_t weapon_gravity_rl;
vmCvar_t weapon_gravity_pg;
vmCvar_t weapon_gravity_bfg;
vmCvar_t weapon_gravity_ng;

// [QL] nailgun cvars
vmCvar_t g_nailspeed;
vmCvar_t g_nailcount;
vmCvar_t g_nailspread;
vmCvar_t g_nailbounce;
vmCvar_t g_nailbouncepercentage;

// [QL] guided rocket cvar
vmCvar_t g_guidedRocket;

// [QL] loadout system
vmCvar_t g_loadout;

// [QL] ammo system
vmCvar_t g_ammoPack;
vmCvar_t g_ammoRespawn;
vmCvar_t g_spawnItemAmmo;

// [QL] game state management
vmCvar_t g_gameState;
vmCvar_t g_warmupReadyPercentage;
vmCvar_t g_forfeit;

// [QL] serverinfo cvars read by cgame
vmCvar_t g_teamsize;
vmCvar_t g_teamSizeMin;
vmCvar_t g_overtime;
vmCvar_t g_scorelimit;
vmCvar_t g_mercylimit;
vmCvar_t g_mercytime;
vmCvar_t g_rrAllowNegativeScores;
vmCvar_t g_spawnItems;
vmCvar_t sv_quitOnExitLevel;
vmCvar_t g_isBotOnly;
vmCvar_t g_training;
vmCvar_t g_itemHeight;
vmCvar_t g_itemTimers;
vmCvar_t g_quadDamageFactor;
vmCvar_t g_freezeRoundDelay;
vmCvar_t g_timeoutCount;

// [QL] damage-through-surface cvars
vmCvar_t g_forceDmgThroughSurface;
vmCvar_t g_dmgThroughSurfaceDistance;
vmCvar_t g_dmgThroughSurfaceDampening;
vmCvar_t g_dmgThroughSurfaceAngularThreshold;

// [QL] player cylinder hitbox
vmCvar_t g_playerCylinders;

// [QL] server framerate
vmCvar_t sv_fps;

// [QL] round-based game mode cvars
vmCvar_t roundtimelimit;
vmCvar_t g_roundWarmupDelay;
vmCvar_t roundlimit;
vmCvar_t g_accuracyFlags;
vmCvar_t g_lastManStandingWarning;
vmCvar_t g_roundDrawLivingCount;
vmCvar_t g_roundDrawHealthCount;
vmCvar_t g_spawnArmor;
vmCvar_t g_adElimScoreBonus;

// [QL] freeze tag cvars
vmCvar_t g_freezeThawTick;
vmCvar_t g_freezeProtectedSpawnTime;
vmCvar_t g_freezeThawTime;
vmCvar_t g_freezeAutoThawTime;
vmCvar_t g_freezeThawRadius;
vmCvar_t g_freezeThawThroughSurface;
vmCvar_t g_freezeThawWinningTeam;
vmCvar_t g_freezeRemovePowerupsOnRound;
vmCvar_t g_freezeResetHealthOnRound;
vmCvar_t g_freezeResetArmorOnRound;
vmCvar_t g_freezeResetWeaponsOnRound;
vmCvar_t g_freezeAllowRespawn;

// [QL] lag compensation
vmCvar_t g_lagHaxHistory;
vmCvar_t g_lagHaxMs;

// [QL] weapon modifiers
vmCvar_t g_ironsights_mg;

// [QL] Quad Hog
vmCvar_t g_quadHog;
vmCvar_t g_quadHogTime;
vmCvar_t g_damagePlums;

// [QL] per-weapon advanced cvars
vmCvar_t g_damage_sg_outer;
vmCvar_t g_damage_sg_falloff;
vmCvar_t g_range_sg_falloff;
vmCvar_t g_damage_lg_falloff;
vmCvar_t g_range_lg_falloff;
vmCvar_t g_headShotDamage_rg;
vmCvar_t g_railJump;

// [QL] RR infection mode
vmCvar_t g_rrInfectedSpreadTime;
vmCvar_t g_rrInfectedSpreadWarningTime;
vmCvar_t g_rrInfectedSurvivorScoreMethod;
vmCvar_t g_rrInfectedSurvivorScoreRate;
vmCvar_t g_rrInfectedSurvivorScoreBonus;

// [QL] tiered armor
vmCvar_t armor_tiered;

// [QL] starting loadout cvars
vmCvar_t g_startingWeapons;
vmCvar_t g_startingHealth;
vmCvar_t g_startingHealthBonus;
vmCvar_t g_startingArmor;
vmCvar_t g_startingAmmo_g;
vmCvar_t g_startingAmmo_mg;
vmCvar_t g_startingAmmo_sg;
vmCvar_t g_startingAmmo_gl;
vmCvar_t g_startingAmmo_rl;
vmCvar_t g_startingAmmo_lg;
vmCvar_t g_startingAmmo_rg;
vmCvar_t g_startingAmmo_pg;
vmCvar_t g_startingAmmo_bfg;
vmCvar_t g_startingAmmo_gh;
vmCvar_t g_startingAmmo_ng;
vmCvar_t g_startingAmmo_pl;
vmCvar_t g_startingAmmo_cg;
vmCvar_t g_startingAmmo_hmg;

// [QL] pmove_* cvars - configurable movement physics
vmCvar_t pmove_JumpVelocity;
vmCvar_t pmove_JumpVelocityMax;
vmCvar_t pmove_JumpVelocityScaleAdd;
vmCvar_t pmove_JumpVelocityTimeThreshold;
vmCvar_t pmove_JumpVelocityTimeThresholdOffset;
vmCvar_t pmove_ChainJump;
vmCvar_t pmove_ChainJumpVelocity;
vmCvar_t pmove_StepJumpVelocity;
vmCvar_t pmove_JumpTimeDeltaMin;
vmCvar_t pmove_RampJump;
vmCvar_t pmove_RampJumpScale;
vmCvar_t pmove_StepJump;
vmCvar_t pmove_CrouchStepJump;
vmCvar_t pmove_AutoHop;
vmCvar_t pmove_BunnyHop;
vmCvar_t pmove_AirAccel;
vmCvar_t pmove_AirStopAccel;
vmCvar_t pmove_AirControl;
vmCvar_t pmove_StrafeAccel;
vmCvar_t pmove_WalkAccel;
vmCvar_t pmove_WalkFriction;
vmCvar_t pmove_CircleStrafeFriction;
vmCvar_t pmove_CrouchSlideFriction;
vmCvar_t pmove_CrouchSlideTime;
vmCvar_t pmove_AirSteps;
vmCvar_t pmove_AirStepFriction;
vmCvar_t pmove_StepHeight;
vmCvar_t pmove_WaterSwimScale;
vmCvar_t pmove_WaterWadeScale;
vmCvar_t pmove_WishSpeed;
vmCvar_t pmove_WeaponDropTime;
vmCvar_t pmove_WeaponRaiseTime;
vmCvar_t pmove_NoPlayerClip;
vmCvar_t pmove_HookPullVelocity;
vmCvar_t pmove_DoubleJump;
vmCvar_t pmove_CrouchSlide;
vmCvar_t pmove_VelocityGH;

// [QL] dirty flag - set when any pmove cvar changes, cleared after sending CS_PMOVEINFO
static qboolean pmoveInfoDirty = qtrue;

// [QL] starting weapons callback - publish bitmask to configstring
static void OnChangedStartingWeapons(void) {
    trap_SetConfigstring(CS_STARTING_WEAPONS, va("%i", g_startingWeapons.integer));
}

// [QL] pmove_* onChanged callbacks - update PM_Set* globals and mark configstring dirty
static void OnChangedPmoveJumpVelocity(void)          { PM_SetJumpVelocity(pmove_JumpVelocity.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveJumpVelocityMax(void)        { PM_SetJumpVelocityMax(pmove_JumpVelocityMax.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveJumpVelocityScaleAdd(void)   { PM_SetJumpVelocityScaleAdd(pmove_JumpVelocityScaleAdd.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveJumpVelocityTimeThreshold(void) { PM_SetJumpVelocityTimeThreshold(pmove_JumpVelocityTimeThreshold.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveJumpVelocityTimeThresholdOffset(void) { PM_SetJumpVelocityTimeThresholdOffset(pmove_JumpVelocityTimeThresholdOffset.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveChainJump(void)              { PM_SetChainJump(pmove_ChainJump.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveChainJumpVelocity(void)      { PM_SetChainJumpVelocity(pmove_ChainJumpVelocity.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveStepJumpVelocity(void)       { PM_SetStepJumpVelocity(pmove_StepJumpVelocity.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveJumpTimeDeltaMin(void)       { PM_SetJumpTimeDeltaMin(pmove_JumpTimeDeltaMin.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveRampJump(void)               { PM_SetRampJump(pmove_RampJump.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveRampJumpScale(void)          { PM_SetRampJumpScale(pmove_RampJumpScale.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveStepJump(void)               { PM_SetStepJump(pmove_StepJump.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveCrouchStepJump(void)         { PM_SetCrouchStepJump(pmove_CrouchStepJump.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAutoHop(void)                { PM_SetAutoHop(pmove_AutoHop.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveBunnyHop(void)               { PM_SetBunnyHop(pmove_BunnyHop.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAirAccel(void)               { PM_SetAirAccel(pmove_AirAccel.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAirStopAccel(void)           { PM_SetAirStopAccel(pmove_AirStopAccel.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAirControl(void)             { PM_SetAirControl(pmove_AirControl.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveStrafeAccel(void)            { PM_SetStrafeAccel(pmove_StrafeAccel.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWalkAccel(void)              { PM_SetWalkAccel(pmove_WalkAccel.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWalkFriction(void)           { PM_SetWalkFriction(pmove_WalkFriction.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveCircleStrafeFriction(void)   { PM_SetCircleStrafeFriction(pmove_CircleStrafeFriction.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveCrouchSlideFriction(void)    { PM_SetCrouchSlideFriction(pmove_CrouchSlideFriction.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveCrouchSlideTime(void)        { PM_SetCrouchSlideTime(pmove_CrouchSlideTime.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAirSteps(void)               { PM_SetAirSteps(pmove_AirSteps.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveAirStepFriction(void)        { PM_SetAirStepFriction(pmove_AirStepFriction.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveStepHeight(void)             { PM_SetStepHeight(pmove_StepHeight.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWaterSwimScale(void)         { PM_SetWaterSwimScale(pmove_WaterSwimScale.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWaterWadeScale(void)         { PM_SetWaterWadeScale(pmove_WaterWadeScale.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWishSpeed(void)              { PM_SetWishSpeed(pmove_WishSpeed.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWeaponDropTime(void)         { PM_SetWeaponDropTime(pmove_WeaponDropTime.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveWeaponRaiseTime(void)        { PM_SetWeaponRaiseTime(pmove_WeaponRaiseTime.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveNoPlayerClip(void)           { PM_SetNoPlayerClip(pmove_NoPlayerClip.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveHookPullVelocity(void)       { PM_SetHookPullVelocity(pmove_HookPullVelocity.value); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveDoubleJump(void)             { PM_SetDoubleJump(pmove_DoubleJump.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveCrouchSlide(void)            { PM_SetCrouchSlide(pmove_CrouchSlide.integer); pmoveInfoDirty = qtrue; }
static void OnChangedPmoveVelocityGH(void)             { PM_SetVelocityGH(pmove_VelocityGH.value); pmoveInfoDirty = qtrue; }

// [QL] weapon_reload_* onChanged callbacks - update bg_weaponReloadTime[]
static void OnChangedWeaponReloadMG(void)   { BG_SetWeaponReload(WP_MACHINEGUN, weapon_reload_mg.integer); }
static void OnChangedWeaponReloadSG(void)   { BG_SetWeaponReload(WP_SHOTGUN, weapon_reload_sg.integer); }
static void OnChangedWeaponReloadGL(void)   { BG_SetWeaponReload(WP_GRENADE_LAUNCHER, weapon_reload_gl.integer); }
static void OnChangedWeaponReloadRL(void)   { BG_SetWeaponReload(WP_ROCKET_LAUNCHER, weapon_reload_rl.integer); }
static void OnChangedWeaponReloadLG(void)   { BG_SetWeaponReload(WP_LIGHTNING, weapon_reload_lg.integer); }
static void OnChangedWeaponReloadRG(void)   { BG_SetWeaponReload(WP_RAILGUN, weapon_reload_rg.integer); }
static void OnChangedWeaponReloadPG(void)   { BG_SetWeaponReload(WP_PLASMAGUN, weapon_reload_pg.integer); }
static void OnChangedWeaponReloadBFG(void)  { BG_SetWeaponReload(WP_BFG, weapon_reload_bfg.integer); }
static void OnChangedWeaponReloadGH(void)   { BG_SetWeaponReload(WP_GAUNTLET, weapon_reload_gh.integer);
                                              BG_SetWeaponReload(WP_GRAPPLING_HOOK, weapon_reload_gh.integer); }
static void OnChangedWeaponReloadNG(void)   { BG_SetWeaponReload(WP_NAILGUN, weapon_reload_ng.integer); }
static void OnChangedWeaponReloadProx(void) { BG_SetWeaponReload(WP_PROX_LAUNCHER, weapon_reload_prox.integer); }
static void OnChangedWeaponReloadCG(void)   { BG_SetWeaponReload(WP_CHAINGUN, weapon_reload_cg.integer); }
static void OnChangedWeaponReloadHMG(void)  { BG_SetWeaponReload(WP_HMG, weapon_reload_hmg.integer); }

static cvarTable_t gameCvarTable[] = {
    // don't override the cheat state set by the system
    {&g_cheats, "sv_cheats", "", 0, 0, NULL},

    // noset vars
    {NULL, "gamename", GAMEVERSION, CVAR_SERVERINFO | CVAR_ROM, 0, NULL},
    {NULL, "gamedate", PRODUCT_DATE, CVAR_ROM, 0, NULL},
    {&g_restarted, "g_restarted", "0", CVAR_ROM, 0, NULL},

    // latched vars
    {&g_gametype, "g_gametype", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH, 0, NULL},

    {&g_maxclients, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, NULL},
    {&g_maxGameClients, "g_maxGameClients", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, NULL},

    // change anytime vars
    {&g_dmflags, "dmflags", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_fraglimit, "fraglimit", "20", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, NULL},
    {&g_timelimit, "timelimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, NULL},
    {&g_capturelimit, "capturelimit", "8", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, NULL},

    {&g_synchronousClients, "g_synchronousClients", "0", CVAR_SYSTEMINFO, 0, NULL},

    {&g_friendlyFire, "g_friendlyFire", "0", CVAR_ARCHIVE, 0, NULL},

    {&g_teamAutoJoin, "g_teamAutoJoin", "0", CVAR_ARCHIVE},
    {&g_teamForceBalance, "g_teamForceBalance", "0", CVAR_SERVERINFO | CVAR_ARCHIVE},

    {&g_warmup, "g_warmup", "20", CVAR_ARCHIVE, 0, NULL},
    {&g_doWarmup, "g_doWarmup", "1", CVAR_ARCHIVE, 0, NULL},
    {&g_gameState, "g_gameState", "PRE_GAME", CVAR_ROM, 0, NULL},
    {&g_warmupReadyPercentage, "g_warmupReadyPercentage", "0.51", CVAR_ARCHIVE, 0, NULL},
    {&g_forfeit, "g_forfeit", "0", CVAR_ARCHIVE, 0, NULL},
    {&g_logfile, "g_log", "games.log", CVAR_ARCHIVE, 0, NULL},
    {&g_logfileSync, "g_logsync", "0", CVAR_ARCHIVE, 0, NULL},

    {&g_password, "g_password", "", CVAR_USERINFO, 0, NULL},

    {&g_banIPs, "g_banIPs", "", CVAR_ARCHIVE, 0, NULL},
    {&g_filterBan, "g_filterBan", "1", CVAR_ARCHIVE, 0, NULL},

    {&g_needpass, "g_needpass", "0", CVAR_SERVERINFO | CVAR_ROM, 0, NULL},

    {&g_dedicated, "dedicated", "0", 0, 0, NULL},

    {&g_speed, "g_speed", "320", 0, 0, NULL},
    {&g_gravity, "g_gravity", "800", CVAR_SERVERINFO, 0, NULL},
    {&g_knockback, "g_knockback", "1000", 0, 0, NULL},
    {&g_quadfactor, "g_quadfactor", "3", 0, 0, NULL},
    {&g_weaponRespawn, "g_weaponrespawn", "5", CVAR_SERVERINFO, 0, NULL},
    {&g_weaponTeamRespawn, "g_weaponTeamRespawn", "30", 0, 0, NULL},
    {&g_forcerespawn, "g_forcerespawn", "20", 0, 0, NULL},
    {&g_inactivity, "g_inactivity", "0", 0, 0, NULL},
    {&g_debugMove, "g_debugMove", "0", 0, 0, NULL},
    {&g_debugDamage, "g_debugDamage", "0", 0, 0, NULL},
    {&g_debugAlloc, "g_debugAlloc", "0", 0, 0, NULL},
    {&g_motd, "g_motd", "", 0, 0, NULL},
    {&g_blood, "com_blood", "1", 0, 0, NULL},

    {&g_podiumDist, "g_podiumDist", "80", 0, 0, NULL},
    {&g_podiumDrop, "g_podiumDrop", "70", 0, 0, NULL},

    {&g_allowVote, "g_allowVote", "1", 0, 0, NULL},
    {&g_allowVoteMidGame, "g_allowVoteMidGame", "0", 0, 0, NULL},
    {&g_allowSpecVote, "g_allowSpecVote", "0", 0, 0, NULL},
    {&g_voteFlags, "g_voteFlags", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_voteDelay, "g_voteDelay", "0", 0, 0, NULL},
    {&g_voteLimit, "g_voteLimit", "0", 0, 0, NULL},
    {&g_listEntity, "g_listEntity", "0", 0, 0, NULL},

    {&g_obeliskHealth, "g_obeliskHealth", "2500", 0, 0, NULL},
    {&g_obeliskRegenPeriod, "g_obeliskRegenPeriod", "1", 0, 0, NULL},
    {&g_obeliskRegenAmount, "g_obeliskRegenAmount", "15", 0, 0, NULL},
    {&g_obeliskRespawnDelay, "g_obeliskRespawnDelay", "10", CVAR_SERVERINFO, 0, NULL},

    {&g_cubeTimeout, "g_cubeTimeout", "30", 0, 0, NULL},

    {&g_enableDust, "g_enableDust", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_enableBreath, "g_enableBreath", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_proxMineTimeout, "g_proxMineTimeout", "20000", 0, 0, NULL},

    {&g_smoothClients, "g_smoothClients", "1", 0, 0, NULL},

    {&g_localTeamPref, "g_localTeamPref", "", 0, 0, NULL},

    {&g_infiniteAmmo, "g_infiniteAmmo", "0", CVAR_SYSTEMINFO, 0, NULL},
    {&g_dropFlag, "g_dropFlag", "7", 0, 0, NULL},  // [QL] bitmask: 1=flag, 2=powerup, 4=weapon
    {&g_runes, "g_runes", "0", 0, 0, NULL},  // [QL] enable rune dropping

    // [QL] per-weapon damage cvars
    {&g_damage_g, "g_damage_g", "50", 0, 0, NULL},
    {&g_damage_mg, "g_damage_mg", "5", 0, 0, NULL},
    {&g_damage_sg, "g_damage_sg", "5", 0, 0, NULL},
    {&g_damage_gl, "g_damage_gl", "100", 0, 0, NULL},
    {&g_damage_rl, "g_damage_rl", "100", 0, 0, NULL},
    {&g_damage_lg, "g_damage_lg", "6", 0, 0, NULL},
    {&g_damage_rg, "g_damage_rg", "80", 0, 0, NULL},
    {&g_damage_pg, "g_damage_pg", "20", 0, 0, NULL},
    {&g_damage_bfg, "g_damage_bfg", "100", 0, 0, NULL},
    {&g_damage_gh, "g_damage_gh", "10", 0, 0, NULL},
    {&g_damage_ng, "g_damage_ng", "12", 0, 0, NULL},
    {&g_damage_cg, "g_damage_cg", "8", 0, 0, NULL},
    {&g_damage_hmg, "g_damage_hmg", "8", 0, 0, NULL},
    {&g_damage_pl, "g_damage_pl", "0", 0, 0, NULL},

    // [QL] per-weapon knockback multiplier cvars
    {&g_knockback_g, "g_knockback_g", "1", 0, 0, NULL},
    {&g_knockback_mg, "g_knockback_mg", "1", 0, 0, NULL},
    {&g_knockback_sg, "g_knockback_sg", "1", 0, 0, NULL},
    {&g_knockback_gl, "g_knockback_gl", "1.10", 0, 0, NULL},
    {&g_knockback_rl, "g_knockback_rl", "0.90", 0, 0, NULL},
    {&g_knockback_rl_self, "g_knockback_rl_self", "1.10", 0, 0, NULL},
    {&g_knockback_lg, "g_knockback_lg", "1.75", 0, 0, NULL},
    {&g_knockback_rg, "g_knockback_rg", "0.85", 0, 0, NULL},
    {&g_knockback_pg, "g_knockback_pg", "1.10", 0, 0, NULL},
    {&g_knockback_pg_self, "g_knockback_pg_self", "1.30", 0, 0, NULL},
    {&g_knockback_bfg, "g_knockback_bfg", "1", 0, 0, NULL},
    {&g_knockback_gh, "g_knockback_gh", "-5", 0, 0, NULL},
    {&g_knockback_ng, "g_knockback_ng", "1", 0, 0, NULL},
    {&g_knockback_pl, "g_knockback_pl", "1", 0, 0, NULL},
    {&g_knockback_cg, "g_knockback_cg", "1", 0, 0, NULL},
    {&g_knockback_hmg, "g_knockback_hmg", "1", 0, 0, NULL},
    {&g_knockback_z, "g_knockback_z", "24", 0, 0, NULL},
    {&g_knockback_z_self, "g_knockback_z_self", "24", 0, 0, NULL},
    {&g_knockback_cripple, "g_knockback_cripple", "0", 0, 0, NULL},

    // [QL] weapon reload (fire interval in ms) cvars - callbacks update bg_weaponReloadTime[]
    {&weapon_reload_mg, "weapon_reload_mg", "100", 0, 0, OnChangedWeaponReloadMG},
    {&weapon_reload_sg, "weapon_reload_sg", "1000", 0, 0, OnChangedWeaponReloadSG},
    {&weapon_reload_gl, "weapon_reload_gl", "800", 0, 0, OnChangedWeaponReloadGL},
    {&weapon_reload_rl, "weapon_reload_rl", "800", 0, 0, OnChangedWeaponReloadRL},
    {&weapon_reload_lg, "weapon_reload_lg", "50", 0, 0, OnChangedWeaponReloadLG},
    {&weapon_reload_rg, "weapon_reload_rg", "1500", 0, 0, OnChangedWeaponReloadRG},
    {&weapon_reload_pg, "weapon_reload_pg", "100", 0, 0, OnChangedWeaponReloadPG},
    {&weapon_reload_bfg, "weapon_reload_bfg", "300", 0, 0, OnChangedWeaponReloadBFG},
    {&weapon_reload_gh, "weapon_reload_gh", "100", 0, 0, OnChangedWeaponReloadGH},
    {&weapon_reload_ng, "weapon_reload_ng", "1000", 0, 0, OnChangedWeaponReloadNG},
    {&weapon_reload_prox, "weapon_reload_prox", "800", 0, 0, OnChangedWeaponReloadProx},
    {&weapon_reload_cg, "weapon_reload_cg", "50", 0, 0, OnChangedWeaponReloadCG},
    {&weapon_reload_hmg, "weapon_reload_hmg", "75", 0, 0, OnChangedWeaponReloadHMG},

    // [QL] per-weapon splash damage cvars
    {&g_splashdamage_gl, "g_splashdamage_gl", "100", 0, 0, NULL},
    {&g_splashdamage_rl, "g_splashdamage_rl", "84", 0, 0, NULL},
    {&g_splashdamage_pg, "g_splashdamage_pg", "15", 0, 0, NULL},
    {&g_splashdamage_bfg, "g_splashdamage_bfg", "100", 0, 0, NULL},
    {&g_splashdamage_pl, "g_splashdamage_pl", "100", 0, 0, NULL},
    {&g_splashdamageOffset, "g_splashdamageOffset", "0.05", 0, 0, NULL},
    {&g_rocketsplashOffset, "g_rocketsplashOffset", "-10.0", 0, 0, NULL},

    // [QL] knockback cap
    {&g_max_knockback, "g_max_knockback", "120", 0, 0, NULL},

    // [QL] per-weapon splash radius cvars
    {&g_splashradius_gl, "g_splashradius_gl", "150", 0, 0, NULL},
    {&g_splashradius_rl, "g_splashradius_rl", "120", 0, 0, NULL},
    {&g_splashradius_pg, "g_splashradius_pg", "20", 0, 0, NULL},
    {&g_splashradius_bfg, "g_splashradius_bfg", "80", 0, 0, NULL},
    {&g_splashradius_pl, "g_splashradius_pl", "150", 0, 0, NULL},

    // [QL] projectile velocity cvars
    {&g_velocity_gl, "g_velocity_gl", "700", 0, 0, NULL},
    {&g_velocity_rl, "g_velocity_rl", "1000", 0, 0, NULL},
    {&g_velocity_pg, "g_velocity_pg", "2000", 0, 0, NULL},
    {&g_velocity_bfg, "g_velocity_bfg", "1800", 0, 0, NULL},
    {&g_velocity_gh, "g_velocity_gh", "1800", 0, 0, NULL},

    // [QL] projectile acceleration cvars
    {&g_accelFactor_rl, "g_accelFactor_rl", "1", 0, 0, NULL},
    {&g_accelFactor_pg, "g_accelFactor_pg", "1", 0, 0, NULL},
    {&g_accelFactor_bfg, "g_accelFactor_bfg", "1", 0, 0, NULL},
    {&g_accelRate_rl, "g_accelRate_rl", "16", 0, 0, NULL},
    {&g_accelRate_pg, "g_accelRate_pg", "16", 0, 0, NULL},
    {&g_accelRate_bfg, "g_accelRate_bfg", "16", 0, 0, NULL},

    // [QL] projectile gravity cvars
    {&weapon_gravity_rl, "weapon_gravity_rl", "0", 0, 0, NULL},
    {&weapon_gravity_pg, "weapon_gravity_pg", "0", 0, 0, NULL},
    {&weapon_gravity_bfg, "weapon_gravity_bfg", "0", 0, 0, NULL},
    {&weapon_gravity_ng, "weapon_gravity_ng", "0", 0, 0, NULL},

    // [QL] nailgun cvars
    {&g_nailspeed, "g_nailspeed", "1000", 0, 0, NULL},
    {&g_nailcount, "g_nailcount", "10", 0, 0, NULL},
    {&g_nailspread, "g_nailspread", "400", 0, 0, NULL},
    {&g_nailbounce, "g_nailbounce", "1", 0, 0, NULL},
    {&g_nailbouncepercentage, "g_nailbouncepercentage", "65", 0, 0, NULL},

    // [QL] guided rocket cvar
    {&g_guidedRocket, "g_guidedRocket", "0", 0, 0, NULL},

    // [QL] loadout system
    {&g_loadout, "g_loadout", "0", CVAR_SERVERINFO, 0, NULL},

    // [QL] ammo system
    {&g_ammoPack, "g_ammoPack", "0", 0, 0, NULL},
    {&g_ammoRespawn, "g_ammoRespawn", "40", 0, 0, NULL},
    {&g_spawnItemAmmo, "g_spawnItemAmmo", "1", 0, 0, NULL},

    // [QL] serverinfo cvars read by cgame (all verified with CVAR_SERVERINFO in binary)
    {&g_teamsize, "teamsize", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_teamSizeMin, "g_teamSizeMin", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_overtime, "g_overtime", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_scorelimit, "scorelimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, NULL},
    {&g_mercylimit, "mercylimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, NULL},
    {&g_mercytime, "g_mercytime", "0", 0, 0, NULL},
    {&g_rrAllowNegativeScores, "g_rrAllowNegativeScores", "0", 0, 0, NULL},
    {&g_spawnItems, "g_spawnItems", "", 0, 0, NULL},
    {&sv_quitOnExitLevel, "sv_quitOnExitLevel", "0", 0, 0, NULL},
    {&g_isBotOnly, "g_isBotOnly", "0", 0, 0, NULL},
    {&g_training, "g_training", "0", 0, 0, NULL},
    {&g_itemHeight, "g_itemHeight", "35", CVAR_SERVERINFO, 0, NULL},
    {&g_itemTimers, "g_itemTimers", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_quadDamageFactor, "g_quadDamageFactor", "3", CVAR_SERVERINFO, 0, NULL},
    {&g_freezeRoundDelay, "g_freezeRoundDelay", "0", CVAR_SERVERINFO, 0, NULL},
    {&g_timeoutCount, "g_timeoutCount", "0", CVAR_SERVERINFO, 0, NULL},

    // [QL] starting loadout cvars
    {&g_startingWeapons, "g_startingWeapons", "3", 0, 0, OnChangedStartingWeapons},
    {&g_startingHealth, "g_startingHealth", "100", CVAR_SERVERINFO, 0, NULL},
    {&g_startingHealthBonus, "g_startingHealthBonus", "25", 0, 0, NULL},
    {&g_startingArmor, "g_startingArmor", "0", 0, 0, NULL},
    {&g_startingAmmo_g, "g_startingAmmo_g", "-1", 0, 0, NULL},
    {&g_startingAmmo_mg, "g_startingAmmo_mg", "100", 0, 0, NULL},
    {&g_startingAmmo_sg, "g_startingAmmo_sg", "10", 0, 0, NULL},
    {&g_startingAmmo_gl, "g_startingAmmo_gl", "10", 0, 0, NULL},
    {&g_startingAmmo_rl, "g_startingAmmo_rl", "5", 0, 0, NULL},
    {&g_startingAmmo_lg, "g_startingAmmo_lg", "100", 0, 0, NULL},
    {&g_startingAmmo_rg, "g_startingAmmo_rg", "5", 0, 0, NULL},
    {&g_startingAmmo_pg, "g_startingAmmo_pg", "50", 0, 0, NULL},
    {&g_startingAmmo_bfg, "g_startingAmmo_bfg", "10", 0, 0, NULL},
    {&g_startingAmmo_gh, "g_startingAmmo_gh", "-1", 0, 0, NULL},
    {&g_startingAmmo_ng, "g_startingAmmo_ng", "10", 0, 0, NULL},
    {&g_startingAmmo_pl, "g_startingAmmo_pl", "5", 0, 0, NULL},
    {&g_startingAmmo_cg, "g_startingAmmo_cg", "100", 0, 0, NULL},
    {&g_startingAmmo_hmg, "g_startingAmmo_hmg", "50", 0, 0, NULL},

    // [QL] movement physics cvars - configurable via factories/rcon
    {&pmove_JumpVelocity, "pmove_JumpVelocity", "270", 0, 0, OnChangedPmoveJumpVelocity},
    {&pmove_JumpVelocityMax, "pmove_JumpVelocityMax", "270", 0, 0, OnChangedPmoveJumpVelocityMax},
    {&pmove_JumpVelocityScaleAdd, "pmove_JumpVelocityScaleAdd", "0", 0, 0, OnChangedPmoveJumpVelocityScaleAdd},
    {&pmove_JumpVelocityTimeThreshold, "pmove_JumpVelocityTimeThreshold", "0", 0, 0, OnChangedPmoveJumpVelocityTimeThreshold},
    {&pmove_JumpVelocityTimeThresholdOffset, "pmove_JumpVelocityTimeThresholdOffset", "0", 0, 0, OnChangedPmoveJumpVelocityTimeThresholdOffset},
    {&pmove_ChainJump, "pmove_ChainJump", "1", 0, 0, OnChangedPmoveChainJump},
    {&pmove_ChainJumpVelocity, "pmove_ChainJumpVelocity", "0", 0, 0, OnChangedPmoveChainJumpVelocity},
    {&pmove_StepJumpVelocity, "pmove_StepJumpVelocity", "0", 0, 0, OnChangedPmoveStepJumpVelocity},
    {&pmove_JumpTimeDeltaMin, "pmove_JumpTimeDeltaMin", "0", 0, 0, OnChangedPmoveJumpTimeDeltaMin},
    {&pmove_RampJump, "pmove_RampJump", "0", 0, 0, OnChangedPmoveRampJump},
    {&pmove_RampJumpScale, "pmove_RampJumpScale", "1", 0, 0, OnChangedPmoveRampJumpScale},
    {&pmove_StepJump, "pmove_StepJump", "0", 0, 0, OnChangedPmoveStepJump},
    {&pmove_CrouchStepJump, "pmove_CrouchStepJump", "0", 0, 0, OnChangedPmoveCrouchStepJump},
    {&pmove_AutoHop, "pmove_AutoHop", "0", 0, 0, OnChangedPmoveAutoHop},
    {&pmove_BunnyHop, "pmove_BunnyHop", "0", 0, 0, OnChangedPmoveBunnyHop},
    {&pmove_AirAccel, "pmove_AirAccel", "1", 0, 0, OnChangedPmoveAirAccel},
    {&pmove_AirStopAccel, "pmove_AirStopAccel", "1", 0, 0, OnChangedPmoveAirStopAccel},
    {&pmove_AirControl, "pmove_AirControl", "0", 0, 0, OnChangedPmoveAirControl},
    {&pmove_StrafeAccel, "pmove_StrafeAccel", "1", 0, 0, OnChangedPmoveStrafeAccel},
    {&pmove_WalkAccel, "pmove_WalkAccel", "10", 0, 0, OnChangedPmoveWalkAccel},
    {&pmove_WalkFriction, "pmove_WalkFriction", "6", 0, 0, OnChangedPmoveWalkFriction},
    {&pmove_CircleStrafeFriction, "pmove_CircleStrafeFriction", "6", 0, 0, OnChangedPmoveCircleStrafeFriction},
    {&pmove_CrouchSlideFriction, "pmove_CrouchSlideFriction", "0", 0, 0, OnChangedPmoveCrouchSlideFriction},
    {&pmove_CrouchSlideTime, "pmove_CrouchSlideTime", "0", 0, 0, OnChangedPmoveCrouchSlideTime},
    {&pmove_AirSteps, "pmove_AirSteps", "0", 0, 0, OnChangedPmoveAirSteps},
    {&pmove_AirStepFriction, "pmove_AirStepFriction", "0", 0, 0, OnChangedPmoveAirStepFriction},
    {&pmove_StepHeight, "pmove_StepHeight", "18", 0, 0, OnChangedPmoveStepHeight},
    {&pmove_WaterSwimScale, "pmove_WaterSwimScale", "0.5", 0, 0, OnChangedPmoveWaterSwimScale},
    {&pmove_WaterWadeScale, "pmove_WaterWadeScale", "0.75", 0, 0, OnChangedPmoveWaterWadeScale},
    {&pmove_WishSpeed, "pmove_WishSpeed", "400", 0, 0, OnChangedPmoveWishSpeed},
    {&pmove_WeaponDropTime, "pmove_WeaponDropTime", "200", 0, 0, OnChangedPmoveWeaponDropTime},
    {&pmove_WeaponRaiseTime, "pmove_WeaponRaiseTime", "250", 0, 0, OnChangedPmoveWeaponRaiseTime},
    {&pmove_NoPlayerClip, "pmove_NoPlayerClip", "0", 0, 0, OnChangedPmoveNoPlayerClip},
    {&pmove_HookPullVelocity, "pmove_HookPullVelocity", "800", 0, 0, OnChangedPmoveHookPullVelocity},
    {&pmove_DoubleJump, "pmove_DoubleJump", "0", 0, 0, OnChangedPmoveDoubleJump},
    {&pmove_CrouchSlide, "pmove_CrouchSlide", "0", 0, 0, OnChangedPmoveCrouchSlide},
    {&pmove_VelocityGH, "pmove_velocity_gh", "800", 0, 0, OnChangedPmoveVelocityGH},

    // [QL] damage-through-surface
    {&g_forceDmgThroughSurface, "g_forceDmgThroughSurface", "0", 0, 0, NULL},
    {&g_dmgThroughSurfaceDistance, "g_dmgThroughSurfaceDistance", "-33.1", 0, 0, NULL},
    {&g_dmgThroughSurfaceDampening, "g_dmgThroughSurfaceDampening", "0.5", 0, 0, NULL},
    {&g_dmgThroughSurfaceAngularThreshold, "g_dmgThroughSurfaceAngularThreshold", "0.5", 0, 0, NULL},

    // [QL] player cylinders
    {&g_playerCylinders, "g_playerCylinders", "1", CVAR_INIT, 0, NULL},
    {&sv_fps, "sv_fps", "40", CVAR_ARCHIVE, 0, NULL},

    // [QL] round-based cvars
    {&roundtimelimit, "roundtimelimit", "180", CVAR_SERVERINFO | CVAR_NORESTART, 0, NULL},
    {&g_roundWarmupDelay, "g_roundWarmupDelay", "10000", CVAR_SERVERINFO, 0, NULL},
    {&roundlimit, "roundlimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, NULL},
    {&g_accuracyFlags, "g_accuracyFlags", "0", 0, 0, NULL},
    {&g_lastManStandingWarning, "g_lastManStandingWarning", "0", 0, 0, NULL},
    {&g_roundDrawLivingCount, "g_roundDrawLivingCount", "1", 0, 0, NULL},
    {&g_roundDrawHealthCount, "g_roundDrawHealthCount", "1", 0, 0, NULL},
    {&g_spawnArmor, "g_spawnArmor", "0", 0, 0, NULL},
    {&g_adElimScoreBonus, "g_adElimScoreBonus", "1", CVAR_SERVERINFO, 0, NULL},

    // [QL] freeze tag cvars
    {&g_freezeThawTick, "g_freezeThawTick", "1", CVAR_INIT, 0, NULL},
    {&g_freezeProtectedSpawnTime, "g_freezeProtectedSpawnTime", "0", CVAR_INIT, 0, NULL},
    {&g_freezeThawTime, "g_freezeThawTime", "2000", 0, 0, NULL},
    {&g_freezeAutoThawTime, "g_freezeAutoThawTime", "120000", 0, 0, NULL},
    {&g_freezeThawRadius, "g_freezeThawRadius", "96", 0, 0, NULL},
    {&g_freezeThawThroughSurface, "g_freezeThawThroughSurface", "0", 0, 0, NULL},
    {&g_freezeThawWinningTeam, "g_freezeThawWinningTeam", "1", 0, 0, NULL},
    {&g_freezeRemovePowerupsOnRound, "g_freezeRemovePowerupsOnRound", "1", 0, 0, NULL},
    {&g_freezeResetHealthOnRound, "g_freezeResetHealthOnRound", "1", 0, 0, NULL},
    {&g_freezeResetArmorOnRound, "g_freezeResetArmorOnRound", "1", 0, 0, NULL},
    {&g_freezeResetWeaponsOnRound, "g_freezeResetWeaponsOnRound", "1", 0, 0, NULL},
    {&g_freezeAllowRespawn, "g_freezeAllowRespawn", "0", 0, 0, NULL},
    {&g_freezeRoundDelay, "g_freezeRoundDelay", "0", CVAR_SERVERINFO, 0, NULL},

    // [QL] lag compensation / weapon modifiers
    {&g_lagHaxHistory, "g_lagHaxHistory", "4", CVAR_LATCH, 0, NULL},
    {&g_lagHaxMs, "g_lagHaxMs", "80", 0, 0, NULL},
    {&g_ironsights_mg, "g_ironsights_mg", "1.0", CVAR_TEMP | CVAR_SERVERINFO, 0, NULL},

    // [QL] Quad Hog
    {&g_quadHog, "g_quadHog", "0", 0, 0, NULL},
    {&g_quadHogTime, "g_quadHogTime", "30", 0, 0, NULL},
    {&g_damagePlums, "g_damagePlums", "1", CVAR_SERVERINFO, 0, NULL},

    // [QL] per-weapon advanced
    {&g_damage_sg_outer, "g_damage_sg_outer", "5", 0, 0, NULL},
    {&g_damage_sg_falloff, "g_damage_sg_falloff", "0", 0, 0, NULL},
    {&g_range_sg_falloff, "g_range_sg_falloff", "768", 0, 0, NULL},
    {&g_damage_lg_falloff, "g_damage_lg_falloff", "0", 0, 0, NULL},
    {&g_range_lg_falloff, "g_range_lg_falloff", "768", 0, 0, NULL},
    {&g_headShotDamage_rg, "g_headShotDamage_rg", "0", 0, 0, NULL},
    {&g_railJump, "g_railJump", "0", 0, 0, NULL},

    // [QL] RR infection mode
    {&g_rrInfectedSpreadTime, "g_rrInfectedSpreadTime", "40", 0, 0, NULL},
    {&g_rrInfectedSpreadWarningTime, "g_rrInfectedSpreadWarningTime", "10", 0, 0, NULL},
    {&g_rrInfectedSurvivorScoreMethod, "g_rrInfectedSurvivorScoreMethod", "2", 0, 0, NULL},
    {&g_rrInfectedSurvivorScoreRate, "g_rrInfectedSurvivorScoreRate", "30", 0, 0, NULL},
    {&g_rrInfectedSurvivorScoreBonus, "g_rrInfectedSurvivorScoreBonus", "1", 0, 0, NULL},

    // [QL] tiered armor
    {&armor_tiered, "armor_tiered", "0", 0, 0, NULL},
    {&g_startingArmor, "g_startingArmor", "0", 0, 0, NULL},

    // NULL terminator
    {NULL, NULL, NULL, 0, 0, NULL}
};

static int gameCvarTableSize = ARRAY_LEN(gameCvarTable);

void G_InitGame(int levelTime, int randomSeed, int restart);
void G_RunFrame(int levelTime);
void G_ShutdownGame(int restart);
void CheckExitRules(void);

// [QL] dllEntry - module entry point
// The engine calls this when loading the game module.
// QL uses a function pointer table instead of Q3's vmMain dispatcher.
Q_EXPORT void dllEntry(void **vmMainOut, gameImport_t *syscallTable, int *apiVersion) {
    imports = syscallTable;

    vmMainOut[GAME_SHUTDOWN]                = (void *)G_ShutdownGame;
    vmMainOut[GAME_RUN_FRAME]               = (void *)G_RunFrame;
    vmMainOut[GAME_REGISTER_CVARS]          = (void *)G_RegisterCvars;
    vmMainOut[GAME_INIT]                    = (void *)G_InitGame;
    vmMainOut[GAME_CONSOLE_COMMAND]         = (void *)ConsoleCommand;
    vmMainOut[GAME_CLIENT_USERINFO_CHANGED] = (void *)ClientUserinfoChanged;
    vmMainOut[GAME_CLIENT_THINK]            = (void *)ClientThink;
    vmMainOut[GAME_CLIENT_DISCONNECT]       = (void *)ClientDisconnect;
    vmMainOut[GAME_CLIENT_CONNECT]          = (void *)ClientConnect;
    vmMainOut[GAME_CLIENT_COMMAND]          = (void *)ClientCommand;

    *apiVersion = GAME_API_VERSION;
}

void QDECL G_Printf(const char* fmt, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    trap_Print(text);
}

void QDECL G_Error(const char* fmt, ...) {
    va_list argptr;
    char text[1024];

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    trap_Error(text);
}

/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMSLAVE flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams(void) {
    gentity_t *e, *e2;
    int i, j;
    int c, c2;

    c = 0;
    c2 = 0;
    for (i = MAX_CLIENTS, e = g_entities + i; i < level.num_entities; i++, e++) {
        if (!e->inuse)
            continue;
        if (!e->team)
            continue;
        if (e->flags & FL_TEAMSLAVE)
            continue;
        e->teammaster = e;
        c++;
        c2++;
        for (j = i + 1, e2 = e + 1; j < level.num_entities; j++, e2++) {
            if (!e2->inuse)
                continue;
            if (!e2->team)
                continue;
            if (e2->flags & FL_TEAMSLAVE)
                continue;
            if (!strcmp(e->team, e2->team)) {
                c2++;
                e2->teamchain = e->teamchain;
                e->teamchain = e2;
                e2->teammaster = e;
                e2->flags |= FL_TEAMSLAVE;

                // make sure that targets only point at the master
                if (e2->targetname) {
                    e->targetname = e2->targetname;
                    e2->targetname = NULL;
                }
            }
        }
    }

    G_Printf("%i teams with %i entities\n", c, c2);
}

void G_RemapTeamShaders(void) {
    return;  // FIXME errors about Pagans/Stroggs files not present in new PAK00
    char string[1024];
    float f = level.time * 0.001;
    Com_sprintf(string, sizeof(string), "team_icon/%s_red", g_redteam.string);
    AddRemap("textures/ctf2/redteam01", string, f);
    AddRemap("textures/ctf2/redteam03", string, f);
    Com_sprintf(string, sizeof(string), "team_icon/%s_blue", g_blueteam.string);
    AddRemap("textures/ctf2/blueteam01", string, f);
    AddRemap("textures/ctf2/blueteam03", string, f);
    trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
}

/*
=================
G_RegisterCvars
=================
*/
void G_RegisterCvars(void) {
    int i;
    cvarTable_t *cv;

    for (i = 0, cv = gameCvarTable; cv->cvarName != NULL; i++, cv++) {
        trap_Cvar_Register(cv->vmCvar, cv->cvarName,
                           cv->defaultString, cv->cvarFlags);
        if (cv->vmCvar)
            cv->modCount = cv->vmCvar->modificationCount;
    }
}

/*
=================
G_UpdateCvars
=================
*/
void G_UpdateCvars(void) {
    int i;
    cvarTable_t *cv;

    for (i = 0, cv = gameCvarTable; cv->cvarName != NULL; i++, cv++) {
        if (cv->vmCvar) {
            trap_Cvar_Update(cv->vmCvar);

            if (cv->modCount != cv->vmCvar->modificationCount) {
                cv->modCount = cv->vmCvar->modificationCount;

                if (cv->onChanged) {
                    cv->onChanged();
                }
            }
        }
    }
}

/*
=================
G_SendPmoveInfo

[QL] Build and send CS_PMOVEINFO configstring so the client can parse
all pmove_* parameters for client-side prediction.
=================
*/
static void G_SendPmoveInfo(void) {
    char info[MAX_INFO_STRING];

    info[0] = '\0';

    Info_SetValueForKey(info, "pmove_AirAccel", va("%g", pmove_AirAccel.value));
    Info_SetValueForKey(info, "pmove_AirStepFriction", va("%g", pmove_AirStepFriction.value));
    Info_SetValueForKey(info, "pmove_AirSteps", va("%i", pmove_AirSteps.integer));
    Info_SetValueForKey(info, "pmove_AirStopAccel", va("%g", pmove_AirStopAccel.value));
    Info_SetValueForKey(info, "pmove_AirControl", va("%g", pmove_AirControl.value));
    Info_SetValueForKey(info, "pmove_AutoHop", va("%i", pmove_AutoHop.integer));
    Info_SetValueForKey(info, "pmove_BunnyHop", va("%i", pmove_BunnyHop.integer));
    Info_SetValueForKey(info, "pmove_ChainJump", va("%i", pmove_ChainJump.integer));
    Info_SetValueForKey(info, "pmove_ChainJumpVelocity", va("%g", pmove_ChainJumpVelocity.value));
    Info_SetValueForKey(info, "pmove_CircleStrafeFriction", va("%g", pmove_CircleStrafeFriction.value));
    Info_SetValueForKey(info, "pmove_CrouchSlideFriction", va("%g", pmove_CrouchSlideFriction.value));
    Info_SetValueForKey(info, "pmove_CrouchSlideTime", va("%i", pmove_CrouchSlideTime.integer));
    Info_SetValueForKey(info, "pmove_CrouchSlide", va("%i", pmove_CrouchSlide.integer));
    Info_SetValueForKey(info, "pmove_CrouchStepJump", va("%i", pmove_CrouchStepJump.integer));
    Info_SetValueForKey(info, "pmove_DoubleJump", va("%i", pmove_DoubleJump.integer));
    Info_SetValueForKey(info, "pmove_JumpTimeDeltaMin", va("%g", pmove_JumpTimeDeltaMin.value));
    Info_SetValueForKey(info, "pmove_JumpVelocity", va("%g", pmove_JumpVelocity.value));
    Info_SetValueForKey(info, "pmove_JumpVelocityMax", va("%g", pmove_JumpVelocityMax.value));
    Info_SetValueForKey(info, "pmove_JumpVelocityScaleAdd", va("%g", pmove_JumpVelocityScaleAdd.value));
    Info_SetValueForKey(info, "pmove_JumpVelocityTimeThreshold", va("%g", pmove_JumpVelocityTimeThreshold.value));
    Info_SetValueForKey(info, "pmove_JumpVelocityTimeThresholdOffset", va("%g", pmove_JumpVelocityTimeThresholdOffset.value));
    Info_SetValueForKey(info, "pmove_noPlayerClip", va("%i", pmove_NoPlayerClip.integer));
    Info_SetValueForKey(info, "pmove_RampJump", va("%i", pmove_RampJump.integer));
    Info_SetValueForKey(info, "pmove_RampJumpScale", va("%g", pmove_RampJumpScale.value));
    Info_SetValueForKey(info, "pmove_StepHeight", va("%g", pmove_StepHeight.value));
    Info_SetValueForKey(info, "pmove_StepJump", va("%i", pmove_StepJump.integer));
    Info_SetValueForKey(info, "pmove_StepJumpVelocity", va("%g", pmove_StepJumpVelocity.value));
    Info_SetValueForKey(info, "pmove_StrafeAccel", va("%g", pmove_StrafeAccel.value));
    Info_SetValueForKey(info, "pmove_velocity_gh", va("%g", pmove_VelocityGH.value));
    Info_SetValueForKey(info, "pmove_WalkAccel", va("%g", pmove_WalkAccel.value));
    Info_SetValueForKey(info, "pmove_WalkFriction", va("%g", pmove_WalkFriction.value));
    Info_SetValueForKey(info, "pmove_WaterSwimScale", va("%g", pmove_WaterSwimScale.value));
    Info_SetValueForKey(info, "pmove_WaterWadeScale", va("%g", pmove_WaterWadeScale.value));
    Info_SetValueForKey(info, "pmove_WeaponRaiseTime", va("%i", pmove_WeaponRaiseTime.integer));
    Info_SetValueForKey(info, "pmove_WeaponDropTime", va("%i", pmove_WeaponDropTime.integer));
    Info_SetValueForKey(info, "pmove_WishSpeed", va("%g", pmove_WishSpeed.value));

    trap_SetConfigstring(CS_PMOVEINFO, info);
}

/*
============
G_InitGame

============
*/
void G_InitGame(int levelTime, int randomSeed, int restart) {
    int i;

    G_Printf("------- Game Initialization -------\n");
    G_Printf("gamename: %s\n", GAMEVERSION);
    G_Printf("gamedate: %s\n", PRODUCT_DATE);

    srand(randomSeed);

    G_RegisterCvars();

    G_ProcessIPBans();

    G_InitMemory();

    // set some level globals
    memset(&level, 0, sizeof(level));
    level.time = levelTime;
    level.startTime = levelTime;
    level.warmupTime = -1;  // PRE_GAME until warmup state machine transitions

    level.snd_fry = G_SoundIndex("sound/player/fry.wav");  // FIXME standing in lava / slime

    if (g_logfile.string[0]) {
        if (g_logfileSync.integer) {
            trap_FS_FOpenFile(g_logfile.string, &level.logFile, FS_APPEND_SYNC);
        } else {
            trap_FS_FOpenFile(g_logfile.string, &level.logFile, FS_APPEND);
        }
        if (!level.logFile) {
            G_Printf("WARNING: Couldn't open logfile: %s\n", g_logfile.string);
        } else {
            char serverinfo[MAX_INFO_STRING];

            trap_GetServerinfo(serverinfo, sizeof(serverinfo));

            G_LogPrintf("------------------------------------------------------------\n");
            G_LogPrintf("InitGame: %s\n", serverinfo);
        }
    } else {
        G_Printf("Not logging to disk.\n");
    }

    G_InitWorldSession();

    // initialize all entities for this game
    memset(g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]));
    level.gentities = g_entities;

    // initialize all clients for this game
    level.maxclients = g_maxclients.integer;
    memset(g_clients, 0, MAX_CLIENTS * sizeof(g_clients[0]));
    level.clients = g_clients;

    // set client fields on player ents
    for (i = 0; i < level.maxclients; i++) {
        g_entities[i].client = level.clients + i;
    }

    // always leave room for the max number of clients,
    // even if they aren't all used, so numbers inside that
    // range are NEVER anything but clients
    level.num_entities = MAX_CLIENTS;

    for (i = 0; i < MAX_CLIENTS; i++) {
        g_entities[i].classname = "clientslot";
    }

    // let the server system know where the entites are
    trap_LocateGameData(level.gentities, level.num_entities, sizeof(gentity_t),
                        &level.clients[0].ps, sizeof(level.clients[0]));

    // reserve some spots for dead player bodies
    InitBodyQue();

    ClearRegisteredItems();

    // parse the key/value pairs and spawn gentities
    G_SpawnEntitiesFromString();

    // general initialization
    G_FindTeams();

    // [QL] Lag compensation init
    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        HAX_Init();
    }

    // make sure we have flags for CTF, etc
    if (g_gametype.integer >= GT_TEAM) {
        G_CheckTeamItems();
    }

    SaveRegisteredItems();

    G_Printf("-----------------------------------\n");

    if (trap_Cvar_VariableIntegerValue("bot_enable")) {
        BotAISetup(restart);
        BotAILoadMap(restart);
        G_InitBots(restart);
    }

    G_RemapTeamShaders();

    trap_SetConfigstring(CS_INTERMISSION, "");

    // [QL] publish starting weapons bitmask
    trap_SetConfigstring(CS_STARTING_WEAPONS, va("%i", g_startingWeapons.integer));

    // [QL] send pmove parameters to clients for prediction
    G_SendPmoveInfo();
}

/*
=================
G_ShutdownGame
=================
*/
void G_ShutdownGame(int restart) {
    G_Printf("==== ShutdownGame ====\n");

    if (level.logFile) {
        G_LogPrintf("ShutdownGame:\n");
        G_LogPrintf("------------------------------------------------------------\n");
        trap_FS_FCloseFile(level.logFile);
        level.logFile = 0;
    }

    // write all the client session data so we can get it back
    G_WriteSessionData();

    if (trap_Cvar_VariableIntegerValue("bot_enable")) {
        BotAIShutdown(restart);
    }
}

//===================================================================

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
========================================================================

PLAYER COUNTING / SCORE SORTING

========================================================================
*/

/*
=============
AddTournamentPlayer

If there are less than two tournament players, put a
spectator in the game and restart
=============
*/
void AddTournamentPlayer(void) {
    int i;
    gclient_t* client;
    gclient_t* nextInLine;

    if (level.numPlayingClients >= 2) {
        return;
    }

    // never change during intermission
    if (level.intermissionTime) {
        return;
    }

    nextInLine = NULL;

    for (i = 0; i < level.maxclients; i++) {
        client = &level.clients[i];
        if (client->pers.connected != CON_CONNECTED) {
            continue;
        }
        if (client->sess.sessionTeam != TEAM_SPECTATOR) {
            continue;
        }
        // never select the dedicated follow or scoreboard clients
        if (client->sess.spectatorState == SPECTATOR_SCOREBOARD ||
            client->sess.spectatorClient < 0) {
            continue;
        }

        if (!nextInLine || client->sess.spectatorTime < nextInLine->sess.spectatorTime)
            nextInLine = client;
    }

    if (!nextInLine) {
        return;
    }

    level.warmupTime = -1;

    // set them to free-for-all team
    SetTeam(&g_entities[nextInLine - level.clients], "f");
}

/*
=======================
AddTournamentQueue

Add client to end of tournament queue
=======================
*/

void AddTournamentQueue(gclient_t* client) {
    int index;
    gclient_t* curclient;

    for (index = 0; index < level.maxclients; index++) {
        curclient = &level.clients[index];

        if (curclient->pers.connected != CON_DISCONNECTED) {
            if (curclient == client)
                curclient->sess.spectatorTime = 0;
            else if (curclient->sess.sessionTeam == TEAM_SPECTATOR)
                curclient->sess.spectatorTime++;
        }
    }
}

/*
=======================
RemoveTournamentLoser

Make the loser a spectator at the back of the line
=======================
*/
void RemoveTournamentLoser(void) {
    int clientNum;

    if (level.numPlayingClients != 2) {
        return;
    }

    clientNum = level.sortedClients[1];

    if (level.clients[clientNum].pers.connected != CON_CONNECTED) {
        return;
    }

    // make them a spectator
    SetTeam(&g_entities[clientNum], "s");
}

/*
=======================
RemoveTournamentWinner
=======================
*/
void RemoveTournamentWinner(void) {
    int clientNum;

    if (level.numPlayingClients != 2) {
        return;
    }

    clientNum = level.sortedClients[0];

    if (level.clients[clientNum].pers.connected != CON_CONNECTED) {
        return;
    }

    // make them a spectator
    SetTeam(&g_entities[clientNum], "s");
}

/*
=======================
AdjustTournamentScores
=======================
*/
void AdjustTournamentScores(void) {
    int clientNum;

    clientNum = level.sortedClients[0];
    if (level.clients[clientNum].pers.connected == CON_CONNECTED) {
        level.clients[clientNum].sess.wins++;
        ClientUserinfoChanged(clientNum);
    }

    clientNum = level.sortedClients[1];
    if (level.clients[clientNum].pers.connected == CON_CONNECTED) {
        level.clients[clientNum].sess.losses++;
        ClientUserinfoChanged(clientNum);
    }
}

/*
=============
SortRanks

=============
*/
int QDECL SortRanks(const void* a, const void* b) {
    gclient_t *ca, *cb;

    ca = &level.clients[*(int*)a];
    cb = &level.clients[*(int*)b];

    // sort special clients last
    if (ca->sess.spectatorState == SPECTATOR_SCOREBOARD || ca->sess.spectatorClient < 0) {
        return 1;
    }
    if (cb->sess.spectatorState == SPECTATOR_SCOREBOARD || cb->sess.spectatorClient < 0) {
        return -1;
    }

    // then connecting clients
    if (ca->pers.connected == CON_CONNECTING) {
        return 1;
    }
    if (cb->pers.connected == CON_CONNECTING) {
        return -1;
    }

    // then spectators
    if (ca->sess.sessionTeam == TEAM_SPECTATOR && cb->sess.sessionTeam == TEAM_SPECTATOR) {
        if (ca->sess.spectatorTime > cb->sess.spectatorTime) {
            return -1;
        }
        if (ca->sess.spectatorTime < cb->sess.spectatorTime) {
            return 1;
        }
        return 0;
    }
    if (ca->sess.sessionTeam == TEAM_SPECTATOR) {
        return 1;
    }
    if (cb->sess.sessionTeam == TEAM_SPECTATOR) {
        return -1;
    }

    // then sort by score
    if (ca->ps.persistant[PERS_SCORE] > cb->ps.persistant[PERS_SCORE]) {
        return -1;
    }
    if (ca->ps.persistant[PERS_SCORE] < cb->ps.persistant[PERS_SCORE]) {
        return 1;
    }
    return 0;
}

/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every client connect, begin, disconnect, death,
and team change.
============
*/
void CalculateRanks(void) {
    int i;
    int rank;
    int score;
    int newScore;
    gclient_t* cl;

    level.follow1 = -1;
    level.follow2 = -1;
    level.numConnectedClients = 0;
    level.numNonSpectatorClients = 0;
    level.numPlayingClients = 0;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected != CON_DISCONNECTED) {
            level.sortedClients[level.numConnectedClients] = i;
            level.numConnectedClients++;

            if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR) {
                level.numNonSpectatorClients++;

                // decide if this should be auto-followed
                if (level.clients[i].pers.connected == CON_CONNECTED) {
                    level.numPlayingClients++;
                    if (level.follow1 == -1) {
                        level.follow1 = i;
                    } else if (level.follow2 == -1) {
                        level.follow2 = i;
                    }
                }
            }
        }
    }

    qsort(level.sortedClients, level.numConnectedClients,
          sizeof(level.sortedClients[0]), SortRanks);

    // set the rank value for all clients that are connected and not spectators
    if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        // [QL] team games: rank is team order, plus serverRank/teamRank tracking
        int teamLastIdx[4] = { -1, -1, -1, -1 };
        int teamLastScore[4] = { 0, 0, 0, 0 };
        int teamRankCounter[4] = { -1, -1, -1, -1 };
        rank = -1;
        score = 0;

        for (i = 0; i < level.numConnectedClients; i++) {
            int ci = level.sortedClients[i];
            cl = &level.clients[ci];

            // team rank (PERS_RANK): red=0, blue=1, tied=2
            if (level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE]) {
                cl->ps.persistant[PERS_RANK] = 2;
            } else if (level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE]) {
                cl->ps.persistant[PERS_RANK] = 0;
            } else {
                cl->ps.persistant[PERS_RANK] = 1;
            }

            if (cl->sess.sessionTeam == TEAM_SPECTATOR)
                continue;

            newScore = cl->ps.persistant[PERS_SCORE];

            // [QL] serverRank: overall ranking among all non-spectator players
            if (i == 0 || newScore != score) {
                rank++;
                cl->expandedStats.serverRank = rank;
                cl->expandedStats.serverRankIsTied = qfalse;
            } else {
                // tied with previous
                if (i > 0) {
                    level.clients[level.sortedClients[i - 1]].expandedStats.serverRankIsTied = qtrue;
                }
                cl->expandedStats.serverRank = rank;
                cl->expandedStats.serverRankIsTied = qtrue;
            }

            // [QL] teamRank: ranking within own team
            {
                int t = cl->sess.sessionTeam;
                if (teamLastIdx[t] == -1 || newScore != teamLastScore[t]) {
                    teamRankCounter[t]++;
                    cl->expandedStats.teamRank = teamRankCounter[t];
                    cl->expandedStats.teamRankIsTied = qfalse;
                } else {
                    level.clients[level.sortedClients[teamLastIdx[t]]].expandedStats.teamRankIsTied = qtrue;
                    cl->expandedStats.teamRank = teamRankCounter[t];
                    cl->expandedStats.teamRankIsTied = qtrue;
                }
                teamLastIdx[t] = i;
                teamLastScore[t] = newScore;
            }

            score = newScore;
        }
    } else {
        // [QL] FFA/Duel/Race/RR: rank by score, with race sentinel
        int raceSentinel = (g_gametype.integer == GT_RACE) ? 0x7FFFFFFF : 0;
        rank = -1;
        score = raceSentinel;
        for (i = 0; i < level.numPlayingClients; i++) {
            int ci = level.sortedClients[i];
            cl = &level.clients[ci];
            newScore = cl->ps.persistant[PERS_SCORE];
            if (i == 0 || newScore != score) {
                rank = i;
                cl->ps.persistant[PERS_RANK] = rank;
                cl->expandedStats.serverRank = rank;
                cl->expandedStats.serverRankIsTied = qfalse;
            } else {
                // tied with previous
                if (i > 0) {
                    level.clients[level.sortedClients[i - 1]].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
                    level.clients[level.sortedClients[i - 1]].expandedStats.serverRankIsTied = qtrue;
                }
                cl->ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
                cl->expandedStats.serverRank = rank;
                cl->expandedStats.serverRankIsTied = qtrue;
            }
            score = newScore;
        }
    }

    // [QL] lead change tracking
    if (level.lastLeadChangeTime == 0) {
        level.lastLeadChangeClient = -1;
        level.lastLeadChangeTime = level.time;
    } else if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        // team lead change
        int newLead;
        if (level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE]) {
            newLead = 2;  // tied
        } else if (level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE]) {
            newLead = 0;
        } else {
            newLead = 1;
        }
        if (level.lastLeadChangeClient != newLead && newLead != 2) {
            level.lastLeadChangeClient = newLead;
            level.lastLeadChangeElapsedTime = level.time - level.startTime;
        }
    } else {
        // FFA/Duel lead change
        if (level.numPlayingClients > 0
            && level.lastLeadChangeClient != level.sortedClients[0]
            && level.clients[level.sortedClients[0]].expandedStats.serverRankIsTied == qfalse) {
            level.lastLeadChangeClient = level.sortedClients[0];
            level.lastLeadChangeElapsedTime = level.time - level.startTime;
        }
    }

    // set the CS_SCORES1/2 configstrings, which will be visible to everyone
    if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        trap_SetConfigstring(CS_SCORES1, va("%i", level.teamScores[TEAM_RED]));
        trap_SetConfigstring(CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE]));
        // [QL] team name configstrings
        trap_SetConfigstring(CS_SCORES1PLAYER, "TEAM_RED");
        trap_SetConfigstring(CS_SCORES2PLAYER, "TEAM_BLUE");
    } else {
        // [QL] FFA/Duel: set scores + player name configstrings
        if (level.numPlayingClients < 1) {
            trap_SetConfigstring(CS_SCORES1, va("%i", SCORE_NOT_PRESENT));
            trap_SetConfigstring(CS_SCORES1PLAYER, "");
        } else {
            trap_SetConfigstring(CS_SCORES1, va("%i", level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE]));
            trap_SetConfigstring(CS_SCORES1PLAYER, level.clients[level.sortedClients[0]].pers.netname);
        }
        if (level.numPlayingClients < 2) {
            trap_SetConfigstring(CS_SCORES2, va("%i", SCORE_NOT_PRESENT));
            trap_SetConfigstring(CS_SCORES2PLAYER, "");
        } else {
            trap_SetConfigstring(CS_SCORES2, va("%i", level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE]));
            trap_SetConfigstring(CS_SCORES2PLAYER, level.clients[level.sortedClients[1]].pers.netname);
        }

        // [QL] Duel-specific: set CS for 1st/2nd player scores + clientNums
        if (g_gametype.integer == GT_DUEL && !level.intermissionQueued && !level.intermissionTime) {
            int p1, p2;

            if (level.numPlayingClients == 0) {
                trap_SetConfigstring(CS_SCORE1STPLAYER, va("%i", SCORE_NOT_PRESENT));
                trap_SetConfigstring(CS_CLIENTNUM1STPLAYER, "-1");
                trap_SetConfigstring(CS_MOST_DAMAGEDEALT_PLYR, "");
                trap_SetConfigstring(CS_SCORE2NDPLAYER, va("%i", SCORE_NOT_PRESENT));
                trap_SetConfigstring(CS_CLIENTNUM2NDPLAYER, "-1");
                trap_SetConfigstring(CS_MOST_ACCURATE_PLYR, "");
            } else {
                // determine p1 (lower clientNum) and p2 (higher clientNum)
                if (level.numPlayingClients >= 2) {
                    if (level.sortedClients[0] < level.sortedClients[1]) {
                        p1 = level.sortedClients[0];
                        p2 = level.sortedClients[1];
                    } else {
                        p1 = level.sortedClients[1];
                        p2 = level.sortedClients[0];
                    }
                } else {
                    p1 = level.sortedClients[0];
                    p2 = -1;
                }

                // 1st player slot
                trap_SetConfigstring(CS_SCORE1STPLAYER, va("%i", level.clients[p1].ps.persistant[PERS_SCORE]));
                trap_SetConfigstring(CS_CLIENTNUM1STPLAYER, va("%i", p1));
                trap_SetConfigstring(CS_MOST_ACCURATE_PLYR, level.clients[p1].pers.netname);

                // 2nd player slot
                if (p2 >= 0) {
                    trap_SetConfigstring(CS_SCORE2NDPLAYER, va("%i", level.clients[p2].ps.persistant[PERS_SCORE]));
                    trap_SetConfigstring(CS_CLIENTNUM2NDPLAYER, va("%i", p2));
                    trap_SetConfigstring(CS_MOST_DAMAGEDEALT_PLYR, level.clients[p2].pers.netname);
                } else {
                    trap_SetConfigstring(CS_SCORE2NDPLAYER, va("%i", SCORE_NOT_PRESENT));
                    trap_SetConfigstring(CS_CLIENTNUM2NDPLAYER, "-1");
                    trap_SetConfigstring(CS_MOST_DAMAGEDEALT_PLYR, "");
                }
            }
        }
    }

    // if we are at the intermission, send the new info to everyone
    if (level.intermissionTime) {
        for (i = 0; i < level.maxclients; i++) {
            if (level.clients[i].pers.connected == CON_CONNECTED) {
                MoveClientToIntermission(g_entities + i);
            }
        }
        SendScoreboardMessageToAllClients();
    }
}

/*
========================================================================

MAP CHANGING

========================================================================
*/

/*
========================
SendScoreboardMessageToAllClients

Do this at BeginIntermission time and whenever ranks are recalculated
due to enters/exits/forced team changes
========================
*/
void SendScoreboardMessageToAllClients(void) {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTED) {
            DeathmatchScoreboardMessage(g_entities + i);
        }
    }
}

/*
========================
MoveClientToIntermission

When the intermission starts, this will be called for all players.
If a new client connects, this will be called after the spawn function.
========================
*/
void MoveClientToIntermission(gentity_t* ent) {
    // take out of follow mode if needed
    if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
        StopFollowing(ent);
    }

    // move to the spot (FindIntermissionPoint called once in BeginIntermission)
    VectorCopy(level.intermission_origin, ent->s.origin);
    VectorCopy(level.intermission_origin, ent->client->ps.origin);
    VectorCopy(level.intermission_angle, ent->client->ps.viewangles);
    ent->client->ps.pm_type = PM_INTERMISSION;

    // clean up powerup info
    memset(ent->client->ps.powerups, 0, sizeof(ent->client->ps.powerups));

    ent->client->ps.eFlags = 0;
    ent->s.eFlags = 0;
    ent->s.eType = ET_GENERAL;
    ent->s.modelindex = 0;
    ent->s.loopSound = 0;
    ent->s.event = 0;
    ent->r.contents = 0;
}

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
void FindIntermissionPoint(void) {
    gentity_t *ent, *target;
    vec3_t dir;

    // find the intermission spot
    ent = G_Find(NULL, FOFS(classname), "info_player_intermission");
    if (!ent) {  // the map creator forgot to put in an intermission point...
        SelectSpawnPoint(vec3_origin, level.intermission_origin, level.intermission_angle, qfalse);
    } else {
        VectorCopy(ent->s.origin, level.intermission_origin);
        VectorCopy(ent->s.angles, level.intermission_angle);
        // if it has a target, look towards it
        if (ent->target) {
            target = G_PickTarget(ent->target);
            if (target) {
                VectorSubtract(target->s.origin, level.intermission_origin, dir);
                vectoangles(dir, level.intermission_angle);
            }
        }
    }
}

/*
==================
BeginIntermission
==================
*/
void BeginIntermission(void) {
    int i;
    gentity_t* ent;
    char nextmaps[MAX_STRING_CHARS];

    if (level.intermissionTime) {
        return;  // already active
    }

    // [QL] set CS_INTERMISSION
    trap_SetConfigstring(CS_INTERMISSION, "1");

    // if in tournement mode, change the wins / losses
    if (g_gametype.integer == GT_DUEL) {
        AdjustTournamentScores();
    }

    level.intermissionTime = level.time;

    // [QL] find the intermission point once (not per-client)
    FindIntermissionPoint();

    // [QL] Set all connected clients to vote-eligible, clear active vote
    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTED) {
            level.clients[i].pers.voteState = VOTE_PENDING;
            level.clients[i].pers.lastMapVoteTime = 0;
            level.clients[i].pers.lastMapVoteIndex = 0;
        }
    }
    ClearVote();

    // [QL] Parse "nextmaps" info string for end-of-match map voting
    trap_Cvar_VariableStringBuffer("nextmaps", nextmaps, sizeof(nextmaps));
    if (nextmaps[0]) {
        char mapInfo[MAX_INFO_STRING];

        for (i = 0; i < 3; i++) {
            char key[16];
            const char *val;

            Com_sprintf(key, sizeof(key), "map_%d", i);
            val = Info_ValueForKey(nextmaps, key);
            Q_strncpyz(level.intermissionMapNames[i], val, sizeof(level.intermissionMapNames[i]));

            Com_sprintf(key, sizeof(key), "title_%d", i);
            val = Info_ValueForKey(nextmaps, key);
            Q_strncpyz(level.intermissionMapTitles[i], val, sizeof(level.intermissionMapTitles[i]));

            Com_sprintf(key, sizeof(key), "cfg_%d", i);
            val = Info_ValueForKey(nextmaps, key);
            Q_strncpyz(level.intermissionMapConfigs[i], val, sizeof(level.intermissionMapConfigs[i]));

            level.intermissionMapVotes[i] = 0;
        }

        // Build map info configstring for clients
        mapInfo[0] = '\0';
        for (i = 0; i < 3; i++) {
            if (level.intermissionMapNames[i][0]) {
                char k[16];
                Com_sprintf(k, sizeof(k), "map_%d", i);
                Info_SetValueForKey(mapInfo, k, level.intermissionMapNames[i]);
                Com_sprintf(k, sizeof(k), "title_%d", i);
                Info_SetValueForKey(mapInfo, k,
                    level.intermissionMapTitles[i][0] ? level.intermissionMapTitles[i] : level.intermissionMapNames[i]);
            }
        }

        trap_SetConfigstring(CS_ROTATIONMAPS, mapInfo);
        trap_SetConfigstring(CS_ROTATIONVOTES, "0 0 0");
        level.voteTime = level.time;
        trap_SetConfigstring(CS_VOTE_TIME, va("%i", level.voteTime));
    }

    // move all clients to the intermission point
    for (i = 0; i < level.maxclients; i++) {
        ent = g_entities + i;
        if (!ent->inuse || !ent->client) {
            continue;
        }
        // [QL] reset per-client ready state
        if (ent->client->pers.connected == CON_CONNECTED) {
            ClearClientReadyToExit();
        }
        MoveClientToIntermission(ent);
    }

    // send the current scoring to all clients
    SendScoreboardMessageToAllClients();
}

/*
=============
ExitLevel

When the intermission has been exited, the server is either killed
or moved to a new level based on the "nextmap" cvar

=============
*/
void ExitLevel(void) {
    int i;
    gclient_t* cl;
    char mapname[MAX_STRING_CHARS];
    char serverinfo[MAX_INFO_STRING];
    char nextmaps[MAX_STRING_CHARS];
    int bestMap = 0;

    // [QL] Bot-only server: kill on exit
    if ( g_isBotOnly.integer ) {
        trap_SendConsoleCommand( EXEC_APPEND, "killserver\n" );
        return;
    }
    // [QL] Quit on exit level
    if ( sv_quitOnExitLevel.integer ) {
        trap_SendConsoleCommand( EXEC_APPEND, "quit\n" );
        return;
    }

    // bot interbreeding
    BotInterbreedEndMatch();

    // [QL] clear CS_INTERMISSION
    trap_SetConfigstring(CS_INTERMISSION, "");

    // if we are running a tournement map, kick the loser to spectator status,
    // which will automatically grab the next spectator and restart
    if (g_gametype.integer == GT_DUEL) {
        if (!level.restarted) {
            RemoveTournamentLoser();
            trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
            level.restarted = qtrue;
            level.intermissionTime = 0;
        }
        return;
    }

    // [QL] resolve map voting if active
    trap_Cvar_VariableStringBuffer("nextmaps", nextmaps, sizeof(nextmaps));
    if (nextmaps[0] && !(g_voteFlags.integer & VF_ENDMAP_VOTING)) {
        // [QL] Random tie-breaking for map votes
        {
            int tiedMaps[3];
            int numTied = 0;
            int bestVotes = 0;

            for ( i = 0; i < 3; i++ ) {
                if ( level.intermissionMapNames[i][0] ) {
                    if ( level.intermissionMapVotes[i] > bestVotes ) {
                        bestVotes = level.intermissionMapVotes[i];
                        numTied = 0;
                    }
                    if ( level.intermissionMapVotes[i] == bestVotes ) {
                        tiedMaps[numTied++] = i;
                    }
                }
            }
            if ( numTied > 0 ) {
                bestMap = tiedMaps[rand() % numTied];
            }
        }

        // Get current map name
        trap_GetServerinfo(serverinfo, sizeof(serverinfo));
        Q_strncpyz(mapname, Info_ValueForKey(serverinfo, "mapname"), sizeof(mapname));

        if (level.intermissionMapNames[bestMap][0]) {
            if (Q_stricmp(mapname, level.intermissionMapNames[bestMap]) == 0) {
                // Same map - restart
                trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
                level.restarted = qtrue;
            } else {
                // Different map - set nextmap and execute
                if (level.intermissionMapConfigs[bestMap][0]) {
                    trap_Cvar_Set("nextmap", va("map %s %s",
                                  level.intermissionMapNames[bestMap],
                                  level.intermissionMapConfigs[bestMap]));
                } else {
                    trap_Cvar_Set("nextmap", va("map %s",
                                  level.intermissionMapNames[bestMap]));
                }
                trap_SendConsoleCommand(EXEC_APPEND, "vstr nextmap\n");
            }
        } else {
            trap_SendConsoleCommand(EXEC_APPEND, "vstr nextmap\n");
        }
    } else {
        trap_SendConsoleCommand(EXEC_APPEND, "vstr nextmap\n");
    }

    ClearVote();

    level.intermissionTime = 0;

    // reset all the scores so we don't enter the intermission again
    level.teamScores[TEAM_RED] = 0;
    level.teamScores[TEAM_BLUE] = 0;
    level.teamScores[TEAM_FREE] = 0;
    level.lastTeamScores[TEAM_RED] = 0;
    level.lastTeamScores[TEAM_BLUE] = 0;
    level.lastTeamRoundScores[TEAM_RED] = 0;
    level.lastTeamRoundScores[TEAM_BLUE] = 0;

    for (i = 0; i < g_maxclients.integer; i++) {
        cl = level.clients + i;
        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        cl->ps.persistant[PERS_SCORE] = 0;
    }

    // we need to do this here before changing to CON_CONNECTING
    G_WriteSessionData();

    // change all client states to connecting, so the early players into the
    // next level will know the others aren't done reconnecting
    for (i = 0; i < g_maxclients.integer; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTED) {
            level.clients[i].pers.connected = CON_CONNECTING;
        }
    }
}

/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
void QDECL G_LogPrintf(const char* fmt, ...) {
    va_list argptr;
    char string[1024];
    int min, tens, sec;

    sec = (level.time - level.startTime) / 1000;

    min = sec / 60;
    sec -= min * 60;
    tens = sec / 10;
    sec -= tens * 10;

    Com_sprintf(string, sizeof(string), "%3i:%i%i ", min, tens, sec);

    va_start(argptr, fmt);
    Q_vsnprintf(string + 7, sizeof(string) - 7, fmt, argptr);
    va_end(argptr);

    if (g_dedicated.integer) {
        G_Printf("%s", string + 7);
    }

    if (!level.logFile) {
        return;
    }

    trap_FS_Write(string, strlen(string), level.logFile);
}

/*
================
LogExit

[QL] Append information about this game to the log file.
Binary: FUN_100575a0 in qagamex86.dll

Args:
  isShutdown: if non-zero, skip setting intermissionQueued (server shutdown path)
  restart:    if non-zero, skip game-over events (map restart, not match end)
  string:     exit reason text
================
*/
void LogExit(int isShutdown, int restart, const char* string) {
    int i, numSorted;
    gclient_t* cl;

    G_LogPrintf("Exit: %s\n", string);

    // [QL] only queue intermission on normal game exits
    if (isShutdown == 0) {
        level.intermissionQueued = level.time;
    }

    // don't send more than 32 scores (FIXME?)
    numSorted = level.numConnectedClients;
    if (numSorted > 32) {
        numSorted = 32;
    }

    if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        G_LogPrintf("red:%i  blue:%i\n",
                    level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]);
    }

    for (i = 0; i < numSorted; i++) {
        int ping;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }
        if (cl->pers.connected == CON_CONNECTING) {
            continue;
        }

        ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

        G_LogPrintf("score: %i  ping: %i  client: %i %s\n",
                    cl->ps.persistant[PERS_SCORE], ping,
                    level.sortedClients[i], cl->pers.netname);
    }

    // [QL] game-over events (only on normal match end, not restart)
    if (isShutdown == 0 && restart == 0) {
        gentity_t *te;
        if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
            // team game: announce winning team
            te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->s.eventParm = (level.teamScores[TEAM_RED] <= level.teamScores[TEAM_BLUE])
                              ? GTS_BLUETEAM_WON : GTS_REDTEAM_WON;
            te->r.svFlags |= SVF_BROADCAST;
        } else {
            // FFA/duel: generic game-over event
            te = G_TempEntity(vec3_origin, EV_GAMEOVER);
            te->r.svFlags |= SVF_BROADCAST;
        }
    }

    // [QL] Save lastScore and mark stats as reported
    for ( i = 0; i < level.numConnectedClients; i++ ) {
        cl = &level.clients[level.sortedClients[i]];
        if ( cl->sess.sessionTeam != TEAM_SPECTATOR ) {
            cl->sess.prevScore = cl->ps.persistant[PERS_SCORE];
        }
    }
    level.gameStatsReported = qtrue;

    // [QL] Publish match-end stats
    STAT_MatchEnd();

    // [QL] Log stats line (binary format: "stats: %d")
    G_LogPrintf("stats: %d\n", level.numPlayingClients);
}

/*
=================
CheckIntermissionExit

[QL] Binary: FUN_10057b60 in qagamex86.dll
Uses a hard timeout model: 10s default, 20s if map voting enabled.
After timeout + 3s minimum, unconditionally exits.
=================
*/
void CheckIntermissionExit(void) {
    int i;
    gclient_t* cl;
    int readyMask;
    int timeout;
    char nextmaps[MAX_STRING_CHARS];

    // [QL] Determine timeout: 20s if map voting active, 10s otherwise
    trap_Cvar_VariableStringBuffer("nextmaps", nextmaps, sizeof(nextmaps));
    if (nextmaps[0] && !(g_voteFlags.integer & VF_ENDMAP_VOTING)) {
        timeout = 20000;
    } else {
        timeout = 10000;
    }

    // Build ready mask from per-client ready state
    readyMask = 0;
    for (i = 0; i < g_maxclients.integer; i++) {
        cl = level.clients + i;
        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        if (g_entities[i].r.svFlags & SVF_BOT) {
            continue;
        }
        if (ClientIsReadyToExit(i)) {
            if (i < 16) {
                readyMask |= 1 << i;
            }
        }
    }

    // Broadcast ready mask to all connected clients
    for (i = 0; i < g_maxclients.integer; i++) {
        cl = level.clients + i;
        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        cl->ps.stats[STAT_CLIENTS_READY] = readyMask;
    }

    // [QL] Mark voting ended after timeout expires
    if (!level.votingEnded &&
        level.time >= level.intermissionTime + timeout) {
        level.votingEnded = qtrue;
    }

    // [QL] Hard exit after timeout + 3s minimum
    if (level.time >= level.intermissionTime + timeout + 3000) {
        level.readyToExit = qtrue;
        level.exitTime = level.time;
        ExitLevel();
    }
}

/*
=============
ScoreIsTied
=============
*/
qboolean ScoreIsTied(void) {
    int a, b;

    if (level.numPlayingClients < 2) {
        return qfalse;
    }

    if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        return level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE];
    }

    a = level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE];
    b = level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE];

    return a == b;
}

/*
=================
CheckExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag.
=================
*/
void CheckExitRules(void) {
    int i;
    gclient_t* cl;
    // if at the intermission, wait for all non-bots to
    // signal ready, then go to next level
    if (level.intermissionTime) {
        CheckIntermissionExit();
        return;
    }

    if (level.intermissionQueued) {
        if (level.time - level.intermissionQueued < INTERMISSION_DELAY_TIME) {
            return;
        }
        level.intermissionQueued = 0;
        BeginIntermission();
        return;
    }

    // [QL] no exit checks during warmup
    if (level.warmupTime != 0) {
        return;
    }

    // [QL] Round-based gametypes: exit rules handled by their round system
    switch ( g_gametype.integer ) {
    case GT_CA:
    case GT_AD:
    case GT_RR:
        return;  // always handled by round logic
    case GT_FREEZE:
        if ( level.roundState.eCurrent != 0 ) {
            return;  // FT: only fall through during pre-round (roundState 0)
        }
        break;
    default:
        break;
    }

    // [QL] Overtime system
    if ( ScoreIsTied() ) {
        if ( !g_timelimit.integer ) {
            return;  // no timelimit = infinite sudden death
        }
        if ( !g_overtime.integer ) {
            return;  // no overtime cvar = infinite sudden death
        }
        if ( level.time - level.startTime < g_timelimit.integer * 60000 + level.timeOvertime ) {
            return;  // overtime not expired yet
        }
        // Overtime expired while still tied - add another overtime period
        {
            gentity_t *te = G_TempEntity( vec3_origin, EV_OVERTIME );
            te->r.svFlags |= SVF_BROADCAST;
        }
        level.timeOvertime += g_overtime.integer * 1000;
        return;
    }

    if (g_timelimit.integer && !level.warmupTime) {
        if (level.time - level.startTime >= g_timelimit.integer * 60000 + level.timeOvertime) {
            trap_SendServerCommand(-1, "print \"Timelimit hit.\n\"");
            LogExit(0, 0, "Timelimit hit.");
            return;
        }
    }

    if (g_fraglimit.integer &&
        (g_gametype.integer == GT_FFA || g_gametype.integer == GT_DUEL || g_gametype.integer == GT_TEAM)) {
        if (level.teamScores[TEAM_RED] >= g_fraglimit.integer) {
            trap_SendServerCommand(-1, "print \"Red hit the fraglimit.\n\"");
            LogExit(0, 0, "Fraglimit hit.");
            return;
        }

        if (level.teamScores[TEAM_BLUE] >= g_fraglimit.integer) {
            trap_SendServerCommand(-1, "print \"Blue hit the fraglimit.\n\"");
            LogExit(0, 0, "Fraglimit hit.");
            return;
        }

        for (i = 0; i < g_maxclients.integer; i++) {
            cl = level.clients + i;
            if (cl->pers.connected != CON_CONNECTED) {
                continue;
            }
            if (cl->sess.sessionTeam != TEAM_FREE) {
                continue;
            }

            if (cl->ps.persistant[PERS_SCORE] >= g_fraglimit.integer) {
                LogExit(0, 0, "Fraglimit hit.");
                trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " hit the fraglimit.\n\"",
                                              cl->pers.netname));
                return;
            }
        }
    }

    if (g_capturelimit.integer &&
        g_gametype.integer >= GT_CTF && g_gametype.integer <= GT_HARVESTER) {
        if (level.teamScores[TEAM_RED] >= g_capturelimit.integer) {
            trap_SendServerCommand(-1, "print \"Red hit the capturelimit.\n\"");
            LogExit(0, 0, "Capturelimit hit.");
            return;
        }

        if (level.teamScores[TEAM_BLUE] >= g_capturelimit.integer) {
            trap_SendServerCommand(-1, "print \"Blue hit the capturelimit.\n\"");
            LogExit(0, 0, "Capturelimit hit.");
            return;
        }
    }

    // [QL] Scorelimit for Domination
    if ( g_scorelimit.integer && g_gametype.integer == GT_DOMINATION ) {
        if ( level.teamScores[TEAM_RED] >= g_scorelimit.integer ) {
            trap_SendServerCommand( -1, "print \"Red hit the scorelimit.\n\"" );
            LogExit( 0, 0, "Scorelimit hit." );
            return;
        }
        if ( level.teamScores[TEAM_BLUE] >= g_scorelimit.integer ) {
            trap_SendServerCommand( -1, "print \"Blue hit the scorelimit.\n\"" );
            LogExit( 0, 0, "Scorelimit hit." );
            return;
        }
    }

    // [QL] Mercylimit for team modes
    if ( g_mercylimit.integer && g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR ) {
        int mercyTime = g_mercytime.integer * 60000 + level.timeOvertime;
        if ( level.time - level.startTime >= mercyTime ) {
            int diff = level.teamScores[TEAM_RED] - level.teamScores[TEAM_BLUE];
            if ( diff >= g_mercylimit.integer ) {
                trap_SendServerCommand( -1, "print \"Red hit the mercylimit.\n\"" );
                LogExit( 0, 0, "Mercylimit hit." );
                return;
            }
            if ( -diff >= g_mercylimit.integer ) {
                trap_SendServerCommand( -1, "print \"Blue hit the mercylimit.\n\"" );
                LogExit( 0, 0, "Mercylimit hit." );
                return;
            }
        }
    }

}

/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/

/*
=============
SetWarmupState

[QL] Sets level.warmupTime, CS_WARMUP configstring, and g_gameState cvar.
Binary: FUN_10059c80 in qagamex86.dll
=============
*/
void SetWarmupState(int warmupTime) {
    const char *gameState;

    level.warmupTime = warmupTime;
    trap_SetConfigstring(CS_WARMUP, va("\\time\\%i", warmupTime));

    if (warmupTime < 0) {
        gameState = "PRE_GAME";
    } else if (warmupTime == 0) {
        gameState = "IN_PROGRESS";
    } else {
        gameState = "COUNT_DOWN";
    }
    trap_Cvar_Set("g_gameState", gameState);
}

/*
=============
CheckWarmupMinPlayers

[QL] Returns qtrue if enough players are present/ready for warmup to proceed.
Checks g_doWarmup, ready percentage, team sizes, team balance.
Binary: FUN_10057830 in qagamex86.dll
=============
*/
static qboolean CheckWarmupMinPlayers(void) {
    int i;
    int numPlaying = 0;
    int numReady = 0;
    int redCount, blueCount;
    gclient_t *cl;

    // [QL] Grace period: non-duel gametypes wait g_warmup seconds from map start
    // before allowing countdown (but only during PRE_GAME with warmup enabled)
    if (g_doWarmup.integer != 0 && g_gametype.integer != GT_DUEL &&
        level.time - level.startTime < g_warmup.integer * 1000) {
        return qfalse;
    }

    // During PRE_GAME: count playing and ready clients
    if (level.warmupTime < 0) {
        for (i = 0; i < level.maxclients; i++) {
            cl = level.clients + i;
            if (cl->pers.connected != CON_CONNECTED) {
                continue;
            }
            if (cl->sess.sessionTeam == TEAM_SPECTATOR) {
                continue;
            }

            if (cl->pers.ready) {
                numReady++;
            }
            numPlaying++;
        }
        level.numReadyClients = numPlaying;
        level.numReadyHumans = numReady;
    }

    // Check g_teamSizeMin (team games need minimum players per team)
    if (g_teamSizeMin.integer != 0) {
        blueCount = TeamCount(-1, TEAM_BLUE);
        redCount = TeamCount(-1, TEAM_RED);

        // If team force balance is off, or teams are below minimum
        if (g_teamForceBalance.integer == 0 ||
            (blueCount < g_teamSizeMin.integer && level.numPlayingClients < g_teamSizeMin.integer)) {
            if (g_gametype.integer < GT_TEAM) {
                if (level.numPlayingClients < 2) {
                    return qfalse;
                }
            } else {
                if (redCount < g_teamSizeMin.integer) {
                    return qfalse;
                }
                if (blueCount < g_teamSizeMin.integer) {
                    return qfalse;
                }
            }
        }
    }

    // Check g_teamForceBalance (teams can't differ by more than 1)
    if (g_teamForceBalance.integer != 0) {
        blueCount = TeamCount(-1, TEAM_BLUE);
        redCount = TeamCount(-1, TEAM_RED);
        if (blueCount + 1 < redCount || redCount + 1 < blueCount) {
            return qfalse;
        }
    }

    // Check ready requirements during PRE_GAME
    if (level.warmupTime < 0) {
        if (g_gametype.integer == GT_DUEL) {
            // Duel: need exactly 2 playing clients
            if (level.numPlayingClients == 2 && numPlaying == 2) {
                return qtrue;
            }
        } else {
            // g_doWarmup 0: treat all players as ready (skip ready-up phase)
            // Team games still require at least 1 player per team
            if (g_doWarmup.integer == 0 && level.numPlayingClients > 0) {
                if (g_gametype.integer >= GT_TEAM) {
                    redCount = TeamCount(-1, TEAM_RED);
                    blueCount = TeamCount(-1, TEAM_BLUE);
                    if (redCount < 1 || blueCount < 1) {
                        return qfalse;
                    }
                }
                return qtrue;
            }
            // Team/FFA: check ready percentage
            if ((numReady != 0 &&
                 (float)numReady / (float)level.numPlayingClients >= g_warmupReadyPercentage.value) ||
                g_warmupReadyPercentage.value == 0.0f) {
                return qtrue;
            }
        }
    } else {
        // During countdown: set PMF_FROZEN on all playing clients
        if (level.warmupTime != 0) {
            for (i = 0; i < level.maxclients; i++) {
                cl = level.clients + i;
                if (cl->pers.connected == CON_CONNECTED &&
                    cl->sess.sessionTeam != TEAM_SPECTATOR) {
                    cl->ps.pm_flags |= PMF_FROZEN;
                }
            }
        }
        return qtrue;
    }

    return qfalse;
}

/*
=============
CheckForfeit

[QL] Returns qtrue if the match should be forfeited (not enough players).
Binary: FUN_10057d60 in qagamex86.dll (param_1=0 for auto-check)
=============
*/
static qboolean CheckForfeit(void) {
    int elapsed;
    int redCount, blueCount;

    // Not during timeout
    // (level.timePauseBegin != 0 means timeout active)

    elapsed = level.time - level.startTime;

    // Need at least 1 second of play time
    if (elapsed / 1000 <= 1) {
        return qfalse;
    }

    // Not during warmup
    if (level.warmupTime != 0) {
        return qfalse;
    }

    if (g_gametype.integer < GT_TEAM || g_gametype.integer == GT_RR) {
        // FFA/Duel/RR: forfeit if less than 2 playing clients
        if (level.numPlayingClients < 2) {
            return qtrue;
        }
    } else {
        // Team games: forfeit if either team is empty
        redCount = TeamCount(-1, TEAM_RED);
        blueCount = TeamCount(-1, TEAM_BLUE);
        if (redCount < 1 || blueCount < 1) {
            return qtrue;
        }
    }

    return qfalse;
}

/*
=============
HandleForfeit

[QL] Ends the match due to forfeit.
Binary: FUN_10057f90 in qagamex86.dll
=============
*/
static void HandleForfeit(void) {
    int redCount, blueCount;

    if (g_gametype.integer == GT_DUEL) {
        // Duel: set losing score on disconnected/spectating players
        int i;
        for (i = 0; i < 2; i++) {
            int idx = (i == 0) ? level.clientNum1stPlayer : level.clientNum2ndPlayer;
            if (idx >= 0) {
                gclient_t *cl = level.clients + idx;
                if (cl->pers.connected == CON_DISCONNECTED ||
                    cl->sess.sessionTeam == TEAM_SPECTATOR) {
                    cl->ps.persistant[PERS_SCORE] = -999;
                }
            }
        }
    } else if (g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_RR) {
        // Team games: set losing score on empty teams
        redCount = TeamCount(-1, TEAM_RED);
        blueCount = TeamCount(-1, TEAM_BLUE);
        if (blueCount < 1) {
            level.teamScores[TEAM_BLUE] = -999;
            trap_SetConfigstring(CS_SCORES2, va("%i", -999));
        }
        if (redCount < 1) {
            level.teamScores[TEAM_RED] = -999;
            trap_SetConfigstring(CS_SCORES1, va("%i", -999));
        }
    }

    level.matchForfeited = qtrue;
    trap_SendServerCommand(-1, "print \"Game has been forfeited.\n\"");
    LogExit(0, 0, "Players have forfeited.");
}

/*
=============
CheckWarmup

[QL] Warmup state machine for ALL gametypes (separate from CheckTournament).
Binary: FUN_10058760 in qagamex86.dll

States:
  warmupTime == 0  → match in progress (check forfeit)
  warmupTime == -1 → PRE_GAME (waiting for players)
  warmupTime > 0   → COUNT_DOWN (countdown to match start)
=============
*/
static void CheckWarmup(void) {
    int i;
    gclient_t *cl;

    if (level.numPlayingClients == 0) {
        return;
    }

    if (level.warmupTime == 0) {
        // Match in progress - check for forfeit
        if (CheckForfeit()) {
            HandleForfeit();
        }
        return;
    }

    // Warmup active (PRE_GAME or COUNT_DOWN)
    if (!CheckWarmupMinPlayers()) {
        // Not enough players
        if (level.warmupTime == -1) {
            // Already in PRE_GAME, nothing to do
            return;
        }

        // Was in COUNT_DOWN, revert to PRE_GAME
        SetWarmupState(-1);
        G_LogPrintf("Warmup:\n");

        // Clear PMF_FROZEN on all playing clients (countdown cancelled)
        for (i = 0; i < level.maxclients; i++) {
            cl = level.clients + i;
            if (cl->pers.connected == CON_CONNECTED &&
                cl->sess.sessionTeam != TEAM_SPECTATOR) {
                if (cl->ps.pm_flags & PMF_FROZEN) {
                    cl->ps.pm_flags &= ~PMF_FROZEN;
                }
                // Reset specOnly if it was set during countdown
                if (cl->sess.specOnly == 1) {
                    cl->sess.specOnly = 0;
                    ClientUserinfoChanged(cl->ps.clientNum);
                }
            }
        }
        return;
    }

    // Enough players - check if g_warmup cvar changed (force restart countdown)
    if (g_warmup.modificationCount != level.warmupModificationCount) {
        level.warmupModificationCount = g_warmup.modificationCount;
        SetWarmupState(-1);
    }

    // Start countdown if in PRE_GAME
    if (level.warmupTime < 0) {
        if (g_warmup.integer > 1) {
            SetWarmupState(level.time + (g_warmup.integer - 1) * 1000);
        } else {
            SetWarmupState(0);
        }
        return;
    }

    // Countdown expired - start match
    if (level.time > level.warmupTime) {
        level.warmupTime += 10000;
        trap_Cvar_Set("g_restarted", "1");
        trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
        level.restarted = qtrue;
        return;
    }
}

// CheckTournament moved to g_gametype_duel.c

/*
==================
ExecuteVoteCommand
[QL] Execute special vote types that need server-side handling
rather than direct console command execution (binary: 0x10058b20).
Returns qtrue if handled, qfalse if caller should execute via console.
==================
*/
static qboolean ExecuteVoteCommand(const char *voteCmd) {
    char arg0[MAX_STRING_TOKENS];
    char arg1[MAX_STRING_TOKENS];
    char *p;

    Q_strncpyz(arg0, voteCmd, sizeof(arg0));
    p = strchr(arg0, ' ');
    if (p) {
        *p = '\0';
        Q_strncpyz(arg1, p + 1, sizeof(arg1));
        // strip quotes
        if (arg1[0] == '"') {
            int len = strlen(arg1);
            if (len > 1 && arg1[len-1] == '"') {
                arg1[len-1] = '\0';
            }
            memmove(arg1, arg1 + 1, strlen(arg1));
        }
    } else {
        arg1[0] = '\0';
    }

    if (!Q_stricmp(arg0, "cointoss")) {
        if (rand() & 1) {
            trap_SendServerCommand(-1, "print \"^3The coin is: ^5HEADS^7\n\"");
        } else {
            trap_SendServerCommand(-1, "print \"^3The coin is: ^5TAILS^7\n\"");
        }
        return qtrue;
    }
    if (!Q_stricmp(arg0, "random")) {
        int val = atoi(arg1);
        if (val == 2) {
            if (rand() & 1) {
                trap_SendServerCommand(-1, "print \"^3The coin is: ^5HEADS^7\n\"");
            } else {
                trap_SendServerCommand(-1, "print \"^3The coin is: ^5TAILS^7\n\"");
            }
        } else if (val >= 3 && val <= 100) {
            trap_SendServerCommand(-1,
                va("print \"^3Random number is: ^5%d^7\n\"", (rand() % val) + 1));
        }
        return qtrue;
    }
    if (!Q_stricmp(arg0, "loadouts")) {
        if (!Q_stricmp(arg1, "ON")) {
            trap_Cvar_Set("g_loadout", "1");
        } else {
            trap_Cvar_Set("g_loadout", "0");
        }
        trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
        return qtrue;
    }
    if (!Q_stricmp(arg0, "ammo")) {
        if (!Q_stricmp(arg1, "GLOBAL")) {
            trap_Cvar_Set("g_ammoPack", "1");
        } else {
            trap_Cvar_Set("g_ammoPack", "0");
        }
        trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
        return qtrue;
    }
    if (!Q_stricmp(arg0, "timers")) {
        if (!Q_stricmp(arg1, "ON")) {
            trap_Cvar_Set("g_itemTimers", "1");
        } else {
            trap_Cvar_Set("g_itemTimers", "0");
        }
        return qtrue;
    }
    if (!Q_stricmp(arg0, "shuffle")) {
        // shuffle teams by randomly reassigning all playing clients
        level.shufflePending = qtrue;
        trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
        return qtrue;
    }
    if (!Q_stricmp(arg0, "teamsize")) {
        trap_Cvar_Set("teamsize", arg1);
        trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
        return qtrue;
    }
    if (!Q_stricmp(arg0, "weaprespawn")) {
        trap_Cvar_Set("g_weaponRespawn", arg1);
        return qtrue;
    }

    return qfalse;
}

/*
==================
CheckVote
==================
*/
void CheckVote(void) {
    int i;
    int voteYes, voteNo, voteEligible;
    gentity_t *ent;

    // [QL] Don't process votes during intermission (map voting uses separate path)
    if (level.intermissionTime) {
        return;
    }

    // Execute pending vote command
    if (level.voteExecuteTime && level.voteExecuteTime < level.time) {
        level.voteExecuteTime = 0;
        if (!ExecuteVoteCommand(level.voteString)) {
            trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.voteString));
        }
    }

    if (!level.voteTime) {
        return;
    }

    // [QL] Recount votes each frame from per-client voteState
    voteYes = 0;
    voteNo = 0;
    voteEligible = 0;

    for (i = 0; i < level.maxclients; i++) {
        ent = &g_entities[i];
        if (!ent->inuse || ent->client->pers.connected != CON_CONNECTED) {
            continue;
        }
        if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }

        switch (ent->client->pers.voteState) {
            case VOTE_YES:
                voteYes++;
                voteEligible++;
                break;
            case VOTE_NO:
                voteNo++;
                // fall through
            case VOTE_PENDING:
                voteEligible++;
                break;
            case VOTE_PASSED:
                // [QL] Admin override - force pass
                trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " passed the vote.\n\"",
                    ent->client->pers.netname));
                goto vote_passed;
            case VOTE_VETOED:
                // [QL] Admin override - force veto
                trap_SendServerCommand(-1, va("print \"%s" S_COLOR_WHITE " vetoed the vote.\n\"",
                    ent->client->pers.netname));
                goto vote_failed;
            default:
                break;
        }
    }

    // Check vote outcome
    if ((float)voteYes > (float)voteEligible * 0.5f) {
vote_passed:
        trap_SendServerCommand(-1, "print \"Vote passed.\n\"");
        ClearVote();
        level.voteExecuteTime = level.time + 3000;
    } else if ((float)voteNo >= (float)voteEligible * 0.5f) {
vote_failed:
        trap_SendServerCommand(-1, "print \"Vote failed.\n\"");
        ClearVote();
    } else {
        // Update vote configstrings only when counts change
        if (level.voteYes != voteYes) {
            level.voteYes = voteYes;
            trap_SetConfigstring(CS_VOTE_YES, va("%i", voteYes));
        }
        if (level.voteNo != voteNo) {
            level.voteNo = voteNo;
            trap_SetConfigstring(CS_VOTE_NO, va("%i", voteNo));
        }
        // Vote timeout (30 seconds)
        if (level.time - level.voteTime > 29999) {
            trap_SendServerCommand(-1, "print \"Vote failed.\n\"");
            ClearVote();
        }
    }
}

/*
==================
PrintTeam
==================
*/
void PrintTeam(int team, char* message) {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].sess.sessionTeam != team)
            continue;
        trap_SendServerCommand(i, message);
    }
}

/*
==================
SetLeader
==================
*/
void SetLeader(int team, int client) {
    int i;

    if (level.clients[client].pers.connected == CON_DISCONNECTED) {
        PrintTeam(team, va("print \"%s is not connected\n\"", level.clients[client].pers.netname));
        return;
    }
    if (level.clients[client].sess.sessionTeam != team) {
        PrintTeam(team, va("print \"%s is not on the team anymore\n\"", level.clients[client].pers.netname));
        return;
    }
    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].sess.sessionTeam != team)
            continue;
        if (level.clients[i].sess.teamLeader) {
            level.clients[i].sess.teamLeader = qfalse;
            ClientUserinfoChanged(i);
        }
    }
    level.clients[client].sess.teamLeader = qtrue;
    ClientUserinfoChanged(client);
    PrintTeam(team, va("print \"%s is the new team leader\n\"", level.clients[client].pers.netname));
}

/*
==================
CheckTeamLeader
==================
*/
void CheckTeamLeader(int team) {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].sess.sessionTeam != team)
            continue;
        if (level.clients[i].sess.teamLeader)
            break;
    }
    if (i >= level.maxclients) {
        for (i = 0; i < level.maxclients; i++) {
            if (level.clients[i].sess.sessionTeam != team)
                continue;
            if (!(g_entities[i].r.svFlags & SVF_BOT)) {
                level.clients[i].sess.teamLeader = qtrue;
                break;
            }
        }

        if (i >= level.maxclients) {
            for (i = 0; i < level.maxclients; i++) {
                if (level.clients[i].sess.sessionTeam != team)
                    continue;
                level.clients[i].sess.teamLeader = qtrue;
                break;
            }
        }
    }
}

/*
==================
CheckCvars
==================
*/
void CheckCvars(void) {
    static int lastMod = -1;

    if (g_password.modificationCount != lastMod) {
        lastMod = g_password.modificationCount;
        if (*g_password.string && Q_stricmp(g_password.string, "none")) {
            trap_Cvar_Set("g_needpass", "1");
        } else {
            trap_Cvar_Set("g_needpass", "0");
        }
    }
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
void G_RunThink(gentity_t* ent) {
    int thinktime;

    thinktime = ent->nextthink;
    if (thinktime <= 0) {
        return;
    }
    if (thinktime > level.time) {
        return;
    }

    ent->nextthink = 0;
    if (!ent->think) {
        G_Error("NULL ent->think");
    }
    ent->think(ent);
}

/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
void G_RunFrame(int levelTime) {
    int i;
    gentity_t* ent;

    // if we are waiting for the level to restart, do nothing
    if (level.restarted) {
        return;
    }

    level.frametime = levelTime - level.time;
    level.time = levelTime;

    // get any cvar changes
    G_UpdateCvars();

    // [QL] update pmove configstring for client prediction (only when dirty)
    if (pmoveInfoDirty) {
        G_SendPmoveInfo();
        pmoveInfoDirty = qfalse;
    }

    // [QL listen server] Auto-begin connecting clients.
    // Real QL dedicated servers rely on the "team" command from UI to trigger
    // ClientBegin.  Q3 had GAME_CLIENT_BEGIN in vmMain which the engine called
    // from SV_ClientEnterWorld - QL removed it.  For listen servers we need to
    // auto-begin clients so they get a valid player state and receive snapshots.
    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == CON_CONNECTING) {
            ClientBegin(i);
        }
    }

    //
    // go through all allocated objects
    //
    ent = &g_entities[0];
    for (i = 0; i < level.num_entities; i++, ent++) {
        if (!ent->inuse) {
            continue;
        }

        // clear events that are too old
        if (level.time - ent->eventTime > EVENT_VALID_MSEC) {
            if (ent->s.event) {
                ent->s.event = 0;  // &= EV_EVENT_BITS;
                if (ent->client) {
                    ent->client->ps.externalEvent = 0;
                    // predicted events should never be set to zero
                    // ent->client->ps.events[0] = 0;
                    // ent->client->ps.events[1] = 0;
                }
            }
            if (ent->freeAfterEvent) {
                // tempEntities or dropped items completely go away after their event
                G_FreeEntity(ent);
                continue;
            } else if (ent->unlinkAfterEvent) {
                // items that will respawn will hide themselves after their pickup event
                ent->unlinkAfterEvent = qfalse;
                trap_UnlinkEntity(ent);
            }
        }

        // temporary entities don't think
        if (ent->freeAfterEvent) {
            continue;
        }

        if (!ent->r.linked && ent->neverFree) {
            continue;
        }

        if (ent->s.eType == ET_MISSILE) {
            G_RunMissile(ent);
            continue;
        }

        if (ent->s.eType == ET_ITEM || ent->physicsObject) {
            G_RunItem(ent);
            continue;
        }

        if (ent->s.eType == ET_MOVER) {
            G_RunMover(ent);
            continue;
        }

        if (i < MAX_CLIENTS) {
            G_RunClient(ent);
            continue;
        }

        G_RunThink(ent);
    }

    // perform final fixups on the players
    ent = &g_entities[0];
    for (i = 0; i < level.maxclients; i++, ent++) {
        if (ent->inuse) {
            ClientEndFrame(ent);
        }
    }

    // [QL] duel queue rotation (pull in spectators)
    CheckTournament();

    // [QL] warmup state machine (all gametypes)
    CheckWarmup();

    // see if it is time to end the level
    CheckExitRules();

    // update to team status?
    CheckTeamStatus();

    // cancel vote if timed out
    CheckVote();

    // for tracking changes
    CheckCvars();

    // [QL] run bot AI - QL removed BOTAI_START_FRAME from vmMain,
    // so the game module drives bot AI from G_RunFrame instead.
    if (trap_Cvar_VariableIntegerValue("bot_enable")) {
        BotAIStartFrame(level.time);
    }

    if (g_listEntity.integer) {
        for (i = 0; i < MAX_GENTITIES; i++) {
            G_Printf("%4i: %s\n", i, g_entities[i].classname);
        }
        trap_Cvar_Set("g_listEntity", "0");
    }
}
