/*
 * g_gametype_1fctf.c -- One Flag CTF (GT_1FCTF, 6)
 *
 * Single neutral flag mode. Teams compete to capture a neutral flag
 * into the enemy base. Flag logic is in g_team.c.
 * Scoreboard: CTFScoreboardMessage (shared with CTF) in g_cmds.c.
 */
#include "g_local.h"

// 1FCTF uses capturelimit/timelimit exit rules.
// Neutral flag entity: team_CTF_neutralflag (in g_team.c).
// Uses CTFScoreboardMessage (same format as CTF).
// No round state.

extern qboolean itemRegistered[];

void OneFCTF_CheckTeamItems(void) {
    gitem_t *item;
    item = BG_FindItem("Red Flag");
    if (!item || !itemRegistered[item - bg_itemlist]) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_redflag in map\n");
    }
    item = BG_FindItem("Blue Flag");
    if (!item || !itemRegistered[item - bg_itemlist]) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_blueflag in map\n");
    }
    item = BG_FindItem("Neutral Flag");
    if (!item || !itemRegistered[item - bg_itemlist]) {
        G_Printf(S_COLOR_YELLOW "WARNING: No team_CTF_neutralflag in map\n");
    }
}
