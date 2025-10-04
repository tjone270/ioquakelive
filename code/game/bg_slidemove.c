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
// bg_slidemove.c -- part of bg_pmove functionality

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"

/*

input: origin, velocity, bounds, groundPlane, trace function

output: origin, velocity, impacts, stairup boolean

*/

/*
==================
PM_SlideMove

Returns qtrue if the velocity was clipped in some way
==================
*/
#define MAX_CLIP_PLANES 5
qboolean PM_SlideMove(qboolean gravity) {
    int bumpcount, numbumps;
    vec3_t dir;
    float d;
    int numplanes;
    vec3_t planes[MAX_CLIP_PLANES];
    vec3_t primal_velocity;
    vec3_t clipVelocity;
    int i, j, k;
    trace_t trace;
    vec3_t end;
    float time_left;
    float into;
    vec3_t endVelocity;
    vec3_t endClipVelocity;

    numbumps = 4;

    VectorCopy(pm->ps->velocity, primal_velocity);

    if (gravity) {
        VectorCopy(pm->ps->velocity, endVelocity);
        endVelocity[2] -= pm->ps->gravity * pml.frametime;
        pm->ps->velocity[2] = (pm->ps->velocity[2] + endVelocity[2]) * 0.5;
        primal_velocity[2] = endVelocity[2];
        if (pml.groundPlane) {
            // slide along the ground plane
            PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal,
                            pm->ps->velocity, OVERCLIP);
        }
    }

    time_left = pml.frametime;

    // never turn against the ground plane
    if (pml.groundPlane) {
        numplanes = 1;
        VectorCopy(pml.groundTrace.plane.normal, planes[0]);
    } else {
        numplanes = 0;
    }

    // never turn against original velocity
    VectorNormalize2(pm->ps->velocity, planes[numplanes]);
    numplanes++;

    for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
        // calculate position we are trying to move to
        VectorMA(pm->ps->origin, time_left, pm->ps->velocity, end);

        // see if we can make it there
        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, end, pm->ps->clientNum, pm->tracemask);

        if (trace.allsolid) {
            // entity is completely trapped in another solid
            pm->ps->velocity[2] = 0;  // don't build up falling damage, but allow sideways acceleration
            return qtrue;
        }

        if (trace.fraction > 0) {
            // actually covered some distance
            VectorCopy(trace.endpos, pm->ps->origin);
        }

        if (trace.fraction == 1) {
            break;  // moved the entire distance
        }

        // save entity for contact
        PM_AddTouchEnt(trace.entityNum);

        time_left -= time_left * trace.fraction;

        if (numplanes >= MAX_CLIP_PLANES) {
            // this shouldn't really happen
            VectorClear(pm->ps->velocity);
            return qtrue;
        }

        //
        // if this is the same plane we hit before, nudge velocity
        // out along it, which fixes some epsilon issues with
        // non-axial planes
        //
        for (i = 0; i < numplanes; i++) {
            if (DotProduct(trace.plane.normal, planes[i]) > 0.99) {
                VectorAdd(trace.plane.normal, pm->ps->velocity, pm->ps->velocity);
                break;
            }
        }
        if (i < numplanes) {
            continue;
        }
        VectorCopy(trace.plane.normal, planes[numplanes]);
        numplanes++;

        //
        // modify velocity so it parallels all of the clip planes
        //

        // find a plane that it enters
        for (i = 0; i < numplanes; i++) {
            into = DotProduct(pm->ps->velocity, planes[i]);
            if (into >= 0.1) {
                continue;  // move doesn't interact with the plane
            }

            // see how hard we are hitting things
            if (-into > pml.impactSpeed) {
                pml.impactSpeed = -into;
            }

            // slide along the plane
            PM_ClipVelocity(pm->ps->velocity, planes[i], clipVelocity, OVERCLIP);

            if (gravity) {
                // slide along the plane
                PM_ClipVelocity(endVelocity, planes[i], endClipVelocity, OVERCLIP);
            }

            // see if there is a second plane that the new move enters
            for (j = 0; j < numplanes; j++) {
                if (j == i) {
                    continue;
                }
                if (DotProduct(clipVelocity, planes[j]) >= 0.1) {
                    continue;  // move doesn't interact with the plane
                }

                // try clipping the move to the plane
                PM_ClipVelocity(clipVelocity, planes[j], clipVelocity, OVERCLIP);

                if (gravity) {
                    PM_ClipVelocity(endClipVelocity, planes[j], endClipVelocity, OVERCLIP);
                }

                // see if it goes back into the first clip plane
                if (DotProduct(clipVelocity, planes[i]) >= 0) {
                    continue;
                }

                // slide the original velocity along the crease
                CrossProduct(planes[i], planes[j], dir);
                VectorNormalize(dir);
                d = DotProduct(dir, pm->ps->velocity);
                VectorScale(dir, d, clipVelocity);

                if (gravity) {
                    CrossProduct(planes[i], planes[j], dir);
                    VectorNormalize(dir);
                    d = DotProduct(dir, endVelocity);
                    VectorScale(dir, d, endClipVelocity);
                }

                // see if there is a third plane the the new move enters
                for (k = 0; k < numplanes; k++) {
                    if (k == i || k == j) {
                        continue;
                    }
                    if (DotProduct(clipVelocity, planes[k]) >= 0.1) {
                        continue;  // move doesn't interact with the plane
                    }

                    // stop dead at a tripple plane interaction
                    VectorClear(pm->ps->velocity);
                    return qtrue;
                }
            }

            // if we have fixed all interactions, try another move
            VectorCopy(clipVelocity, pm->ps->velocity);

            if (gravity) {
                VectorCopy(endClipVelocity, endVelocity);
            }

            break;
        }
    }

    if (gravity) {
        VectorCopy(endVelocity, pm->ps->velocity);
    }

    // don't change velocity if in a timer (FIXME: is this correct?)
    if (pm->ps->pm_time) {
        VectorCopy(primal_velocity, pm->ps->velocity);
    }

    return (bumpcount != 0);
}

/*
==================
PM_CanCrouchStepJump - [QL] pure predicate: can we do a crouch-step-jump?
Matches binary's PM_CheckDoubleJump used in step-slide context (does NOT call PM_DoJump).
==================
*/
static qboolean PM_CanCrouchStepJump(void) {
    if (!(pm->ps->pm_flags & PMF_DUCKED)) {
        return qfalse;
    }
    if (pml.groundPlane) {
        return qfalse;
    }
    if (pm->ps->velocity[2] < 0.0f) {
        return qfalse;
    }
    if ((float)(pm->cmd.serverTime - pm->ps->jumpTime) < pm_jumpVelocityTimeThreshold) {
        return qfalse;
    }
    return qtrue;
}

/*
==================
PM_StepSlideMove

==================
*/
void PM_StepSlideMove(qboolean gravity) {
    vec3_t start_o, start_v;
    vec3_t pred;
    trace_t trace;
    vec3_t up, down;
    float stepSize;
    float actualStepHeight = pm_stepHeight;  // [QL] configurable step height

    VectorCopy(pm->ps->origin, start_o);
    VectorCopy(pm->ps->velocity, start_v);

    if (PM_SlideMove(gravity) == 0) {
        return;  // we got exactly where we wanted to go first try
    }

    // [QL] predict position after one frame (used for ground checks and step-jump validation)
    VectorMA(start_o, pml.frametime, start_v, pred);

    // [QL] upward-velocity rejection: only check when airborne (binary skips this when grounded)
    if (!pml.groundPlane) {
        vec3_t predDown;

        // trace from start to predicted position
        pm->trace(&trace, start_o, pm->mins, pm->maxs, pred, pm->ps->clientNum, pm->tracemask);

        // from trace endpoint, trace down by stepHeight to find ground
        VectorCopy(trace.endpos, predDown);
        predDown[2] -= actualStepHeight;
        pm->trace(&trace, trace.endpos, pm->mins, pm->maxs, predDown, pm->ps->clientNum, pm->tracemask);

        // if moving upward and no walkable ground at predicted position, skip step-up
        if (start_v[2] > 0 && (trace.fraction == 1.0 ||
                               trace.plane.normal[2] < 0.7)) {
            return;
        }
    }

    VectorCopy(start_o, up);
    up[2] += actualStepHeight;

    // test the player position if they were a stepheight higher
    pm->trace(&trace, start_o, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask);
    if (trace.allsolid) {
        if (pm->debugLevel) {
            Com_Printf("%i:bend can't step\n", c_pmove);
        }
        return;
    }

    stepSize = trace.endpos[2] - start_o[2];
    VectorCopy(trace.endpos, pm->ps->origin);
    VectorCopy(start_v, pm->ps->velocity);

    PM_SlideMove(gravity);

    // push down the final amount
    VectorCopy(pm->ps->origin, down);
    down[2] -= stepSize;
    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
    if (!trace.allsolid) {
        VectorCopy(trace.endpos, pm->ps->origin);
    }

    // [QL] only clip velocity if moving INTO the surface (dot < 0 or nearly parallel)
    // Binary does NOT unconditionally clip - preserves speed when velocity is away from surface
    if (trace.fraction < 1.0) {
        float dot = DotProduct(pm->ps->velocity, trace.plane.normal);
        if (dot < 0.0f || (dot >= 0.0f && dot < 0.001f)) {
            PM_ClipVelocity(pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP);
        }
    }

    // [QL] validate step by tracing from start to current position
    pm->trace(&trace, start_o, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask);

    if (trace.fraction < 1.0) {
        float delta = pm->ps->origin[2] - start_o[2];

        // step smoothing: store step height for cgame view smoothing
        if (delta > 2) {
            pm->stepHeight = delta;
            pm->stepTime = pm->cmd.serverTime;
        }

        // air step friction: dampen X/Y velocity when stepping up while airborne
        if (!pml.groundPlane && delta > 0 && start_v[2] > 0) {
            float dampen = 1.0f - pm_airStepFriction;
            pm->ps->velocity[0] *= dampen;
            pm->ps->velocity[1] *= dampen;
        }

        // [QL] step-jump: auto-jump off stairs when holding jump
        if (pm_stepJump && pm->ps->pm_type == PM_NORMAL
            && delta > 0 && pm->waterlevel < 2) {
            if (PM_WouldJump() || (pm_doubleJump && PM_CanCrouchStepJump())) {
                // verify walkable ground at predicted position
                vec3_t sjUp, sjDown;
                trace_t sjTrace;

                VectorCopy(pred, sjUp);
                sjUp[2] += actualStepHeight;
                VectorCopy(pred, sjDown);
                sjDown[2] -= actualStepHeight;
                pm->trace(&sjTrace, sjUp, pm->mins, pm->maxs, sjDown,
                          pm->ps->clientNum, pm->tracemask);

                if (!sjTrace.startsolid && !sjTrace.allsolid
                    && sjTrace.plane.normal[2] >= 0.7) {
                    if (PM_WouldJump()) {
                        pml.stepJumpFlag = 1;
                        PM_DoJump();
                        pml.stepJumpFlag = 0;
                    } else if (pm_doubleJump && PM_CanCrouchStepJump()) {
                        // crouch step jump: verify clearance with shrunk bounding box
                        vec3_t sjMins, sjMaxs, crouchDown;
                        trace_t crouchTrace;

                        sjMins[0] = pm->mins[0] + 1;
                        sjMins[1] = pm->mins[1] + 1;
                        sjMins[2] = pm->mins[2];
                        sjMaxs[0] = pm->maxs[0] - 1;
                        sjMaxs[1] = pm->maxs[1] - 1;
                        sjMaxs[2] = pm->maxs[2];
                        VectorCopy(pm->ps->origin, crouchDown);
                        crouchDown[2] -= 64;
                        pm->trace(&crouchTrace, pm->ps->origin, sjMins, sjMaxs,
                                  crouchDown, pm->ps->clientNum, pm->tracemask);
                        if (crouchTrace.fraction == 1.0) {
                            pml.crouchStepJumpFlag = 1;
                            PM_DoJump();
                            pml.crouchStepJumpFlag = 0;
                        }
                    }
                }
            }
        }

        if (pm->debugLevel) {
            Com_Printf("%i:stepped %f\n", c_pmove, (double)delta);
        }
    }
}
