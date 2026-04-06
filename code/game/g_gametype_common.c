/*
 * g_gametype_common.c -- Shared gametype functions
 *
 * Functions used by multiple gametypes (CA, AD, etc.)
 */
#include "g_local.h"

/*
============
ClientBegin_RoundBased

[QL] CA/AD begin - spawn behavior depends on round state:
  roundState 0 (pre-round): normal spawn + weapon select
  roundState 1 (countdown): spawn + freeze (PMF_TIME_KNOCKBACK)
  roundState 2+ (active round): spectator mode until next round
============
*/
void ClientBegin_RoundBased(gentity_t* ent) {
    gclient_t* client = ent->client;

    if (level.roundState.eCurrent == 0) {
        // pre-round: normal spawn
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
    } else if (level.roundState.eCurrent == 1) {
        // countdown: spawn but freeze
        ClientSpawn(ent);
        SelectSpawnWeapon(ent);
        client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
    } else {
        // round active: force to spectator, wait for next round
        client->ps.pm_type = PM_SPECTATOR;
        ClientSpawn(ent);
        UpdateTeamAliveCount(NULL, NULL);
        // auto-follow if warmup is off, not spectator team, and both teams have players
        if (!level.warmupTime
            && client->sess.sessionTeam != TEAM_SPECTATOR
            && level.numPlayingClients > 0) {
            Cmd_FollowCycle_f(ent, 1);
        }
    }
}

/*
==================
STAT_PublishMedal / STAT_RoundOver

[QL] Stats publishing stubs. The original binary uses C++ jsoncpp
for stats; these are no-ops in the C reimplementation.
Shared by CA, RR, DOM gametypes.
==================
*/
void STAT_PublishMedal(gentity_t *ent, const char *medal) {
    (void)ent; (void)medal;
}

void STAT_RoundOver(int round, int winTeam, int isDraw) {
    (void)round; (void)winTeam; (void)isDraw;
}

/*
==================
LastManStanding

[QL] Binary: 0x1006b200
Plays GTS_LAST_STANDING sound event and sends g_lastManStandingMessage
as a centerprint to the sole survivor on the given team.
Used by CA and RR gametypes.
==================
*/
void LastManStanding(int team) {
    int i;
    for (i = 0; i < level.maxclients; i++) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected != CON_CONNECTED) continue;
        if (cl->ps.pm_type != PM_NORMAL) continue;
        if (cl->sess.sessionTeam != team) continue;

        {
            gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);
            te->r.svFlags |= SVF_BROADCAST;
            te->s.eventParm = GTS_LAST_STANDING;  // 0x13 = 19
            te->s.otherEntityNum = cl->sess.sessionTeam;
            trap_SendServerCommand(i,
                va("cp \"%s\n\"", g_lastManStandingMessage.string));
        }
        return;
    }
}

/*
==================
G_ObfuscateEnemyInfoInSnapshotCheck

[QL] Binary: 0x10052e80
Returns qtrue if clientNum2's info should be VISIBLE to clientNum1.
The engine strips position data when this returns qfalse.
Return qtrue = allow visibility (don't strip), qfalse = obfuscate (strip position).

Matches binary exactly. Only meaningful for team gametypes (gametype >= 3).
FFA/duel always return qtrue (no obfuscation).
==================
*/
qboolean G_ObfuscateEnemyInfoInSnapshotCheck(int clientNum1, int clientNum2) {
    if (clientNum1 >= MAX_CLIENTS || clientNum2 >= MAX_CLIENTS)
        return qtrue;  // out of range — don't strip

    // FFA and duel: no obfuscation
    if (g_gametype.integer < GT_TEAM)
        return qtrue;

    // 1FCTF: flag carrier is always visible to everyone
    if (g_gametype.integer == GT_1FCTF) {
        if (g_entities[clientNum2].s.powerups & (1 << PW_NEUTRALFLAG))
            return qtrue;
    }

    // Team games: same team always visible
    if (level.clients[clientNum1].sess.sessionTeam ==
        level.clients[clientNum2].sess.sessionTeam)
        return qtrue;

    // Spectators always see everyone
    if (level.clients[clientNum1].sess.sessionTeam == TEAM_SPECTATOR)
        return qtrue;

    // RR Infected: enemies are also visible (infection needs position tracking)
    if (g_gametype.integer == GT_RR && g_rrInfected.integer != 0)
        return qtrue;

    // Default for team games: obfuscate enemy positions
    return qfalse;
}

/*
==================
SendScoreboardMessageToTeam

Sends the current scoreboard to all connected clients on a given team.
The actual scoreboard content should have been prepared via va() before calling.
Address: 0x10067f80
==================
*/
/*
==================
STAT_GetBestWeapon

[QL] Returns the weapon index with the highest damage dealt for a player.
Falls back to the highest weapon in their loadout if no damage dealt.
Address: 0x1007b820
==================
*/
int STAT_GetBestWeapon(gclient_t *cl) {
    int bestWeapon = 0;
    int bestDamage = 0;
    int weapon;
    int fallbackWeapon = 1;

    // Find highest weapon in loadout
    for (weapon = 2; weapon < WP_NUM_WEAPONS; weapon++) {
        if (cl->ps.stats[STAT_WEAPONS] & (1 << (weapon - 1))) {
            fallbackWeapon = weapon;
        }
    }

    // Find weapon with most damage dealt
    for (weapon = 1; weapon < WP_NUM_WEAPONS; weapon++) {
        if (cl->expandedStats.damageDealt[weapon] > bestDamage) {
            bestDamage = cl->expandedStats.damageDealt[weapon];
            bestWeapon = weapon;
        }
    }

    if (bestWeapon == 0) {
        bestWeapon = fallbackWeapon;
    }
    return bestWeapon;
}

void SendScoreboardMessageToTeam(int team) {
    int i;
    gclient_t *cl;

    for (i = 0; i < level.maxclients; i++) {
        cl = &level.clients[i];
        if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == team) {
            trap_SendServerCommand(i, NULL);  // sends last va() result
        }
    }
}
