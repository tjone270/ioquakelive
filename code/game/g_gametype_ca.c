/*
 * g_gametype_ca.c -- Clan Arena (GT_CA) round state machine
 *
 * Ported from ql-decompiled/game/g_ca_ad.c (Quake Live build 1069)
 *
 * CA: Round-based elimination. Teams fight until one is eliminated.
 * Rounds award +1 to the winning team's score.
 */
#include "g_local.h"

// ============================================================================
// Helper stubs for features not yet implemented
// ============================================================================

// Team health totals for tiebreaker
static void TeamCount_Health(int *aliveCounts, int *healthTotals) {
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

// [QL] Last Man Standing announcement - plays sound and centerprint to survivor
static void LastManStanding(int team) {
    int i;
    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected == CON_CONNECTED &&
            cl->ps.stats[STAT_HEALTH] > 0 &&
            cl->sess.sessionTeam == team) {
            gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->r.svFlags |= SVF_BROADCAST;
            te->s.eventParm = GTS_LAST_STANDING;
            te->s.otherEntityNum = cl->sess.sessionTeam;
            trap_SendServerCommand(i, "cp \"LAST MAN STANDING\n\"");
            return;
        }
    }
}

// [QL] EV_AWARD-based award: broadcasts medal event to all clients
static void PlayerAwardEV(gentity_t *ent, int awardIndex) {
    gentity_t *te;
    te = G_TempEntity(ent->r.currentOrigin, EV_AWARD);
    te->r.svFlags |= SVF_BROADCAST;
    te->s.otherEntityNum = ent->s.number;
    te->s.eventParm = awardIndex;
    te->s.otherEntityNum2 = 1;
    ent->client->rewardTime = level.time + REWARD_SPRITE_TIME;
}

// Stats stubs - ZMQ stats publishing requires Steam backend
static void STAT_PublishMedal(gentity_t *ent, const char *medal) {
    (void)ent; (void)medal;
}

static void STAT_RoundOver(int round, int winTeam, int isDraw) {
    (void)round; (void)winTeam; (void)isDraw;
}

// Team color strings
static const char *CA_TeamColor(int team) {
    if (team == TEAM_RED) return "^1";
    if (team == TEAM_BLUE) return "^4";
    return "^7";
}

// ============================================================================
// CA_RoundStateTransition
//
// State machine for Clan Arena rounds.
//   RS_WARMUP(0), RS_COUNTDOWN(1), RS_PLAYING(2), RS_ROUND_OVER(3), 5=exit
// ============================================================================
void CA_RoundStateTransition(void) {
    int i;
    int winTeam = 0;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        CA_RoundStateTransition();
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
            CA_RoundStateTransition();
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
                if (g_spawnArmor.integer != 0) {
                    cl->ps.powerups[PW_QUAD] =
                        (level.time / 1000) * 1000 + g_spawnArmor.integer;
                }
            }
        }

        level.roundState.round = level.teamScores[TEAM_BLUE] + level.teamScores[TEAM_RED] + 1;
        trap_SetConfigstring(CS_ROUND_TIME, va("%d", level.time));
        trap_SetConfigstring(CS_ROUND_STATUS,
            va("\\round\\%d", level.roundState.round));
        return;

    case RS_ROUND_OVER:
    {
        int aliveCounts[4] = {0}, healthTotals[4] = {0};

        // Freeze all non-spectator players
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR) {
                cl->ps.pm_flags |= PMF_FROZEN;
            }
        }

        UpdateTeamAliveCount(&aliveCounts[TEAM_RED], &aliveCounts[TEAM_BLUE]);
        TeamCount_Health(aliveCounts, healthTotals);

        // Determine winner
        if (aliveCounts[TEAM_RED] == 0) {
            if (aliveCounts[TEAM_BLUE] != 0) {
                winTeam = TEAM_BLUE;
                level.teamScores[TEAM_BLUE]++;
            }
        } else if (aliveCounts[TEAM_BLUE] == 0) {
            winTeam = TEAM_RED;
            level.teamScores[TEAM_RED]++;
        } else {
            // Both alive - living count then health comparison
            if (g_roundDrawLivingCount.integer != 0 &&
                aliveCounts[TEAM_RED] != aliveCounts[TEAM_BLUE]) {
                if (aliveCounts[TEAM_BLUE] < aliveCounts[TEAM_RED]) {
                    winTeam = TEAM_RED;
                    level.teamScores[TEAM_RED]++;
                } else {
                    winTeam = TEAM_BLUE;
                    level.teamScores[TEAM_BLUE]++;
                }
            } else if (g_roundDrawHealthCount.integer != 0) {
                if (healthTotals[TEAM_BLUE] < healthTotals[TEAM_RED]) {
                    winTeam = TEAM_RED;
                    level.teamScores[TEAM_RED]++;
                } else if (healthTotals[TEAM_RED] < healthTotals[TEAM_BLUE]) {
                    winTeam = TEAM_BLUE;
                    level.teamScores[TEAM_BLUE]++;
                }
            }
        }

        level.roundState.round++;
        CalculateRanks();

        // Announce winner
        if (winTeam) {
            const char *teamName = TeamName(winTeam);
            const char *teamColor = CA_TeamColor(winTeam);

            if (aliveCounts[winTeam] < 2) {
                trap_SendServerCommand(-1,
                    va("print \"%s%s TEAM^3 WINS the round!^7 (%i hp remaining)\n\"",
                        teamColor, teamName, healthTotals[winTeam]));
            } else {
                trap_SendServerCommand(-1,
                    va("print \"%s%s TEAM^3 WINS the round!^7 (%i players remaining)\n\"",
                        teamColor, teamName, aliveCounts[winTeam]));
            }
        }

        // Award medals
        for (i = 0; i < level.maxclients; i++) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent = &g_entities[i];

            if (cl->pers.connected != CON_CONNECTED)
                continue;

            // Accuracy check uses per-round stats
            if (cl->pers.roundShotsHit != 0) {
                // Award based on round performance
            }

            if (cl->sess.sessionTeam == winTeam && cl->pers.roundDamageTaken == 0) {
                PlayerAwardEV(ent, AWARD_PERFECT);
                STAT_PublishMedal(ent, "PERFECT");
            }
        }

        STAT_RoundOver(level.roundState.round - 1, winTeam, winTeam == 0);

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
        return;
    }

    default:  // state 5 = Exit
        CA_CheckExitRules(1);
        return;
    }
}

// ============================================================================
// CA_CheckExitRules
//
// Check if CA game should end (timelimit, roundlimit, or mercylimit).
// ============================================================================
qboolean CA_CheckExitRules(int doExit) {
    int mercyTime;

    if (level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE])
        return qfalse;

    if (g_timelimit.integer != 0 &&
        g_timelimit.integer * 60000 <= level.time - level.startTime) {
        if (!doExit) return qtrue;
        trap_SendServerCommand(-1, "print \"Timelimit hit.\n\"");
        LogExit(0, 0, "Timelimit hit.");
        return qtrue;
    }

    if (roundlimit.integer != 0) {
        if (roundlimit.integer <= level.teamScores[TEAM_RED]) {
            if (!doExit) return qtrue;
            trap_SendServerCommand(-1, "print \"Red hit the roundlimit.\n\"");
            LogExit(0, 0, "Roundlimit hit.");
            return qtrue;
        }
        if (roundlimit.integer <= level.teamScores[TEAM_BLUE]) {
            if (!doExit) return qtrue;
            trap_SendServerCommand(-1, "print \"Blue hit the roundlimit.\n\"");
            LogExit(0, 0, "Roundlimit hit.");
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
// CA_AccuracyMessage
//
// Modify damage/knockback based on g_accuracyFlags and round state.
// Also implements CA score-per-damage mechanic (100 dmg = +1 score).
// Returns qtrue if damage should be applied, qfalse to suppress.
// ============================================================================
qboolean CA_AccuracyMessage(gentity_t *target, gentity_t *attacker,
                             int *damage, int *knockback) {
    int flags = g_accuracyFlags.integer;

    if (target == attacker) {
        // Self-damage
        if (flags & 4) *damage = 0;
        flags &= 8;
    } else {
        gclient_t *tCl = target->client;
        gclient_t *aCl = attacker->client;

        if (tCl == NULL)
            goto checkRound;

        // Team damage filter
        if (aCl != NULL &&
            (g_gametype.integer >= GT_TEAM || tCl->sess.sessionTeam == TEAM_SPECTATOR) &&
            tCl->sess.sessionTeam == aCl->sess.sessionTeam &&
            (flags & 1)) {
            *damage = 0;
        }

        if (tCl == NULL || aCl == NULL ||
            (g_gametype.integer < GT_TEAM && tCl->sess.sessionTeam != TEAM_SPECTATOR) ||
            tCl->sess.sessionTeam != aCl->sess.sessionTeam)
            goto checkRound;

        flags &= 2;
    }

    if (flags != 0)
        *knockback = 0;

checkRound:
    if (level.warmupTime != 0)
        return qtrue;

    // Must be in playing state
    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return qfalse;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        CA_RoundStateTransition();
    }

    if (level.roundState.eCurrent != RS_PLAYING)
        return qfalse;

    // CA score-per-damage: accumulate damage, +1 score per 100
    if (attacker->client != NULL && !OnSameTeam(target, attacker) &&
        target->client != NULL && target->health > 0) {
        int dmg = target->client->ps.stats[STAT_ARMOR];
        if (dmg > *damage) dmg = *damage;
        int kb = target->client->ps.stats[STAT_MAX_HEALTH];
        if (kb > *knockback) kb = *knockback;

        attacker->client->pers.caDamageAccum += kb + dmg;
        if (attacker->client->pers.caDamageAccum >= 100) {
            attacker->client->pers.caDamageAccum -= 100;
            attacker->client->ps.persistant[PERS_SCORE]++;
            CalculateRanks();
        }
    }

    return qtrue;
}

// ============================================================================
// CA_RunFrame
//
// Per-frame logic for Clan Arena. Checks alive counts and triggers round end.
// Called from G_RunFrame.
// ============================================================================
void CA_RunFrame(void) {
    int redAlive, blueAlive;

    if (level.roundState.tNext != 0) {
        if (level.time < level.roundState.tNext)
            return;
        level.roundState.tNext = 0;
        level.roundState.eCurrent = level.roundState.eNext;
        level.roundState.startTime = level.time;
        CA_RoundStateTransition();
    }

    if (level.roundState.eCurrent != RS_PLAYING)
        return;
    if (level.intermissionQueued || level.intermissionTime || level.warmupTime)
        return;

    UpdateTeamAliveCount(&redAlive, &blueAlive);

    // Check if round is over (one team eliminated)
    if (redAlive == 0 || blueAlive == 0) {
        level.roundState.eCurrent = RS_ROUND_OVER;
        CA_RoundStateTransition();
        return;
    }

    // Round timelimit check
    if (roundtimelimit.integer != 0 &&
        level.time - level.roundState.startTime >= roundtimelimit.integer * 1000) {
        level.roundState.eCurrent = RS_ROUND_OVER;
        CA_RoundStateTransition();
        return;
    }

    // Last man standing warning
    if (g_lastManStandingWarning.integer != 0) {
        if (redAlive == 1) {
            LastManStanding(TEAM_RED);
        }
        if (blueAlive == 1) {
            LastManStanding(TEAM_BLUE);
        }
    }
}

// ============================================================================
// CA_PlayerDied
//
// Called when a player dies in CA. Checks if the round should end.
// ============================================================================
void CA_PlayerDied(gentity_t *self) {
    int redAlive, blueAlive;

    if (level.roundState.eCurrent != RS_PLAYING)
        return;

    UpdateTeamAliveCount(&redAlive, &blueAlive);

    if (redAlive == 0 || blueAlive == 0) {
        level.roundState.eCurrent = RS_ROUND_OVER;
        CA_RoundStateTransition();
    }
}

/*
==================
CAScoreboardMessage

Clan Arena scoreboard. 16 fields per player.
Address: 0x1003e4f0

Format: "scores_ca %i %i %i%s" numPlayers redScore blueScore playerData
Fields per player (16):
  client team score ping time frags deaths accuracy bestWeapon bestWeaponAccuracy
  damageDone impressive excellent gauntlet perfect alive
==================
*/
void CAScoreboardMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, j;
    gclient_t *cl;

    string[0] = 0;
    stringlength = 0;

    for (i = 0; i < level.numConnectedClients; i++) {
        int ping, accuracy, perfect, bestWeapon, bestWeaponAccuracy;
        int alive;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->accuracy_shots) {
            accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
        } else {
            accuracy = 0;
        }

        perfect = (cl->ps.persistant[PERS_RANK] == 0 && cl->ps.persistant[PERS_KILLED] == 0) ? 1 : 0;
        alive = (cl->ps.pm_type == PM_NORMAL) ? 1 : 0;

        if (cl->pers.connected == CON_CONNECTING) {
            ping = -1;
        } else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        bestWeapon = STAT_GetBestWeapon(cl);
        if (cl->expandedStats.shotsFired[bestWeapon] > 0) {
            bestWeaponAccuracy = cl->expandedStats.shotsHit[bestWeapon] * 100
                                / cl->expandedStats.shotsFired[bestWeapon];
        } else {
            bestWeaponAccuracy = 0;
        }

        // 16 fields per player
        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->sess.sessionTeam,
                    cl->ps.persistant[PERS_SCORE], ping,
                    (level.time - cl->pers.enterTime) / 60000,
                    cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                    accuracy, bestWeapon, bestWeaponAccuracy,
                    cl->expandedStats.totalDamageDealt,
                    cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                    cl->ps.persistant[PERS_EXCELLENT_COUNT],
                    cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                    perfect, alive);
        j = strlen(entry);
        if (stringlength + j >= (int)sizeof(string))
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(ent - g_entities,
        va("scores_ca %i %i %i%s", i,
           level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
           string));
}
