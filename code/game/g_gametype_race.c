/*
 * g_gametype_race.c -- Race mode (GT_RACE) checkpoint system
 *
 * Ported from ql-decompiled/game/g_race.c (Quake Live build 1069)
 *
 * Players race through checkpoint entities (race_point). Checkpoints are
 * linked via target/target2 chains. Start checkpoint has target2 but no
 * target; end checkpoint has no target2.
 */
#include "g_local.h"

#define MAX_RACE_POINTS      64
#define MAX_RACE_CHECKPOINTS 64

static int numRacePoints;

// ============================================================================
// FinishSpawningRacePoint - drop to ground, quantize coordinates
// ============================================================================
static void FinishSpawningRacePoint(gentity_t *ent) {
    trace_t tr;
    vec3_t dest;

    if (!(ent->spawnflags & 1)) {
        // Drop to ground using point trace (not bounding box) to avoid ceiling clipping
        VectorCopy(ent->s.origin, dest);
        dest[2] -= 4096.0f;

        trap_Trace(&tr, ent->s.origin, NULL, NULL, dest,
            ENTITYNUM_WORLD, CONTENTS_SOLID);

        if (tr.startsolid) {
            vec3_t start;
            VectorCopy(ent->s.origin, start);
            start[2] += 128.0f;
            trap_Trace(&tr, start, NULL, NULL, dest,
                ENTITYNUM_WORLD, CONTENTS_SOLID);
        }

        if (tr.fraction < 1.0f) {
            VectorCopy(tr.endpos, ent->s.origin);
            ent->s.groundEntityNum = tr.entityNum;
        }
    }

    // Quantize to integer + 1 unit above ground
    ent->s.origin[0] = (float)(int)ent->s.origin[0];
    ent->s.origin[1] = (float)(int)ent->s.origin[1];
    ent->s.origin[2] = (float)((int)(ent->s.origin[2] + 1.0f));

    VectorCopy(ent->s.origin, ent->s.pos.trBase);
    VectorClear(ent->s.pos.trDelta);
    VectorClear(ent->s.apos.trDelta);
    VectorCopy(ent->s.origin, ent->r.currentOrigin);

    ent->r.contents = CONTENTS_TRIGGER;
    trap_LinkEntity(ent);
}

// ============================================================================
// Touch_RaceCheckpoint - handles start, intermediate, and finish checkpoints
// ============================================================================
void Touch_RaceCheckpoint(gentity_t *self, gentity_t *other, trace_t *trace) {
    gclient_t *cl = other->client;
    raceInfo_t *ri;

    if (!cl || level.warmupTime > 0)
        return;

    ri = &cl->race;

    // Race checkpoint chaining uses standard target/targetname:
    //   START:  has target (forward), no targetname (not referenced by previous)
    //   MIDDLE: has both target (forward) and targetname (referenced by previous)
    //   FINISH: has targetname (referenced by previous), no target (end of chain)

    if (self->target == NULL) {
        // --- FINISH CHECKPOINT (no forward target = end of chain) ---
        if (!ri->racingActive)
            return;
        if (ri->nextRacePoint != self)
            return;

        // Record finish time
        ri->lastTime = level.time - ri->startTime;

        // Check personal best
        {
            qboolean newBest = qfalse;
            int i;

            if (ri->lastTime < cl->ps.localPersistant[0] || cl->ps.localPersistant[0] == 0) {
                cl->ps.localPersistant[0] = ri->lastTime;
                memcpy(ri->best_race, ri->current_race, sizeof(ri->best_race));
                newBest = qtrue;
            }

            cl->ps.persistant[PERS_SCORE] = ri->lastTime;

            // Race finish event - fields must match cgame EV_RACE_FINISH handler
            {
                gentity_t *te = G_TempEntity(other->r.currentOrigin, EV_RACE_FINISH);
                te->s.clientNum = other->s.number;
                te->s.time = ri->lastTime;  // finishTime
                te->s.powerups = 1;  // valid finish (not DNF)
                te->r.svFlags |= SVF_BROADCAST;
            }

            if (newBest) {
                trap_SendServerCommand(-1,
                    va("print \"^1Personal best! ^7%s^7 finished in %d.%03d\n\"",
                        cl->pers.netname, ri->lastTime / 1000, ri->lastTime % 1000));
            } else {
                trap_SendServerCommand(-1,
                    va("print \"%s^7 finished in %d.%03d\n\"",
                        cl->pers.netname, ri->lastTime / 1000, ri->lastTime % 1000));
            }
        }

        ri->nextRacePoint = NULL;
        ri->racingActive = qfalse;
        CalculateRanks();

    } else if (self->targetname == NULL) {
        // --- START CHECKPOINT (has target but no targetname = start of chain) ---

        // Rate limit: only fire once per second
        if (ri->racingActive && level.time - ri->startTime < 1000)
            return;

        gentity_t *next = G_PickTarget(self->target);
        if (!next)
            return;

        // Clear previous next-checkpoint highlight
        if (ri->nextRacePoint) {
            ri->nextRacePoint->s.generic1 = 0;
        }

        ri->nextRacePoint = next;
        ri->startTime = level.time;
        ri->racingActive = qtrue;
        ri->currentCheckPoint = 0;
        memset(ri->current_race, 0, sizeof(ri->current_race));

        // Highlight next checkpoint
        next->s.generic1 = 1;

        // Race start event - fields must match cgame EV_RACE_START handler
        {
            gentity_t *te = G_TempEntity(other->r.currentOrigin, EV_RACE_START);
            te->s.clientNum = other->s.number;
            te->s.otherEntityNum = level.time;  // startTime
            te->s.otherEntityNum2 = next->s.number;  // first checkpoint entity number
            te->s.eventParm = numRacePoints;  // total checkpoint count
            te->r.svFlags |= SVF_BROADCAST;
        }

    } else {
        // --- INTERMEDIATE CHECKPOINT (has both target and targetname) ---
        if (!ri->racingActive || ri->nextRacePoint != self)
            return;

        // Clear current highlight
        self->s.generic1 = 0;

        // Record split time
        ri->current_race[ri->currentCheckPoint] = level.time - ri->startTime;
        ri->currentCheckPoint++;

        // Advance to next checkpoint and highlight it
        {
            gentity_t *next = G_PickTarget(self->target);
            ri->nextRacePoint = next;
            if (next) {
                next->s.generic1 = 1;
            }
        }

        // Checkpoint event - fields must match cgame EV_RACE_CHECKPOINT handler
        {
            gentity_t *te = G_TempEntity(other->r.currentOrigin, EV_RACE_CHECKPOINT);
            te->s.clientNum = other->s.number;
            te->s.time = level.time - ri->startTime;  // split time
            te->s.otherEntityNum2 = numRacePoints;  // total checkpoint count
            te->s.generic1 = ri->nextRacePoint ? ri->nextRacePoint->s.number : 0;
            te->r.svFlags |= SVF_BROADCAST;
        }
    }
}

// ============================================================================
// SP_race_point - spawn function for "race_point" entities
// ============================================================================
void SP_race_point(gentity_t *ent) {
    if (g_gametype.integer != GT_RACE) {
        G_FreeEntity(ent);
        return;
    }

    // Must have at least target (forward) or targetname (referenced by previous)
    if (ent->target == NULL && ent->targetname == NULL) {
        G_FreeEntity(ent);
        return;
    }

    // [QL] race points are VISIBLE to clients - rendered as flag3 checkpoint markers
    // SVF_BROADCAST ensures they're sent to all clients regardless of PVS
    ent->r.svFlags |= SVF_BROADCAST;
    VectorSet(ent->r.mins, -40, -40, -15);
    VectorSet(ent->r.maxs, 40, 40, 128);
    ent->r.contents = CONTENTS_TRIGGER;

    ent->touch = Touch_RaceCheckpoint;
    ent->think = FinishSpawningRacePoint;
    ent->nextthink = level.time + 1;

    // [QL] encode spawnflags into eType for client-side identification
    ent->s.eType = ent->spawnflags;

    // [QL] Register the checkpoint visual model (binary-verified from qagamex86.dll 0x10063e47)
    // Three flag3 models depending on checkpoint type:
    {
        const char *modelName;
        if (ent->target == NULL) {
            // FINISH checkpoint (no forward target) - finish flag
            modelName = "models/flag3/f_flag3.md3";
        } else if (ent->targetname == NULL) {
            // START checkpoint (no targetname, has target) - green flag
            modelName = "models/flag3/g_flag3.md3";
            ent->count = 1;  // start marker
        } else {
            // INTERMEDIATE checkpoint (has both) - default checkpoint flag
            modelName = "models/flag3/d_flag3.md3";
        }
        ent->s.modelindex = G_ModelIndex((char *)modelName);
        ent->s.modelindex2 = ent->s.modelindex;
    }

    if (numRacePoints < MAX_RACE_POINTS) {
        numRacePoints++;
        trap_LinkEntity(ent);
    } else {
        G_FreeEntity(ent);
    }
}

// ============================================================================
// ClientBegin_Race - race gametype begin
// ============================================================================
void ClientBegin_Race(gentity_t* ent) {
    gclient_t* client = ent->client;

    // clear race info (timing, checkpoints)
    memset(&client->race, 0, sizeof(client->race));
    // sentinel value: no best time yet
    client->ps.persistant[PERS_SCORE] = 0x7FFFFFFF;

    // Find the start checkpoint and highlight the first target
    Race_ResetCheckpoints(ent);

    // send race_init to the client
    trap_SendServerCommand(ent - g_entities, "race_init");
    ClientSpawn(ent);
}

// ============================================================================
// Race_ResetCheckpoints - reset race state and highlight first checkpoint
// Called on spawn, death, and race_init
// ============================================================================
void Race_ResetCheckpoints(gentity_t *playerEnt) {
    int i;
    gentity_t *startPoint = NULL;
    gclient_t *client = playerEnt->client;

    // Clear racing state
    client->race.racingActive = qfalse;
    client->race.nextRacePoint = NULL;
    client->race.startTime = 0;
    client->race.currentCheckPoint = 0;

    // Find the start checkpoint (has target but no targetname)
    for (i = 0; i < level.num_entities; i++) {
        gentity_t *e = &g_entities[i];
        if (!e->inuse) continue;
        if (e->touch != Touch_RaceCheckpoint) continue;
        if (e->target != NULL && e->targetname == NULL) {
            startPoint = e;
            break;
        }
    }

    if (startPoint) {
        // Find the first checkpoint (targeted by start)
        gentity_t *firstCheckpoint = G_PickTarget(startPoint->target);
        if (firstCheckpoint) {
            // Clear all checkpoint highlights
            for (i = 0; i < level.num_entities; i++) {
                gentity_t *e = &g_entities[i];
                if (e->inuse && e->touch == Touch_RaceCheckpoint) {
                    e->s.generic1 = 0;
                }
            }
            // Highlight the first checkpoint - cgame reads s.generic1 for glow
            firstCheckpoint->s.generic1 = 1;
        }
    }
}

// ============================================================================
// Race_GetNumPoints - accessor for race point count
// ============================================================================
int Race_GetNumPoints(void) {
    return numRacePoints;
}
