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
