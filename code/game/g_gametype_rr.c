/*
 * g_gametype_rr.c -- Red Rover (GT_RR) round state machine
 *
 * Ported from ql-decompiled/game/g_redrover.c (Quake Live build 1069)
 *
 * RR: Players switch teams on death. Round ends when one team is eliminated.
 * Optional "rounds mode" with infection mechanic.
 */
#include "g_local.h"

// RR infection mode globals (binary: DAT_105df5a8-105df5b4)
static int rr_lastRedClient = -1;
static int rr_lastBlueClient = -1;
static int rr_survivalNextTime = 0;
static int rr_infectionStartTime = -1;

// ============================================================================
// RR_CheckInfection - periodic forced team switch (weakest blue → red)
// ============================================================================
void RR_CheckInfection(void) {
    int i;
    int redAlive, blueAlive;

    if (g_rrInfected.integer == 0)
        return;

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

    if (g_rrInfected.integer == 0 || g_rrInfectedSurvivorScoreMethod.integer == 0)
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
            trap_SendServerCommand(cl - level.clients,
                va("print \"Survival Bonus! +%i\n\"", g_rrInfectedSurvivorScoreBonus.integer));
        }
    }

    {
        gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
        te->s.eFlags |= EF_NODRAW;
        te->s.otherEntityNum2 = 24;
        te->s.otherEntityNum = TEAM_BLUE;
    }

    if (g_rrInfectedSurvivorScoreMethod.integer == 1) {
        rr_survivalNextTime = level.time + g_rrInfectedSurvivorScoreRate.integer * 1000;
    }

    CalculateRanks();
}

// ============================================================================
// PickTeam_RoundAware
// Binary: 0x10064820
//
// Round-state-aware team picker for RR infection mode.
// During active play, new joiners go to red (zombie) team.
// Between rounds, assigns to blue unless the client was the last red/blue.
// ============================================================================
team_t PickTeam_RoundAware(int clientNum) {
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            goto pickBlue;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        RR_RoundStateTransition();
    }

    if (level.roundState.eCurrent == RS_PLAYING)
        return TEAM_RED;

pickBlue:
    if (clientNum != rr_lastRedClient && clientNum != rr_lastBlueClient)
        return TEAM_BLUE;

    return TEAM_RED;
}

// ============================================================================
// RR_InitRoundState
// Binary: 0x100656e0
// Initializes the Red Rover round state at game start.
// When g_rrInfected is enabled and warmup is off, enters RS_SHUFFLE state
// to force a team shuffle before the first round.
// ============================================================================
void RR_InitRoundState(void) {
    if (g_rrInfected.integer != 0) {
        rr_lastRedClient = -1;
        rr_lastBlueClient = -1;
        rr_infectionStartTime = -1;
        level.roundState.tNext = level.time + 500;
        level.roundState.eNext = (level.warmupTime != 0) ? RS_WARMUP : RS_SHUFFLE;
        level.roundState.round = 0;
        return;
    }

    level.roundState.tNext = 0;
    level.roundState.eCurrent = (level.warmupTime == 0) ? RS_COUNTDOWN : RS_WARMUP;
    RR_RoundStateTransition();
    level.roundState.round = 0;
}

// ============================================================================
// StartNewRound
// Binary: 0x10064880 (exists but has zero callers in binary — dead code)
//
// Between-round team reset for infection mode:
// 1. Move all RED players to BLUE
// 2. If rr_lastBlueClient is valid, make them the first zombie (RED)
// 3. If no RED exists, pick a random playing client as zombie
// 4. Track the new zombie in rr_lastRedClient
// ============================================================================
static void StartNewRound(void) {
    int i;
    int redCount = 0;

    level.scoringDisabled = qtrue;

    // Move all red players to blue
    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected == CON_CONNECTED &&
            cl->ps.pm_type != PM_SPECTATOR &&
            cl->sess.sessionTeam == TEAM_RED) {
            SetTeam(&g_entities[i], "blue");
        }
    }

    // Last blue survivor becomes first zombie next round
    if (rr_lastBlueClient != -1) {
        gclient_t *cl = &level.clients[rr_lastBlueClient];
        if (cl->pers.connected == CON_CONNECTED &&
            cl->sess.sessionTeam == TEAM_BLUE) {
            SetTeam(&g_entities[rr_lastBlueClient], "red");
        }
    }

    // Count red players after the swap
    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected == CON_CONNECTED &&
            cl->ps.pm_type != PM_SPECTATOR &&
            cl->sess.sessionTeam == TEAM_RED) {
            redCount++;
        }
    }

    // If no red player, pick a random playing client
    if (redCount == 0 && level.numPlayingClients > 0) {
        int pick = rand() % level.numPlayingClients;
        int idx = level.sortedClients[pick];
        gclient_t *cl = &level.clients[idx];
        if (cl->pers.connected == CON_CONNECTED &&
            cl->ps.pm_type != PM_SPECTATOR) {
            SetTeam(&g_entities[idx], "red");
            rr_lastRedClient = cl->ps.clientNum;
        }
    }

    level.scoringDisabled = qfalse;
}

// ============================================================================
// RR_CheckExitRules
// Binary: 0x10064770
// Check if RR game should end (timelimit or roundlimit).
// RR uses total rounds (red+blue) for roundlimit, not per-team scores.
// No mercy limit for RR.
// ============================================================================
qboolean RR_CheckExitRules(int doExit) {
    if (ScoreIsTied())
        return qfalse;

    if (g_timelimit.integer != 0 &&
        g_timelimit.integer * 60000 <= level.time - level.startTime) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Timelimit hit.\n\"");
        LogExit(0, 0, "Timelimit hit.");
        return qtrue;
    }

    if (roundlimit.integer != 0 &&
        level.teamScores[TEAM_RED] + level.teamScores[TEAM_BLUE] >= roundlimit.integer) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Game hit the roundlimit.\n\"");
        LogExit(0, 0, "Roundlimit hit.");
        return qtrue;
    }

    return qfalse;
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

    case RS_SHUFFLE:
        if (g_rrInfected.integer == 0) {
            Svcmd_ForceShuffle_f();
            level.roundState.tNext = level.time + 1;
            level.roundState.eNext = RS_COUNTDOWN;
            return;
        }
        // Infected mode: no-op here. The infected RS_SHUFFLE team reset
        // (StartNewRound) is handled in RR_RunFrame after state transition.
        return;

    case RS_PLAYING:
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED) {
                cl->ps.pm_flags &= ~PMF_FROZEN;
                cl->round_shots = 0;
                cl->round_hits = 0;
                cl->round_damage = 0;
                cl->expandedStats.killStreak = 0;
                if (g_spawnArmor.integer != 0) {
                    cl->ps.powerups[PW_QUAD] =
                        (level.time / 1000) * 1000 + g_spawnArmor.integer;
                }
            }
        }
        level.roundState.round = level.teamScores[TEAM_BLUE] + level.teamScores[TEAM_RED] + 1;
        rr_survivalNextTime = 0;
        rr_lastBlueClient = -1;
        rr_infectionStartTime = level.time;
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
                level.roundState.eNext = RS_EXIT;
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
        level.roundState.eNext = RS_SHUFFLE;
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", -1));
        return;
    }

    case RS_EXIT:
        RR_CheckExitRules(1);
        Svcmd_ForceShuffle_f();
        return;

    default:
        G_Error("RR_RoundStateTransition: invalid state");
        return;
    }
}

// ============================================================================
// RR_RunFrame - per-frame logic for Red Rover
// Binary: RR_CheckRound (called from G_RunFrame case 0xc)
//
// Handles round state transitions via G_GetRoundState pattern:
//  - RS_SHUFFLE (2) + infected: calls StartNewRound, schedules RS_COUNTDOWN
//  - RS_PLAYING (3): alive count, survival bonus, infection, round timeout
// ============================================================================
void RR_RunFrame(void) {
    int redAlive, blueAlive;
    int state;

    if (level.intermissionQueued || level.intermissionTime || level.warmupTime > 0)
        return;

    if (level.restarted) {
        level.roundState.startTime += level.frametime;
    }

    // G_GetRoundState: process pending transition, return current state
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        RR_RoundStateTransition();
    }
    state = level.roundState.eCurrent;

    // Infected RS_SHUFFLE: reset teams and schedule countdown
    // Binary: RR_CheckRound handles this AFTER G_GetRoundState, not in
    // RR_RoundStateTransition (where infected RS_SHUFFLE is a no-op).
    if (state == RS_SHUFFLE) {
        if (g_rrInfected.integer != 0) {
            StartNewRound();
            level.roundState.tNext = level.time + 1;
            level.roundState.eNext = RS_COUNTDOWN;
        }
        return;
    }

    if (state != RS_PLAYING)
        return;

    UpdateTeamAliveCount(&redAlive, &blueAlive);

    // [QL] Infection mode: survival bonus + periodic team switch (binary order)
    RR_SurvivalBonus(0);
    RR_CheckInfection();

    // Check round end: team eliminated or timelimit (binary: CheckRoundTimeout)
    if (redAlive == 0 || blueAlive == 0 ||
        (roundtimelimit.integer != 0 &&
         level.time - level.roundState.startTime >= roundtimelimit.integer * 1000)) {
        level.roundState.tNext = 0;
        level.roundState.eCurrent = RS_ROUND_OVER;
        RR_RoundStateTransition();
    }
}

// ============================================================================
// RR_OnPlayerDeath - team switch on death (core RR mechanic)
// Binary: 0x10065760
//
// Non-infected mode: dead player swaps to opposite team.
// Infected mode: only blue->red on death, fires EV_INFECTED event,
//   resets infection timer, awards survival bonus, tracks last survivor.
// ============================================================================
void RR_OnPlayerDeath(gentity_t *victim) {
    int redAlive, blueAlive;
    int killedTeam;

    // Process pending round state transition
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

    if (!victim->client)
        return;

    killedTeam = victim->client->sess.sessionTeam;

    // Team switch on death (binary: direct sess.sessionTeam set + ClientUserinfoChanged)
    if (g_rrInfected.integer == 0 || killedTeam == TEAM_BLUE) {
        // Non-infected: swap any dead player; infected: only blue->red
        int newTeam = (killedTeam == TEAM_RED) ? TEAM_BLUE : TEAM_RED;
        victim->client->sess.sessionTeam = newTeam;
        ClientUserinfoChanged(victim->client->ps.clientNum);

        // In infected mode, fire EV_INFECTED event (binary: SVF_SINGLECLIENT to victim)
        if (g_rrInfected.integer != 0) {
            gentity_t *te = G_TempEntity(victim->r.currentOrigin, EV_INFECTED);
            te->r.svFlags |= SVF_SINGLECLIENT;
            te->r.singleClient = victim->client->ps.clientNum;
            te->s.otherEntityNum2 = victim->client->ps.clientNum;
        }
    }

    UpdateTeamAliveCount(&redAlive, &blueAlive);

    // Infected mode: reset infection timer + survival bonus on blue death
    if (g_rrInfected.integer != 0 && killedTeam == TEAM_BLUE) {
        rr_infectionStartTime = level.time;
        RR_SurvivalBonus(1);
        if (blueAlive == 0) {
            rr_lastBlueClient = victim->client->ps.clientNum;
        }
    }

    // Last man standing announcement
    if (redAlive != 0 && blueAlive != 0 && g_lastManStandingWarning.integer != 0) {
        if (redAlive == 1 && killedTeam == TEAM_RED && g_rrInfected.integer == 0) {
            LastManStanding(TEAM_RED);
        } else if (blueAlive == 1 && killedTeam == TEAM_BLUE) {
            LastManStanding(TEAM_BLUE);
        }
    }
}

/*
============
ClientSpawn_RedRover

[QL] Post-spawn adjustments for Red Rover. Binary: 0x10064640
In infected mode, red team gets forced loadout flags (gauntlet only, promode, double jump).
Blue team respects pmove cvars. Freezes during warmup/countdown.
============
*/
void ClientSpawn_RedRover(gentity_t *ent) {
    if (g_rrInfected.integer != 0) {
        gclient_t *cl = ent->client;
        if (cl->sess.sessionTeam == TEAM_RED) {
            // Zombies: forced loadout (gauntlet only), promode, double jump
            cl->ps.pm_flags |= PMF_LOADOUT_FORCED;
            cl->ps.pm_flags |= PMF_DOUBLE_JUMPED;
            cl->ps.pm_flags |= PMF_PROMODE;
        } else {
            // Survivors: clear forced loadout, respect pmove cvars
            cl->ps.pm_flags &= ~PMF_LOADOUT_FORCED;
            if (!pmove_DoubleJump.integer) {
                cl->ps.pm_flags &= ~PMF_DOUBLE_JUMPED;
            }
            if (!pmove_AirControl.integer) {
                cl->ps.pm_flags &= ~PMF_PROMODE;
            }
        }
    }

    // Freeze during warmup or countdown
    if (level.warmupTime > 0 || level.roundState.eCurrent == RS_COUNTDOWN) {
        ent->client->ps.pm_flags |= PMF_FROZEN;
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

    if (level.roundState.eCurrent != RS_COUNTDOWN) {
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
