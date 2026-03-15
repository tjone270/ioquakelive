/*
 * g_gametype_rr.c -- Red Rover (GT_RR) round state machine
 *
 * Ported from ql-decompiled/game/g_redrover.c (Quake Live build 1069)
 *
 * RR: Players switch teams on death. Round ends when one team is eliminated.
 * Optional "rounds mode" with infection mechanic.
 */
#include "g_local.h"

// RR infection mode globals
static int rr_infectionStartTime = -1;
static int rr_survivalNextTime = 0;

// ============================================================================
// RR_CheckInfection - periodic forced team switch (weakest blue → red)
// ============================================================================
void RR_CheckInfection(void) {
    int i;
    int redAlive, blueAlive;

    if (g_rrInfectedSpreadTime.integer == 0 || rr_infectionStartTime == -1)
        return;

    UpdateTeamAliveCount(&redAlive, &blueAlive);
    if (blueAlive <= 1)
        return;

    // Warning message
    if (g_rrInfectedSpreadWarningTime.integer != 0) {
        int elapsed = level.time - rr_infectionStartTime;
        int warningTime = (g_rrInfectedSpreadTime.integer - g_rrInfectedSpreadWarningTime.integer) * 1000;

        if (elapsed > warningTime && elapsed - warningTime <= level.frametime) {
            trap_SendServerCommand(-1,
                va("cp \" ^3Infection spreads in %i second%s!\"",
                    g_rrInfectedSpreadWarningTime.integer,
                    (g_rrInfectedSpreadWarningTime.integer != 1) ? "s" : ""));
        }
    }

    // Execute infection - find weakest blue (last in sorted order)
    if (level.time - rr_infectionStartTime > g_rrInfectedSpreadTime.integer * 1000) {
        for (i = level.numPlayingClients - 1; i >= 0; i--) {
            gclient_t *cl = &level.clients[level.sortedClients[i]];
            if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == TEAM_BLUE) {
                gentity_t *ent = &g_entities[level.sortedClients[i]];
                level.scoringDisabled = qtrue;
                SetTeam(ent, "red");
                level.scoringDisabled = qfalse;
                rr_infectionStartTime = level.time;
                return;
            }
        }
    }
}

// ============================================================================
// RR_SurvivalBonus - award points to surviving blue team
// ============================================================================
void RR_SurvivalBonus(int mode) {
    int i;

    if (g_rrInfectedSurvivorScoreMethod.integer == 0)
        return;

    if (rr_survivalNextTime == 0) {
        rr_survivalNextTime = level.time + g_rrInfectedSurvivorScoreRate.integer * 1000;
    }

    if (g_rrInfectedSurvivorScoreMethod.integer == 1) {
        if (level.time <= rr_survivalNextTime)
            return;
    } else if (g_rrInfectedSurvivorScoreMethod.integer == 2) {
        if (mode != 1)
            return;
    } else {
        return;
    }

    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == TEAM_BLUE) {
            cl->ps.persistant[PERS_SCORE] += g_rrInfectedSurvivorScoreBonus.integer;
        }
    }

    {
        gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
        te->s.eFlags |= EF_NODRAW;
        te->s.otherEntityNum2 = 24;
    }

    if (g_rrInfectedSurvivorScoreMethod.integer == 1) {
        rr_survivalNextTime = level.time + g_rrInfectedSurvivorScoreRate.integer * 1000;
    }

    CalculateRanks();
}

// ============================================================================
// RR_RoundStateTransition
// ============================================================================
void RR_RoundStateTransition(void) {
    int i;
    int winTeam = 0;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        RR_RoundStateTransition();
    }

    switch (level.roundState.eCurrent) {
    case RS_WARMUP:
        trap_SetConfigstring(CS_ROUND_STATUS, "\\time\\-1\\round\\0");
        return;

    case RS_COUNTDOWN:
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent = &g_entities[i];
            if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_type = PM_NORMAL;
                ClientSpawn(ent);
                cl->ps.pm_flags |= PMF_FROZEN;
            }
        }
        UpdateTeamAliveCount(NULL, NULL);

        if (g_roundWarmupDelay.integer == 0) {
            level.roundState.tNext = 0;
            level.roundState.eCurrent = RS_PLAYING;
            RR_RoundStateTransition();
        } else {
            level.roundState.tNext = level.time + g_roundWarmupDelay.integer;
            level.roundState.eNext = RS_PLAYING;
        }

        level.roundState.round = level.teamScores[TEAM_BLUE] + level.teamScores[TEAM_RED] + 1;
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\time\\%d\\round\\%d",
                level.time + g_roundWarmupDelay.integer, level.roundState.round));
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
        level.roundState.round = level.teamScores[TEAM_BLUE] + level.teamScores[TEAM_RED] + 1;
        rr_infectionStartTime = level.time;
        rr_survivalNextTime = 0;
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", level.time));
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\round\\%d", level.roundState.round));
        return;

    case RS_ROUND_OVER:
    {
        int redAlive, blueAlive;

        // Freeze all non-spectators
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_flags |= PMF_FROZEN;
            }
        }

        UpdateTeamAliveCount(&redAlive, &blueAlive);

        // Determine winner
        if (redAlive == 0 && blueAlive != 0) {
            winTeam = TEAM_BLUE;
            level.teamScores[TEAM_BLUE]++;
        } else if (blueAlive == 0 && redAlive != 0) {
            winTeam = TEAM_RED;
            level.teamScores[TEAM_RED]++;
        }

        level.roundState.round++;
        CalculateRanks();

        // Announce winner
        if (winTeam) {
            const char *teamColor = (winTeam == TEAM_RED) ? "^1" : "^4";
            trap_SendServerCommand(-1,
                va("print \"%s%s TEAM^3 WINS the round!\n\"",
                    teamColor, TeamName(winTeam)));
        }

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
        }

        // Round-end sound
        {
            int soundType = (winTeam == TEAM_RED || winTeam == TEAM_BLUE) ? 20 : 18;
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
// RR_RunFrame - per-frame logic for Red Rover
// ============================================================================
void RR_RunFrame(void) {
    int redAlive, blueAlive;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        RR_RoundStateTransition();
    }

    if (level.roundState.eCurrent != RS_PLAYING)
        return;
    if (level.intermissionQueued || level.intermissionTime || level.warmupTime)
        return;

    UpdateTeamAliveCount(&redAlive, &blueAlive);

    // [QL] Infection mode: periodic team switch + survival bonus
    RR_CheckInfection();
    RR_SurvivalBonus(0);

    if (redAlive == 0 || blueAlive == 0) {
        level.roundState.eCurrent = RS_ROUND_OVER;
        RR_RoundStateTransition();
        return;
    }

    // Round timelimit
    if (roundtimelimit.integer != 0 &&
        level.time - level.roundState.startTime >= roundtimelimit.integer * 1000) {
        level.roundState.eCurrent = RS_ROUND_OVER;
        RR_RoundStateTransition();
    }
}

// ============================================================================
// RR_OnPlayerDeath - team switch on death (core RR mechanic)
// ============================================================================
void RR_OnPlayerDeath(gentity_t *victim) {
    if (level.roundState.eCurrent != RS_PLAYING)
        return;
    if (!victim->client)
        return;

    // Switch team on death
    if (victim->client->sess.sessionTeam == TEAM_RED) {
        SetTeam(victim, "blue");
    } else if (victim->client->sess.sessionTeam == TEAM_BLUE) {
        SetTeam(victim, "red");
    }
}

/*
============
ClientBegin_RedRover

[QL] Red Rover begin - spawn normally, freeze during countdown
============
*/
void ClientBegin_RedRover(gentity_t* ent) {
    gclient_t* client = ent->client;

    if (level.roundState.eCurrent != 1) {
        // not in countdown: normal spawn
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        return;
    }
    // countdown: spawn + freeze
    ClientSpawn(ent);
    SelectSpawnWeapon(ent);
    client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
}

/*
==================
RRScoreboardMessage

Red Rover scoreboard. 19 fields per player.
Address: 0x1003f440

Format: "scores_rr %i %i %i%s" numPlayers redScore blueScore playerData
Fields per player (19):
  client score roundScore ping time frags deaths accuracy bestWeapon bestWeaponAccuracy
  damageDone impressive excellent gauntlet defend assist perfect captures alive
==================
*/
void RRScoreboardMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, j;
    gclient_t *cl;

    memset(string, 0, sizeof(string));
    stringlength = 0;

    for (i = 0; i < level.numConnectedClients; i++) {
        int ping, accuracy, perfect, bestWeapon, bestWeaponAccuracy;
        int alive;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->pers.connected == CON_CONNECTING) {
            ping = -1;
        } else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        if (cl->accuracy_shots) {
            accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
        } else {
            accuracy = 0;
        }

        perfect = (cl->ps.persistant[PERS_RANK] == 0 && cl->ps.persistant[PERS_KILLED] == 0) ? 1 : 0;
        alive = (cl->ps.pm_type == PM_NORMAL) ? 1 : 0;

        bestWeapon = STAT_GetBestWeapon(cl);
        if (cl->expandedStats.shotsFired[bestWeapon] > 0) {
            bestWeaponAccuracy = cl->expandedStats.shotsHit[bestWeapon] * 100
                                / cl->expandedStats.shotsFired[bestWeapon];
        } else {
            bestWeaponAccuracy = 0;
        }

        // 19 fields per player
        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->ps.persistant[PERS_SCORE],
                    cl->ps.localPersistant[0],  // round score
                    ping,
                    (level.time - cl->pers.enterTime) / 60000,
                    cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                    accuracy, bestWeapon, bestWeaponAccuracy,
                    cl->expandedStats.totalDamageDealt,
                    cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                    cl->ps.persistant[PERS_EXCELLENT_COUNT],
                    cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                    cl->ps.persistant[PERS_DEFEND_COUNT],
                    cl->ps.persistant[PERS_ASSIST_COUNT],
                    perfect,
                    cl->ps.persistant[PERS_CAPTURES],
                    alive);
        j = strlen(entry);
        if (stringlength + j >= (int)sizeof(string))
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(ent - g_entities,
        va("scores_rr %i %i %i%s", i,
           level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
           string));
}
