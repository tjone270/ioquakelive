/*
 * g_gametype_har.c -- Harvester (GT_HARVESTER, 8)
 *
 * Teams collect skulls from a central skull generator and deliver them
 * to the enemy base. Skull logic is in g_team.c.
 * Scoreboard: CTFScoreboardMessage (shared format) in g_cmds.c.
 */
#include "g_local.h"

// Harvester uses capturelimit/timelimit exit rules.
// Skull generator entity and skull pickup in g_team.c.
// Uses CTFScoreboardMessage format.
// No round state.

void Harvester_CheckTeamItems(void) {
    gentity_t *ent;
    ent = G_Find(NULL, FOFS(classname), "team_redobelisk");
    if (!ent) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_redobelisk in map\n");
    }
    ent = G_Find(NULL, FOFS(classname), "team_blueobelisk");
    if (!ent) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_blueobelisk in map\n");
    }
    ent = G_Find(NULL, FOFS(classname), "team_neutralobelisk");
    if (!ent) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_neutralobelisk in map\n");
    }
}

void Harvester_RegisterItems(void) {
    RegisterItem(BG_FindItem("Red Cube"));
    RegisterItem(BG_FindItem("Blue Cube"));
}
