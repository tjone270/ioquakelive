/*
 * g_gametype_ffa.c -- Free For All (GT_FFA, 0)
 *
 * FFA is the base gametype. Most logic is in g_main.c (CheckExitRules)
 * and g_combat.c (scoring). This file provides gametype-specific hooks.
 */
#include "g_local.h"

// FFA uses standard fraglimit/timelimit exit rules (in CheckExitRules).
// No round state, no team scoring.
// Quad Hog mode (g_quadHog) is FFA-specific - see G_QG_* in g_items.c.

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage(gentity_t* ent) {
    char entry[1024];
    char string[1400];
    int stringlength;
    int i, j;
    gclient_t* cl;
    int numSorted, accuracy, perfect;
    int alive, frags, deaths, bestWeapon, bestWeaponAccuracy, damageDone;

    // don't send scores to bots, they don't parse it
    if (ent->r.svFlags & SVF_BOT) {
        return;
    }

    // send the latest information on all clients
    string[0] = 0;
    stringlength = 0;

    numSorted = level.numConnectedClients;

    for (i = 0; i < numSorted; i++) {
        int ping;

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

        // [QL] additional fields - binary uses pm_type==PM_NORMAL for alive check
        alive = (cl->ps.pm_type == PM_NORMAL) ? 1 : 0;
        frags = cl->expandedStats.numKills;
        deaths = cl->expandedStats.numDeaths;
        damageDone = cl->expandedStats.totalDamageDealt;

        // [QL] best weapon via STAT_GetBestWeapon
        bestWeapon = STAT_GetBestWeapon(cl);
        // [QL] best weapon accuracy percentage
        if (cl->expandedStats.shotsFired[bestWeapon] > 0) {
            bestWeaponAccuracy = cl->expandedStats.shotsHit[bestWeapon] * 100
                                / cl->expandedStats.shotsFired[bestWeapon];
        } else {
            bestWeaponAccuracy = 0;
        }

        // [QL] scores_ffa: 18 fields per player (binary-verified from 0x1003d280)
        // client score ping time accuracy impressive excellent gauntlet defend assist
        // perfect captures alive frags deaths bestWeapon bestWeaponAccuracy damageDone
        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime) / 60000,
                    accuracy,
                    cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
                    cl->ps.persistant[PERS_EXCELLENT_COUNT],
                    cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
                    cl->ps.persistant[PERS_DEFEND_COUNT],
                    cl->ps.persistant[PERS_ASSIST_COUNT],
                    perfect,
                    cl->ps.persistant[PERS_CAPTURES],
                    alive, frags, deaths, bestWeapon,
                    bestWeaponAccuracy,
                    damageDone);
        j = strlen(entry);
        if (stringlength + j >= sizeof(string))
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(ent - g_entities, va("scores_ffa %i %i %i%s", i,
                                                level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
                                                string));
}

/*
==================
FFAScoreboardMessage_impl

Simple scoreboard for match-end stats (smscores command).
8 fields per player: client score ping time captures alive frags deaths
Address: 0x1003cf60
==================
*/
void FFAScoreboardMessage_impl(void) {
    char entry[1024];
    char string[1024];
    int stringlength;
    int i, j;
    gclient_t *cl;

    string[0] = 0;
    stringlength = 0;

    for (i = 0; i < level.numConnectedClients; i++) {
        int ping;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->pers.connected == CON_CONNECTING) {
            ping = -1;
        } else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        // 8 fields per player
        Com_sprintf(entry, sizeof(entry), " %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    cl->ps.persistant[PERS_SCORE],
                    ping,
                    (level.time - cl->pers.enterTime) / 60000,
                    cl->ps.persistant[PERS_CAPTURES],
                    (cl->ps.pm_type == PM_NORMAL) ? 1 : 0,
                    cl->expandedStats.numKills,
                    cl->expandedStats.numDeaths);
        j = strlen(entry);
        if (stringlength + j >= (int)sizeof(string))
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(-1, va("smscores %i %i %i%s", i,
                                   level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
                                   string));
}
