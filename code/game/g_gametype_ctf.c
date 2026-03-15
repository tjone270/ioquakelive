/*
 * g_gametype_ctf.c -- Capture The Flag (GT_CTF, 5)
 *
 * Flag-based team mode. Flag logic is in g_team.c (Team_TouchOurFlag,
 * Team_TouchEnemyFlag, Team_ReturnFlag, Team_CaptureFlagAt).
 * Scoreboard: CTFScoreboardMessage, CTFScoreboardMessage_impl (below).
 */
#include "g_local.h"

// CTF uses capturelimit/timelimit exit rules.
// Flag entities: team_CTF_redflag, team_CTF_blueflag (spawned in g_team.c).
// Flag return logic, carrier kill bonuses in g_team.c.
// No round state.

extern qboolean itemRegistered[];

void CTF_CheckTeamItems(void) {
    gitem_t *item;
    item = BG_FindItem("Red Flag");
    if (!item || !itemRegistered[item - bg_itemlist]) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_redflag in map\n");
    }
    item = BG_FindItem("Blue Flag");
    if (!item || !itemRegistered[item - bg_itemlist]) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_blueflag in map\n");
    }
}

/*
==================
CTFScoreboardMessage

CTF scoreboard. Includes 17 team stat categories * 2 teams = 34 header values.
17 fields per player.
Address: 0x1003e8c0

Header (37 values):
  Same 14 as TDM + numFlagPickups[R/B] numMedkitPickups[R/B] flagPossessionTime[R/B]
  numPlayers teamScoreRed teamScoreBlue

Fields per player (17):
  client team score ping time frags deaths accuracy bestWeapon
  impressive excellent gauntlet defend assist captures perfect alive
==================
*/
void CTFScoreboardMessage(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, j;
    gclient_t *cl;
    int viewerTeam;
    int redStats[17], blueStats[17];

    string[0] = 0;
    stringlength = 0;

    // 14 base categories (same as TDM) + 3 CTF-specific
    redStats[0]  = level.numRedArmorPickups[TEAM_RED];
    redStats[1]  = level.numYellowArmorPickups[TEAM_RED];
    redStats[2]  = level.numGreenArmorPickups[TEAM_RED];
    redStats[3]  = level.numMegaHealthPickups[TEAM_RED];
    redStats[4]  = level.numQuadDamagePickups[TEAM_RED];
    redStats[5]  = level.numBattleSuitPickups[TEAM_RED];
    redStats[6]  = level.numRegenerationPickups[TEAM_RED];
    redStats[7]  = level.numHastePickups[TEAM_RED];
    redStats[8]  = level.numInvisibilityPickups[TEAM_RED];
    redStats[9]  = level.numFlagPickups[TEAM_RED];
    redStats[10] = level.numMedkitPickups[TEAM_RED];
    redStats[11] = level.quadDamagePossessionTime[TEAM_RED];
    redStats[12] = level.battleSuitPossessionTime[TEAM_RED];
    redStats[13] = level.regenerationPossessionTime[TEAM_RED];
    redStats[14] = level.hastePossessionTime[TEAM_RED];
    redStats[15] = level.invisibilityPossessionTime[TEAM_RED];
    redStats[16] = level.flagPossessionTime[TEAM_RED];

    blueStats[0]  = level.numRedArmorPickups[TEAM_BLUE];
    blueStats[1]  = level.numYellowArmorPickups[TEAM_BLUE];
    blueStats[2]  = level.numGreenArmorPickups[TEAM_BLUE];
    blueStats[3]  = level.numMegaHealthPickups[TEAM_BLUE];
    blueStats[4]  = level.numQuadDamagePickups[TEAM_BLUE];
    blueStats[5]  = level.numBattleSuitPickups[TEAM_BLUE];
    blueStats[6]  = level.numRegenerationPickups[TEAM_BLUE];
    blueStats[7]  = level.numHastePickups[TEAM_BLUE];
    blueStats[8]  = level.numInvisibilityPickups[TEAM_BLUE];
    blueStats[9]  = level.numFlagPickups[TEAM_BLUE];
    blueStats[10] = level.numMedkitPickups[TEAM_BLUE];
    blueStats[11] = level.quadDamagePossessionTime[TEAM_BLUE];
    blueStats[12] = level.battleSuitPossessionTime[TEAM_BLUE];
    blueStats[13] = level.regenerationPossessionTime[TEAM_BLUE];
    blueStats[14] = level.hastePossessionTime[TEAM_BLUE];
    blueStats[15] = level.invisibilityPossessionTime[TEAM_BLUE];
    blueStats[16] = level.flagPossessionTime[TEAM_BLUE];

    for (i = 0; i < level.numConnectedClients; i++) {
        int ping, accuracy, perfect, bestWeapon;
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
                    cl->ps.persistant[PERS_DEFEND_COUNT],
                    cl->ps.persistant[PERS_ASSIST_COUNT],
                    cl->ps.persistant[PERS_CAPTURES],
                    perfect, alive);
        j = strlen(entry);
        if (stringlength + j >= (int)sizeof(string))
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(ent - g_entities,
        va("scores_ctf %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i "
           "%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i%s",
           redStats[0], redStats[1], redStats[2], redStats[3], redStats[4],
           redStats[5], redStats[6], redStats[7], redStats[8],
           redStats[9], redStats[10],
           redStats[11], redStats[12], redStats[13], redStats[14], redStats[15], redStats[16],
           blueStats[0], blueStats[1], blueStats[2], blueStats[3], blueStats[4],
           blueStats[5], blueStats[6], blueStats[7], blueStats[8],
           blueStats[9], blueStats[10],
           blueStats[11], blueStats[12], blueStats[13], blueStats[14], blueStats[15], blueStats[16],
           i, level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
           string));
}

/*
==================
CTFScoreboardMessage_impl

CTF/CA per-player detail stats sent as "castats" command.
Per player: damageDone damageTaken + per-weapon (accuracy numWeaponKills) * 15.
Address: 0x1003e700
==================
*/
void CTFScoreboardMessage_impl(gentity_t *ent) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, w;
    gclient_t *cl;

    for (i = 0; i < level.numConnectedClients; i++) {
        cl = &level.clients[level.sortedClients[i]];

        string[0] = 0;
        stringlength = 0;

        // First: damageDone damageTaken
        Com_sprintf(entry, sizeof(entry), " %i %i",
                    cl->expandedStats.totalDamageDealt,
                    cl->expandedStats.totalDamageTaken);
        stringlength = strlen(entry);
        if (stringlength < (int)sizeof(string)) {
            strcpy(string, entry);
        }

        // Per-weapon: accuracy numWeaponKills (weapons 0-14)
        for (w = 0; w < 15; w++) {
            int j, weapAcc = 0;

            if (cl->expandedStats.shotsHit[w] && cl->expandedStats.shotsFired[w]) {
                weapAcc = cl->expandedStats.shotsHit[w] * 100 / cl->expandedStats.shotsFired[w];
            }

            Com_sprintf(entry, sizeof(entry), " %i %i",
                        weapAcc, cl->expandedStats.numWeaponKills[w]);
            j = strlen(entry);
            if (stringlength + j >= (int)sizeof(string))
                break;
            strcpy(string + stringlength, entry);
            stringlength += j;
        }

        trap_SendServerCommand(ent - g_entities,
            va("castats %i%s", i, string));
    }
}
