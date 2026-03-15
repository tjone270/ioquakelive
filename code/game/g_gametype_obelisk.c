/*
 * g_gametype_obelisk.c -- Overload (GT_OBELISK, 7)
 *
 * Teams attack each other's obelisk. Obelisk logic is in g_team.c
 * (ObeliskTouch, ObeliskDie, ObeliskRegen, ObeliskPain).
 * Scoreboard: DeathmatchScoreboardMessage (FFA format fallback).
 */
#include "g_local.h"

// Overload uses capturelimit/timelimit exit rules.
// Obelisk entities: team_redobelisk, team_blueobelisk (in g_team.c).
// Obelisk health regeneration and destruction handling in g_team.c.
// No round state.

void Obelisk_CheckTeamItems(void) {
    gentity_t *ent;
    ent = G_Find(NULL, FOFS(classname), "team_redobelisk");
    if (!ent) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_redobelisk in map\n");
    }
    ent = G_Find(NULL, FOFS(classname), "team_blueobelisk");
    if (!ent) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_blueobelisk in map\n");
    }
}
