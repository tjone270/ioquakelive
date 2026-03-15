/*
 * g_gametype_tdm.c -- Team Deathmatch (GT_TEAM, 3)
 *
 * Team-based fraglimit/timelimit mode. Team scoring via kills.
 * Scoreboard: TDMScoreboardMessage, TDMScoreboardMessage_impl (below).
 */
#include "g_local.h"

// TDM uses fraglimit/timelimit exit rules + mercylimit.
// Team balance: g_teamForceBalance, g_teamSizeMin, g_teamsize.
// No round state.

/*
==================
TDMScoreboardMessage

TDM scoreboard. Includes team item pickup stats (14 categories * 2 teams = 28 header values).
Hides opponent team's item stats unless spectating or at intermission.
15 fields per player.
Address: 0x1003df10

Header (31 values before player data):
  redArmor[RED] redArmor[BLUE] yellowArmor[R] yellowArmor[B] greenArmor[R] greenArmor[B]
  megaHealth[R] megaHealth[B] quad[R] quad[B] battleSuit[R] battleSuit[B]
  regen[R] regen[B] haste[R] haste[B] invis[R] invis[B]
  quadTime[R] quadTime[B] battleSuitTime[R] battleSuitTime[B]
  regenTime[R] regenTime[B] hasteTime[R] hasteTime[B] invisTime[R] invisTime[B]
  numPlayers teamScoreRed teamScoreBlue

Fields per player (15):
  client team score ping time frags deaths accuracy bestWeapon
  impressive excellent gauntlet teamKills teamKilled damageDone
==================
*/
void TDMScoreboardMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, j;
    gclient_t *cl;
    int viewerTeam;
    int redStats[14], blueStats[14];

    string[0] = 0;
    stringlength = 0;

    // Load team item stats (14 categories)
    // Order: redArmor, yellowArmor, greenArmor, megaHealth, quad, battleSuit,
    //        regen, haste, invis, quadTime, battleSuitTime, regenTime, hasteTime, invisTime
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

        cl = &level.clients[level.sortedClients[i]];

        if (cl->accuracy_shots) {
            accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
        } else {
            accuracy = 0;
        }

        if (cl->pers.connected == CON_CONNECTING) {
            ping = -1;
        } else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        bestWeapon = STAT_GetBestWeapon(cl);

        // Hide opponent team's stats (unless spectating or intermission)
        viewerTeam = ent->client->sess.sessionTeam;
        if (viewerTeam == TEAM_RED && level.intermissionTime == 0) {
            memset(blueStats, 0, sizeof(blueStats));
        } else if (viewerTeam == TEAM_BLUE && level.intermissionTime == 0) {
            memset(redStats, 0, sizeof(redStats));
        }

        // 15 fields per player
        Com_sprintf(entry, sizeof(entry), " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->sess.sessionTeam,
                    cl->ps.persistant[PERS_SCORE], ping,
                    (level.time - cl->pers.enterTime) / 60000,
                    cl->expandedStats.numKills, cl->expandedStats.numDeaths,
                    accuracy, bestWeapon,
                    cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                    cl->ps.persistant[PERS_EXCELLENT_COUNT],
                    cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                    cl->expandedStats.numTeamKills,
                    cl->expandedStats.numTeamKilled,
                    cl->expandedStats.totalDamageDealt);
        j = strlen(entry);
        if (stringlength + j >= (int)sizeof(string))
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(ent - g_entities,
        va("scores_tdm %i %i %i %i %i %i %i %i %i %i %i %i %i %i "
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
TDMScoreboardMessage_impl

TDM detail stats sent per-player as "tdmstats" command.
11 fields per player: suicides teamKills teamKilled damageDone damageTaken
  redArmorPickups yellowArmorPickups greenArmorPickups megaHealthPickups
  quadPickups battleSuitPickups
Address: 0x1003e3a0
==================
*/
void TDMScoreboardMessage_impl(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int i;
    gclient_t *cl;

    for (i = 0; i < level.numConnectedClients; i++) {
        cl = &level.clients[level.sortedClients[i]];

        string[0] = 0;
        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i",
                    cl->expandedStats.numSuicides,
                    cl->expandedStats.numTeamKills,
                    cl->expandedStats.numTeamKilled,
                    cl->expandedStats.totalDamageDealt,
                    cl->expandedStats.totalDamageTaken,
                    cl->expandedStats.numRedArmorPickups,
                    cl->expandedStats.numYellowArmorPickups,
                    cl->expandedStats.numGreenArmorPickups,
                    cl->expandedStats.numMegaHealthPickups,
                    cl->expandedStats.numQuadDamagePickups,
                    cl->expandedStats.numBattleSuitPickups);
        if (strlen(entry) < sizeof(string)) {
            strcpy(string, entry);
        }
        trap_SendServerCommand(ent - g_entities,
            va("tdmstats %i%s", i, string));
    }
}
