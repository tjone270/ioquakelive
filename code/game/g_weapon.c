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
// g_weapon.c
// perform the server side effects of a weapon firing

#include "g_local.h"

static float s_quadFactor;
static vec3_t forward, right, up;
static vec3_t muzzle;


/*
================
G_BounceProjectile
================
*/
void G_BounceProjectile(vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout) {
    vec3_t v, newv;
    float dot;

    VectorSubtract(impact, start, v);
    dot = DotProduct(v, dir);
    VectorMA(v, -2 * dot, dir, newv);

    VectorNormalize(newv);
    VectorMA(impact, 8192, newv, endout);
}

/*
======================================================================

GAUNTLET

======================================================================
*/

void Weapon_Gauntlet(gentity_t* ent) {
}

/*
===============
CheckGauntletAttack
===============
*/
qboolean CheckGauntletAttack(gentity_t* ent) {
    trace_t tr;
    vec3_t end;
    gentity_t* tent;
    gentity_t* traceEnt;
    int damage;

    // set aiming directions
    AngleVectors(ent->client->ps.viewangles, forward, right, up);

    CalcMuzzlePoint(ent, forward, right, up, muzzle);

    VectorMA(muzzle, 32, forward, end);

    trap_Trace(&tr, muzzle, NULL, NULL, end, ent->s.number, MASK_SHOT);
    if (tr.surfaceFlags & SURF_NOIMPACT) {
        return qfalse;
    }

    if (ent->client->noclip) {
        return qfalse;
    }

    traceEnt = &g_entities[tr.entityNum];

    // send blood impact
    if (traceEnt->takedamage && traceEnt->client) {
        tent = G_TempEntity(tr.endpos, EV_MISSILE_HIT);
        tent->s.otherEntityNum = traceEnt->s.number;
        tent->s.eventParm = DirToByte(tr.plane.normal);
        tent->s.weapon = ent->s.weapon;
    }

    if (!traceEnt->takedamage) {
        return qfalse;
    }

    if (ent->client->ps.powerups[PW_QUAD]) {
        G_AddEvent(ent, EV_POWERUP_QUAD, 0);
        s_quadFactor = g_quadfactor.value;
    } else {
        s_quadFactor = 1;
    }

    if (ent->client->persistantPowerup && ent->client->persistantPowerup->item && ent->client->persistantPowerup->item->giTag == PW_DOUBLER) {
        s_quadFactor *= 2;
    }

    damage = g_damage_g.integer * s_quadFactor;
    G_Damage(traceEnt, ent, ent, forward, tr.endpos,
             damage, 0, MOD_GAUNTLET);

    return qtrue;
}

/*
======================================================================

MACHINEGUN

======================================================================
*/

/*
======================
SnapVectorTowards

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating
into a wall.
======================
*/
void SnapVectorTowards(vec3_t v, vec3_t to) {
    int i;

    for (i = 0; i < 3; i++) {
        if (to[i] <= v[i]) {
            v[i] = floor(v[i]);
        } else {
            v[i] = ceil(v[i]);
        }
    }
}

#define CHAINGUN_SPREAD 600
#define CHAINGUN_DAMAGE 7

// [QL] Heavy Machine Gun
#define HMG_SPREAD  350
#define HMG_DAMAGE  8

#define MACHINEGUN_SPREAD 200
#define MACHINEGUN_DAMAGE 7
#define MACHINEGUN_TEAM_DAMAGE 5  // wimpier MG in teamplay

void Bullet_Fire(gentity_t* ent, float spread, int damage, int mod) {
    trace_t tr;
    vec3_t end;
    vec3_t impactpoint, bouncedir;
    float r;
    float u;
    gentity_t* tent;
    gentity_t* traceEnt;
    int i, passent;

    damage *= s_quadFactor;

    r = random() * M_PI * 2.0f;
    u = sin(r) * crandom() * spread * 16;
    r = cos(r) * crandom() * spread * 16;
    VectorMA(muzzle, 8192 * 16, forward, end);
    VectorMA(end, r, right, end);
    VectorMA(end, u, up, end);

    passent = ent->s.number;
    for (i = 0; i < 10; i++) {
        // g_playerCylinders: capsule trace for player hits; pmove_noPlayerClip: adjust mask
        {
            int mask = pmove_NoPlayerClip.integer ? (MASK_SHOT & ~CONTENTS_PLAYERCLIP) : MASK_SHOT;
            G_TracePlayerHit(&tr, muzzle, NULL, NULL, end, passent, mask);
        }
        if (tr.surfaceFlags & SURF_NOIMPACT) {
            return;
        }

        traceEnt = &g_entities[tr.entityNum];

        // snap the endpos to integers, but nudged towards the line
        SnapVectorTowards(tr.endpos, muzzle);

        // send bullet impact
        if (traceEnt->takedamage && traceEnt->client) {
            tent = G_TempEntity(tr.endpos, EV_BULLET_HIT_FLESH);
            tent->s.eventParm = traceEnt->s.number;
            if (LogAccuracyHit(traceEnt, ent)) {
                ent->client->accuracy_hits++;
                if (ent->s.weapon < 16) {
                    ent->client->expandedStats.shotsHit[ent->s.weapon]++;
                }
            }
        } else {
            tent = G_TempEntity(tr.endpos, EV_BULLET_HIT_WALL);
            tent->s.eventParm = DirToByte(tr.plane.normal);
        }
        tent->s.otherEntityNum = ent->s.number;

        if (traceEnt->takedamage) {
            if (traceEnt->client && traceEnt->client->invulnerabilityTime > level.time) {
                if (G_InvulnerabilityEffect(traceEnt, forward, tr.endpos, impactpoint, bouncedir)) {
                    G_BounceProjectile(muzzle, impactpoint, bouncedir, end);
                    VectorCopy(impactpoint, muzzle);
                    // the player can hit him/herself with the bounced rail
                    passent = ENTITYNUM_NONE;
                } else {
                    VectorCopy(tr.endpos, muzzle);
                    passent = traceEnt->s.number;
                }
                continue;
            } else {
                G_Damage(traceEnt, ent, ent, forward, tr.endpos,
                         damage, 0, mod);
            }
        }
        break;
    }
}

/*
======================================================================

BFG

======================================================================
*/

void Weapon_BFG_Fire(gentity_t* ent) {
    gentity_t* m;

    m = fire_bfg(ent, muzzle, forward);
    m->damage *= s_quadFactor;
    m->splashDamage *= s_quadFactor;

    //	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

/*
======================================================================

SHOTGUN

======================================================================
*/

// DEFAULT_SHOTGUN_SPREAD and DEFAULT_SHOTGUN_COUNT	are in bg_public.h, because
// client predicts same spreads
#define DEFAULT_SHOTGUN_DAMAGE 10

qboolean ShotgunPellet(vec3_t start, vec3_t end, gentity_t* ent, int ring) {
    trace_t tr;
    int damage, i, passent;
    gentity_t* traceEnt;
    vec3_t impactpoint, bouncedir;
    vec3_t tr_start, tr_end;

    passent = ent->s.number;
    VectorCopy(start, tr_start);
    VectorCopy(end, tr_end);
    for (i = 0; i < 2; i++) {  // [QL] 2 bounces, not 10
        // g_playerCylinders + pmove_noPlayerClip
        {
            int mask = pmove_NoPlayerClip.integer ? (MASK_SHOT & ~CONTENTS_PLAYERCLIP) : MASK_SHOT;
            G_TracePlayerHit(&tr, tr_start, NULL, NULL, tr_end, passent, mask);
        }
        traceEnt = &g_entities[tr.entityNum];

        if (tr.surfaceFlags & SURF_NOIMPACT) {
            return qfalse;
        }

        if (!traceEnt->takedamage) {
            return qfalse;
        }

        // [QL] inner vs outer ring damage
        if (ring == 1) {
            damage = g_damage_sg.integer;
        } else {
            damage = g_damage_sg_outer.integer;
        }

        // [QL] distance-based damage falloff
        if (g_damage_sg_falloff.integer && g_range_sg_falloff.integer > 0) {
            float dist = Distance(start, tr.endpos);
            int falloffRange = (int)dist;
            while (falloffRange > 0 && g_range_sg_falloff.integer != 0) {
                falloffRange -= g_range_sg_falloff.integer;
                damage -= g_damage_sg_falloff.integer;
            }
            if (damage < 1) damage = 1;
        }

        damage = (int)(damage * s_quadFactor);

        if (traceEnt->client && traceEnt->client->invulnerabilityTime > level.time) {
            if (G_InvulnerabilityEffect(traceEnt, forward, tr.endpos, impactpoint, bouncedir)) {
                G_BounceProjectile(tr_start, impactpoint, bouncedir, tr_end);
                VectorCopy(impactpoint, tr_start);
                passent = ENTITYNUM_NONE;
            } else {
                VectorCopy(tr.endpos, tr_start);
                passent = traceEnt->s.number;
            }
            continue;
        }

        // [QL] accumulate damage for plum display
        if (traceEnt->client && ent->client) {
            ent->client->damagePlum[traceEnt->s.number] += damage;
        }
        G_Damage(traceEnt, ent, ent, forward, tr.endpos, damage, 0, MOD_SHOTGUN);
        if (LogAccuracyHit(traceEnt, ent)) {
            return qtrue;
        }
        return qfalse;
    }
    return qfalse;
}

// [QL] Ring-based shotgun pattern (binary-verified from 0x1006d450)
// 3 rings: inner(6 pellets), middle(6), outer(8) = 20 total
// this should match CG_ShotgunPattern
void ShotgunPattern(vec3_t origin, vec3_t origin2, int seed, gentity_t* ent) {
    int i;
    float r, u, angle, ringRadius;
    int ring;
    vec3_t end;
    vec3_t forward, right, up;
    qboolean hitClient = qfalse;

    VectorNormalize2(origin2, forward);
    PerpendicularVector(right, forward);
    CrossProduct(forward, right, up);

    // [QL] clear damage plum accumulator
    if (ent->client) {
        memset(ent->client->damagePlum, 0, sizeof(ent->client->damagePlum));
    }

    for (i = 0; i < 20; i++) {
        // [QL] ring-based pattern
        if (i < 6) {
            // Inner ring: 6 pellets, radius 8, 60-degree spacing
            ringRadius = 8;
            angle = (float)(i - 20) * (M_PI / 3.0f);
            ring = 1;  // inner = full damage
        } else if (i < 12) {
            // Middle ring: 6 pellets, radius 16
            ringRadius = 16;
            angle = (float)i * (M_PI / 3.0f) + (30.0f * M_PI / 180.0f);
            ring = 0;  // outer damage
        } else {
            // Outer ring: 8 pellets, radius 24, 45-degree spacing
            ringRadius = 24;
            angle = (float)i * (M_PI / 4.0f);
            ring = 0;  // outer damage
        }

        r = cos(angle) * ringRadius;
        u = sin(angle) * ringRadius;

        VectorMA(origin, 8192 * 16, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        if (ShotgunPellet(origin, end, ent, ring) && !hitClient) {
            hitClient = qtrue;
            ent->client->accuracy_hits++;
            ent->client->expandedStats.shotsHit[WP_SHOTGUN]++;
        }
    }

    // [QL] Emit EV_DAMAGEPLUM events for each hit client
    if (g_damagePlums.integer && ent->client) {
        int j;
        int totalDamage = 0;
        for (j = 0; j < level.maxclients; j++) {
            if (ent->client->damagePlum[j] != 0) {
                gentity_t *plum;
                vec3_t org;

                totalDamage += ent->client->damagePlum[j];

                VectorCopy(g_entities[j].r.currentOrigin, org);
                org[2] += 32.0f;

                plum = G_TempEntity(org, EV_DAMAGEPLUM);
                plum->s.eFlags |= EF_NODRAW;
                plum->s.clientNum = ent->s.clientNum;
                plum->s.time = ent->client->damagePlum[j];
                plum->s.generic1 = WP_SHOTGUN;
                ent->client->damagePlum[j] = 0;
            }
        }

        // [QL] Encode shotgun quality into eFlags bits 6-7
        {
            int quality;
            if (totalDamage >= 75) quality = 3;
            else if (totalDamage >= 50) quality = 2;
            else if (totalDamage > 24) quality = 1;
            else quality = 0;
            ent->client->ps.eFlags = (ent->client->ps.eFlags & ~0xC0) | (quality << 6);
        }
    }
}

void Weapon_Shotgun_Fire(gentity_t* ent) {
    gentity_t* tent;

    // send shotgun blast
    tent = G_TempEntity(muzzle, EV_SHOTGUN);
    VectorScale(forward, 4096, tent->s.origin2);
    SnapVector(tent->s.origin2);
    tent->s.eventParm = rand() & 255;  // seed for spread pattern
    tent->s.otherEntityNum = ent->s.number;

    ShotgunPattern(tent->s.pos.trBase, tent->s.origin2, tent->s.eventParm, ent);
}

/*
======================================================================

GRENADE LAUNCHER

======================================================================
*/

void Weapon_GrenadeLauncher_Fire(gentity_t* ent) {
    gentity_t* m;

    // extra vertical velocity
    forward[2] += 0.2f;
    VectorNormalize(forward);

    m = fire_grenade(ent, muzzle, forward);
    m->damage *= s_quadFactor;
    m->splashDamage *= s_quadFactor;

    //	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

/*
======================================================================

ROCKET

======================================================================
*/

void Weapon_RocketLauncher_Fire(gentity_t* ent) {
    gentity_t* m;

    m = fire_rocket(ent, muzzle, forward);
    m->damage *= s_quadFactor;
    m->splashDamage *= s_quadFactor;

    //	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

/*
======================================================================

PLASMA GUN

======================================================================
*/

void Weapon_Plasmagun_Fire(gentity_t* ent) {
    gentity_t* m;

    m = fire_plasma(ent, muzzle, forward);
    m->damage *= s_quadFactor;
    m->splashDamage *= s_quadFactor;

    //	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

/*
======================================================================

RAILGUN

======================================================================
*/

/*
=================
Weapon_Railgun_Fire
=================
*/
#define MAX_RAIL_HITS 4
void Weapon_Railgun_Fire(gentity_t* ent) {
    vec3_t end;
    vec3_t impactpoint, bouncedir;
    trace_t trace;
    gentity_t* tent;
    gentity_t* traceEnt;
    int damage;
    int i;
    int hits;
    int unlinked;
    int passent;
    gentity_t* unlinkedEntities[MAX_RAIL_HITS];

    damage = g_damage_rg.integer * s_quadFactor;

    VectorMA(muzzle, 8192, forward, end);

    // trace only against the solids, so the railgun will go through people
    unlinked = 0;
    hits = 0;
    passent = ent->s.number;
    do {
        // g_playerCylinders + pmove_noPlayerClip
        {
            int mask = pmove_NoPlayerClip.integer ? (MASK_SHOT & ~CONTENTS_PLAYERCLIP) : MASK_SHOT;
            G_TracePlayerHit(&trace, muzzle, NULL, NULL, end, passent, mask);
        }
        if (trace.entityNum >= ENTITYNUM_MAX_NORMAL) {
            break;
        }
        traceEnt = &g_entities[trace.entityNum];
        if (traceEnt->takedamage) {
            if (traceEnt->client && traceEnt->client->invulnerabilityTime > level.time) {
                if (G_InvulnerabilityEffect(traceEnt, forward, trace.endpos, impactpoint, bouncedir)) {
                    G_BounceProjectile(muzzle, impactpoint, bouncedir, end);
                    // snap the endpos to integers to save net bandwidth, but nudged towards the line
                    SnapVectorTowards(trace.endpos, muzzle);
                    // send railgun beam effect
                    tent = G_TempEntity(trace.endpos, EV_RAILTRAIL);
                    // set player number for custom colors on the railtrail
                    tent->s.clientNum = ent->s.clientNum;
                    VectorCopy(muzzle, tent->s.origin2);
                    // move origin a bit to come closer to the drawn gun muzzle
                    VectorMA(tent->s.origin2, 4, right, tent->s.origin2);
                    VectorMA(tent->s.origin2, -1, up, tent->s.origin2);
                    tent->s.eventParm = 255;  // don't make the explosion at the end
                    //
                    VectorCopy(impactpoint, muzzle);
                    // the player can hit him/herself with the bounced rail
                    passent = ENTITYNUM_NONE;
                }
            } else {
                if (LogAccuracyHit(traceEnt, ent)) {
                    hits++;
                }
                // [QL] headshot detection: surface flag 0x400 on player models = head
                if ((trace.surfaceFlags & 0x400) && g_headShotDamage_rg.integer) {
                    G_Damage(traceEnt, ent, ent, forward, trace.endpos,
                             damage + g_headShotDamage_rg.integer, 0, MOD_RAILGUN_HEADSHOT);
                } else {
                    G_Damage(traceEnt, ent, ent, forward, trace.endpos, damage, 0, MOD_RAILGUN);
                }
            }
        }
        if (trace.contents & CONTENTS_SOLID) {
            break;  // we hit something solid enough to stop the beam
        }
        // unlink this entity, so the next trace will go past it
        trap_UnlinkEntity(traceEnt);
        unlinkedEntities[unlinked] = traceEnt;
        unlinked++;
    } while (unlinked < MAX_RAIL_HITS);

    // link back in any entities we unlinked
    for (i = 0; i < unlinked; i++) {
        trap_LinkEntity(unlinkedEntities[i]);
    }

    // the final trace endpos will be the terminal point of the rail trail

    // snap the endpos to integers to save net bandwidth, but nudged towards the line
    SnapVectorTowards(trace.endpos, muzzle);

    // send railgun beam effect
    tent = G_TempEntity(trace.endpos, EV_RAILTRAIL);

    // set player number for custom colors on the railtrail
    tent->s.clientNum = ent->s.clientNum;

    VectorCopy(muzzle, tent->s.origin2);
    // move origin a bit to come closer to the drawn gun muzzle
    VectorMA(tent->s.origin2, 4, right, tent->s.origin2);
    VectorMA(tent->s.origin2, -1, up, tent->s.origin2);

    // no explosion at end if SURF_NOIMPACT, but still make the trail
    if (trace.surfaceFlags & SURF_NOIMPACT) {
        tent->s.eventParm = 255;  // don't make the explosion at the end
    } else {
        tent->s.eventParm = DirToByte(trace.plane.normal);
    }
    tent->s.clientNum = ent->s.clientNum;

    // [QL] rail jump: self-knockback when shooting a wall at close range
    if (g_railJump.integer && trace.entityNum == ENTITYNUM_WORLD) {
        float dist = Distance(muzzle, trace.endpos);
        if (dist < 120.0f) {
            vec3_t kickDir;
            VectorScale(forward, -1, kickDir);
            VectorMA(ent->client->ps.velocity, (float)g_railJump.integer, kickDir, ent->client->ps.velocity);
            ent->client->ps.velocity[2] += 20.0f;
        }
    }

    // give the shooter a reward sound if they have made two railgun hits in a row
    if (hits == 0) {
        // complete miss
        ent->client->accurateCount = 0;
    } else {
        // check for "impressive" reward sound
        ent->client->accurateCount += hits;
        if (ent->client->accurateCount >= 2) {
            ent->client->accurateCount -= 2;
            ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
            // add the sprite over the player's head
            ent->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP);
            ent->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
            ent->client->rewardTime = level.time + REWARD_SPRITE_TIME;
        }
        ent->client->accuracy_hits++;
        ent->client->expandedStats.shotsHit[WP_RAILGUN]++;
    }
}

/*
======================================================================

GRAPPLING HOOK

======================================================================
*/

void Weapon_GrapplingHook_Fire(gentity_t* ent) {
    if (!ent->client->fireHeld && !ent->client->hook)
        fire_grapple(ent, muzzle, forward);

    ent->client->fireHeld = qtrue;
}

void Weapon_HookFree(gentity_t* ent) {
    if (ent->parent && ent->parent->client) {
        ent->parent->client->hook = NULL;
        VectorClear(ent->parent->client->ps.grapplePoint);
        ent->parent->client->ps.pm_flags &= ~PMF_GRAPPLE_PULL;
        ent->parent->client->ps.pm_flags |= PMF_TIME_GRAPPLE;
        ent->parent->client->ps.eFlags &= ~EF_FIRING;
    }
    G_FreeEntity(ent);
}

void Weapon_HookThink(gentity_t* ent) {
    if (!ent->parent || !ent->parent->client) {
        Weapon_HookFree(ent);
        return;
    }

    // [QL] check if owner switched away from grapple or is raising a new weapon
    if (ent->parent->client->ps.weapon != WP_GRAPPLING_HOOK ||
        ent->parent->client->ps.weaponstate == WEAPON_RAISING) {
        Weapon_HookFree(ent);
        return;
    }

    if (ent->enemy) {
        vec3_t v, oldorigin;

        // [QL] check if enemy is still valid (alive and not spectator)
        if (!ent->enemy->client ||
            ent->enemy->client->ps.stats[STAT_HEALTH] <= 0 ||
            ent->enemy->client->sess.sessionTeam == TEAM_SPECTATOR) {
            Weapon_HookFree(ent);
            return;
        }

        // track enemy center position
        VectorCopy(ent->r.currentOrigin, oldorigin);
        v[0] = ent->enemy->r.currentOrigin[0] + (ent->enemy->r.mins[0] + ent->enemy->r.maxs[0]) * 0.5;
        v[1] = ent->enemy->r.currentOrigin[1] + (ent->enemy->r.mins[1] + ent->enemy->r.maxs[1]) * 0.5;
        v[2] = ent->enemy->r.currentOrigin[2] + (ent->enemy->r.mins[2] + ent->enemy->r.maxs[2]) * 0.5;
        SnapVectorTowards(v, oldorigin);

        G_SetOrigin(ent, v);

        // [QL] periodic damage to hooked enemy
        if (ent->count < level.time) {
            vec3_t dir;
            VectorSubtract(ent->parent->r.currentOrigin, ent->enemy->r.currentOrigin, dir);
            VectorNormalize(dir);
            G_Damage(ent->enemy, ent, ent->parent, dir, NULL, g_damage_gh.integer, 0, MOD_GRAPPLE);
            ent->count = level.time + weapon_reload_gh.integer;
        }
    }

    VectorCopy(ent->r.currentOrigin, ent->parent->client->ps.grapplePoint);
    ent->nextthink = level.time + FRAMETIME;
}

/*
======================================================================

LIGHTNING GUN

======================================================================
*/

void Weapon_Lightning_Fire(gentity_t* ent) {
    trace_t tr;
    vec3_t end;
    vec3_t impactpoint, bouncedir;
    gentity_t *traceEnt, *tent;
    int damage, i, passent;

    damage = g_damage_lg.integer * s_quadFactor;

    passent = ent->s.number;
    for (i = 0; i < 2; i++) {  // [QL] binary uses 2 iterations, not 10
        // [QL] water discharge: if firing in water with g_infiniteAmmo, discharge all ammo
        if (g_infiniteAmmo.integer) {
            int contents = trap_PointContents(muzzle, -1);
            if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
                int cells = ent->client->ps.ammo[WP_LIGHTNING];
                if (cells != 0) {
                    int dischargeRadius = (cells != -1 && !g_loadout.integer) ? cells + 1 : 60;
                    int extraRadius = dischargeRadius * g_damage_lg.integer;

                    SnapVector(muzzle);
                    tent = G_TempEntity(muzzle, EV_LIGHTNING_DISCHARGE);
                    tent->s.eventParm = dischargeRadius;
                    ent->client->ps.ammo[WP_LIGHTNING] = 0;

                    if (G_WaterRadiusDamage(muzzle, ent, (float)extraRadius, (float)(extraRadius + 16))) {
                        ent->client->accuracy_hits++;
                    }
                }
                break;
            }
        }

        VectorMA(muzzle, LIGHTNING_RANGE, forward, end);

        // g_playerCylinders + pmove_noPlayerClip
        {
            int mask = pmove_NoPlayerClip.integer ? (MASK_SHOT & ~CONTENTS_PLAYERCLIP) : MASK_SHOT;
            G_TracePlayerHit(&tr, muzzle, NULL, NULL, end, passent, mask);
        }

        // if not the first trace (the lightning bounced of an invulnerability sphere)
        if (i) {
            // add bounced off lightning bolt temp entity
            // the first lightning bolt is a cgame only visual
            //
            tent = G_TempEntity(muzzle, EV_LIGHTNINGBOLT);
            VectorCopy(tr.endpos, end);
            SnapVector(end);
            VectorCopy(end, tent->s.origin2);
        }

        if (tr.entityNum == ENTITYNUM_NONE) {
            return;
        }

        traceEnt = &g_entities[tr.entityNum];

        if (traceEnt->takedamage) {
            if (traceEnt->client && traceEnt->client->invulnerabilityTime > level.time) {
                if (G_InvulnerabilityEffect(traceEnt, forward, tr.endpos, impactpoint, bouncedir)) {
                    G_BounceProjectile(muzzle, impactpoint, bouncedir, end);
                    VectorCopy(impactpoint, muzzle);
                    VectorSubtract(end, impactpoint, forward);
                    VectorNormalize(forward);
                    // the player can hit him/herself with the bounced lightning
                    passent = ENTITYNUM_NONE;
                } else {
                    VectorCopy(tr.endpos, muzzle);
                    passent = traceEnt->s.number;
                }
                continue;
            }
            if (LogAccuracyHit(traceEnt, ent)) {
                ent->client->accuracy_hits++;
                ent->client->expandedStats.shotsHit[WP_LIGHTNING]++;
            }
            // [QL] LG distance falloff
            {
                int actualDamage = damage;
                if (g_damage_lg_falloff.integer && g_range_lg_falloff.integer > 0) {
                    float dist = Distance(muzzle, tr.endpos);
                    int steps = (int)(dist / g_range_lg_falloff.integer);
                    actualDamage -= steps * g_damage_lg_falloff.integer;
                    if (actualDamage < 1) actualDamage = 1;
                }
                G_Damage(traceEnt, ent, ent, forward, tr.endpos, actualDamage, 0, MOD_LIGHTNING);
            }
        }

        if (traceEnt->takedamage && traceEnt->client) {
            tent = G_TempEntity(tr.endpos, EV_MISSILE_HIT);
            tent->s.otherEntityNum = traceEnt->s.number;
            tent->s.eventParm = DirToByte(tr.plane.normal);
            tent->s.weapon = ent->s.weapon;
        } else if (!(tr.surfaceFlags & SURF_NOIMPACT)) {
            tent = G_TempEntity(tr.endpos, EV_MISSILE_MISS);
            tent->s.eventParm = DirToByte(tr.plane.normal);
        }

        break;
    }
}

/*
======================================================================

NAILGUN

======================================================================
*/

void Weapon_Nailgun_Fire(gentity_t* ent) {
    gentity_t* m;
    int count;

    for (count = 0; count < g_nailcount.integer; count++) {
        m = fire_nail(ent, muzzle, forward, right, up);
        m->damage *= s_quadFactor;
        m->splashDamage *= s_quadFactor;
    }

    //	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

/*
======================================================================

PROXIMITY MINE LAUNCHER

======================================================================
*/

void Weapon_ProxLauncher_Fire(gentity_t* ent) {
    gentity_t* m;

    // extra vertical velocity
    forward[2] += 0.2f;
    VectorNormalize(forward);

    m = fire_prox(ent, muzzle, forward);
    m->damage *= s_quadFactor;
    m->splashDamage *= s_quadFactor;

    //	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}

//======================================================================

/*
===============
LogAccuracyHit
===============
*/
qboolean LogAccuracyHit(gentity_t* target, gentity_t* attacker) {
    if (!target->takedamage) {
        return qfalse;
    }

    if (target == attacker) {
        return qfalse;
    }

    if (!target->client) {
        return qfalse;
    }

    if (!attacker->client) {
        return qfalse;
    }

    if (target->client->ps.stats[STAT_HEALTH] <= 0) {
        return qfalse;
    }

    if (OnSameTeam(target, attacker)) {
        return qfalse;
    }

    return qtrue;
}

/*
===============
CalcMuzzlePoint

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePoint(gentity_t* ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint) {
    VectorCopy(ent->s.pos.trBase, muzzlePoint);
    muzzlePoint[2] += ent->client->ps.viewheight;
    VectorMA(muzzlePoint, 14, forward, muzzlePoint);
    // snap to integer coordinates for more efficient network bandwidth usage
    SnapVector(muzzlePoint);
}

/*
===============
CalcMuzzlePointOrigin

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePointOrigin(gentity_t* ent, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint) {
    VectorCopy(ent->s.pos.trBase, muzzlePoint);
    muzzlePoint[2] += ent->client->ps.viewheight;
    VectorMA(muzzlePoint, 14, forward, muzzlePoint);
    // snap to integer coordinates for more efficient network bandwidth usage
    SnapVector(muzzlePoint);
}

/*
===============
FireWeapon
===============
*/
void FireWeapon(gentity_t* ent) {
    if (ent->client->ps.powerups[PW_QUAD]) {
        s_quadFactor = g_quadfactor.value;
    } else {
        s_quadFactor = 1;
    }

    if (ent->client->persistantPowerup && ent->client->persistantPowerup->item && ent->client->persistantPowerup->item->giTag == PW_DOUBLER) {
        s_quadFactor *= 2;
    }

    // track shots taken for accuracy tracking.  Grapple is not a weapon and gauntet is just not tracked
    if (ent->s.weapon != WP_GRAPPLING_HOOK && ent->s.weapon != WP_GAUNTLET) {
        if (ent->s.weapon == WP_NAILGUN) {
            ent->client->accuracy_shots += g_nailcount.integer;
        } else {
            ent->client->accuracy_shots++;
        }
        // [QL] per-weapon shot tracking
        if (ent->s.weapon < 16) {
            ent->client->expandedStats.shotsFired[ent->s.weapon]++;
        }
    }

    // set aiming directions
    AngleVectors(ent->client->ps.viewangles, forward, right, up);

    CalcMuzzlePointOrigin(ent, ent->client->ps.origin, forward, right, up, muzzle);

    // [QL] Lag compensation for hitscan weapons
    // Bitmask 0x60cc = weapons 2,3,6,7,13,14 (MG, SG, LG, RG, CG, HMG)
    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        if ((0x60cc >> (ent->s.weapon & 0x1f)) & 1) {
            HAX_Begin(ent, ent->client->ps.commandTime);
        }
    }

    // fire the specific weapon
    switch (ent->s.weapon) {
        case WP_GAUNTLET:
            Weapon_Gauntlet(ent);
            break;
        case WP_LIGHTNING:
            Weapon_Lightning_Fire(ent);
            break;
        case WP_SHOTGUN:
            Weapon_Shotgun_Fire(ent);
            break;
        case WP_MACHINEGUN:
            Bullet_Fire(ent, MACHINEGUN_SPREAD, g_damage_mg.integer, MOD_MACHINEGUN);
            break;
        case WP_GRENADE_LAUNCHER:
            Weapon_GrenadeLauncher_Fire(ent);
            break;
        case WP_ROCKET_LAUNCHER:
            Weapon_RocketLauncher_Fire(ent);
            break;
        case WP_PLASMAGUN:
            Weapon_Plasmagun_Fire(ent);
            break;
        case WP_RAILGUN:
            Weapon_Railgun_Fire(ent);
            break;
        case WP_BFG:
            Weapon_BFG_Fire(ent);
            break;
        case WP_GRAPPLING_HOOK:
            Weapon_GrapplingHook_Fire(ent);
            break;
        case WP_NAILGUN:
            Weapon_Nailgun_Fire(ent);
            break;
        case WP_PROX_LAUNCHER:
            Weapon_ProxLauncher_Fire(ent);
            break;
        case WP_CHAINGUN:
            Bullet_Fire(ent, CHAINGUN_SPREAD, g_damage_cg.integer, MOD_CHAINGUN);
            break;
        case WP_HMG:
            Bullet_Fire(ent, HMG_SPREAD, g_damage_hmg.integer, MOD_HMG);
            break;
        default:
            // FIXME		G_Error( "Bad ent->s.weapon" );
            break;
    }

    // [QL] End lag compensation
    if (g_lagHaxMs.integer != 0 && g_lagHaxHistory.integer != 0) {
        if ((0x60cc >> (ent->s.weapon & 0x1f)) & 1) {
            HAX_End(ent);
        }
    }
}

/*
===============
KamikazeRadiusDamage
===============
*/
static void KamikazeRadiusDamage(vec3_t origin, gentity_t* attacker, float damage, float radius) {
    float dist;
    gentity_t* ent;
    int entityList[MAX_GENTITIES];
    int numListedEntities;
    vec3_t mins, maxs;
    vec3_t v;
    vec3_t dir;
    int i, e;

    if (radius < 1) {
        radius = 1;
    }

    for (i = 0; i < 3; i++) {
        mins[i] = origin[i] - radius;
        maxs[i] = origin[i] + radius;
    }

    numListedEntities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    for (e = 0; e < numListedEntities; e++) {
        ent = &g_entities[entityList[e]];

        if (!ent->takedamage) {
            continue;
        }

        // don't hit things we have already hit
        if (ent->kamikazeTime > level.time) {
            continue;
        }

        // find the distance from the edge of the bounding box
        for (i = 0; i < 3; i++) {
            if (origin[i] < ent->r.absmin[i]) {
                v[i] = ent->r.absmin[i] - origin[i];
            } else if (origin[i] > ent->r.absmax[i]) {
                v[i] = origin[i] - ent->r.absmax[i];
            } else {
                v[i] = 0;
            }
        }

        dist = VectorLength(v);
        if (dist >= radius) {
            continue;
        }

        //		if( CanDamage (ent, origin) ) {
        VectorSubtract(ent->r.currentOrigin, origin, dir);
        // push the center of mass higher than the origin so players
        // get knocked into the air more
        dir[2] += 24;
        G_Damage(ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS | DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE);
        ent->kamikazeTime = level.time + 3000;
        //		}
    }
}

/*
===============
KamikazeShockWave
===============
*/
static void KamikazeShockWave(vec3_t origin, gentity_t* attacker, float damage, float push, float radius) {
    float dist;
    gentity_t* ent;
    int entityList[MAX_GENTITIES];
    int numListedEntities;
    vec3_t mins, maxs;
    vec3_t v;
    vec3_t dir;
    int i, e;

    if (radius < 1)
        radius = 1;

    for (i = 0; i < 3; i++) {
        mins[i] = origin[i] - radius;
        maxs[i] = origin[i] + radius;
    }

    numListedEntities = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    for (e = 0; e < numListedEntities; e++) {
        ent = &g_entities[entityList[e]];

        // don't hit things we have already hit
        if (ent->kamikazeShockTime > level.time) {
            continue;
        }

        // find the distance from the edge of the bounding box
        for (i = 0; i < 3; i++) {
            if (origin[i] < ent->r.absmin[i]) {
                v[i] = ent->r.absmin[i] - origin[i];
            } else if (origin[i] > ent->r.absmax[i]) {
                v[i] = origin[i] - ent->r.absmax[i];
            } else {
                v[i] = 0;
            }
        }

        dist = VectorLength(v);
        if (dist >= radius) {
            continue;
        }

        //		if( CanDamage (ent, origin) ) {
        VectorSubtract(ent->r.currentOrigin, origin, dir);
        dir[2] += 24;
        G_Damage(ent, NULL, attacker, dir, origin, damage, DAMAGE_RADIUS | DAMAGE_NO_TEAM_PROTECTION, MOD_KAMIKAZE);
        //
        dir[2] = 0;
        VectorNormalize(dir);
        if (ent->client) {
            ent->client->ps.velocity[0] = dir[0] * push;
            ent->client->ps.velocity[1] = dir[1] * push;
            ent->client->ps.velocity[2] = 100;
        }
        ent->kamikazeShockTime = level.time + 3000;
        //		}
    }
}

/*
===============
KamikazeDamage
===============
*/
static void KamikazeDamage(gentity_t* self) {
    int i;
    float t;
    gentity_t* ent;
    vec3_t newangles;

    self->count += 100;

    if (self->count >= KAMI_SHOCKWAVE_STARTTIME) {
        // shockwave push back
        t = self->count - KAMI_SHOCKWAVE_STARTTIME;
        KamikazeShockWave(self->s.pos.trBase, self->activator, 25, 400, (int)(float)t * KAMI_SHOCKWAVE_MAXRADIUS / (KAMI_SHOCKWAVE_ENDTIME - KAMI_SHOCKWAVE_STARTTIME));
    }
    //
    if (self->count >= KAMI_EXPLODE_STARTTIME) {
        // do our damage
        t = self->count - KAMI_EXPLODE_STARTTIME;
        KamikazeRadiusDamage(self->s.pos.trBase, self->activator, 400, (int)(float)t * KAMI_BOOMSPHERE_MAXRADIUS / (KAMI_IMPLODE_STARTTIME - KAMI_EXPLODE_STARTTIME));
    }

    // either cycle or kill self
    if (self->count >= KAMI_SHOCKWAVE_ENDTIME) {
        G_FreeEntity(self);
        return;
    }
    self->nextthink = level.time + 100;

    // add earth quake effect
    newangles[0] = crandom() * 2;
    newangles[1] = crandom() * 2;
    newangles[2] = 0;
    for (i = 0; i < MAX_CLIENTS; i++) {
        ent = &g_entities[i];
        if (!ent->inuse)
            continue;
        if (!ent->client)
            continue;

        if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE) {
            ent->client->ps.velocity[0] += crandom() * 120;
            ent->client->ps.velocity[1] += crandom() * 120;
            ent->client->ps.velocity[2] = 30 + random() * 25;
        }

        ent->client->ps.delta_angles[0] += ANGLE2SHORT(newangles[0] - self->movedir[0]);
        ent->client->ps.delta_angles[1] += ANGLE2SHORT(newangles[1] - self->movedir[1]);
        ent->client->ps.delta_angles[2] += ANGLE2SHORT(newangles[2] - self->movedir[2]);
    }
    VectorCopy(newangles, self->movedir);
}

/*
===============
G_StartKamikaze
===============
*/
void G_StartKamikaze(gentity_t* ent) {
    gentity_t* explosion;
    gentity_t* te;
    vec3_t snapped;

    // start up the explosion logic
    explosion = G_Spawn();

    explosion->s.eType = ET_EVENTS + EV_KAMIKAZE;
    explosion->eventTime = level.time;

    if (ent->client) {
        VectorCopy(ent->s.pos.trBase, snapped);
    } else {
        VectorCopy(ent->activator->s.pos.trBase, snapped);
    }
    SnapVector(snapped);  // save network bandwidth
    G_SetOrigin(explosion, snapped);

    explosion->classname = "kamikaze";
    explosion->s.pos.trType = TR_STATIONARY;

    explosion->kamikazeTime = level.time;

    explosion->think = KamikazeDamage;
    explosion->nextthink = level.time + 100;
    explosion->count = 0;
    VectorClear(explosion->movedir);

    trap_LinkEntity(explosion);

    if (ent->client) {
        //
        explosion->activator = ent;
        //
        ent->s.eFlags &= ~EF_KAMIKAZE;
        // nuke the guy that used it
        G_Damage(ent, ent, ent, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_KAMIKAZE);
    } else {
        if (!strcmp(ent->activator->classname, "bodyque")) {
            explosion->activator = &g_entities[ent->activator->r.ownerNum];
        } else {
            explosion->activator = ent->activator;
        }
    }

    // play global sound at all clients
    te = G_TempEntity(snapped, EV_GLOBAL_TEAM_SOUND);
    te->r.svFlags |= SVF_BROADCAST;
    te->s.eventParm = GTS_KAMIKAZE;
}