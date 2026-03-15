/*
 * g_unlagged.c - Lag compensation (HAX_* system)
 *
 * Based on Quake Live's lag compensation system (qagamex64.so symbols:
 * HAX_Init, HAX_Clear, HAX_Update, HAX_Begin, HAX_End).
 *
 * Stores position history per client. When a hitscan weapon fires,
 * all OTHER clients are rewound to their positions at the firer's
 * command time, traces are performed, then positions are restored.
 *
 * Controlled by g_lagHaxMs (max rewind in ms) and g_lagHaxHistory
 * (number of history entries per client).
 */
#include "g_local.h"

#define MAX_LAG_HISTORY 32  // max entries per client (g_lagHaxHistory clamped to this)

typedef struct {
    vec3_t mins;
    vec3_t maxs;
    vec3_t currentOrigin;
    int time;
    qboolean valid;
} lagRecord_t;

typedef struct {
    lagRecord_t history[MAX_LAG_HISTORY];
    int head;           // current write index
    int numEntries;     // g_lagHaxHistory.integer (clamped)
    // saved state for restore
    vec3_t savedOrigin;
    vec3_t savedMins;
    vec3_t savedMaxs;
    qboolean rewound;
} lagState_t;

static lagState_t lagState[MAX_CLIENTS];

/*
============
HAX_Init - allocate/initialize history buffers
Called from G_InitGame when g_lagHaxMs and g_lagHaxHistory are both non-zero.
============
*/
void HAX_Init(void) {
    int i;
    int numEntries = g_lagHaxHistory.integer;

    if (numEntries > MAX_LAG_HISTORY) {
        numEntries = MAX_LAG_HISTORY;
    }
    if (numEntries < 1) {
        numEntries = 1;
    }

    memset(lagState, 0, sizeof(lagState));
    for (i = 0; i < MAX_CLIENTS; i++) {
        lagState[i].numEntries = numEntries;
    }
}

/*
============
HAX_Clear - clear history for a specific player
Called on ClientSpawn and ClientDisconnect.
============
*/
void HAX_Clear(gentity_t *ent) {
    int clientNum = ent->s.number;

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        return;
    }

    memset(lagState[clientNum].history, 0, sizeof(lagState[clientNum].history));
    lagState[clientNum].head = 0;
    lagState[clientNum].rewound = qfalse;
}

/*
============
HAX_Update - store current position in history ring buffer
Called at the end of ClientEndFrame for each connected player.
============
*/
void HAX_Update(gentity_t *ent) {
    int clientNum = ent->s.number;
    lagState_t *ls;
    lagRecord_t *rec;

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        return;
    }
    if (!ent->client) {
        return;
    }

    ls = &lagState[clientNum];
    rec = &ls->history[ls->head % ls->numEntries];

    VectorCopy(ent->r.currentOrigin, rec->currentOrigin);
    VectorCopy(ent->r.mins, rec->mins);
    VectorCopy(ent->r.maxs, rec->maxs);
    rec->time = level.time;
    rec->valid = qtrue;

    ls->head++;
}

/*
============
HAX_Begin - rewind all OTHER clients to historical positions
Called before hitscan weapon traces. commandTime is the firing
player's ps.commandTime (their latency-adjusted server time).
============
*/
void HAX_Begin(gentity_t *shooter, int commandTime) {
    int i, j;
    int maxRewind = g_lagHaxMs.integer;
    int targetTime;
    int shooterNum = shooter->s.number;

    // Clamp rewind to g_lagHaxMs
    targetTime = commandTime;
    if (level.time - targetTime > maxRewind) {
        targetTime = level.time - maxRewind;
    }

    for (i = 0; i < level.maxclients; i++) {
        lagState_t *ls = &lagState[i];
        gentity_t *ent = &g_entities[i];

        ls->rewound = qfalse;

        // Don't rewind the shooter
        if (i == shooterNum) {
            continue;
        }
        // Only rewind connected, alive players
        if (!ent->inuse || !ent->client) {
            continue;
        }
        if (ent->client->pers.connected != CON_CONNECTED) {
            continue;
        }
        if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
            continue;
        }
        if (ent->health <= 0) {
            continue;
        }

        // Save current position
        VectorCopy(ent->r.currentOrigin, ls->savedOrigin);
        VectorCopy(ent->r.mins, ls->savedMins);
        VectorCopy(ent->r.maxs, ls->savedMaxs);

        // Find the two history entries bracketing targetTime and interpolate
        {
            lagRecord_t *prev = NULL;
            lagRecord_t *next = NULL;

            for (j = ls->head - 1; j >= 0 && j >= ls->head - ls->numEntries; j--) {
                lagRecord_t *rec = &ls->history[((j % ls->numEntries) + ls->numEntries) % ls->numEntries];
                if (!rec->valid) {
                    break;
                }
                if (rec->time <= targetTime) {
                    prev = rec;
                    break;
                }
                next = rec;
            }

            if (prev && next && next->time != prev->time) {
                // Interpolate between prev and next
                float frac = (float)(targetTime - prev->time) / (float)(next->time - prev->time);
                vec3_t newOrigin;

                newOrigin[0] = prev->currentOrigin[0] + frac * (next->currentOrigin[0] - prev->currentOrigin[0]);
                newOrigin[1] = prev->currentOrigin[1] + frac * (next->currentOrigin[1] - prev->currentOrigin[1]);
                newOrigin[2] = prev->currentOrigin[2] + frac * (next->currentOrigin[2] - prev->currentOrigin[2]);

                VectorCopy(newOrigin, ent->r.currentOrigin);
                VectorCopy(prev->mins, ent->r.mins);
                VectorCopy(prev->maxs, ent->r.maxs);
                ls->rewound = qtrue;
            } else if (prev) {
                // Exact match or only older record - use prev
                VectorCopy(prev->currentOrigin, ent->r.currentOrigin);
                VectorCopy(prev->mins, ent->r.mins);
                VectorCopy(prev->maxs, ent->r.maxs);
                ls->rewound = qtrue;
            } else if (next) {
                // Only newer record - use it
                VectorCopy(next->currentOrigin, ent->r.currentOrigin);
                VectorCopy(next->mins, ent->r.mins);
                VectorCopy(next->maxs, ent->r.maxs);
                ls->rewound = qtrue;
            }
            // If no records found, don't rewind (use current position)
        }

        if (ls->rewound) {
            trap_LinkEntity(ent);
        }
    }
}

/*
============
HAX_End - restore all clients to current positions
Called after hitscan weapon traces.
============
*/
void HAX_End(gentity_t *shooter) {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        lagState_t *ls = &lagState[i];
        gentity_t *ent = &g_entities[i];

        if (!ls->rewound) {
            continue;
        }

        VectorCopy(ls->savedOrigin, ent->r.currentOrigin);
        VectorCopy(ls->savedMins, ent->r.mins);
        VectorCopy(ls->savedMaxs, ent->r.maxs);
        ls->rewound = qfalse;

        trap_LinkEntity(ent);
    }
}
