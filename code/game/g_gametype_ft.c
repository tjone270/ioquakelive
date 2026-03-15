/*
 * g_gametype_ft.c -- Freeze Tag (GT_FREEZETAG) round state machine
 *
 * Ported from ql-decompiled/game/g_freeze.c (Quake Live build 1069)
 *
 * Players are frozen (PM_FREEZE) instead of killed. Teammates can thaw
 * frozen allies by standing near them. Round ends when all members of
 * one team are frozen.
 */
#include "g_local.h"

// ============================================================================
// Helper stubs (same as CA)
// ============================================================================
static void TeamCount_Health_FT(int *aliveCounts, int *healthTotals) {
    int i;
    healthTotals[TEAM_FREE] = 0;
    healthTotals[TEAM_RED] = 0;
    healthTotals[TEAM_BLUE] = 0;
    healthTotals[TEAM_SPECTATOR] = 0;

    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected != CON_CONNECTED) continue;
        if (cl->sess.sessionTeam == TEAM_SPECTATOR) continue;
        if (cl->ps.pm_type == PM_SPECTATOR) continue;
        if (cl->ps.stats[STAT_HEALTH] <= 0) continue;
        healthTotals[cl->sess.sessionTeam] += cl->ps.stats[STAT_HEALTH] + cl->ps.stats[STAT_ARMOR];
    }
}

static const char *FT_TeamColor(int team) {
    if (team == TEAM_RED) return "^1";
    if (team == TEAM_BLUE) return "^4";
    return "^7";
}

// ============================================================================
// Freeze_TeamFrozen
//
// Returns qtrue if all players on the given team are frozen (health <= 0).
// ============================================================================
qboolean Freeze_TeamFrozen(int team) {
    int i;
    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected == CON_CONNECTED &&
            cl->sess.sessionTeam == team &&
            cl->ps.stats[STAT_HEALTH] > 0) {
            return qfalse;
        }
    }
    return qtrue;
}

// ============================================================================
// Freeze_GameIsOver
//
// Check if the freeze round is over. Uses alive counts, frozen status,
// and optional round time limit. Sets prevRoundWinningTeam.
// ============================================================================
qboolean Freeze_GameIsOver(int *aliveCounts, int *healthTotals) {
    // Round time limit check
    if (roundtimelimit.integer != 0 &&
        roundtimelimit.integer * 1000 <= level.time - level.roundState.startTime) {
        if (g_roundDrawLivingCount.integer != 0) {
            if (aliveCounts[TEAM_BLUE] < aliveCounts[TEAM_RED]) {
                level.roundState.prevRoundWinningTeam = TEAM_RED;
                return qtrue;
            }
            if (aliveCounts[TEAM_RED] < aliveCounts[TEAM_BLUE]) {
                level.roundState.prevRoundWinningTeam = TEAM_BLUE;
                return qtrue;
            }
        }
        if (g_roundDrawHealthCount.integer != 0) {
            if (healthTotals[TEAM_BLUE] < healthTotals[TEAM_RED]) {
                level.roundState.prevRoundWinningTeam = TEAM_RED;
                return qtrue;
            }
            if (healthTotals[TEAM_RED] < healthTotals[TEAM_BLUE]) {
                level.roundState.prevRoundWinningTeam = TEAM_BLUE;
                return qtrue;
            }
        }
        level.roundState.prevRoundWinningTeam = TEAM_FREE;
        return qtrue;
    }

    // Check alive counts
    if (aliveCounts[TEAM_RED] == 0 && aliveCounts[TEAM_BLUE] == 0) {
        level.roundState.prevRoundWinningTeam = TEAM_FREE;
        return qtrue;
    }
    if (aliveCounts[TEAM_RED] == 0) {
        level.roundState.prevRoundWinningTeam = TEAM_BLUE;
        return qtrue;
    }
    if (aliveCounts[TEAM_BLUE] == 0) {
        level.roundState.prevRoundWinningTeam = TEAM_RED;
        return qtrue;
    }

    // Check frozen status (team frozen = all health <= 0)
    {
        qboolean redFrozen = Freeze_TeamFrozen(TEAM_RED);
        qboolean blueFrozen = Freeze_TeamFrozen(TEAM_BLUE);

        if (redFrozen && blueFrozen) {
            level.roundState.prevRoundWinningTeam = TEAM_FREE;
            return qtrue;
        }
        if (blueFrozen) {
            level.roundState.prevRoundWinningTeam = TEAM_RED;
            return qtrue;
        }
        if (redFrozen) {
            level.roundState.prevRoundWinningTeam = TEAM_BLUE;
            return qtrue;
        }
    }

    return qfalse;
}

// ============================================================================
// Freeze_RoundStateTransition
//
// Handle state transitions for Freeze Tag rounds.
// ============================================================================
void Freeze_RoundStateTransition(void) {
    int i;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        Freeze_RoundStateTransition();
    }

    switch (level.roundState.eCurrent) {
    case RS_WARMUP:
        trap_SetConfigstring(CS_ROUND_STATUS, "\\time\\-1\\round\\0");
        return;

    case RS_COUNTDOWN:
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent = &g_entities[i];

            if (cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR)
                continue;

            // Handle frozen/dead players from previous round
            if (cl->ps.pm_type == PM_FREEZE || cl->ps.pm_type == PM_DEAD) {
                if (g_freezeThawWinningTeam.integer != 0 ||
                    cl->sess.sessionTeam != level.roundState.prevRoundWinningTeam) {
                    cl->ps.pm_type = PM_NORMAL;
                    ClientSpawn(ent);
                }
            } else if (cl->ps.pm_type == PM_SPECTATOR) {
                cl->ps.pm_type = PM_NORMAL;
                ClientSpawn(ent);
            }

            cl->ps.pm_flags |= PMF_FROZEN;
        }

        UpdateTeamAliveCount(NULL, NULL);
        level.roundState.round = level.teamScores[TEAM_BLUE] + level.teamScores[TEAM_RED] + 1;

        {
            int countdown = g_freezeRoundDelay.integer;
            if (countdown <= 0) countdown = g_roundWarmupDelay.integer;
            if (countdown == 0) {
                level.roundState.tNext = 0;
                level.roundState.eCurrent = RS_PLAYING;
                Freeze_RoundStateTransition();
                return;
            }
            level.roundState.tNext = level.time + countdown;
            level.roundState.eNext = RS_PLAYING;
        }

        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\time\\%d\\round\\%d",
                level.roundState.tNext, level.roundState.round));
        return;

    case RS_PLAYING:
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED) {
                cl->ps.pm_flags &= ~PMF_FROZEN;
                cl->pers.roundDamageDealt = 0;
                cl->pers.roundShotsHit = 0;
                cl->pers.roundKillCount = 0;
            }
        }
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", level.time));
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\round\\%d", level.roundState.round));
        return;

    case RS_ROUND_OVER:
    {
        int winTeam = level.roundState.prevRoundWinningTeam;
        int aliveCounts[4] = {0}, healthTotals[4] = {0};

        // Freeze all non-spectators
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED &&
                cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_flags |= PMF_FROZEN;
            }
        }

        // Award team score
        if (winTeam == TEAM_RED) {
            level.teamScores[TEAM_RED]++;
        } else if (winTeam == TEAM_BLUE) {
            level.teamScores[TEAM_BLUE]++;
        }

        UpdateTeamAliveCount(&aliveCounts[TEAM_RED], &aliveCounts[TEAM_BLUE]);
        TeamCount_Health_FT(aliveCounts, healthTotals);

        // Announce winner
        if (winTeam == TEAM_RED || winTeam == TEAM_BLUE) {
            const char *teamName = TeamName(winTeam);
            const char *teamColor = FT_TeamColor(winTeam);
            int count = aliveCounts[winTeam];

            if (count < 2) {
                trap_SendServerCommand(-1,
                    va("print \"%s%s TEAM^3 WINS the round!^7 (%i hp remaining)\n\"",
                        teamColor, teamName, healthTotals[winTeam]));
            } else {
                trap_SendServerCommand(-1,
                    va("print \"%s%s TEAM^3 WINS the round!^7 (%i players remaining)\n\"",
                        teamColor, teamName, count));
            }
        }

        level.roundState.round++;
        CalculateRanks();

        // Check for game end
        if (level.teamScores[TEAM_RED] != level.teamScores[TEAM_BLUE]) {
            if ((g_timelimit.integer != 0 &&
                 g_timelimit.integer * 60000 <= level.time - level.startTime) ||
                (roundlimit.integer != 0 &&
                 (roundlimit.integer <= level.teamScores[TEAM_RED] ||
                  roundlimit.integer <= level.teamScores[TEAM_BLUE]))) {
                level.roundState.tNext = level.time + 1500;
                level.roundState.eNext = (roundStateState_t)5;
                return;
            }
            if (g_mercylimit.integer != 0) {
                int mercyTime = g_mercytime.integer * 60000 + level.timeOvertime;
                if (mercyTime <= level.time - level.startTime) {
                    if (g_mercylimit.integer <= level.teamScores[TEAM_RED] - level.teamScores[TEAM_BLUE] ||
                        g_mercylimit.integer <= level.teamScores[TEAM_BLUE] - level.teamScores[TEAM_RED]) {
                        level.roundState.tNext = level.time + 1500;
                        level.roundState.eNext = (roundStateState_t)5;
                        return;
                    }
                }
            }
        }

        // Round-end sound
        {
            int soundType;
            if (winTeam == TEAM_RED) soundType = 8;
            else if (winTeam == TEAM_BLUE) soundType = 9;
            else soundType = 18;
            gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->s.eFlags |= EF_NODRAW;
            te->s.otherEntityNum2 = soundType;
        }

        level.roundState.tNext = level.time + 3500;
        level.roundState.eNext = RS_COUNTDOWN;
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", -1));
        return;
    }

    default:  // state 5 = exit
        CA_CheckExitRules(1);
        return;
    }
}

// ============================================================================
// Freeze_Think
//
// Per-frame logic for Freeze Tag. Checks if round is over.
// Called from G_RunFrame.
// ============================================================================
void Freeze_Think(void) {
    int state;
    int aliveCounts[4] = {0}, healthTotals[4] = {0};

    if (level.intermissionQueued || level.intermissionTime || level.warmupTime)
        return;

    // Process pending state transition
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        Freeze_RoundStateTransition();
    }

    state = level.roundState.eCurrent;
    if (state == RS_PLAYING) {
        UpdateTeamAliveCount(&aliveCounts[TEAM_RED], &aliveCounts[TEAM_BLUE]);
        TeamCount_Health_FT(aliveCounts, healthTotals);
        if (Freeze_GameIsOver(aliveCounts, healthTotals)) {
            level.roundState.tNext = 0;
            level.roundState.eCurrent = RS_ROUND_OVER;
            Freeze_RoundStateTransition();
        }
    }
}

// ============================================================================
// Freeze_ClientThawCheck
//
// Per-frame per-frozen-player thaw detection. Checks if a teammate is
// within g_freezeThawRadius and has line of sight.
// Called from ClientEndFrame for each frozen player.
// ============================================================================
void Freeze_ClientThawCheck(gentity_t *ent) {
    int entityList[MAX_GENTITIES];
    int numNearby;
    int i;
    vec3_t mins, maxs;
    float radius;
    qboolean thawing = qfalse;
    gclient_t *cl = ent->client;

    if (!cl || cl->ps.stats[STAT_HEALTH] > 0)
        return;  // not frozen
    if (cl->ps.pm_type != PM_FREEZE && cl->ps.pm_type != PM_DEAD)
        return;
    if (level.roundState.eCurrent != RS_PLAYING)
        return;

    radius = g_freezeThawRadius.value;

    // Build search box
    for (i = 0; i < 3; i++) {
        mins[i] = ent->r.currentOrigin[i] - radius;
        maxs[i] = ent->r.currentOrigin[i] + radius;
    }

    numNearby = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    // Check for teammates in range
    for (i = 0; i < numNearby; i++) {
        gentity_t *other = &g_entities[entityList[i]];
        if (!other->client)
            continue;
        if (other == ent)
            continue;
        if (other->client->sess.sessionTeam != cl->sess.sessionTeam)
            continue;
        if (other->client->ps.stats[STAT_HEALTH] <= 0)
            continue;  // also frozen
        if (other->client->ps.pm_type == PM_SPECTATOR)
            continue;

        // Line of sight check (unless thaw through surface)
        if (!g_freezeThawThroughSurface.integer) {
            trace_t tr;
            trap_Trace(&tr, other->r.currentOrigin, NULL, NULL,
                       ent->r.currentOrigin, other->s.number, MASK_SOLID);
            if (tr.fraction < 1.0f)
                continue;  // blocked
        }

        thawing = qtrue;
        break;
    }

    if (thawing) {
        // Decrement thaw progress
        cl->freezePlayer--;  // repurpose as thaw counter
        if (cl->freezePlayer <= 0) {
            // Thaw complete!
            cl->ps.pm_type = PM_NORMAL;
            cl->ps.stats[STAT_HEALTH] = g_startingHealth.integer + g_startingHealthBonus.integer;
            if (cl->ps.stats[STAT_HEALTH] < 1) cl->ps.stats[STAT_HEALTH] = 100;
            ent->health = cl->ps.stats[STAT_HEALTH];
            cl->ps.stats[STAT_ARMOR] = g_startingArmor.integer;
            ent->takedamage = qtrue;
            cl->freezePlayer = 0;

            // Thaw event sound
            {
                gentity_t *te = G_TempEntity(ent->r.currentOrigin, EV_GLOBAL_TEAM_SOUND);
                te->s.eFlags |= EF_NODRAW;
                te->s.otherEntityNum2 = (cl->sess.sessionTeam == TEAM_RED) ? 4 : 5;
            }

            UpdateTeamAliveCount(NULL, NULL);
        }
    } else {
        // Refreeze progress (reset counter)
        if (cl->freezePlayer < g_freezeThawTime.integer / 50) {
            cl->freezePlayer++;
        }
    }
}

// ============================================================================
// Freeze_PlayerFrozen
//
// Called when a player is "killed" in Freeze Tag. Freezes them instead.
// ============================================================================
void Freeze_PlayerFrozen(gentity_t *self) {
    if (level.roundState.eCurrent != RS_PLAYING)
        return;

    self->client->ps.stats[STAT_HEALTH] = 0;
    self->client->ps.pm_type = PM_FREEZE;
    self->takedamage = qfalse;
    self->health = -40;
    self->client->freezePlayer = g_freezeThawTime.integer / 50;  // thaw counter

    UpdateTeamAliveCount(NULL, NULL);
}

/*
============
ClientBegin_Freeze

[QL] Freeze Tag begin - like RoundBased but clears freeze/dead pm_type first
============
*/
void ClientBegin_Freeze(gentity_t* ent) {
    gclient_t* client = ent->client;

    if (level.roundState.eCurrent == 0) {
        // pre-round: unfreeze if needed
        if (client->ps.pm_type == PM_FREEZE || client->ps.pm_type == PM_DEAD) {
            client->ps.pm_type = PM_NORMAL;
        }
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
    } else if (level.roundState.eCurrent == 1) {
        // countdown
        if (client->ps.pm_type == PM_FREEZE || client->ps.pm_type == PM_DEAD) {
            client->ps.pm_type = PM_NORMAL;
        }
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        if (level.roundState.eCurrent != 0) {
            client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
        }
    } else {
        // round active: spectator mode
        client->ps.pm_type = PM_SPECTATOR;
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        UpdateTeamAliveCount(NULL, NULL);
        if (!level.warmupTime
            && client->sess.sessionTeam != TEAM_SPECTATOR
            && level.numPlayingClients > 0) {
            Cmd_FollowCycle_f(ent, 1);
        }
    }
}

/*
==================
FTScoreboardMessage

Freeze Tag scoreboard. Same team stats structure as TDM (14 categories).
17 fields per player.
Address: 0x1003ef80

Format: "scores_ft %i %i ... %i %i %i%s"
  28 team stats + numPlayers + teamScoreRed + teamScoreBlue + playerData

Fields per player (17):
  client team score ping time frags deaths accuracy bestWeapon
  impressive excellent gauntlet assist teamKills teamKilled damageDone alive
==================
*/
void FTScoreboardMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, j;
    gclient_t *cl;
    int viewerTeam;
    int redStats[14], blueStats[14];

    string[0] = 0;
    stringlength = 0;

    // Load team item stats (same 14 categories as TDM)
    redStats[0]  = level.numRedArmorPickups[TEAM_RED];
    redStats[1]  = level.numYellowArmorPickups[TEAM_RED];
    redStats[2]  = level.numGreenArmorPickups[TEAM_RED];
    redStats[3]  = level.numMegaHealthPickups[TEAM_RED];
    redStats[4]  = level.numQuadDamagePickups[TEAM_RED];
    redStats[5]  = level.numBattleSuitPickups[TEAM_RED];
    redStats[6]  = level.numRegenerationPickups[TEAM_RED];
    redStats[7]  = level.numHastePickups[TEAM_RED];
    redStats[8]  = level.numInvisibilityPickups[TEAM_RED];
    redStats[9]  = level.quadDamagePossessionTime[TEAM_RED];
    redStats[10] = level.battleSuitPossessionTime[TEAM_RED];
    redStats[11] = level.regenerationPossessionTime[TEAM_RED];
    redStats[12] = level.hastePossessionTime[TEAM_RED];
    redStats[13] = level.invisibilityPossessionTime[TEAM_RED];

    blueStats[0]  = level.numRedArmorPickups[TEAM_BLUE];
    blueStats[1]  = level.numYellowArmorPickups[TEAM_BLUE];
    blueStats[2]  = level.numGreenArmorPickups[TEAM_BLUE];
    blueStats[3]  = level.numMegaHealthPickups[TEAM_BLUE];
    blueStats[4]  = level.numQuadDamagePickups[TEAM_BLUE];
    blueStats[5]  = level.numBattleSuitPickups[TEAM_BLUE];
    blueStats[6]  = level.numRegenerationPickups[TEAM_BLUE];
    blueStats[7]  = level.numHastePickups[TEAM_BLUE];
    blueStats[8]  = level.numInvisibilityPickups[TEAM_BLUE];
    blueStats[9]  = level.quadDamagePossessionTime[TEAM_BLUE];
    blueStats[10] = level.battleSuitPossessionTime[TEAM_BLUE];
    blueStats[11] = level.regenerationPossessionTime[TEAM_BLUE];
    blueStats[12] = level.hastePossessionTime[TEAM_BLUE];
    blueStats[13] = level.invisibilityPossessionTime[TEAM_BLUE];

    for (i = 0; i < level.numConnectedClients; i++) {
        int ping, accuracy, bestWeapon;
        int alive;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->accuracy_shots) {
            accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
        } else {
            accuracy = 0;
        }

        alive = (cl->ps.pm_type == PM_NORMAL) ? 1 : 0;

        if (cl->pers.connected == CON_CONNECTING) {
            ping = -1;
        } else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        bestWeapon = STAT_GetBestWeapon(cl);

        // Hide opponent team's stats
        viewerTeam = ent->client->sess.sessionTeam;
        if (viewerTeam == TEAM_RED && level.intermissionTime == 0) {
            memset(blueStats, 0, sizeof(blueStats));
        } else if (viewerTeam == TEAM_BLUE && level.intermissionTime == 0) {
            memset(redStats, 0, sizeof(redStats));
        }

        // 17 fields per player
        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->sess.sessionTeam,
                    cl->ps.persistant[PERS_SCORE], ping,
                    (level.time - cl->pers.enterTime) / 60000,
                    cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                    accuracy, bestWeapon,
                    cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                    cl->ps.persistant[PERS_EXCELLENT_COUNT],
                    cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                    cl->ps.persistant[PERS_ASSIST_COUNT],
                    cl->expandedStats.numTeamKills,
                    cl->expandedStats.numTeamKilled,
                    cl->expandedStats.totalDamageDealt,
                    alive);
        j = strlen(entry);
        if (stringlength + j >= (int)sizeof(string))
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(ent - g_entities,
        va("scores_ft %i %i %i %i %i %i %i %i %i %i %i %i %i %i "
           "%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i%s",
           redStats[0], redStats[1], redStats[2], redStats[3], redStats[4],
           redStats[5], redStats[6], redStats[7], redStats[8],
           redStats[9], redStats[10], redStats[11], redStats[12], redStats[13],
           blueStats[0], blueStats[1], blueStats[2], blueStats[3], blueStats[4],
           blueStats[5], blueStats[6], blueStats[7], blueStats[8],
           blueStats[9], blueStats[10], blueStats[11], blueStats[12], blueStats[13],
           i, level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
           string));
}

/*
==================
FTScoreboardMessage_impl

Freeze Tag per-player detail stats sent as "ctfstats" command.
12 fields per player: suicides damageDone damageTaken
  redArmorPickups yellowArmorPickups greenArmorPickups megaHealthPickups
  quadPickups battleSuitPickups regenPickups hastePickups invisPickups
Address: 0x1003ee30
==================
*/
void FTScoreboardMessage_impl(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int i;
    gclient_t *cl;

    for (i = 0; i < level.numConnectedClients; i++) {
        cl = &level.clients[level.sortedClients[i]];

        string[0] = 0;
        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i %i",
                    cl->expandedStats.numSuicides,
                    cl->expandedStats.totalDamageDealt,
                    cl->expandedStats.totalDamageTaken,
                    cl->expandedStats.numRedArmorPickups,
                    cl->expandedStats.numYellowArmorPickups,
                    cl->expandedStats.numGreenArmorPickups,
                    cl->expandedStats.numMegaHealthPickups,
                    cl->expandedStats.numQuadDamagePickups,
                    cl->expandedStats.numBattleSuitPickups,
                    cl->expandedStats.numRegenerationPickups,
                    cl->expandedStats.numHastePickups,
                    cl->expandedStats.numInvisibilityPickups);
        if (strlen(entry) < sizeof(string)) {
            strcpy(string, entry);
        }
        trap_SendServerCommand(ent - g_entities,
            va("ctfstats %i%s", i, string));
    }
}
