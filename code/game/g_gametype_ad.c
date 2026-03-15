/*
 * g_gametype_ad.c -- Attack & Defend (GT_AD) round state machine
 *
 * Ported from ql-decompiled/game/g_ca_ad.c (Quake Live build 1069)
 *
 * AD: Two-turn rounds where teams alternate attacking/defending flags.
 * Attackers score by capturing; defenders score by preventing capture.
 */
#include "g_local.h"

#define AD_SCORE_HISTORY_SIZE 20

// AD score history ring buffer
static int ad_scoreRaw[AD_SCORE_HISTORY_SIZE];
static int ad_scoreSorted[AD_SCORE_HISTORY_SIZE];
static int ad_scoreHistoryIndex;

// AD previous scores per turn
static int ad_prevScore[2];  // [0]=red, [1]=blue
static int ad_turnDelta[2];

// ============================================================================
// AD_UpdateScoreHistory
// ============================================================================
static void AD_UpdateScoreHistory(void) {
    int turn = level.roundState.turn;
    int idx, i;

    ad_turnDelta[turn] = level.teamScores[turn + 1] - ad_prevScore[turn];
    ad_scoreRaw[ad_scoreHistoryIndex % AD_SCORE_HISTORY_SIZE] = ad_turnDelta[turn];
    ad_prevScore[turn] = level.teamScores[turn + 1];

    ad_scoreHistoryIndex++;
    idx = ad_scoreHistoryIndex % AD_SCORE_HISTORY_SIZE;

    if (idx == ad_scoreHistoryIndex) {
        for (i = 0; i < AD_SCORE_HISTORY_SIZE; i++) {
            if (i == idx)
                ad_scoreSorted[i] = -2;
            else if (i > idx)
                ad_scoreSorted[i] = -1;
            else
                ad_scoreSorted[i] = ad_scoreRaw[i];
        }
    } else {
        int offset = idx + (turn != 0 ? 1 : 0) + 1;
        int count = 0;

        if (offset < AD_SCORE_HISTORY_SIZE) {
            count = AD_SCORE_HISTORY_SIZE - offset;
            memcpy(ad_scoreSorted, &ad_scoreRaw[offset], count * sizeof(int));
        }
        if (offset > 0) {
            memcpy(&ad_scoreSorted[count], ad_scoreRaw, (idx + (turn != 0 ? 1 : 0) + 1) * sizeof(int));
        }
        if (turn == 0) {
            ad_scoreSorted[AD_SCORE_HISTORY_SIZE - 1] = -1;
        }
    }

    trap_SendServerCommand(-1,
        va("scores_ad %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
            ad_scoreSorted[0], ad_scoreSorted[1], ad_scoreSorted[2], ad_scoreSorted[3],
            ad_scoreSorted[4], ad_scoreSorted[5], ad_scoreSorted[6], ad_scoreSorted[7],
            ad_scoreSorted[8], ad_scoreSorted[9], ad_scoreSorted[10], ad_scoreSorted[11],
            ad_scoreSorted[12], ad_scoreSorted[13], ad_scoreSorted[14], ad_scoreSorted[15],
            ad_scoreSorted[16], ad_scoreSorted[17], ad_scoreSorted[18], ad_scoreSorted[19],
            level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]));
}

// ============================================================================
// AD_InitScoreboard - reset score history and send initial scores_ad
// ============================================================================
static void AD_InitScoreboard(void) {
    int i;
    for (i = 0; i < AD_SCORE_HISTORY_SIZE; i++) {
        ad_scoreRaw[i] = -1;
        ad_scoreSorted[i] = -1;
    }
    ad_scoreHistoryIndex = 0;
    ad_prevScore[0] = 0;
    ad_prevScore[1] = 0;

    trap_SendServerCommand(-1,
        va("scores_ad %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]));
}

// ============================================================================
// AD_CheckExitRules - only checks after both turns (turn == 1)
// ============================================================================
qboolean AD_CheckExitRules(int doExit) {
    int mercyTime;

    if (level.roundState.turn != 1)
        return qfalse;
    if (level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE])
        return qfalse;

    if (g_timelimit.integer != 0 &&
        g_timelimit.integer * 60000 <= level.time - level.startTime) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Timelimit hit.\n\"");
        LogExit(0, 0, "Timelimit hit.");
        return qtrue;
    }

    if (g_scorelimit.integer != 0) {
        if (g_scorelimit.integer <= level.teamScores[TEAM_RED] &&
            level.teamScores[TEAM_BLUE] < level.teamScores[TEAM_RED]) {
            if (!doExit) return qtrue;
            trap_SendServerCommand(-1, "print \"Red hit the scorelimit.\n\"");
            LogExit(0, 0, "Scorelimit hit.");
            return qtrue;
        }
        if (g_scorelimit.integer <= level.teamScores[TEAM_BLUE] &&
            level.teamScores[TEAM_RED] < level.teamScores[TEAM_BLUE]) {
            if (!doExit) return qtrue;
            trap_SendServerCommand(-1, "print \"Blue hit the scorelimit.\n\"");
            LogExit(0, 0, "Scorelimit hit.");
            return qtrue;
        }
    }

    if (g_mercylimit.integer == 0)
        return qfalse;

    mercyTime = g_mercytime.integer * 60000 + level.timeOvertime;
    if (level.time - level.startTime < mercyTime)
        return qfalse;

    if (level.teamScores[TEAM_RED] - level.teamScores[TEAM_BLUE] >= g_mercylimit.integer) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Red hit the mercylimit.\n\"");
        LogExit(0, 0, "Mercylimit hit.");
        return qtrue;
    }
    if (level.teamScores[TEAM_BLUE] - level.teamScores[TEAM_RED] >= g_mercylimit.integer) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Blue hit the mercylimit.\n\"");
        LogExit(0, 0, "Mercylimit hit.");
        return qtrue;
    }

    return qfalse;
}

// ============================================================================
// AD_RoundStateTransition - two-turn round state machine
// ============================================================================
void AD_RoundStateTransition(void) {
    int i;
    int winTeam = 0;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        AD_RoundStateTransition();
    }

    switch (level.roundState.eCurrent) {
    case RS_WARMUP:
        AD_InitScoreboard();
        trap_SetConfigstring(CS_ROUND_STATUS,
            "\\time\\-1\\round\\0\\turn\\0\\state\\0");
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
            AD_RoundStateTransition();
        } else {
            level.roundState.tNext = level.time + g_roundWarmupDelay.integer;
            level.roundState.eNext = RS_PLAYING;
        }

        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\time\\%d\\round\\%d\\turn\\%d\\state\\1",
                level.time + g_roundWarmupDelay.integer, level.roundState.round, level.roundState.turn));
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

        level.roundState.capture = 0;
        level.roundState.touch = 0;

        trap_SetConfigstring(CS_ROUND_TIME, va("%d", level.time));
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\turn\\%d\\state\\2", level.roundState.turn));
        return;

    case RS_ROUND_OVER:
    {
        int redAlive, blueAlive;

        // Freeze all
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_flags |= PMF_FROZEN;
                cl->ps.powerups[PW_QUAD] = 0;
                cl->ps.powerups[PW_BATTLESUIT] = 0;
            }
        }

        UpdateTeamAliveCount(&redAlive, &blueAlive);

        // Determine winner
        if (level.roundState.capture == 1) {
            winTeam = (level.roundState.turn == 0) ? TEAM_RED : TEAM_BLUE;
        } else if (redAlive == 0 && blueAlive != 0) {
            if (level.roundState.turn == 1) {
                level.teamScores[TEAM_BLUE] += g_adElimScoreBonus.integer;
            }
            winTeam = TEAM_BLUE;
        } else if (blueAlive == 0 && redAlive != 0) {
            if (level.roundState.turn == 0) {
                level.teamScores[TEAM_RED] += g_adElimScoreBonus.integer;
            }
            winTeam = TEAM_RED;
        }

        CalculateRanks();
        AD_UpdateScoreHistory();

        // Return flags
        Team_ReturnFlag(TEAM_RED);
        Team_ReturnFlag(TEAM_BLUE);

        // Check for game end (only after second turn)
        if (level.roundState.turn == 1 && level.teamScores[TEAM_RED] != level.teamScores[TEAM_BLUE]) {
            if ((g_timelimit.integer != 0 &&
                 g_timelimit.integer * 60000 <= level.time - level.startTime) ||
                (g_scorelimit.integer != 0 &&
                 ((g_scorelimit.integer <= level.teamScores[TEAM_RED] &&
                   level.teamScores[TEAM_BLUE] < level.teamScores[TEAM_RED]) ||
                  (g_scorelimit.integer <= level.teamScores[TEAM_BLUE] &&
                   level.teamScores[TEAM_RED] < level.teamScores[TEAM_BLUE])))) {
                level.roundState.tNext = level.time + 1500;
                level.roundState.eNext = (roundStateState_t)5;
                goto toggleTurn;
            }
            if (g_mercylimit.integer != 0) {
                int mercyTime = g_mercytime.integer * 60000 + level.timeOvertime;
                if (mercyTime <= level.time - level.startTime) {
                    if (g_mercylimit.integer <= level.teamScores[TEAM_RED] - level.teamScores[TEAM_BLUE] ||
                        g_mercylimit.integer <= level.teamScores[TEAM_BLUE] - level.teamScores[TEAM_RED]) {
                        level.roundState.tNext = level.time + 1500;
                        level.roundState.eNext = (roundStateState_t)5;
                        goto toggleTurn;
                    }
                }
            }
        }

        // Round-end sound
        {
            int soundType;
            if (winTeam == TEAM_RED) soundType = 16;
            else if (winTeam == TEAM_BLUE) soundType = 17;
            else soundType = 18;
            gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->s.eFlags |= EF_NODRAW;
            te->s.otherEntityNum2 = soundType;
        }

        level.roundState.tNext = level.time + 3500;
        level.roundState.eNext = RS_COUNTDOWN;
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", -1));

    toggleTurn:
        level.roundState.turn ^= 1;
        if (level.roundState.turn == 0) {
            level.roundState.round++;
        }
        return;
    }

    default:  // state 5 = exit
        AD_CheckExitRules(1);
        return;
    }
}

// ============================================================================
// AD_RunFrame - per-frame logic
// ============================================================================
void AD_RunFrame(void) {
    int redAlive, blueAlive;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        AD_RoundStateTransition();
    }

    if (level.roundState.eCurrent != RS_PLAYING)
        return;
    if (level.intermissionQueued || level.intermissionTime || level.warmupTime)
        return;

    UpdateTeamAliveCount(&redAlive, &blueAlive);

    // Check round end: both dead, capture, or timeout
    if (redAlive == 0 || blueAlive == 0 || level.roundState.capture == 1) {
        level.roundState.eCurrent = RS_ROUND_OVER;
        AD_RoundStateTransition();
        return;
    }

    if (roundtimelimit.integer != 0 &&
        level.time - level.roundState.startTime >= roundtimelimit.integer * 1000) {
        level.roundState.eCurrent = RS_ROUND_OVER;
        AD_RoundStateTransition();
    }
}

// ============================================================================
// AD_IsInPlayState - returns attacking team (1=RED, 2=BLUE), 0 if not playing
// ============================================================================
int AD_IsInPlayState(void) {
    if (level.roundState.eCurrent != RS_PLAYING)
        return 0;
    return (level.roundState.turn != 0) ? 2 : 1;
}

// ============================================================================
// AD_CanScore - returns defending team number, 0 if not playing
// ============================================================================
int AD_CanScore(void) {
    if (level.roundState.eCurrent != RS_PLAYING)
        return 0;
    return 2 - (level.roundState.turn != 0 ? 1 : 0);
}

/*
==================
ADScoreboardMessage

Attack & Defend scoreboard. Sends 22 values (20 AD score history slots + 2 team scores).
All AD score slots are initialized to -1.
Address: 0x100365a0

Format: "scores_ad %d %d ... %d %d %d %d"
  20 AD score history slots (all -1) + teamScoreRed + teamScoreBlue
==================
*/
void ADScoreboardMessage(gentity_t *ent) {
    // [QL] AD mode: all 20 round score slots are -1 (placeholder)
    trap_SendServerCommand(-1,
        va("scores_ad %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
           -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
           -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
           level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]));
}
