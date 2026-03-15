/*
 * g_gametype_duel.c -- Duel / Tournament (GT_DUEL, 1)
 *
 * 1v1 mode with a queue. Players wait as spectators and rotate in.
 * Queue management: CheckTournament() (below).
 * Scoreboard: DuelScoreboardMessage (below).
 */
#include "g_local.h"

// Duel uses fraglimit/timelimit exit rules.
// No round state.

/*
==================
DuelScoreboardMessage_impl

Simple duel scoreboard (scores command) - same 18 fields as FFA but with
zeros for frags/deaths fields, adds bestWeapon at end.
Address: 0x1003d0b0
==================
*/
void DuelScoreboardMessage_impl(void) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, j;
    gclient_t *cl;

    string[0] = 0;
    stringlength = 0;

    for (i = 0; i < level.numConnectedClients; i++) {
        int ping, accuracy, perfect, bestWeapon;

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
        bestWeapon = STAT_GetBestWeapon(cl);

        // 18 fields per player (frags/deaths hardcoded to 0)
        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime) / 60000,
                    0, 0,  // frags, deaths hardcoded to 0 in binary
                    accuracy,
                    cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                    cl->ps.persistant[PERS_EXCELLENT_COUNT],
                    cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                    cl->ps.persistant[PERS_DEFEND_COUNT],
                    cl->ps.persistant[PERS_ASSIST_COUNT],
                    perfect,
                    cl->ps.persistant[PERS_CAPTURES],
                    (cl->ps.pm_type == PM_NORMAL) ? 1 : 0,
                    cl->expandedStats.numKills,
                    cl->expandedStats.numDeaths,
                    bestWeapon);
        j = strlen(entry);
        if (stringlength + j >= (int)sizeof(string))
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(-1, va("scores %i %i %i%s", i,
                                   level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
                                   string));
}

/*
==================
DuelScoreboardMessage

Full duel scoreboard with per-weapon stats and item timing data.
Per player: 14 base fields + 5*14 weapon fields + 8 item timing fields (via 2 passes).
Builds separate strings for player 1 and player 2 with "self" and "opponent" views.
Address: 0x1003d4a0

Per-weapon fields (weapons 1-14, 5 fields each):
  shotsHit shotsFired accuracy damageDealt numWeaponKills

Base fields per player (14):
  client score ping time frags deaths accuracy bestWeapon damageDone
  impressive excellent gauntlet perfect

Item timing fields (4 categories, 2 fields each):
  redArmorPickups redArmorAvgTime yellowArmorPickups yellowArmorAvgTime
  greenArmorPickups greenArmorAvgTime megaHealthPickups megaHealthAvgTime

Final format: "scores_duel %i %s %s"
  numPlayers player1String player2String
==================
*/
void DuelScoreboardMessage(gentity_t *ent) {
    char weaponString[1024];
    char playerBuf[1024];
    char selfView[1024];
    char oppView[1024];
    char selfView2[1024];
    char oppView2[1024];
    int weaponLen;
    int pass, w;
    int player1, player2;
    gclient_t *cl;
    int numPlayers;
    qboolean showDetail1 = qfalse, showDetail2 = qfalse;

    // Determine the two duel players
    if (level.numPlayingClients == 0) {
        player1 = -1;
        player2 = -1;
    } else if (level.numPlayingClients == 1) {
        player1 = level.sortedClients[0];
        player2 = -1;
    } else {
        // Lower clientNum first
        if (level.sortedClients[0] < level.sortedClients[1]) {
            player1 = level.sortedClients[0];
            player2 = level.sortedClients[1];
        } else {
            player1 = level.sortedClients[1];
            player2 = level.sortedClients[0];
        }
        level.clientNum1stPlayer = player1;
        level.clientNum2ndPlayer = player2;
    }

    // Build scoreboard for player 1
    cl = &level.clients[player1];
    {
        int ping, accuracy, perfect, bestWeapon;
        float redArmorAvg = 0, yellowArmorAvg = 0, greenArmorAvg = 0, megaHealthAvg = 0;
        int redArmorTotal = 0, yellowArmorTotal = 0, greenArmorTotal = 0, megaHealthTotal = 0;

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
        bestWeapon = STAT_GetBestWeapon(cl);

        // Build per-weapon stats string (weapons 1-14)
        weaponString[0] = 0;
        weaponLen = 0;
        for (w = 1; w < 15; w++) {
            char wentry[1024];
            int wj, weapAcc = 0;

            if (cl->expandedStats.shotsHit[w] && cl->expandedStats.shotsFired[w]) {
                weapAcc = cl->expandedStats.shotsHit[w] * 100 / cl->expandedStats.shotsFired[w];
            }
            Com_sprintf(wentry, sizeof(wentry), " %i %i %i %i %i",
                        cl->expandedStats.shotsHit[w],
                        cl->expandedStats.shotsFired[w],
                        weapAcc,
                        cl->expandedStats.damageDealt[w],
                        cl->expandedStats.numWeaponKills[w]);
            wj = strlen(wentry);
            if (weaponLen + wj >= (int)sizeof(weaponString))
                break;
            strcpy(weaponString + weaponLen, wentry);
            weaponLen += wj;
        }

        // Two passes: pass 0 = without item timing, pass 1 = with item timing
        for (pass = 0; pass < 2; pass++) {
            if (pass == 1) {
                // Calculate item timing averages
                redArmorTotal = cl->expandedStats.numRedArmorPickups;
                if (redArmorTotal != cl->expandedStats.numFirstRedArmorPickups &&
                    redArmorTotal - cl->expandedStats.numFirstRedArmorPickups > 0) {
                    int diff = redArmorTotal - cl->expandedStats.numFirstRedArmorPickups;
                    redArmorAvg = ((float)cl->expandedStats.redArmorPickupTime / 1000.0f) / (float)diff;
                }
                yellowArmorTotal = cl->expandedStats.numYellowArmorPickups;
                if (yellowArmorTotal != cl->expandedStats.numFirstYellowArmorPickups &&
                    yellowArmorTotal - cl->expandedStats.numFirstYellowArmorPickups > 0) {
                    int diff = yellowArmorTotal - cl->expandedStats.numFirstYellowArmorPickups;
                    yellowArmorAvg = ((float)cl->expandedStats.yellowArmorPickupTime / 1000.0f) / (float)diff;
                }
                greenArmorTotal = cl->expandedStats.numGreenArmorPickups;
                if (greenArmorTotal != cl->expandedStats.numFirstGreenArmorPickups &&
                    greenArmorTotal - cl->expandedStats.numFirstGreenArmorPickups > 0) {
                    int diff = greenArmorTotal - cl->expandedStats.numFirstGreenArmorPickups;
                    greenArmorAvg = ((float)cl->expandedStats.greenArmorPickupTime / 1000.0f) / (float)diff;
                }
                megaHealthTotal = cl->expandedStats.numMegaHealthPickups;
                if (megaHealthTotal != cl->expandedStats.numFirstMegaHealthPickups &&
                    megaHealthTotal - cl->expandedStats.numFirstMegaHealthPickups > 0) {
                    int diff = megaHealthTotal - cl->expandedStats.numFirstMegaHealthPickups;
                    megaHealthAvg = ((float)cl->expandedStats.megaHealthPickupTime / 1000.0f) / (float)diff;
                }
            }

            // 14 base + 4*2 item timing + weapon string appended
            Com_sprintf(playerBuf, sizeof(playerBuf),
                        "%i %i %i %i %i %i %i %i %i %i %i %i %i "
                        "%i %3.2f %i %3.2f %i %3.2f %i %3.2f%s",
                        player1,
                        cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime) / 60000,
                        cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                        accuracy, bestWeapon, cl->expandedStats.totalDamageDealt,
                        cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                        cl->ps.persistant[PERS_EXCELLENT_COUNT],
                        cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                        perfect,
                        redArmorTotal, (double)redArmorAvg,
                        yellowArmorTotal, (double)yellowArmorAvg,
                        greenArmorTotal, (double)greenArmorAvg,
                        megaHealthTotal, (double)megaHealthAvg,
                        weaponString);

            if (pass == 0) {
                Q_strncpyz(oppView, playerBuf, sizeof(oppView));
            } else {
                Q_strncpyz(selfView, playerBuf, sizeof(selfView));
            }
        }
    }

    // Check if requesting client is player 1 or spectator
    if (ent->client == cl || ent->client->sess.sessionTeam == TEAM_SPECTATOR || level.intermissionTime) {
        showDetail1 = qtrue;
    }

    // Archive player 1's detail view
    if (player1 != -1) {
        if (player1 == level.clientNum1stPlayer) {
            Q_strncpyz(level.scoreboardArchive1, selfView, sizeof(level.scoreboardArchive1));
        } else {
            Q_strncpyz(level.scoreboardArchive2, selfView, sizeof(level.scoreboardArchive2));
        }
    }

    // Build scoreboard for player 2 (if exists)
    if (level.numPlayingClients > 1) {
        cl = &level.clients[player2];
        {
            int ping, accuracy, perfect, bestWeapon;
            float redArmorAvg = 0, yellowArmorAvg = 0, greenArmorAvg = 0, megaHealthAvg = 0;
            int redArmorTotal = 0, yellowArmorTotal = 0, greenArmorTotal = 0, megaHealthTotal = 0;

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
            bestWeapon = STAT_GetBestWeapon(cl);

            weaponString[0] = 0;
            weaponLen = 0;
            for (w = 1; w < 15; w++) {
                char wentry[1024];
                int wj, weapAcc = 0;

                if (cl->expandedStats.shotsHit[w] && cl->expandedStats.shotsFired[w]) {
                    weapAcc = cl->expandedStats.shotsHit[w] * 100 / cl->expandedStats.shotsFired[w];
                }
                Com_sprintf(wentry, sizeof(wentry), " %i %i %i %i %i",
                            cl->expandedStats.shotsHit[w],
                            cl->expandedStats.shotsFired[w],
                            weapAcc,
                            cl->expandedStats.damageDealt[w],
                            cl->expandedStats.numWeaponKills[w]);
                wj = strlen(wentry);
                if (weaponLen + wj >= (int)sizeof(weaponString))
                    break;
                strcpy(weaponString + weaponLen, wentry);
                weaponLen += wj;
            }

            for (pass = 0; pass < 2; pass++) {
                if (pass == 1) {
                    redArmorTotal = cl->expandedStats.numRedArmorPickups;
                    if (redArmorTotal != cl->expandedStats.numFirstRedArmorPickups &&
                        redArmorTotal - cl->expandedStats.numFirstRedArmorPickups > 0) {
                        int diff = redArmorTotal - cl->expandedStats.numFirstRedArmorPickups;
                        redArmorAvg = ((float)cl->expandedStats.redArmorPickupTime / 1000.0f) / (float)diff;
                    }
                    yellowArmorTotal = cl->expandedStats.numYellowArmorPickups;
                    if (yellowArmorTotal != cl->expandedStats.numFirstYellowArmorPickups &&
                        yellowArmorTotal - cl->expandedStats.numFirstYellowArmorPickups > 0) {
                        int diff = yellowArmorTotal - cl->expandedStats.numFirstYellowArmorPickups;
                        yellowArmorAvg = ((float)cl->expandedStats.yellowArmorPickupTime / 1000.0f) / (float)diff;
                    }
                    greenArmorTotal = cl->expandedStats.numGreenArmorPickups;
                    if (greenArmorTotal != cl->expandedStats.numFirstGreenArmorPickups &&
                        greenArmorTotal - cl->expandedStats.numFirstGreenArmorPickups > 0) {
                        int diff = greenArmorTotal - cl->expandedStats.numFirstGreenArmorPickups;
                        greenArmorAvg = ((float)cl->expandedStats.greenArmorPickupTime / 1000.0f) / (float)diff;
                    }
                    megaHealthTotal = cl->expandedStats.numMegaHealthPickups;
                    if (megaHealthTotal != cl->expandedStats.numFirstMegaHealthPickups &&
                        megaHealthTotal - cl->expandedStats.numFirstMegaHealthPickups > 0) {
                        int diff = megaHealthTotal - cl->expandedStats.numFirstMegaHealthPickups;
                        megaHealthAvg = ((float)cl->expandedStats.megaHealthPickupTime / 1000.0f) / (float)diff;
                    }
                }

                Com_sprintf(playerBuf, sizeof(playerBuf),
                            "%i %i %i %i %i %i %i %i %i %i %i %i %i "
                            "%i %3.2f %i %3.2f %i %3.2f %i %3.2f%s",
                            player2,
                            cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime) / 60000,
                            cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                            accuracy, bestWeapon, cl->expandedStats.totalDamageDealt,
                            cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                            cl->ps.persistant[PERS_EXCELLENT_COUNT],
                            cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                            perfect,
                            redArmorTotal, (double)redArmorAvg,
                            yellowArmorTotal, (double)yellowArmorAvg,
                            greenArmorTotal, (double)greenArmorAvg,
                            megaHealthTotal, (double)megaHealthAvg,
                            weaponString);

                if (pass == 0) {
                    Q_strncpyz(oppView2, playerBuf, sizeof(oppView2));
                } else {
                    Q_strncpyz(selfView2, playerBuf, sizeof(selfView2));
                }
            }

            if (ent->client == cl || ent->client->sess.sessionTeam == TEAM_SPECTATOR || level.intermissionTime) {
                showDetail2 = qtrue;
            }

            // Archive player 2's detail view
            if (player2 != -1) {
                Q_strncpyz(level.scoreboardArchive2, selfView2, sizeof(level.scoreboardArchive2));
            }
        }
    }

    numPlayers = level.numPlayingClients < 2 ? level.numPlayingClients : 2;

    // Send the scores
    if (level.intermissionQueued || level.intermissionTime) {
        // During intermission, use archived data
        trap_SendServerCommand(ent - g_entities,
            va("scores_duel 2 %s %s", level.scoreboardArchive1, level.scoreboardArchive2));
    } else if (level.numPlayingClients > 1) {
        trap_SendServerCommand(ent - g_entities,
            va("scores_duel %i %s %s",
               numPlayers,
               showDetail1 ? selfView : oppView,
               showDetail2 ? selfView2 : oppView2));
    } else if (numPlayers > 0) {
        trap_SendServerCommand(ent - g_entities,
            va("scores_duel %i %s", numPlayers, selfView));
    } else {
        trap_SendServerCommand(ent - g_entities,
            va("scores_duel %i %s", numPlayers, ""));
    }
}

/*
=============
CheckTournament

[QL] Duel-only: pull in spectators from the queue when a slot opens.
Binary: FUN_100557f0 in qagamex86.dll - only runs for GT_TOURNAMENT.
Warmup state machine is now in CheckWarmup() (separate function).
=============
*/
void CheckTournament(void) {
    int i;
    int numNonSpec;
    gclient_t *cl;

    if (g_gametype.integer != GT_TOURNAMENT) {
        return;
    }

    // Count non-spectator connected clients
    numNonSpec = 0;
    for (i = 0; i < level.maxclients; i++) {
        cl = level.clients + i;
        if (cl->pers.connected != CON_DISCONNECTED &&
            cl->sess.sessionTeam != TEAM_SPECTATOR) {
            numNonSpec++;
        }
    }

    // If we already have 2 players, nothing to do
    if (numNonSpec >= 2) {
        return;
    }

    // Pull in next spectator from the queue
    AddTournamentPlayer();

    if (level.warmupTime != -1) {
        SetWarmupState(-1);

        // Clear allReadyTime countdown if active
        if (level.allReadyTime != 0) {
            level.allReadyTime = 0;
        }
    }

    // Reset specOnly on connected playing clients
    for (i = 0; i < level.maxclients; i++) {
        cl = level.clients + i;
        if (cl->pers.connected != CON_DISCONNECTED &&
            cl->sess.sessionTeam != TEAM_SPECTATOR) {
            if (cl->sess.specOnly == 1) {
                cl->sess.specOnly = 0;
            }
            ClientUserinfoChanged(i);
        }
    }
}
