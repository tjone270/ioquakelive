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
// bg_pmove.c -- both games player movement code
// takes a playerstate and a usercmd as input and returns a modifed playerstate

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"

pmove_t* pm;
pml_t pml;

// movement parameters (Q3 base values)
float pm_stopspeed = 100.0f;
float pm_duckScale = 0.25f;
float pm_swimScale = 0.50f;

float pm_accelerate = 10.0f;
float pm_airaccelerate = 1.0f;
float pm_wateraccelerate = 4.0f;
float pm_flyaccelerate = 8.0f;

float pm_friction = 6.0f;
float pm_waterfriction = 1.0f;
float pm_flightfriction = 3.0f;
float pm_spectatorfriction = 5.0f;

// [QL] configurable movement parameters (set via PM_Set* functions)
float pm_jumpVelocity = 275.0f;
float pm_jumpVelocityMax = 700.0f;
float pm_jumpVelocityScaleAdd = 0.4f;
float pm_jumpVelocityTimeThreshold = 500.0f;
float pm_jumpVelocityTimeThresholdOffset = 0.6f;
float pm_chainJumpVelocity = 110.0f;
float pm_stepJumpVelocity = 48.0f;
float pm_jumpTimeDeltaMin = 100.0f;
int pm_chainJump = 1;             // 0=off, 1=scale, 2=additive, 3=interpolated
int pm_rampJump = 0;
float pm_rampJumpScale = 1.0f;
int pm_stepJump = 0;
int pm_crouchStepJump = 1;
int pm_autoHop = 1;
int pm_bunnyHop = 1;
float pm_bunnyHopOveraccel = 0.55f;
float pm_airAccel = 1.0f;
float pm_airStopAccel = 1.0f;
float pm_airControl = 0.0f;
float pm_strafeAccel = 1.0f;
float pm_walkAccel = 10.0f;
float pm_walkFriction = 6.0f;
float pm_circleStrafeFriction = 6.0f;
float pm_crouchSlideFriction = 0.5f;
int pm_crouchSlideTime = 2000;
int pm_airSteps = 1;
float pm_airStepFriction = 0.03f;
float pm_stepHeight = 22.0f;
float pm_waterSwimScale = 0.559f;
float pm_waterWadeScale = 0.75f;
float pm_wishSpeed = 400.0f;
int pm_weaponDropTime = 200;
int pm_weaponRaiseTime = 200;
int pm_noPlayerClip = 0;
float pm_hookPullVelocity = 800.0f;
int pm_doubleJump = 0;
int pm_crouchSlide = 0;
float pm_velocityGH = 800.0f;

// [QL] backup globals - server-configured values that VQ3 mode restores from.
// CPM mode hardcodes these, but VQ3 respects whatever the server set.
static float pm_cfg_walkAccel = 10.0f;
static float pm_cfg_circleStrafeFriction = 6.0f;
static float pm_cfg_wishSpeed = 400.0f;

int c_pmove = 0;

/*
==================
PM_Set* functions - configure movement physics at runtime
==================
*/
void PM_SetAirAccel(float value)                      { pm_airAccel = value; }
void PM_SetAirControl(float value)                    { pm_airControl = value; }
void PM_SetAirStepFriction(float value)               { pm_airStepFriction = value; }
void PM_SetAirSteps(int value)                        { pm_airSteps = value; }
void PM_SetAirStopAccel(float value)                  { pm_airStopAccel = value; }
void PM_SetAutoHop(int value)                         { pm_autoHop = value; }
void PM_SetBunnyHop(int value)                        { pm_bunnyHop = value; }
void PM_SetChainJump(int value)                       { pm_chainJump = value; }
void PM_SetChainJumpVelocity(float value)             { pm_chainJumpVelocity = value; }
void PM_SetCircleStrafeFriction(float value)          { pm_circleStrafeFriction = value; pm_cfg_circleStrafeFriction = value; }
void PM_SetCrouchSlideFriction(float value)           { pm_crouchSlideFriction = value; }
void PM_SetCrouchSlideTime(int value)                 { pm_crouchSlideTime = value; }
void PM_SetCrouchStepJump(int value)                  { pm_crouchStepJump = value; }
void PM_SetHookPullVelocity(float value)              { pm_hookPullVelocity = value; }
void PM_SetJumpTimeDeltaMin(float value)              { pm_jumpTimeDeltaMin = value; }
void PM_SetJumpVelocity(float value)                  { pm_jumpVelocity = value; }
void PM_SetJumpVelocityMax(float value)               { pm_jumpVelocityMax = value; }
void PM_SetJumpVelocityScaleAdd(float value)          { pm_jumpVelocityScaleAdd = value; }
void PM_SetJumpVelocityTimeThreshold(float value)     { pm_jumpVelocityTimeThreshold = value; }
void PM_SetJumpVelocityTimeThresholdOffset(float value) { pm_jumpVelocityTimeThresholdOffset = value; }
void PM_SetNoPlayerClip(int value)                    { pm_noPlayerClip = value; }
void PM_SetRampJump(int value)                        { pm_rampJump = value; }
void PM_SetRampJumpScale(float value)                 { pm_rampJumpScale = value; }
void PM_SetStepHeight(float value)                    { pm_stepHeight = value; }
void PM_SetStepJump(int value)                        { pm_stepJump = value; }
void PM_SetStepJumpVelocity(float value)              { pm_stepJumpVelocity = value; }
void PM_SetStrafeAccel(float value)                   { pm_strafeAccel = value; }
void PM_SetWalkAccel(float value)                     { pm_walkAccel = value; pm_cfg_walkAccel = value; }
void PM_SetWalkFriction(float value)                  { pm_walkFriction = value; }
void PM_SetWaterSwimScale(float value)                { pm_waterSwimScale = value; }
void PM_SetWaterWadeScale(float value)                { pm_waterWadeScale = value; }
void PM_SetWeaponDropTime(int value)                  { pm_weaponDropTime = value; }
void PM_SetWeaponRaiseTime(int value)                 { pm_weaponRaiseTime = value; }
void PM_SetWishSpeed(float value)                     { pm_wishSpeed = value; pm_cfg_wishSpeed = value; }
void PM_SetDoubleJump(int value)                      { pm_doubleJump = value; }
void PM_SetCrouchSlide(int value)                     { pm_crouchSlide = value; }
void PM_SetVelocityGH(float value)                    { pm_velocityGH = value; }

/*
==================
PM_SetMovementConfig - [QL] set VQ3 or CPM physics mode based on pm_flags.
Called at start of each PmoveSingle.

CPM mode hardcodes specific physics values.
VQ3 mode restores values from the "configured" backup globals, which are set
by PM_Set* from the server's CS_PMOVEINFO configstring. This allows gametypes
and factories to customize VQ3 physics without the CPM overrides persisting.
==================
*/
void PM_SetMovementConfig(void) {
    if (pm->ps->pm_flags & PMF_PROMODE) {
        // CPM/PQL mode - hardcoded physics values
        pm_airControl = 150.0f;
        pm_airAccel = 1.1f;
        pm_airStopAccel = 2.5f;
        pm_chainJump = 3;
        pm_circleStrafeFriction = 5.5f;
        pm_strafeAccel = 70.0f;
        pm_walkAccel = 15.0f;
        pm_wishSpeed = 35.0f;
    } else {
        // VQ3 mode - restore server-configured values
        pm_airControl = 0.0f;
        pm_airAccel = 1.0f;
        pm_airStopAccel = 1.0f;
        pm_strafeAccel = 1.0f;
        pm_chainJump = 1;
        pm_walkAccel = pm_cfg_walkAccel;
        pm_circleStrafeFriction = pm_cfg_circleStrafeFriction;
        pm_wishSpeed = pm_cfg_wishSpeed;
    }
}

/*
===============
PM_AddEvent

===============
*/
void PM_AddEvent(int newEvent) {
    BG_AddPredictableEventToPlayerstate(newEvent, 0, pm->ps);
}

/*
===============
PM_AddTouchEnt
===============
*/
void PM_AddTouchEnt(int entityNum) {
    int i;

    if (entityNum == ENTITYNUM_WORLD) {
        return;
    }
    if (pm->numtouch == MAXTOUCH) {
        return;
    }

    // see if it is already added
    for (i = 0; i < pm->numtouch; i++) {
        if (pm->touchents[i] == entityNum) {
            return;
        }
    }

    // add it
    pm->touchents[pm->numtouch] = entityNum;
    pm->numtouch++;
}

/*
===================
PM_StartTorsoAnim
===================
*/
static void PM_StartTorsoAnim(int anim) {
    if (pm->ps->pm_type >= PM_DEAD) {
        return;
    }
    pm->ps->torsoAnim = ((pm->ps->torsoAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
}
static void PM_StartLegsAnim(int anim) {
    if (pm->ps->pm_type >= PM_DEAD) {
        return;
    }
    if (pm->ps->legsTimer > 0) {
        return;  // a high priority animation is running
    }
    pm->ps->legsAnim = ((pm->ps->legsAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
}

static void PM_ContinueLegsAnim(int anim) {
    if ((pm->ps->legsAnim & ~ANIM_TOGGLEBIT) == anim) {
        return;
    }
    if (pm->ps->legsTimer > 0) {
        return;  // a high priority animation is running
    }
    PM_StartLegsAnim(anim);
}

static void PM_ContinueTorsoAnim(int anim) {
    if ((pm->ps->torsoAnim & ~ANIM_TOGGLEBIT) == anim) {
        return;
    }
    if (pm->ps->torsoTimer > 0) {
        return;  // a high priority animation is running
    }
    PM_StartTorsoAnim(anim);
}

static void PM_ForceLegsAnim(int anim) {
    pm->ps->legsTimer = 0;
    PM_StartLegsAnim(anim);
}

/*
==================
PM_ClipVelocity

Slide off of the impacting surface
==================
*/
void PM_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, float overbounce) {
    float backoff;
    float change;
    int i;

    backoff = DotProduct(in, normal);

    if (backoff < 0) {
        backoff *= overbounce;
    } else {
        backoff /= overbounce;
    }

    for (i = 0; i < 3; i++) {
        change = normal[i] * backoff;
        out[i] = in[i] - change;
    }
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
static void PM_Friction(void) {
    float* vel;
    float speed, newspeed, control;
    float drop;

    vel = pm->ps->velocity;

    // [QL] always use 2D speed for friction calculation
    speed = sqrt(vel[0] * vel[0] + vel[1] * vel[1]);

    if (speed < 1) {
        vel[0] = 0;
        vel[1] = 0;
        return;
    }

    drop = 0;

    // apply ground friction
    if (pm->waterlevel < 2 && pml.walking
        && !(pml.groundTrace.surfaceFlags & SURF_SLICK)
        && !(pm->ps->pm_flags & PMF_TIME_KNOCKBACK)) {
        control = speed < pm_stopspeed ? pm_stopspeed : speed;

        // [QL] three-tier friction: crouchSlide > circleStrafe > walk
        if ((pm->ps->pm_flags & PMF_CROUCH_SLIDE)
            && pm->cmd.upmove < 0 && pm->ps->crouchSlideTime > 0) {
            // crouch sliding on ground
            drop += control * pm_crouchSlideFriction * pml.frametime;
        } else if ((pm->ps->pm_flags & PMF_PROMODE)
                   && pm->cmd.forwardmove != 0
                   && pm->cmd.rightmove != 0
                   && (pm->ps->movementDir & 0xFFFFFFF9) == 1) {
            // diagonal forward+strafe in promode - higher friction
            drop += control * pm_circleStrafeFriction * pml.frametime;
        } else {
            drop += control * pm_walkFriction * pml.frametime;
        }
    }

    // [QL] water friction - always additive for any waterlevel > 0, unless on ladder
    if (pm->waterlevel > 0 && !pml.ladder) {
        drop += speed * (float)pm->waterlevel * pml.frametime;
    }

    // [QL] spectator friction (flight friction removed in QL)
    if (pm->ps->pm_type == PM_SPECTATOR) {
        drop += speed * pm_spectatorfriction * pml.frametime;
    }

    // [QL] reduce friction when frozen or in scoreboard
    if (pm->ps->pm_type == PM_FREEZE || (pm->ps->pm_flags & PMF_SCOREBOARD)) {
        drop *= 0.25f;
    }

    // [QL] ladder friction
    if (pml.ladder) {
        drop += speed * 6.0f * pml.frametime;
    }

    // scale the velocity
    newspeed = speed - drop;
    if (newspeed < 0) {
        newspeed = 0;
    }
    newspeed /= speed;

    vel[0] *= newspeed;
    vel[1] *= newspeed;
    vel[2] *= newspeed;
}

/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/
static void PM_Accelerate(vec3_t wishdir, float wishspeed, float accel) {
    float addspeed, accelspeed, currentspeed;
    float speed2d, maxspeed, maxspeed2;

    currentspeed = DotProduct(pm->ps->velocity, wishdir);
    addspeed = wishspeed - currentspeed;

    speed2d = sqrt(pm->ps->velocity[0] * pm->ps->velocity[0]
                 + pm->ps->velocity[1] * pm->ps->velocity[1]);
    maxspeed = (float)pm->ps->speed;
    maxspeed2 = maxspeed + maxspeed;

    if (addspeed > 0) {
        // normal acceleration
        accelspeed = accel * pml.frametime * wishspeed;
        if (accelspeed > addspeed) {
            accelspeed = addspeed;
        }

        pm->ps->velocity[0] += accelspeed * wishdir[0];
        pm->ps->velocity[1] += accelspeed * wishdir[1];
        pm->ps->velocity[2] += accelspeed * wishdir[2];
        return;
    }

    // [QL] bunny-hop overacceleration - when already at/above wishspeed,
    // add diminishing acceleration instead of just returning (CPM mechanic).
    // Only applies when moving forward (movementDir == 0).
    if (pm_bunnyHop && pm->ps->movementDir == 0 && speed2d <= maxspeed2) {
        float overaccel = pm_bunnyHopOveraccel;
        if (speed2d > maxspeed) {
            // linear ramp: overaccel at maxspeed, 0 at 2*maxspeed
            if (speed2d > maxspeed2) {
                speed2d = maxspeed2;
            }
            overaccel = ((maxspeed2 - speed2d) / maxspeed) * 0.55f;
        }

        pm->ps->velocity[0] += overaccel * wishdir[0];
        pm->ps->velocity[1] += overaccel * wishdir[1];
        pm->ps->velocity[2] += overaccel * wishdir[2];
    }
}

/*
============
PM_CmdScale

Returns the scale factor to apply to cmd movements
This allows the clients to use axial -127 to 127 values for all directions
without getting a sqrt(2) distortion in speed.
============
*/
static float PM_CmdScale(usercmd_t* cmd) {
    int max;
    float total;
    float scale;

    max = abs(cmd->forwardmove);
    if (abs(cmd->rightmove) > max) {
        max = abs(cmd->rightmove);
    }
    if (abs(cmd->upmove) > max) {
        max = abs(cmd->upmove);
    }
    if (!max) {
        return 0;
    }

    total = sqrt(cmd->forwardmove * cmd->forwardmove + cmd->rightmove * cmd->rightmove + cmd->upmove * cmd->upmove);
    scale = (float)pm->ps->speed * max / (127.0 * total);

    return scale;
}

/*
================
PM_SetMovementDir

Determine the rotation of the legs relative
to the facing dir
================
*/
static void PM_SetMovementDir(void) {
    if (pm->cmd.forwardmove || pm->cmd.rightmove) {
        if (pm->cmd.rightmove == 0 && pm->cmd.forwardmove > 0) {
            pm->ps->movementDir = 0;
        } else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove > 0) {
            pm->ps->movementDir = 1;
        } else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove == 0) {
            pm->ps->movementDir = 2;
        } else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove < 0) {
            pm->ps->movementDir = 3;
        } else if (pm->cmd.rightmove == 0 && pm->cmd.forwardmove < 0) {
            pm->ps->movementDir = 4;
        } else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove < 0) {
            pm->ps->movementDir = 5;
        } else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove == 0) {
            pm->ps->movementDir = 6;
        } else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove > 0) {
            pm->ps->movementDir = 7;
        }
    } else {
        // if they aren't actively going directly sideways,
        // change the animation to the diagonal so they
        // don't stop too crooked
        if (pm->ps->movementDir == 2) {
            pm->ps->movementDir = 1;
        } else if (pm->ps->movementDir == 6) {
            pm->ps->movementDir = 7;
        }
    }
}

/*
=============
PM_CheckLadder - [QL] check if player is on a ladder surface
=============
*/
static void PM_CheckLadder(void) {
    vec3_t flatforward;
    vec3_t spot;
    trace_t trace;

    pml.ladder = qfalse;

    flatforward[0] = pml.forward[0];
    flatforward[1] = pml.forward[1];
    flatforward[2] = 0;
    VectorNormalize(flatforward);

    VectorMA(pm->ps->origin, 1, flatforward, spot);

    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, spot,
              pm->ps->clientNum, CONTENTS_SOLID);

    if (trace.fraction < 1.0 && (trace.surfaceFlags & SURF_LADDER)) {
        pml.ladder = qtrue;
    }
}

/*
=============
PM_LadderMove - [QL] movement on ladder surfaces
=============
*/
static void PM_LadderMove(void) {
    int i;
    vec3_t wishvel;
    vec3_t wishdir;
    float wishspeed;
    float scale;
    float fmove, smove;
    float vel;

    PM_Friction();

    scale = PM_CmdScale(&pm->cmd);

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    if (scale != 0) {
        for (i = 0; i < 3; i++) {
            wishvel[i] = scale * pml.forward[i] * fmove + scale * pml.right[i] * smove;
        }
        wishvel[2] += scale * pm->cmd.upmove;

        // upmove drives vertical speed on ladders at 66% of ground speed
        if (pm->cmd.upmove > 0) {
            wishvel[2] = pm->ps->speed * 0.66f;
        } else if (pm->cmd.upmove < 0) {
            // going down on ladder
            pm->ps->pm_flags &= ~PMF_DUCKED;
            pm->ps->legsAnim = LEGS_IDLE;  // 26
            wishvel[2] = -pm->ps->speed * 0.66f;
        }
    } else {
        wishvel[0] = 0;
        wishvel[1] = 0;
        wishvel[2] = 0;
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    // clamp to 66% of speed
    vel = pm->ps->speed * 0.66f;
    if (wishspeed > vel) {
        wishspeed = vel;
    }

    PM_Accelerate(wishdir, wishspeed, pm_walkAccel);

    // if moving against the ground, clip and preserve speed
    if (pml.groundPlane && DotProduct(pm->ps->velocity, pml.groundTrace.plane.normal) < 0.0f) {
        vel = VectorLength(pm->ps->velocity);
        PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                        pm->ps->velocity, OVERCLIP);
        VectorNormalize(pm->ps->velocity);
        VectorScale(pm->ps->velocity, vel, pm->ps->velocity);
    }

    PM_SlideMove(qfalse);
}

/*
=============
PM_DoJump - [QL] execute the actual jump with chain-jump and ramp-jump support
=============
*/
void PM_DoJump(void) {
    float jumpVel = pm_jumpVelocity;
    float timeDelta;

    pml.groundPlane = qfalse;
    pml.walking = qfalse;
    pm->ps->pm_flags |= PMF_JUMP_HELD;

    // [QL] chain-jump: boost velocity based on time since last jump
    timeDelta = (float)(pm->cmd.serverTime - pm->ps->jumpTime);
    if (timeDelta > 0 && timeDelta < pm_jumpVelocityTimeThreshold) {
        if (pm_chainJump == 2) {
            // additive mode
            if (pml.stepJumpFlag == 1) {
                jumpVel += pm_stepJumpVelocity;
            } else {
                jumpVel += pm_chainJumpVelocity;
            }
        } else if (pm_chainJump == 3) {
            // interpolated mode
            float threshold = pm_jumpVelocityTimeThreshold * pm_jumpVelocityTimeThresholdOffset;
            if (timeDelta < threshold) {
                if (pml.stepJumpFlag == 1) {
                    jumpVel += pm_stepJumpVelocity;
                } else {
                    jumpVel += pm_chainJumpVelocity;
                }
            } else {
                jumpVel += pm_chainJumpVelocity + ((threshold - timeDelta) / threshold) * pm_chainJumpVelocity;
            }
        } else if (pm_chainJump == 1) {
            // scale mode
            float factor = ((pm_jumpVelocityTimeThreshold - timeDelta) / pm_jumpVelocityTimeThreshold) * pm_jumpVelocityScaleAdd + 1.0f;
            jumpVel *= factor;
        }
    }

    // [QL] ramp-jump: preserve and add to existing upward velocity
    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    if (pm_rampJump && pml.crouchStepJumpFlag != 1) {
        pm->ps->velocity[2] *= pm_rampJumpScale;
        pm->ps->velocity[2] += jumpVel;
        if (pm->ps->velocity[2] < jumpVel) {
            pm->ps->velocity[2] = jumpVel;
        }
        if (pm->ps->velocity[2] > pm_jumpVelocityMax) {
            pm->ps->velocity[2] = pm_jumpVelocityMax;
        }
    } else {
        pm->ps->velocity[2] = jumpVel;
    }

    PM_AddEvent(EV_JUMP);

    if (pm->cmd.forwardmove >= 0) {
        PM_ForceLegsAnim(LEGS_JUMP);
        pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
    } else {
        PM_ForceLegsAnim(LEGS_JUMPB);
        pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
    }

    // record jump time for chain-jump calculations
    pm->ps->jumpTime = pm->cmd.serverTime;
}

/*
=============
PM_WouldJump - [QL] check if a jump would be executed (for step-jump prediction)
=============
*/
qboolean PM_WouldJump(void) {
    // [QL] zoom blocks jumping unless autoHop overrides
    if (pm_autoHop && !(pm->ps->pm_flags & PMF_NO_AUTOHOP)) {
        // autoHop allows jump while holding and bypasses zoom check
    } else if (pm->ps->pm_flags & PMF_ZOOM) {
        // can't jump while zoomed
        return qfalse;
    }

    if (pm->cmd.upmove <= 9) {
        return qfalse;
    }

    if ((float)(pm->cmd.serverTime - pm->ps->jumpTime) < pm_jumpTimeDeltaMin) {
        return qfalse;
    }

    // [QL] autoHop or not holding jump
    if ((pm_autoHop || !(pm->ps->pm_flags & PMF_JUMP_HELD))
        && (!(pm->ps->pm_flags & PMF_NO_AUTOHOP) || !(pm->ps->pm_flags & PMF_JUMP_HELD))) {
        return qtrue;
    }

    // holding jump without autoHop - consume and reject
    pm->cmd.upmove = 0;
    return qfalse;
}

/*
=============
PM_WouldCrouchSlideJump - [QL] check if a crouch-slide jump would occur
=============
*/
static qboolean PM_WouldCrouchSlideJump(void) {
    if (!(pm->ps->pm_flags & PMF_DUCKED)) {
        return qfalse;
    }
    if (pml.groundPlane) {
        return qfalse;
    }
    if (pm->ps->velocity[2] < 0) {
        return qfalse;
    }
    if ((float)(pm->cmd.serverTime - pm->ps->jumpTime) < pm_jumpVelocityTimeThreshold) {
        return qfalse;
    }
    return qtrue;
}

/*
=============
PM_CheckJump - [QL] rewritten with autoHop, chain-jump, crouch-slide support
=============
*/
static qboolean PM_CheckJump(void) {
    if (pm->ps->pm_flags & PMF_RESPAWNED) {
        return qfalse;
    }

    if (!PM_WouldJump()) {
        return qfalse;
    }

    // [QL] reset crouchSlideTime when crouch-slide flag is active
    if (pm->ps->pm_flags & PMF_CROUCH_SLIDE) {
        pm->ps->crouchSlideTime = 0;
    }

    PM_DoJump();

    return qtrue;
}

/*
=============
PM_CheckDoubleJump - [QL] allow a second jump while airborne if pmove_DoubleJump enabled
=============
*/
static qboolean PM_CheckDoubleJump(void) {
    if (!PM_WouldJump()) {
        return qfalse;
    }

    // can't double-jump if PMF_DOUBLE_JUMPED is set and JUMP_HELD
    if ((pm->ps->pm_flags & PMF_DOUBLE_JUMPED) && (pm->ps->pm_flags & PMF_JUMP_HELD)) {
        return qfalse;
    }

    // already used the double-jump
    if ((pm->ps->pm_flags & PMF_DOUBLE_JUMPED) && pm->ps->doubleJumped) {
        return qfalse;
    }

    // reset crouch slide
    if (pm->ps->pm_flags & PMF_CROUCH_SLIDE) {
        pm->ps->crouchSlideTime = 0;
    }

    PM_DoJump();

    // mark that double-jump has been used
    if (pm->ps->pm_flags & PMF_DOUBLE_JUMPED) {
        pm->ps->doubleJumped = 1;
    }

    return qtrue;
}

/*
=============
PM_CheckWaterJump
=============
*/
static qboolean PM_CheckWaterJump(void) {
    vec3_t spot;
    int cont;
    vec3_t flatforward;

    if (pm->ps->pm_time) {
        return qfalse;
    }

    // check for water jump
    if (pm->waterlevel != 2) {
        return qfalse;
    }

    flatforward[0] = pml.forward[0];
    flatforward[1] = pml.forward[1];
    flatforward[2] = 0;
    VectorNormalize(flatforward);

    VectorMA(pm->ps->origin, 30, flatforward, spot);
    spot[2] += 4;
    cont = pm->pointcontents(spot, pm->ps->clientNum);
    if (!(cont & CONTENTS_SOLID)) {
        return qfalse;
    }

    spot[2] += 16;
    cont = pm->pointcontents(spot, pm->ps->clientNum);
    if (cont & (CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY)) {
        return qfalse;
    }

    // jump out of water
    VectorScale(pml.forward, 200, pm->ps->velocity);
    pm->ps->velocity[2] = 350;

    pm->ps->pm_flags |= PMF_TIME_WATERJUMP;
    pm->ps->pm_time = 2000;

    return qtrue;
}

//============================================================================

/*
===================
PM_WaterJumpMove

Flying out of the water
===================
*/
static void PM_WaterJumpMove(void) {
    // waterjump has no control, but falls

    PM_StepSlideMove(qtrue);

    pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
    if (pm->ps->velocity[2] < 0) {
        // cancel as soon as we are falling down again
        pm->ps->pm_flags &= ~PMF_ALL_TIMES;
        pm->ps->pm_time = 0;
    }
}

/*
===================
PM_WaterMove

===================
*/
static void PM_WaterMove(void) {
    int i;
    vec3_t wishvel;
    float wishspeed;
    vec3_t wishdir;
    float scale;
    float vel;

    if (PM_CheckWaterJump()) {
        PM_WaterJumpMove();
        return;
    }
#if 0
	// jump = head for surface
	if (pm->cmd.upmove >= 10) {
		if (pm->ps->velocity[2] > -300) {
			if (pm->watertype & CONTENTS_WATER) {
				pm->ps->velocity[2] = 100;
			} else if (pm->watertype & CONTENTS_SLIME) {
				pm->ps->velocity[2] = 80;
			} else {
				pm->ps->velocity[2] = 50;
			}
		}
	}
#endif
    PM_Friction();

    scale = PM_CmdScale(&pm->cmd);
    //
    // user intentions
    //
    if (!scale) {
        wishvel[0] = 0;
        wishvel[1] = 0;
        wishvel[2] = -60;  // sink towards bottom
    } else {
        for (i = 0; i < 3; i++)
            wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;

        wishvel[2] += scale * pm->cmd.upmove;
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    // [QL] clamp speed based on waterlevel using QL-specific scale globals
    {
        float waterScale;
        if (pm->waterlevel == 1) {
            waterScale = pm_waterWadeScale;
        } else {
            waterScale = pm_waterSwimScale;
        }
        if (wishspeed > pm->ps->speed * waterScale) {
            wishspeed = pm->ps->speed * waterScale;
        }
    }

    PM_Accelerate(wishdir, wishspeed, pm_wateraccelerate);

    // make sure we can go up slopes easily under water
    if (pml.groundPlane && DotProduct(pm->ps->velocity, pml.groundTrace.plane.normal) < 0) {
        vel = VectorLength(pm->ps->velocity);
        // slide along the ground plane
        PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                        pm->ps->velocity, OVERCLIP);

        VectorNormalize(pm->ps->velocity);
        VectorScale(pm->ps->velocity, vel, pm->ps->velocity);
    }

    PM_SlideMove(qfalse);
}

/*
===================
PM_InvulnerabilityMove

Only with the invulnerability powerup
===================
*/
static void PM_InvulnerabilityMove(void) {
    pm->cmd.forwardmove = 0;
    pm->cmd.rightmove = 0;
    pm->cmd.upmove = 0;
    VectorClear(pm->ps->velocity);
}

/*
===================
PM_FlyMove

Only with the flight powerup
===================
*/
static void PM_FlyMove(void) {
    int i;
    vec3_t wishvel;
    float wishspeed;
    vec3_t wishdir;
    float scale;

    // normal slowdown
    PM_Friction();

    scale = PM_CmdScale(&pm->cmd);
    //
    // user intentions
    //
    if (!scale) {
        wishvel[0] = 0;
        wishvel[1] = 0;
        wishvel[2] = 0;
    } else {
        for (i = 0; i < 3; i++) {
            wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;
        }

        wishvel[2] += scale * pm->cmd.upmove;
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    PM_Accelerate(wishdir, wishspeed, pm_flyaccelerate);

    PM_StepSlideMove(qfalse);
}

/*
===================
PM_AirControl - [QL] CPM-style air control
===================
*/
static void PM_AirControl(vec3_t wishdir, float wishspeed) {
    float zspeed, speed, dot, k;
    int i;

    if (pm_airControl <= 0) {
        return;
    }
    // [QL] air control only for forward/backward movement (not diagonal or strafing)
    if ((pm->ps->movementDir != 0 && pm->ps->movementDir != 4) || wishspeed == 0) {
        return;
    }

    zspeed = pm->ps->velocity[2];
    pm->ps->velocity[2] = 0;
    speed = VectorNormalize(pm->ps->velocity);

    dot = DotProduct(pm->ps->velocity, wishdir);
    k = 32.0f * pm_airControl * dot * dot * pml.frametime;

    if (dot > 0) {
        for (i = 0; i < 2; i++) {
            pm->ps->velocity[i] = pm->ps->velocity[i] * speed + wishdir[i] * k;
        }
        VectorNormalize(pm->ps->velocity);
    }

    for (i = 0; i < 2; i++) {
        pm->ps->velocity[i] *= speed;
    }
    pm->ps->velocity[2] = zspeed;
}

/*
===================
PM_AirMove - [QL] rewritten with air control, per-axis acceleration
===================
*/
static void PM_AirMove(void) {
    int i;
    vec3_t wishvel;
    float fmove, smove;
    vec3_t wishdir;
    float wishspeed;
    float scale;
    float accel;
    usercmd_t cmd;

    // [QL] charge crouch slide timer while airborne with crouch slide active
    if ((pm->ps->pm_flags & PMF_CROUCH_SLIDE) && pm->cmd.upmove >= 0) {
        pm->ps->crouchSlideTime += pml.msec * 5;
        if (pm->ps->crouchSlideTime > pm_crouchSlideTime) {
            pm->ps->crouchSlideTime = pm_crouchSlideTime;
        }
    }

    PM_Friction();

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    cmd = pm->cmd;
    scale = PM_CmdScale(&cmd);

    PM_SetMovementDir();

    // project moves down to flat plane
    pml.forward[2] = 0;
    pml.right[2] = 0;
    VectorNormalize(pml.forward);
    VectorNormalize(pml.right);

    for (i = 0; i < 2; i++) {
        wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
    }
    wishvel[2] = 0;

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;

    // [QL] air acceleration
    accel = pm_airAccel;
    if (pm->ps->pm_flags & PMF_PROMODE) {
        // CPM/PQL mode: air stop, strafe, and air control
        if (DotProduct(pm->ps->velocity, wishdir) < 0) {
            accel = pm_airStopAccel;
        }
        if (pm->ps->movementDir == 2 || pm->ps->movementDir == 6) {
            // pure strafing - cap wishspeed and override accel
            if (wishspeed > pm_wishSpeed) {
                wishspeed = pm_wishSpeed;
            }
            accel = pm_strafeAccel;
        }
    }

    PM_Accelerate(wishdir, wishspeed, accel);

    // [QL] air control applies to all movement directions in promode
    if (pm->ps->pm_flags & PMF_PROMODE) {
        PM_AirControl(wishdir, wishspeed);
    }

    // slide along steep ground planes
    if (pml.groundPlane) {
        PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                        pm->ps->velocity, OVERCLIP);
    }

    PM_StepSlideMove(qtrue);

    // [QL] double-jump: allow a second jump while airborne
    if (pm->ps->pm_flags & PMF_DOUBLE_JUMPED) {
        PM_CheckDoubleJump();
    }
}

/*
===================
PM_GrappleMove

===================
*/
static void PM_GrappleMove(void) {
    vec3_t vel, v;
    float vlen;

    if (pm_hookPullVelocity == 0.0f) {
        return;
    }

    VectorScale(pml.forward, -16, v);
    VectorAdd(pm->ps->grapplePoint, v, v);
    VectorSubtract(v, pm->ps->origin, vel);
    vlen = VectorLength(vel);
    VectorNormalize(vel);

    VectorScale(vel, pm_hookPullVelocity, vel);

    // [QL] close-range slowdown: threshold is 10% of pull velocity
    if (vlen <= pm_hookPullVelocity * 0.1f) {
        float scale = (vlen * 10.0f) / pm_hookPullVelocity;
        VectorScale(vel, scale, vel);
    }

    // [QL] enemy hook: reduced pull speed (25%); wall hook: clear ground plane
    if (pm->hookEnemy) {
        VectorScale(vel, 0.25f, vel);
    } else {
        pml.groundPlane = qfalse;
    }

    // [QL] water damping: 75% speed in deep water
    if (pm->waterlevel > 1) {
        VectorScale(vel, 0.75f, vel);
    }

    VectorCopy(vel, pm->ps->velocity);
}

/*
===================
PM_WalkMove

===================
*/
static void PM_WalkMove(void) {
    int i;
    vec3_t wishvel;
    float fmove, smove;
    vec3_t wishdir;
    float wishspeed;
    float scale;
    usercmd_t cmd;
    float accelerate;

    if (pm->waterlevel > 2 && DotProduct(pml.forward, pml.groundTrace.plane.normal) > 0) {
        PM_WaterMove();
        return;
    }

    // [QL] grapple-while-grounded: if using grapple hook and pulling, slide without jumping
    if (pm->ps->weapon == WP_GRAPPLING_HOOK &&
        (pm->ps->pm_flags & PMF_RESPAWNED) &&
        pm->ps->pm_type != PM_DEAD) {
        PM_StepSlideMove(qfalse);
        return;
    }

    if (PM_CheckJump()) {
        if (pm->waterlevel > 1) {
            PM_WaterMove();
        } else {
            PM_AirMove();
        }
        return;
    }

    PM_Friction();

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    cmd = pm->cmd;
    scale = PM_CmdScale(&cmd);

    PM_SetMovementDir();

    // project moves down to flat plane
    pml.forward[2] = 0;
    pml.right[2] = 0;

    // project the forward and right directions onto the ground plane
    PM_ClipVelocity(pml.forward, pml.groundTrace.plane.normal, pml.forward, OVERCLIP);
    PM_ClipVelocity(pml.right, pml.groundTrace.plane.normal, pml.right, OVERCLIP);
    VectorNormalize(pml.forward);
    VectorNormalize(pml.right);

    for (i = 0; i < 3; i++) {
        wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;

    // [QL] clamp speed: crouch slide gets 75%, normal duck gets 25%
    if ((pm->ps->pm_flags & PMF_CROUCH_SLIDE) && (pm->ps->pm_flags & PMF_DUCKED)) {
        if (wishspeed > pm->ps->speed * 0.75f) {
            wishspeed = pm->ps->speed * 0.75f;
        }
    } else if (pm->ps->pm_flags & PMF_DUCKED) {
        if (wishspeed > pm->ps->speed * pm_duckScale) {
            wishspeed = pm->ps->speed * pm_duckScale;
        }
    }

    // [QL] clamp speed in water using interpolated scale based on waterlevel
    if (pm->waterlevel) {
        float waterScale;
        if (pm->waterlevel == 1) {
            waterScale = pm_waterWadeScale;
        } else {
            waterScale = pm_waterSwimScale;
        }
        float scaledSpeed = 1.0f - ((float)pm->waterlevel / 3.0f) * (1.0f - waterScale);
        if (wishspeed > pm->ps->speed * scaledSpeed) {
            wishspeed = pm->ps->speed * scaledSpeed;
        }
    }

    // [QL] acceleration - slick/knockback uses reduced accel, ducked on slick gets 1.4
    if ((pml.groundTrace.surfaceFlags & SURF_SLICK) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK) {
        accelerate = pm_airaccelerate;
        if ((pm->ps->pm_flags & PMF_DUCKED) && (pml.groundTrace.surfaceFlags & SURF_SLICK)) {
            accelerate = 1.4f;
        }
    } else {
        accelerate = pm_walkAccel;  // [QL] configurable walk acceleration
    }

    PM_Accelerate(wishdir, wishspeed, accelerate);

    if ((pml.groundTrace.surfaceFlags & SURF_SLICK) || pm->ps->pm_flags & PMF_TIME_KNOCKBACK) {
        pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
    }

    // [QL] slide along the ground plane - no velocity magnitude preservation
    PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                    pm->ps->velocity, OVERCLIP);

    if (!pm->ps->velocity[0] && !pm->ps->velocity[1]) {
        return;
    }

    PM_StepSlideMove(qfalse);
}

/*
==============
PM_DeadMove
==============
*/
static void PM_DeadMove(void) {
    float forward;

    if (!pml.walking) {
        return;
    }

    // extra friction

    forward = VectorLength(pm->ps->velocity);
    forward -= 20;
    if (forward <= 0) {
        VectorClear(pm->ps->velocity);
    } else {
        VectorNormalize(pm->ps->velocity);
        VectorScale(pm->ps->velocity, forward, pm->ps->velocity);
    }
}

/*
===============
PM_NoclipMove
===============
*/
static void PM_NoclipMove(void) {
    float speed, drop, friction, control, newspeed;
    int i;
    vec3_t wishvel;
    float fmove, smove;
    vec3_t wishdir;
    float wishspeed;
    float scale;

    pm->ps->viewheight = DEFAULT_VIEWHEIGHT;

    // friction

    speed = VectorLength(pm->ps->velocity);
    if (speed < 1) {
        VectorCopy(vec3_origin, pm->ps->velocity);
    } else {
        drop = 0;

        friction = pm_friction * 1.5;  // extra friction
        control = speed < pm_stopspeed ? pm_stopspeed : speed;
        drop += control * friction * pml.frametime;

        // scale the velocity
        newspeed = speed - drop;
        if (newspeed < 0)
            newspeed = 0;
        newspeed /= speed;

        VectorScale(pm->ps->velocity, newspeed, pm->ps->velocity);
    }

    // accelerate
    scale = PM_CmdScale(&pm->cmd);

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    for (i = 0; i < 3; i++)
        wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
    wishvel[2] += pm->cmd.upmove;

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;

    PM_Accelerate(wishdir, wishspeed, pm_accelerate);

    // move
    VectorMA(pm->ps->origin, pml.frametime, pm->ps->velocity, pm->ps->origin);
}

//============================================================================

/*
================
PM_FootstepForSurface

Returns an event number appropriate for the groundsurface
================
*/
static int PM_FootstepForSurface(void) {
    if (pml.groundTrace.surfaceFlags & SURF_NOSTEPS) {
        return 0;
    }
    if (pml.groundTrace.surfaceFlags & SURF_METALSTEPS) {
        return EV_FOOTSTEP_METAL;
    }
    // [QL] snow and wood surface footsteps
    if (pml.groundTrace.surfaceFlags & SURF_SNOWSTEPS) {
        return EV_FOOTSTEP_SNOW;
    }
    if (pml.groundTrace.surfaceFlags & SURF_WOODSTEPS) {
        return EV_FOOTSTEP_WOOD;
    }
    return EV_FOOTSTEP;
}

/*
=================
PM_CrashLand

Check for hard landings that generate sound events
=================
*/
static void PM_CrashLand(void) {
    float delta;
    float dist;
    float vel, acc;
    float t;
    float a, b, c, den;

    // decide which landing animation to use
    if (pm->ps->pm_flags & PMF_BACKWARDS_JUMP) {
        PM_ForceLegsAnim(LEGS_LANDB);
    } else {
        PM_ForceLegsAnim(LEGS_LAND);
    }

    pm->ps->legsTimer = TIMER_LAND;

    // calculate the exact velocity on landing
    dist = pm->ps->origin[2] - pml.previous_origin[2];
    vel = pml.previous_velocity[2];
    acc = -pm->ps->gravity;

    a = acc / 2;
    b = vel;
    c = -dist;

    den = b * b - 4 * a * c;
    if (den < 0) {
        return;
    }
    t = (-b - sqrt(den)) / (2 * a);

    delta = vel + t * acc;
    delta = delta * delta * 0.0001;

    // ducking while falling doubles damage
    if (pm->ps->pm_flags & PMF_DUCKED) {
        delta *= 2;
    }

    // never take falling damage if completely underwater
    if (pm->waterlevel == 3) {
        return;
    }

    // reduce falling damage if there is standing water
    if (pm->waterlevel == 2) {
        delta *= 0.25;
    }
    if (pm->waterlevel == 1) {
        delta *= 0.5;
    }

    if (delta < 1) {
        return;
    }

    // create a local entity event to play the sound

    // SURF_NODAMAGE is used for bounce pads where you don't ever
    // want to take damage or play a crunch sound
    if (!(pml.groundTrace.surfaceFlags & SURF_NODAMAGE)) {
        if (delta > 60) {
            PM_AddEvent(EV_FALL_FAR);
        } else if (delta > 40) {
            // this is a pain grunt, so don't play it if dead
            if (pm->ps->stats[STAT_HEALTH] > 0) {
                PM_AddEvent(EV_FALL_MEDIUM);
            }
        } else if (delta > 7) {
            PM_AddEvent(EV_FALL_SHORT);
        } else {
            PM_AddEvent(PM_FootstepForSurface());
        }
    }

    // start footstep cycle over
    pm->ps->bobCycle = 0;
}

/*
=============
PM_CheckStuck
=============
*/
/*
void PM_CheckStuck(void) {
    trace_t trace;

    pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask);
    if (trace.allsolid) {
        //int shit = qtrue;
    }
}
*/

/*
=============
PM_CorrectAllSolid
=============
*/
static int PM_CorrectAllSolid(trace_t* trace) {
    int i, j, k;
    vec3_t point;

    if (pm->debugLevel) {
        Com_Printf("%i:allsolid\n", c_pmove);
    }

    // jitter around
    for (i = -1; i <= 1; i++) {
        for (j = -1; j <= 1; j++) {
            for (k = -1; k <= 1; k++) {
                VectorCopy(pm->ps->origin, point);
                point[0] += (float)i;
                point[1] += (float)j;
                point[2] += (float)k;
                pm->trace(trace, point, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
                if (!trace->allsolid) {
                    point[0] = pm->ps->origin[0];
                    point[1] = pm->ps->origin[1];
                    point[2] = pm->ps->origin[2] - 0.25;

                    pm->trace(trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
                    pml.groundTrace = *trace;
                    return qtrue;
                }
            }
        }
    }

    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = qfalse;
    pml.walking = qfalse;

    return qfalse;
}

/*
=============
PM_GroundTraceMissed

The ground trace didn't hit a surface, so we are in freefall
=============
*/
static void PM_GroundTraceMissed(void) {
    trace_t trace;
    vec3_t point;

    if (pm->ps->groundEntityNum != ENTITYNUM_NONE) {
        // we just transitioned into freefall
        if (pm->debugLevel) {
            Com_Printf("%i:lift\n", c_pmove);
        }

        // if they aren't in a jumping animation and the ground is a ways away, force into it
        // if we didn't do the trace, the player would be backflipping down staircases
        VectorCopy(pm->ps->origin, point);
        point[2] -= 64;

        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
        if (trace.fraction == 1.0) {
            if (pm->cmd.forwardmove >= 0) {
                PM_ForceLegsAnim(LEGS_JUMP);
                pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
            } else {
                PM_ForceLegsAnim(LEGS_JUMPB);
                pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
            }
        }
    }

    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = qfalse;
    pml.walking = qfalse;
}

/*
=============
PM_GroundTrace
=============
*/
static void PM_GroundTrace(void) {
    vec3_t point;
    trace_t trace;

    point[0] = pm->ps->origin[0];
    point[1] = pm->ps->origin[1];
    point[2] = pm->ps->origin[2] - 0.25;

    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
    pml.groundTrace = trace;

    // do something corrective if the trace starts in a solid...
    if (trace.allsolid) {
        if (!PM_CorrectAllSolid(&trace))
            return;
    }

    // if the trace didn't hit anything, we are in free fall
    if (trace.fraction == 1.0) {
        PM_GroundTraceMissed();
        pml.groundPlane = qfalse;
        pml.walking = qfalse;
        return;
    }

    // check if getting thrown off the ground
    if (pm->ps->velocity[2] > 0 && DotProduct(pm->ps->velocity, trace.plane.normal) > 10) {
        if (pm->debugLevel) {
            Com_Printf("%i:kickoff\n", c_pmove);
        }
        // go into jump animation
        if (pm->cmd.forwardmove >= 0) {
            PM_ForceLegsAnim(LEGS_JUMP);
            pm->ps->pm_flags &= ~PMF_BACKWARDS_JUMP;
        } else {
            PM_ForceLegsAnim(LEGS_JUMPB);
            pm->ps->pm_flags |= PMF_BACKWARDS_JUMP;
        }

        pm->ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = qfalse;
        pml.walking = qfalse;
        return;
    }

    // slopes that are too steep will not be considered onground
    if (trace.plane.normal[2] < MIN_WALK_NORMAL) {
        if (pm->debugLevel) {
            Com_Printf("%i:steep\n", c_pmove);
        }
        // FIXME: if they can't slide down the slope, let them
        // walk (sharp crevices)
        pm->ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = qtrue;
        pml.walking = qfalse;
        return;
    }

    pml.groundPlane = qtrue;
    pml.walking = qtrue;

    // hitting solid ground will end a waterjump
    if (pm->ps->pm_flags & PMF_TIME_WATERJUMP) {
        pm->ps->pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND);
        pm->ps->pm_time = 0;
    }

    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        // just hit the ground
        if (pm->debugLevel) {
            Com_Printf("%i:Land\n", c_pmove);
        }

        PM_CrashLand();

        // [QL] reset double-jump on landing
        pm->ps->doubleJumped = 0;

        // don't do landing time if we were just going down a slope
        if (pml.previous_velocity[2] < -200) {
            // don't allow another jump for a little while
            pm->ps->pm_flags |= PMF_TIME_LAND;
            pm->ps->pm_time = 250;
        }
    }

    pm->ps->groundEntityNum = trace.entityNum;

    // don't reset the z velocity for slopes
    //	pm->ps->velocity[2] = 0;

    PM_AddTouchEnt(trace.entityNum);
}

/*
=============
PM_SetWaterLevel	FIXME: avoid this twice?  certainly if not moving
=============
*/
static void PM_SetWaterLevel(void) {
    vec3_t point;
    int cont;
    int sample1;
    int sample2;

    //
    // get waterlevel, accounting for ducking
    //
    pm->waterlevel = 0;
    pm->watertype = 0;

    point[0] = pm->ps->origin[0];
    point[1] = pm->ps->origin[1];
    point[2] = pm->ps->origin[2] + MINS_Z + 1;
    cont = pm->pointcontents(point, pm->ps->clientNum);

    if (cont & MASK_WATER) {
        sample2 = pm->ps->viewheight - MINS_Z;
        sample1 = sample2 / 2;

        pm->watertype = cont;
        pm->waterlevel = 1;
        point[2] = pm->ps->origin[2] + MINS_Z + sample1;
        cont = pm->pointcontents(point, pm->ps->clientNum);
        if (cont & MASK_WATER) {
            pm->waterlevel = 2;
            point[2] = pm->ps->origin[2] + MINS_Z + sample2;
            cont = pm->pointcontents(point, pm->ps->clientNum);
            if (cont & MASK_WATER) {
                pm->waterlevel = 3;
            }
        }
    }
}

/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->ps->viewheight
==============
*/
static void PM_CheckDuck(void) {
    trace_t trace;

    if (pm->ps->powerups[PW_INVULNERABILITY]) {
        if (pm->ps->pm_flags & PMF_INVULEXPAND) {
            // invulnerability sphere has a 42 units radius
            VectorSet(pm->mins, -42, -42, -42);
            VectorSet(pm->maxs, 42, 42, 42);
        } else {
            VectorSet(pm->mins, -15, -15, MINS_Z);
            VectorSet(pm->maxs, 15, 15, 16);
        }
        pm->ps->pm_flags |= PMF_DUCKED;
        pm->ps->viewheight = CROUCH_VIEWHEIGHT;
        return;
    }
    pm->ps->pm_flags &= ~PMF_INVULEXPAND;

    pm->mins[0] = -15;
    pm->mins[1] = -15;

    pm->maxs[0] = 15;
    pm->maxs[1] = 15;

    pm->mins[2] = MINS_Z;

    if (pm->ps->pm_type == PM_DEAD) {
        pm->maxs[2] = -8;
        pm->ps->viewheight = DEAD_VIEWHEIGHT;
        return;
    }

    if (pm->cmd.upmove < 0) {  // duck
        pm->ps->pm_flags |= PMF_DUCKED;
        // [QL] record crouch start time (separate from slide timer)
        if (pm->ps->crouchTime == 0) {
            pm->ps->crouchTime = pm->cmd.serverTime;
        }
    } else {  // stand up if possible
        if (pm->ps->pm_flags & PMF_DUCKED) {
            // try to stand up
            pm->maxs[2] = 32;
            pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask);
            if (!trace.allsolid)
                pm->ps->pm_flags &= ~PMF_DUCKED;
        }
    }

    if (pm->ps->pm_flags & PMF_DUCKED) {
        // [QL] crouch slide speed boost: ensure minimum speed of 400 when starting a slide
        if (pm->ps->crouchSlideTime == 0 && (pm->ps->pm_flags & PMF_CROUCH_SLIDE)
            && pm->ps->speed < 400) {
            pm->ps->speed = 400;
        }
        pm->maxs[2] = 16;
        pm->ps->viewheight = CROUCH_VIEWHEIGHT;
    } else {
        pm->maxs[2] = 32;
        pm->ps->viewheight = DEFAULT_VIEWHEIGHT;

        // [QL] clear crouch slide timer when standing on ground
        if ((pm->ps->pm_flags & PMF_CROUCH_SLIDE) && pm->ps->crouchSlideTime != 0
            && pml.groundPlane) {
            pm->ps->crouchSlideTime = 0;
        }
    }
}

//===================================================================

/*
===============
PM_Footsteps
===============
*/
static void PM_Footsteps(void) {
    float bobmove;
    int old;
    qboolean footstep;

    //
    // calculate speed and cycle to be used for
    // all cyclic walking effects
    //
    pm->xyspeed = sqrt(pm->ps->velocity[0] * pm->ps->velocity[0] + pm->ps->velocity[1] * pm->ps->velocity[1]);

    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        if (pm->ps->powerups[PW_INVULNERABILITY]) {
            PM_ContinueLegsAnim(LEGS_IDLECR);
        }
        // airborne leaves position in cycle intact, but doesn't advance
        if (pm->waterlevel > 1) {
            PM_ContinueLegsAnim(LEGS_SWIM);
        }
        return;
    }

    // if not trying to move
    if (!pm->cmd.forwardmove && !pm->cmd.rightmove) {
        if (pm->xyspeed < 5) {
            pm->ps->bobCycle = 0;  // start at beginning of cycle again
            if (pm->ps->pm_flags & PMF_DUCKED) {
                PM_ContinueLegsAnim(LEGS_IDLECR);
            } else {
                PM_ContinueLegsAnim(LEGS_IDLE);
            }
        }
        return;
    }

    footstep = qfalse;

    if (pm->ps->pm_flags & PMF_DUCKED) {
        bobmove = 0.5;  // ducked characters bob much faster
        if (pm->ps->pm_flags & PMF_BACKWARDS_RUN) {
            PM_ContinueLegsAnim(LEGS_BACKCR);
        } else {
            PM_ContinueLegsAnim(LEGS_WALKCR);
        }
        // ducked characters never play footsteps
        /*
        } else 	if ( pm->ps->pm_flags & PMF_BACKWARDS_RUN ) {
            if ( !( pm->cmd.buttons & BUTTON_WALKING ) ) {
                bobmove = 0.4;	// faster speeds bob faster
                footstep = qtrue;
            } else {
                bobmove = 0.3;
            }
            PM_ContinueLegsAnim( LEGS_BACK );
        */
    } else {
        if (!(pm->cmd.buttons & BUTTON_WALKING)) {
            bobmove = 0.4f;  // faster speeds bob faster
            if (pm->ps->pm_flags & PMF_BACKWARDS_RUN) {
                PM_ContinueLegsAnim(LEGS_BACK);
            } else {
                PM_ContinueLegsAnim(LEGS_RUN);
            }
            footstep = qtrue;
        } else {
            bobmove = 0.3f;  // walking bobs slow
            if (pm->ps->pm_flags & PMF_BACKWARDS_RUN) {
                PM_ContinueLegsAnim(LEGS_BACKWALK);
            } else {
                PM_ContinueLegsAnim(LEGS_WALK);
            }
        }
    }

    // check for footstep / splash sounds
    old = pm->ps->bobCycle;
    pm->ps->bobCycle = (int)(old + bobmove * pml.msec) & 255;

    // if we just crossed a cycle boundary, play an appropriate footstep event
    if (((old + 64) ^ (pm->ps->bobCycle + 64)) & 128) {
        if (pm->waterlevel == 0) {
            // on ground will only play sounds if running
            if (footstep && !pm->noFootsteps) {
                PM_AddEvent(PM_FootstepForSurface());
            }
        } else if (pm->waterlevel == 1) {
            // splashing
            PM_AddEvent(EV_FOOTSPLASH);
        } else if (pm->waterlevel == 2) {
            // wading / swimming at surface
            PM_AddEvent(EV_SWIM);
        } else if (pm->waterlevel == 3) {
            // no sound when completely underwater
        }
    }
}

/*
==============
PM_WaterEvents

Generate sound events for entering and leaving water
==============
*/
static void PM_WaterEvents(void) {  // FIXME?
    //
    // if just entered a water volume, play a sound
    //
    if (!pml.previous_waterlevel && pm->waterlevel) {
        PM_AddEvent(EV_WATER_TOUCH);
    }

    //
    // if just completely exited a water volume, play a sound
    //
    if (pml.previous_waterlevel && !pm->waterlevel) {
        PM_AddEvent(EV_WATER_LEAVE);
    }

    //
    // check for head just going under water
    //
    if (pml.previous_waterlevel != 3 && pm->waterlevel == 3) {
        PM_AddEvent(EV_WATER_UNDER);
    }

    //
    // check for head just coming out of water
    //
    if (pml.previous_waterlevel == 3 && pm->waterlevel != 3) {
        PM_AddEvent(EV_WATER_CLEAR);
    }
}

/*
===============
PM_BeginWeaponChange
===============
*/
static void PM_BeginWeaponChange(int weapon) {
    if (weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS) {
        return;
    }

    if (!(pm->ps->stats[STAT_WEAPONS] & (1 << weapon))) {
        return;
    }

    if (pm->ps->weaponstate == WEAPON_DROPPING) {
        return;
    }

    PM_AddEvent(EV_CHANGE_WEAPON);
    pm->ps->weaponstate = WEAPON_DROPPING;
    pm->ps->weaponTime += pm_weaponDropTime;  // [QL] configurable
    PM_StartTorsoAnim(TORSO_DROP);
}

/*
===============
PM_FinishWeaponChange
===============
*/
static void PM_FinishWeaponChange(void) {
    int weapon;

    weapon = pm->cmd.weapon;
    if (weapon < WP_NONE || weapon >= WP_NUM_WEAPONS) {
        weapon = WP_NONE;
    }

    if (!(pm->ps->stats[STAT_WEAPONS] & (1 << weapon))) {
        weapon = WP_NONE;
    }

    pm->ps->weapon = weapon;
    pm->ps->weaponstate = WEAPON_RAISING;
    pm->ps->weaponTime += pm_weaponRaiseTime;  // [QL] configurable
    PM_StartTorsoAnim(TORSO_RAISE);
}

/*
==============
PM_TorsoAnimation

==============
*/
static void PM_TorsoAnimation(void) {
    if (pm->ps->weaponstate == WEAPON_READY) {
        if (pm->ps->weapon == WP_GAUNTLET) {
            PM_ContinueTorsoAnim(TORSO_STAND2);
        } else {
            PM_ContinueTorsoAnim(TORSO_STAND);
        }
        return;
    }
}

/*
==============
PM_Weapon

Generates weapon events and modifes the weapon counter
==============
*/
static void PM_Weapon(void) {
    int addTime;

    // [QL] skip weapon logic while zoomed
    if (pm->ps->pm_flags & PMF_ZOOM) {
        return;
    }

    // don't allow attack until all buttons are up
    if (pm->ps->pm_flags & PMF_RESPAWNED) {
        return;
    }

    // ignore if spectator
    if (pm->ps->persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        return;
    }

    // check for dead player
    if (pm->ps->stats[STAT_HEALTH] <= 0) {
        pm->ps->weapon = WP_NONE;
        return;
    }

    // [QL] one-frame weapon reset after grapple release
    if (pm->ps->pm_flags & PMF_TIME_GRAPPLE) {
        pm->ps->weaponTime = 0;
        pm->ps->weaponstate = WEAPON_READY;
        pm->ps->eFlags &= ~EF_FIRING;
        pm->ps->pm_flags &= ~PMF_TIME_GRAPPLE;
        return;
    }

    // check for item using
    if (pm->cmd.buttons & BUTTON_USE_HOLDABLE) {
        if (!(pm->ps->pm_flags & PMF_USE_ITEM_HELD)) {
            if (bg_itemlist[pm->ps->stats[STAT_HOLDABLE_ITEM]].giTag == HI_MEDKIT && pm->ps->stats[STAT_HEALTH] >= (pm->ps->stats[STAT_MAX_HEALTH] + 25)) {
                // don't use medkit if at max health
            } else {
                pm->ps->pm_flags |= PMF_USE_ITEM_HELD;
                PM_AddEvent(EV_USE_ITEM0 + bg_itemlist[pm->ps->stats[STAT_HOLDABLE_ITEM]].giTag);
                pm->ps->stats[STAT_HOLDABLE_ITEM] = 0;
            }
            return;
        }
    } else {
        pm->ps->pm_flags &= ~PMF_USE_ITEM_HELD;
    }

    // make weapon function
    if (pm->ps->weaponTime > 0) {
        pm->ps->weaponTime -= pml.msec;
    }

    // check for weapon change
    // can't change if weapon is firing, but can change
    // again if lowering or raising
    if (pm->ps->weaponTime <= 0 || pm->ps->weaponstate != WEAPON_FIRING) {
        if (pm->ps->weapon != pm->cmd.weapon) {
            PM_BeginWeaponChange(pm->cmd.weapon);
        }
    }

    if (pm->ps->weaponTime > 0) {
        return;
    }

    // change weapon if time
    if (pm->ps->weaponstate == WEAPON_DROPPING) {
        PM_FinishWeaponChange();
        return;
    }

    if (pm->ps->weaponstate == WEAPON_RAISING) {
        pm->ps->weaponstate = WEAPON_READY;
        if (pm->ps->weapon == WP_GAUNTLET) {
            PM_StartTorsoAnim(TORSO_STAND2);
        } else {
            PM_StartTorsoAnim(TORSO_STAND);
        }
        return;
    }

    // check for fire
    if (!(pm->cmd.buttons & BUTTON_ATTACK)) {
        pm->ps->weaponTime = 0;
        pm->ps->weaponstate = WEAPON_READY;
        return;
    }

    // start the animation even if out of ammo
    if (pm->ps->weapon == WP_GAUNTLET) {
        // the guantlet only "fires" when it actually hits something
        if (!pm->gauntletHit) {
            pm->ps->weaponTime = 0;
            pm->ps->weaponstate = WEAPON_READY;
            return;
        }
        PM_StartTorsoAnim(TORSO_ATTACK2);
    } else {
        PM_StartTorsoAnim(TORSO_ATTACK);
    }

    pm->ps->weaponstate = WEAPON_FIRING;

    // check for out of ammo
    if (!pm->ps->ammo[pm->ps->weapon]) {
        PM_AddEvent(EV_NOAMMO);
        pm->ps->weaponTime += 500;
        return;
    }

    // take an ammo away if not infinite
    if (pm->ps->ammo[pm->ps->weapon] != -1) {
        pm->ps->ammo[pm->ps->weapon]--;
    }

    // fire weapon
    PM_AddEvent(EV_FIRE_WEAPON);

    // [QL] fire intervals from bg_weaponReloadTime[] (updated by weapon_reload_* cvars)
    addTime = bg_weaponReloadTime[pm->ps->weapon];

    if (bg_itemlist[pm->ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_SCOUT) {
        addTime /= 1.5;
    } else if (bg_itemlist[pm->ps->stats[STAT_PERSISTANT_POWERUP]].giTag == PW_AMMOREGEN) {
        addTime /= 1.3;
    } else if (pm->ps->powerups[PW_HASTE]) {
        addTime /= 1.3;
    }

    pm->ps->weaponTime += addTime;
}

/*
================
PM_Animate
================
*/

static void PM_Animate(void) {
    if (pm->cmd.buttons & BUTTON_GESTURE) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_GESTURE);
            pm->ps->torsoTimer = TIMER_GESTURE;
            PM_AddEvent(EV_TAUNT);
        }
    } else if (pm->cmd.buttons & BUTTON_GETFLAG) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_GETFLAG);
            pm->ps->torsoTimer = 600;  // TIMER_GESTURE;
        }
    } else if (pm->cmd.buttons & BUTTON_GUARDBASE) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_GUARDBASE);
            pm->ps->torsoTimer = 600;  // TIMER_GESTURE;
        }
    } else if (pm->cmd.buttons & BUTTON_PATROL) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_PATROL);
            pm->ps->torsoTimer = 600;  // TIMER_GESTURE;
        }
    } else if (pm->cmd.buttons & BUTTON_FOLLOWME) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_FOLLOWME);
            pm->ps->torsoTimer = 600;  // TIMER_GESTURE;
        }
    } else if (pm->cmd.buttons & BUTTON_AFFIRMATIVE) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_AFFIRMATIVE);
            pm->ps->torsoTimer = 600;  // TIMER_GESTURE;
        }
    } else if (pm->cmd.buttons & BUTTON_NEGATIVE) {
        if (pm->ps->torsoTimer == 0) {
            PM_StartTorsoAnim(TORSO_NEGATIVE);
            pm->ps->torsoTimer = 600;  // TIMER_GESTURE;
        }
    }
}

/*
================
PM_DropTimers
================
*/
static void PM_DropTimers(void) {
    // drop misc timing counter
    if (pm->ps->pm_time) {
        if (pml.msec >= pm->ps->pm_time) {
            pm->ps->pm_flags &= ~PMF_ALL_TIMES;
            pm->ps->pm_time = 0;
        } else {
            pm->ps->pm_time -= pml.msec;
        }
    }

    // drop animation counter
    if (pm->ps->legsTimer > 0) {
        pm->ps->legsTimer -= pml.msec;
        if (pm->ps->legsTimer < 0) {
            pm->ps->legsTimer = 0;
        }
    }

    if (pm->ps->torsoTimer > 0) {
        pm->ps->torsoTimer -= pml.msec;
        if (pm->ps->torsoTimer < 0) {
            pm->ps->torsoTimer = 0;
        }
    }

    // [QL] decrement crouch slide timer while on ground
    if ((pm->ps->pm_flags & PMF_CROUCH_SLIDE) && pml.groundPlane
        && pm->ps->crouchSlideTime != 0) {
        if (pm->ps->crouchSlideTime <= pml.msec) {
            pm->ps->crouchSlideTime = 0;
        } else {
            pm->ps->crouchSlideTime -= pml.msec;
        }
    }
}

/*
================
PM_UpdateViewAngles

This can be used as another entry point when only the viewangles
are being updated instead of a full move
================
*/
void PM_UpdateViewAngles(playerState_t* ps, const usercmd_t* cmd) {
    short temp;
    int i;

    if (ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_TUTORIAL) {
        return;  // no view changes at all
    }

    if (ps->pm_type != PM_SPECTATOR && ps->stats[STAT_HEALTH] <= 0) {
        return;  // no view changes at all
    }

    // circularly clamp the angles with deltas
    for (i = 0; i < 3; i++) {
        temp = cmd->angles[i] + ps->delta_angles[i];
        if (i == PITCH) {
            // don't let the player look up or down more than 90 degrees
            if (temp > 16000) {
                ps->delta_angles[i] = 16000 - cmd->angles[i];
                temp = 16000;
            } else if (temp < -16000) {
                ps->delta_angles[i] = -16000 - cmd->angles[i];
                temp = -16000;
            }
        }
        ps->viewangles[i] = SHORT2ANGLE(temp);
    }
}

/*
================
PmoveSingle

================
*/
void trap_SnapVector(float* v);

void PmoveSingle(pmove_t* pmove) {
    pm = pmove;

    // this counter lets us debug movement problems with a journal
    // by setting a conditional breakpoint fot the previous frame
    c_pmove++;

    // clear results
    pm->numtouch = 0;
    pm->watertype = 0;
    pm->waterlevel = 0;

    // [QL] configure VQ3/CPM physics mode based on pm_flags
    PM_SetMovementConfig();

    // [QL] corpses can fly through bodies, unless frozen (for thaw mechanics)
    if (pm->ps->stats[STAT_HEALTH] <= 0 && !pm->ps->powerups[PW_FREEZE]) {
        pm->tracemask &= ~CONTENTS_BODY;
    }

    // make sure walking button is clear if they are running, to avoid
    // proxy no-footsteps cheats
    if (abs(pm->cmd.forwardmove) > 64 || abs(pm->cmd.rightmove) > 64) {
        pm->cmd.buttons &= ~BUTTON_WALKING;
    }

    // set the talk balloon flag
    if (pm->cmd.buttons & BUTTON_TALK) {
        pm->ps->eFlags |= EF_TALK;
    } else {
        pm->ps->eFlags &= ~EF_TALK;
    }

    // set the firing flag for continuous beam weapons
    if (!(pm->ps->pm_flags & PMF_RESPAWNED) && pm->ps->pm_type != PM_INTERMISSION && pm->ps->pm_type != PM_NOCLIP && (pm->cmd.buttons & BUTTON_ATTACK) && pm->ps->ammo[pm->ps->weapon]) {
        pm->ps->eFlags |= EF_FIRING;
    } else {
        pm->ps->eFlags &= ~EF_FIRING;
    }

    // clear the respawned flag if attack and use are cleared
    if (pm->ps->stats[STAT_HEALTH] > 0 &&
        !(pm->cmd.buttons & (BUTTON_ATTACK | BUTTON_USE_HOLDABLE))) {
        pm->ps->pm_flags &= ~PMF_RESPAWNED;
        // [QL] clear zoom when not holding attack or use
        pm->ps->pm_flags &= ~PMF_ZOOM;
    }

    // if talk button is down, dissallow all other input
    // this is to prevent any possible intercept proxy from
    // adding fake talk balloons
    if (pmove->cmd.buttons & BUTTON_TALK) {
        // keep the talk button set tho for when the cmd.serverTime > 66 msec
        // and the same cmd is used multiple times in Pmove
        pmove->cmd.buttons = BUTTON_TALK;
        pmove->cmd.forwardmove = 0;
        pmove->cmd.rightmove = 0;
        pmove->cmd.upmove = 0;
    }

    // clear all pmove local vars
    memset(&pml, 0, sizeof(pml));

    // determine the time
    pml.msec = pmove->cmd.serverTime - pm->ps->commandTime;
    if (pml.msec < 1) {
        pml.msec = 1;
    } else if (pml.msec > 200) {
        pml.msec = 200;
    }
    pm->ps->commandTime = pmove->cmd.serverTime;

    // [QL] PMF_PAUSED: movement completely frozen (warmup countdown, etc.)
    if (pm->ps->pm_flags & PMF_PAUSED) {
        return;
    }

    // save old org in case we get stuck
    VectorCopy(pm->ps->origin, pml.previous_origin);

    // save old velocity for crashlanding
    VectorCopy(pm->ps->velocity, pml.previous_velocity);

    pml.frametime = pml.msec * 0.001;

    // update the viewangles
    PM_UpdateViewAngles(pm->ps, &pm->cmd);

    // [QL] copy cmd.weaponPrimary → ps->weaponPrimary (valid weapon indices 1-14)
    if (pm->cmd.weaponPrimary >= 1 && pm->cmd.weaponPrimary <= 14 &&
        pm->ps->weaponPrimary != pm->cmd.weaponPrimary) {
        pm->ps->weaponPrimary = pm->cmd.weaponPrimary;
    }

    // [QL] copy cmd.fov → ps->fov (valid FOV range 10-130)
    if (pm->cmd.fov >= 10 && pm->cmd.fov <= 130 &&
        pm->ps->fov != pm->cmd.fov) {
        pm->ps->fov = pm->cmd.fov;
    }

    // [QL] copy movement inputs to playerState for spectator/demo replay
    if (pm->ps->forwardmove != pm->cmd.forwardmove) {
        pm->ps->forwardmove = pm->cmd.forwardmove;
    }
    if (pm->ps->rightmove != (char)pm->cmd.rightmove) {
        pm->ps->rightmove = pm->cmd.rightmove;
    }
    if (pm->ps->upmove != pm->cmd.upmove) {
        pm->ps->upmove = pm->cmd.upmove;
    }

    AngleVectors(pm->ps->viewangles, pml.forward, pml.right, pml.up);

    if (pm->cmd.upmove < 10) {
        // not holding jump
        pm->ps->pm_flags &= ~PMF_JUMP_HELD;
    }

    // decide if backpedaling animations should be used
    if (pm->cmd.forwardmove < 0) {
        pm->ps->pm_flags |= PMF_BACKWARDS_RUN;
    } else if (pm->cmd.forwardmove > 0 || (pm->cmd.forwardmove == 0 && pm->cmd.rightmove)) {
        pm->ps->pm_flags &= ~PMF_BACKWARDS_RUN;
    }

    if (pm->ps->pm_type >= PM_DEAD) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        pm->cmd.upmove = 0;
    }

    if (pm->ps->pm_type == PM_SPECTATOR) {
        PM_CheckDuck();
        PM_FlyMove();
        PM_DropTimers();
        return;
    }

    if (pm->ps->pm_type == PM_NOCLIP) {
        PM_NoclipMove();
        PM_DropTimers();
        return;
    }

    // [QL] PM_FREEZE clears movement inputs but falls through to water/ground checks
    if (pm->ps->pm_type == PM_FREEZE) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        pm->cmd.upmove = 0;
    }

    if (pm->ps->pm_type == PM_TUTORIAL) {
        return;
    }

    if (pm->ps->pm_type == PM_INTERMISSION) {
        return;
    }

    // set watertype, and waterlevel
    PM_SetWaterLevel();
    pml.previous_waterlevel = pmove->waterlevel;

    // set mins, maxs, and viewheight
    PM_CheckDuck();

    // set groundentity
    PM_GroundTrace();

    // [QL] dead players: friction only when on ground, no free camera
    if (pm->ps->pm_type == PM_DEAD) {
        if (pml.walking) {
            float speed = VectorLength(pm->ps->velocity);
            float newspeed = speed - 20.0f;
            if (newspeed <= 0.0f) {
                VectorClear(pm->ps->velocity);
            } else {
                VectorNormalize(pm->ps->velocity);
                VectorScale(pm->ps->velocity, newspeed, pm->ps->velocity);
            }
        }
    }

    PM_DropTimers();

    // [QL] check for ladder surfaces
    PM_CheckLadder();

    // [QL] clear flight powerup each frame (prevents residual flight during normal movement)
    pm->ps->powerups[PW_FLIGHT] = 0;

    // [QL] invulnerability sphere freezes all movement
    if (pm->ps->powerups[PW_INVULNERABILITY]) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        pm->cmd.upmove = 0;
        VectorClear(pm->ps->velocity);
        goto postMovement;
    }

    // [QL] grapple pull: set velocity, then also run air move for air control
    if (pm->ps->pm_flags & PMF_GRAPPLE_PULL) {
        PM_GrappleMove();
        PM_AirMove();
    } else if (pm->ps->pm_flags & PMF_TIME_WATERJUMP) {
        PM_DeadMove();
    } else if (pml.ladder) {
        // [QL] ladder movement
        PM_LadderMove();
    } else if (pm->waterlevel > 1) {
        PM_WaterMove();
    } else if (pml.walking) {
        PM_WalkMove();
    } else {
        // airborne
        PM_AirMove();
    }

postMovement:
    PM_Animate();

    // set groundentity, watertype, and waterlevel
    PM_GroundTrace();
    PM_SetWaterLevel();

    // weapons
    PM_Weapon();

    // torso animation
    PM_TorsoAnimation();

    // footstep events / legs animations
    PM_Footsteps();

    // entering / leaving water splashes
    PM_WaterEvents();

    // snap some parts of playerstate to save network bandwidth
    trap_SnapVector(pm->ps->velocity);
}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void Pmove(pmove_t* pmove) {
    int finalTime;

    finalTime = pmove->cmd.serverTime;

    if (finalTime < pmove->ps->commandTime) {
        return;  // should not happen
    }

    if (finalTime > pmove->ps->commandTime + 1000) {
        pmove->ps->commandTime = finalTime - 1000;
    }

    pmove->ps->pmove_framecount = (pmove->ps->pmove_framecount + 1) & ((1 << PS_PMOVEFRAMECOUNTBITS) - 1);

    // chop the move up if it is too long, to prevent framerate
    // dependent behavior
    while (pmove->ps->commandTime != finalTime) {
        int msec;

        msec = finalTime - pmove->ps->commandTime;

        // [QL] pmove_fixed/pmove_msec removed from pmove_t
        if (msec > 66) {
            msec = 66;
        }
        pmove->cmd.serverTime = pmove->ps->commandTime + msec;
        PmoveSingle(pmove);

        if (pmove->ps->pm_flags & PMF_JUMP_HELD) {
            pmove->cmd.upmove = 20;
        }
    }

    // PM_CheckStuck();
}
