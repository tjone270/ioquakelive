/*
 * g_gametype_dom.c -- Domination (GT_DOMINATION, 10)
 *
 * Ported from ql-decompiled/game/g_dom.c (Quake Live build 1069)
 *
 * Teams capture "team_dom_point" entities by standing near them.
 * Captured points periodically award team score via DOM_FlagThink.
 */
#include "g_local.h"

#define MAX_DOM_POINTS  5
#define DOM_SCORE_INTERVAL 5000  // score every 5 seconds

static gentity_t *domPoints[MAX_DOM_POINTS];
static int numDomPoints;

// ============================================================================
// DOM_FlagTouch - capture a dom point by standing on it
// ============================================================================
static void DOM_FlagTouch(gentity_t *ent, gentity_t *other, trace_t *trace) {
    if (!other->client)
        return;
    if (other->client->sess.sessionTeam != TEAM_RED &&
        other->client->sess.sessionTeam != TEAM_BLUE)
        return;

    // Already owned by this team
    if (ent->count == other->client->sess.sessionTeam)
        return;

    // Capture
    ent->count = other->client->sess.sessionTeam;
    trap_SendServerCommand(-1,
        va("print \"%s^7 captured a control point for %s!\n\"",
            other->client->pers.netname,
            TeamName(other->client->sess.sessionTeam)));

    // Update configstrings with capture counts
    {
        int i, redCount = 0, blueCount = 0;
        for (i = 0; i < numDomPoints; i++) {
            if (domPoints[i]->count == TEAM_RED) redCount++;
            else if (domPoints[i]->count == TEAM_BLUE) blueCount++;
        }
        trap_SetConfigstring(700, va("%d", redCount));
        trap_SetConfigstring(701, va("%d", blueCount));
    }
}

// ============================================================================
// DOM_FlagThink - periodic scoring for captured dom points
// ============================================================================
static void DOM_FlagThink(gentity_t *ent) {
    ent->nextthink = level.time + DOM_SCORE_INTERVAL;

    if (level.intermissionQueued || level.intermissionTime || level.warmupTime)
        return;

    if (ent->count == TEAM_RED) {
        level.teamScores[TEAM_RED]++;
        CalculateRanks();
    } else if (ent->count == TEAM_BLUE) {
        level.teamScores[TEAM_BLUE]++;
        CalculateRanks();
    }
}

// ============================================================================
// DOM_FlagFirstThink - drop dom point to ground, then activate
// ============================================================================
static void DOM_FlagFirstThink(gentity_t *ent) {
    trace_t tr;
    vec3_t dest;

    VectorCopy(ent->s.origin, dest);
    dest[2] -= 256.0f;

    trap_Trace(&tr, ent->s.origin, ent->r.mins, ent->r.maxs, dest,
        ENTITYNUM_WORLD, CONTENTS_SOLID);

    if (!tr.startsolid && tr.fraction < 1.0f) {
        VectorCopy(tr.endpos, ent->s.origin);
    }

    VectorCopy(ent->s.origin, ent->s.pos.trBase);
    VectorCopy(ent->s.origin, ent->r.currentOrigin);
    ent->s.pos.trType = TR_STATIONARY;

    ent->think = DOM_FlagThink;
    ent->touch = DOM_FlagTouch;
    ent->nextthink = level.time + DOM_SCORE_INTERVAL;
    ent->r.contents = CONTENTS_TRIGGER;

    trap_LinkEntity(ent);
}

// ============================================================================
// SP_team_dom_point - spawn function for "team_dom_point" entities
// ============================================================================
void SP_team_dom_point(gentity_t *ent) {
    if (g_gametype.integer != GT_DOMINATION) {
        G_FreeEntity(ent);
        return;
    }

    if (numDomPoints >= MAX_DOM_POINTS) {
        G_Printf("SP_team_dom_point: too many dom points (max %d)\n", MAX_DOM_POINTS);
        G_FreeEntity(ent);
        return;
    }

    VectorSet(ent->r.mins, -40, -40, -15);
    VectorSet(ent->r.maxs, 40, 40, 128);
    ent->r.contents = CONTENTS_TRIGGER;
    ent->count = TEAM_FREE;  // uncaptured

    ent->think = DOM_FlagFirstThink;
    ent->nextthink = level.time + 1;

    domPoints[numDomPoints] = ent;
    numDomPoints++;

    VectorCopy(ent->s.origin, ent->s.pos.trBase);
    VectorCopy(ent->s.origin, ent->r.currentOrigin);
    trap_LinkEntity(ent);
}
