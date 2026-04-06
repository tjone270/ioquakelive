/*
 * g_gametype_dom.c -- Domination (GT_DOMINATION, 10)
 *
 * Ported from ql-decompiled/game/g_dom.c (Quake Live build 1069)
 *
 * Teams capture "team_dom_point" entities by standing near them.
 * Captured points periodically award team score via DOM_FlagThink.
 * The capturer gets CAPTURE medal, nearby teammates get ASSIST.
 * Killing enemies near owned points awards DEFENSE medal.
 *
 * Binary addresses (qagamex86.dll):
 *   DOM_CheckFlagScore    0x1004ac40
 *   DOM_AddTeamScores     0x1004adb0
 *   DOM_FlagTouch         0x1004aea0
 *   DOM_FlagThink         0x1004b790
 *   DOM_FlagFirstThink    0x1004b8e0
 *   DOM_CheckPlayers      0x1004bac0
 *   DOM_FragBonuses       0x1004bb40
 *   SP_team_dom_point     0x1004bcd0
 */
#include "g_local.h"

#define MAX_DOM_POINTS      5
#define DOM_SCORE_INTERVAL  5000    // score every 5 seconds

static gentity_t *domPoints[MAX_DOM_POINTS];
static int numDomPoints;

// Forward declarations
static void DOM_FlagThink(gentity_t *ent);
static void DOM_FlagTouch(gentity_t *ent, gentity_t *other, trace_t *trace);

// ============================================================================
// DOM_CheckFlagScore (binary: 0x1004ac40)
//
// Award individual player scores when a dom point ticks.
// The capturer (ent->domCapturer) gets CAPTURE medal + 25 score.
// Other nearby same-team players get ASSIST medal + 15 score.
// ============================================================================
static void DOM_CheckFlagScore(gentity_t *ent, gentity_t **nearbyPlayers, int count) {
    int i;

    for (i = 0; i < count; i++) {
        gentity_t *player = nearbyPlayers[i];
        gclient_t *cl = player->client;

        if (ent->domCapturer != NULL && player == ent->domCapturer) {
            // Capturer gets CAPTURE medal
            cl->ps.persistant[PERS_CAPTURES]++;
            cl->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
            cl->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT |
                               EF_AWARD_GAUNTLET | EF_AWARD_ASSIST |
                               EF_AWARD_DEFEND | EF_AWARD_CAP);
            cl->ps.eFlags |= EF_AWARD_CAP;
            cl->rewardTime = level.time + REWARD_SPRITE_TIME;
            AddScore(player, player->r.currentOrigin, 25);
            STAT_PublishMedal(player, "CAPTURE");
        } else {
            // Teammates get ASSIST medal
            cl->ps.persistant[PERS_ASSIST_COUNT]++;
            cl->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
            cl->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT |
                               EF_AWARD_GAUNTLET | EF_AWARD_ASSIST |
                               EF_AWARD_DEFEND | EF_AWARD_CAP);
            cl->ps.eFlags |= EF_AWARD_ASSIST;
            cl->rewardTime = level.time + REWARD_SPRITE_TIME;
            AddScore(player, player->r.currentOrigin, 15);
            STAT_PublishMedal(player, "ASSIST");
        }
    }
}

// ============================================================================
// DOM_AddTeamScores (binary: 0x1004adb0)
//
// Determine which player gets credit as capturer based on team presence.
// Single player on a team = that player is capturer.
// Multiple players = keep existing capturer if still present.
// Both teams present = contested (no capturer).
// ============================================================================
static void DOM_AddTeamScores(int redCount, int blueCount, gentity_t *ent,
                              gentity_t **redPlayers, gentity_t **bluePlayers) {
    gentity_t *prev;
    int j;

    if (redCount == 0 && blueCount <= 0)
        return;

    if (redCount == 0 && blueCount > 0) {
        // Only blue team present
        if (blueCount == 1) {
            ent->domCapturer = bluePlayers[0];
            return;
        }
        // Multiple blue — keep existing capturer if still present
        prev = ent->domCapturer;
        if (prev != NULL && prev->client != NULL) {
            if (prev->client->sess.sessionTeam == TEAM_BLUE) {
                for (j = 0; j < blueCount; j++) {
                    if (prev == bluePlayers[j])
                        return;  // existing capturer still nearby
                }
            }
        }
        ent->domCapturer = bluePlayers[0];
        return;
    }

    if (redCount > 0 && blueCount == 0) {
        // Only red team present
        if (redCount == 1) {
            ent->domCapturer = redPlayers[0];
            return;
        }
        // Multiple red — keep existing capturer if still present
        prev = ent->domCapturer;
        if (prev != NULL && prev->client != NULL) {
            if (prev->client->sess.sessionTeam == TEAM_RED) {
                for (j = 0; j < redCount; j++) {
                    if (prev == redPlayers[j])
                        return;  // existing capturer still nearby
                }
            }
        }
        ent->domCapturer = redPlayers[0];
        return;
    }

    // Both teams present — contested, no capturer
    ent->domCapturer = NULL;
}

// ============================================================================
// DOM_FlagTouch (binary: 0x1004aea0)
//
// Called when a player touches a dom point. Scans nearby players by team,
// determines capturer, and handles capture/neutralize transitions.
// ============================================================================
static void DOM_FlagTouch(gentity_t *ent, gentity_t *other, trace_t *trace) {
    int i;
    int redCount = 0, blueCount = 0;
    gentity_t *redPlayers[MAX_CLIENTS];
    gentity_t *bluePlayers[MAX_CLIENTS];

    if (!other->client)
        return;
    if (other->client->sess.sessionTeam != TEAM_RED &&
        other->client->sess.sessionTeam != TEAM_BLUE)
        return;

    // Scan all connected players to find who's near this point
    for (i = 0; i < level.maxclients; i++) {
        gentity_t *player = &g_entities[i];
        gclient_t *cl = player->client;

        if (!cl || cl->pers.connected != CON_CONNECTED)
            continue;
        if (cl->sess.sessionTeam != TEAM_RED && cl->sess.sessionTeam != TEAM_BLUE)
            continue;
        if (cl->ps.pm_type == PM_DEAD)
            continue;

        // Check distance (use trigger bounds as radius — roughly 40 units)
        if (Distance(ent->s.pos.trBase, player->r.currentOrigin) > 80.0f)
            continue;

        if (cl->sess.sessionTeam == TEAM_RED) {
            redPlayers[redCount++] = player;
        } else {
            bluePlayers[blueCount++] = player;
        }
    }

    DOM_AddTeamScores(redCount, blueCount, ent, redPlayers, bluePlayers);

    // Determine new team ownership
    if (redCount > 0 && blueCount == 0 && ent->count != TEAM_RED) {
        ent->count = TEAM_RED;
        trap_SendServerCommand(-1,
            va("print \"%s^7 captured a control point for %s!\n\"",
                other->client->pers.netname, TeamName(TEAM_RED)));
        DOM_CheckFlagScore(ent, redPlayers, redCount);
    } else if (blueCount > 0 && redCount == 0 && ent->count != TEAM_BLUE) {
        ent->count = TEAM_BLUE;
        trap_SendServerCommand(-1,
            va("print \"%s^7 captured a control point for %s!\n\"",
                other->client->pers.netname, TeamName(TEAM_BLUE)));
        DOM_CheckFlagScore(ent, bluePlayers, blueCount);
    }

    // Update configstrings
    {
        int rc = 0, bc = 0;
        for (i = 0; i < numDomPoints; i++) {
            if (domPoints[i]->count == TEAM_RED) rc++;
            else if (domPoints[i]->count == TEAM_BLUE) bc++;
        }
        trap_SetConfigstring(700, va("%d", rc));
        trap_SetConfigstring(701, va("%d", bc));
    }
}

// ============================================================================
// DOM_FlagThink (binary: 0x1004b790)
//
// Periodic scoring for captured dom points. Awards +1 team score per tick.
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
// DOM_FlagFirstThink (binary: 0x1004b8e0)
//
// Initial think: drop dom point to ground, then activate touch/think.
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
// DOM_FragBonuses (binary: 0x1004bb40)
//
// Award DEFENSE medal (+10) for killing an enemy near a dom point your team owns.
// Requires distance < 500 and PVS line of sight.
// ============================================================================
void DOM_FragBonuses(gentity_t *attacker, gentity_t *victim) {
    int i;
    float dist;

    if (!attacker || !attacker->client || !victim || !victim->client)
        return;
    if (attacker->client->sess.sessionTeam == victim->client->sess.sessionTeam)
        return;

    for (i = 0; i < numDomPoints; i++) {
        gentity_t *point = domPoints[i];

        if (point->count != attacker->client->sess.sessionTeam)
            continue;

        dist = Distance(point->s.pos.trBase, victim->s.pos.trBase);
        if (dist >= 500.0f)
            continue;

        if (!trap_InPVS(point->s.pos.trBase, victim->s.pos.trBase))
            continue;

        // Defense bonus
        attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
        attacker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
        attacker->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT |
                                         EF_AWARD_GAUNTLET | EF_AWARD_ASSIST |
                                         EF_AWARD_DEFEND | EF_AWARD_CAP);
        attacker->client->ps.eFlags |= EF_AWARD_DEFEND;
        attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
        AddScore(attacker, attacker->r.currentOrigin, 10);
        STAT_PublishMedal(attacker, "DEFEND");
        return;  // only one defense bonus per kill
    }
}

// ============================================================================
// SP_team_dom_point (binary: 0x1004bcd0)
//
// Spawn function for "team_dom_point" entities. Only active in GT_DOMINATION.
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
    ent->count = TEAM_FREE;       // uncaptured
    ent->domCapturer = NULL;

    ent->think = DOM_FlagFirstThink;
    ent->nextthink = level.time + 1;

    domPoints[numDomPoints] = ent;
    numDomPoints++;

    VectorCopy(ent->s.origin, ent->s.pos.trBase);
    VectorCopy(ent->s.origin, ent->r.currentOrigin);
    trap_LinkEntity(ent);
}
