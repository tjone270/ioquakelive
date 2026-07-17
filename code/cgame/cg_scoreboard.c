/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// cg_scoreboard -- draw the scoreboard on top of the game screen
#include "cg_local.h"

#define SCOREBOARD_X (0)

#define SB_HEADER 86
#define SB_TOP (SB_HEADER + 32)

// Where the status bar starts, so we don't overwrite it
#define SB_STATUSBAR 420

#define SB_NORMAL_HEIGHT 40
#define SB_INTER_HEIGHT 16  // interleaved height

#define SB_MAXCLIENTS_NORMAL ((SB_STATUSBAR - SB_TOP) / SB_NORMAL_HEIGHT)
#define SB_MAXCLIENTS_INTER ((SB_STATUSBAR - SB_TOP) / SB_INTER_HEIGHT - 1)

// Used when interleaved

#define SB_LEFT_BOTICON_X (SCOREBOARD_X + 0)
#define SB_LEFT_HEAD_X (SCOREBOARD_X + 32)
#define SB_RIGHT_BOTICON_X (SCOREBOARD_X + 64)
#define SB_RIGHT_HEAD_X (SCOREBOARD_X + 96)
// Normal
#define SB_BOTICON_X (SCOREBOARD_X + 32)
#define SB_HEAD_X (SCOREBOARD_X + 64)

#define SB_SCORELINE_X 112

#define SB_RATING_WIDTH (6 * BIGCHAR_WIDTH)                  // width 6
#define SB_SCORE_X (SB_SCORELINE_X + BIGCHAR_WIDTH)          // width 6
#define SB_RATING_X (SB_SCORELINE_X + 6 * BIGCHAR_WIDTH)     // width 6
#define SB_PING_X (SB_SCORELINE_X + 12 * BIGCHAR_WIDTH + 8)  // width 5
#define SB_TIME_X (SB_SCORELINE_X + 17 * BIGCHAR_WIDTH + 8)  // width 5
#define SB_NAME_X (SB_SCORELINE_X + 22 * BIGCHAR_WIDTH)      // width 15

// The new and improved score board
//
// In cases where the number of clients is high, the score board heads are interleaved
// here's the layout

//
//	0   32   80  112  144   240  320  400   <-- pixel position
//  bot head bot head score ping time name
//
//  wins/losses are drawn on bot icon now

static qboolean localClient;  // true if local client has been displayed

/*
=================
CG_DrawScoreboard
=================
*/
static void CG_DrawClientScore(int y, score_t* score, float* color, float fade, qboolean largeFormat) {
    char string[1024];
    vec3_t headAngles;
    clientInfo_t* ci;
    int iconx, headx;

    if (score->client < 0 || score->client >= cgs.maxclients) {
        Com_Printf("Bad score->client: %i\n", score->client);
        return;
    }

    ci = &cgs.clientinfo[score->client];

    iconx = SB_BOTICON_X + (SB_RATING_WIDTH / 2);
    headx = SB_HEAD_X + (SB_RATING_WIDTH / 2);

    // draw the handicap or bot skill marker (unless player has flag)
    if (ci->powerups & (1 << PW_NEUTRALFLAG)) {
        if (largeFormat) {
            CG_DrawFlagModel(iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, TEAM_FREE, qfalse);
        } else {
            CG_DrawFlagModel(iconx, y, 16, 16, TEAM_FREE, qfalse);
        }
    } else if (ci->powerups & (1 << PW_REDFLAG)) {
        if (largeFormat) {
            CG_DrawFlagModel(iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, TEAM_RED, qfalse);
        } else {
            CG_DrawFlagModel(iconx, y, 16, 16, TEAM_RED, qfalse);
        }
    } else if (ci->powerups & (1 << PW_BLUEFLAG)) {
        if (largeFormat) {
            CG_DrawFlagModel(iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, TEAM_BLUE, qfalse);
        } else {
            CG_DrawFlagModel(iconx, y, 16, 16, TEAM_BLUE, qfalse);
        }
    } else {
        if (ci->botSkill > 0 && ci->botSkill <= 5) {
            if (cg_drawIcons.integer) {
                if (largeFormat) {
                    CG_DrawPic(iconx, y - (32 - BIGCHAR_HEIGHT) / 2, 32, 32, cgs.media.botSkillShaders[ci->botSkill - 1]);
                } else {
                    CG_DrawPic(iconx, y, 16, 16, cgs.media.botSkillShaders[ci->botSkill - 1]);
                }
            }
        } else if (ci->handicap < 100) {
            Com_sprintf(string, sizeof(string), "%i", ci->handicap);
            if (cgs.gametype == GT_DUEL)
                CG_DrawSmallStringColor(iconx, y - SMALLCHAR_HEIGHT / 2, string, color);
            else
                CG_DrawSmallStringColor(iconx, y, string, color);
        }

        // draw the wins / losses
        if (cgs.gametype == GT_DUEL) {
            Com_sprintf(string, sizeof(string), "%i/%i", ci->wins, ci->losses);
            if (ci->handicap < 100 && !ci->botSkill) {
                CG_DrawSmallStringColor(iconx, y + SMALLCHAR_HEIGHT / 2, string, color);
            } else {
                CG_DrawSmallStringColor(iconx, y, string, color);
            }
        }
    }

    // draw the face
    VectorClear(headAngles);
    headAngles[YAW] = 180;
    if (largeFormat) {
        CG_DrawHead(headx, y - (ICON_SIZE - BIGCHAR_HEIGHT) / 2, ICON_SIZE, ICON_SIZE,
                    score->client, headAngles);
    } else {
        CG_DrawHead(headx, y, 16, 16, score->client, headAngles);
    }

    // draw the team task
    if (ci->teamTask != TEAMTASK_NONE) {
        if (ci->teamTask == TEAMTASK_OFFENSE) {
            CG_DrawPic(headx + 48, y, 16, 16, cgs.media.assaultShader);
        } else if (ci->teamTask == TEAMTASK_DEFENSE) {
            CG_DrawPic(headx + 48, y, 16, 16, cgs.media.defendShader);
        }
    }

    // draw the score line
    if (score->ping == -1) {
        Com_sprintf(string, sizeof(string),
                    " connecting    %s", ci->name);
    } else if (ci->team == TEAM_SPECTATOR) {
        Com_sprintf(string, sizeof(string),
                    " SPECT %3i %4i %s", score->ping, score->time, ci->name);
    } else {
        Com_sprintf(string, sizeof(string),
                    "%5i %4i %4i %s", score->score, score->ping, score->time, ci->name);
    }

    // highlight your position
    if (score->client == cg.snap->ps.clientNum) {
        float hcolor[4];
        int rank;

        localClient = qtrue;

        if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR || cgs.gametype >= GT_TEAM) {
            rank = -1;
        } else {
            rank = cg.snap->ps.persistant[PERS_RANK] & ~RANK_TIED_FLAG;
        }
        if (rank == 0) {
            hcolor[0] = 0;
            hcolor[1] = 0;
            hcolor[2] = 0.7f;
        } else if (rank == 1) {
            hcolor[0] = 0.7f;
            hcolor[1] = 0;
            hcolor[2] = 0;
        } else if (rank == 2) {
            hcolor[0] = 0.7f;
            hcolor[1] = 0.7f;
            hcolor[2] = 0;
        } else {
            hcolor[0] = 0.7f;
            hcolor[1] = 0.7f;
            hcolor[2] = 0.7f;
        }

        hcolor[3] = fade * 0.7;
        CG_FillRect(SB_SCORELINE_X + BIGCHAR_WIDTH + (SB_RATING_WIDTH / 2), y,
                    640 - SB_SCORELINE_X - BIGCHAR_WIDTH, BIGCHAR_HEIGHT + 1, hcolor);
    }

    CG_DrawBigString(SB_SCORELINE_X + (SB_RATING_WIDTH / 2), y, string, fade);

    // add the "ready" marker for intermission exiting
    if (cg.snap->ps.stats[STAT_CLIENTS_READY] & (1 << score->client)) {
        CG_DrawBigStringColor(iconx, y, "READY", color);
    }
}

/*
=================
CG_TeamScoreboard
=================
*/
static int CG_TeamScoreboard(int y, team_t team, float fade, int maxClients, int lineHeight) {
    int i;
    score_t* score;
    float color[4];
    int count;
    clientInfo_t* ci;

    color[0] = color[1] = color[2] = 1.0;
    color[3] = fade;

    count = 0;
    for (i = 0; i < cg.numScores && count < maxClients; i++) {
        score = &cg.scores[i];
        ci = &cgs.clientinfo[score->client];

        if (team != ci->team) {
            continue;
        }

        CG_DrawClientScore(y + lineHeight * count, score, color, fade, lineHeight == SB_NORMAL_HEIGHT);

        count++;
    }

    return count;
}

/*
=================
CG_DrawScoreboard

Draw the normal in-game scoreboard
=================
*/
qboolean CG_DrawOldScoreboard(void) {
    int x, y, w, i, n1, n2;
    float fade;
    float* fadeColor;
    char* s;
    int maxClients;
    int lineHeight;
    int topBorderSize, bottomBorderSize;

    // don't draw amuthing if the menu or console is up
    if (cg_paused.integer) {
        cg.deferredPlayerLoading = 0;
        return qfalse;
    }

    // don't draw scoreboard during death while warmup up
    if (cg.warmup && !cg.showScores) {
        return qfalse;
    }

    if (cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD ||
        cg.predictedPlayerState.pm_type == PM_INTERMISSION) {
        fade = 1.0;
        fadeColor = colorWhite;
    } else {
        fadeColor = CG_FadeColor(cg.scoreFadeTime, FADE_TIME);

        if (!fadeColor) {
            // next time scoreboard comes up, don't print killer
            cg.deferredPlayerLoading = 0;
            cg.killerName[0] = 0;
            return qfalse;
        }
        fade = *fadeColor;
    }

    // fragged by ... line
    if (cg.killerName[0]) {
        s = va("Fragged by %s", cg.killerName);
        w = CG_DrawStrlen(s) * BIGCHAR_WIDTH;
        x = (SCREEN_WIDTH - w) / 2;
        y = 40;
        CG_DrawBigString(x, y, s, fade);
    }

    // current rank
    if (cgs.gametype < GT_TEAM) {
        if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
            s = va("%s place with %i",
                   CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1),
                   cg.snap->ps.persistant[PERS_SCORE]);
            w = CG_DrawStrlen(s) * BIGCHAR_WIDTH;
            x = (SCREEN_WIDTH - w) / 2;
            y = 60;
            CG_DrawBigString(x, y, s, fade);
        }
    } else {
        if (cg.teamScores[0] == cg.teamScores[1]) {
            s = va("Teams are tied at %i", cg.teamScores[0]);
        } else if (cg.teamScores[0] >= cg.teamScores[1]) {
            s = va("Red leads %i to %i", cg.teamScores[0], cg.teamScores[1]);
        } else {
            s = va("Blue leads %i to %i", cg.teamScores[1], cg.teamScores[0]);
        }

        w = CG_DrawStrlen(s) * BIGCHAR_WIDTH;
        x = (SCREEN_WIDTH - w) / 2;
        y = 60;
        CG_DrawBigString(x, y, s, fade);
    }

    // scoreboard
    y = SB_HEADER;

    CG_DrawPic(SB_SCORE_X + (SB_RATING_WIDTH / 2), y, 64, 32, cgs.media.scoreboardScore);
    CG_DrawPic(SB_PING_X - (SB_RATING_WIDTH / 2), y, 64, 32, cgs.media.scoreboardPing);
    CG_DrawPic(SB_TIME_X - (SB_RATING_WIDTH / 2), y, 64, 32, cgs.media.scoreboardTime);
    CG_DrawPic(SB_NAME_X - (SB_RATING_WIDTH / 2), y, 64, 32, cgs.media.scoreboardName);

    y = SB_TOP;

    // If there are more than SB_MAXCLIENTS_NORMAL, use the interleaved scores
    if (cg.numScores > SB_MAXCLIENTS_NORMAL) {
        maxClients = SB_MAXCLIENTS_INTER;
        lineHeight = SB_INTER_HEIGHT;
        topBorderSize = 8;
        bottomBorderSize = 16;
    } else {
        maxClients = SB_MAXCLIENTS_NORMAL;
        lineHeight = SB_NORMAL_HEIGHT;
        topBorderSize = 16;
        bottomBorderSize = 16;
    }

    localClient = qfalse;

    if (cgs.gametype >= GT_TEAM) {
        //
        // teamplay scoreboard
        //
        y += lineHeight / 2;

        if (cg.teamScores[0] >= cg.teamScores[1]) {
            n1 = CG_TeamScoreboard(y, TEAM_RED, fade, maxClients, lineHeight);
            CG_DrawTeamBackground(0, y - topBorderSize, SCREEN_WIDTH, n1 * lineHeight + bottomBorderSize, 0.33f, TEAM_RED);
            y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
            maxClients -= n1;
            n2 = CG_TeamScoreboard(y, TEAM_BLUE, fade, maxClients, lineHeight);
            CG_DrawTeamBackground(0, y - topBorderSize, SCREEN_WIDTH, n2 * lineHeight + bottomBorderSize, 0.33f, TEAM_BLUE);
            y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
            maxClients -= n2;
        } else {
            n1 = CG_TeamScoreboard(y, TEAM_BLUE, fade, maxClients, lineHeight);
            CG_DrawTeamBackground(0, y - topBorderSize, SCREEN_WIDTH, n1 * lineHeight + bottomBorderSize, 0.33f, TEAM_BLUE);
            y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
            maxClients -= n1;
            n2 = CG_TeamScoreboard(y, TEAM_RED, fade, maxClients, lineHeight);
            CG_DrawTeamBackground(0, y - topBorderSize, SCREEN_WIDTH, n2 * lineHeight + bottomBorderSize, 0.33f, TEAM_RED);
            y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
            maxClients -= n2;
        }
        n1 = CG_TeamScoreboard(y, TEAM_SPECTATOR, fade, maxClients, lineHeight);
        y += (n1 * lineHeight) + BIGCHAR_HEIGHT;

    } else {
        //
        // free for all scoreboard
        //
        n1 = CG_TeamScoreboard(y, TEAM_FREE, fade, maxClients, lineHeight);
        y += (n1 * lineHeight) + BIGCHAR_HEIGHT;
        n2 = CG_TeamScoreboard(y, TEAM_SPECTATOR, fade, maxClients - n1, lineHeight);
        y += (n2 * lineHeight) + BIGCHAR_HEIGHT;
    }

    if (!localClient) {
        // draw local client at the bottom
        for (i = 0; i < cg.numScores; i++) {
            if (cg.scores[i].client == cg.snap->ps.clientNum) {
                CG_DrawClientScore(y, &cg.scores[i], fadeColor, fade, lineHeight == SB_NORMAL_HEIGHT);
                break;
            }
        }
    }

    // load any models that have been deferred
    if (++cg.deferredPlayerLoading > 10) {
        CG_LoadDeferredPlayers();
    }

    return qtrue;
}

//================================================================================

//================================================================================
// [QL] Extended per-client team-stats parsers
//
// These are QL additions, separate from the main scoreboard (scores_tdm /
// scores_ca / scores_ctf) and from the team-wide pickup-count
// CG_ParseTeamStats. The server emits ONE command per connected client; the
// first argument is that client's scoreboard slot index, used verbatim as the
// row index into cg.teamStats[] (a per-client store with a 0x268-byte /
// 154-int stride). All values are signed decimal integers read with atoi().
// The feeder reader (CG_FeederItemTextStats) lives in cg_main.c.
//================================================================================

/*
=================
CG_ParseTeamStats_TDM

TDM / FT extended stats. Wire: "tdmstats <client> <selfKills> <teamKills>
<tkDeaths> <damageDone> <damageReceived> <ra> <ya> <ga> <mh> <quad> <bs>".
=================
*/
// Address: 0x10046610
void CG_ParseTeamStats_TDM(void) {
    int slot;
    int* row;

    slot = atoi(CG_Argv(1));
    row = cg.teamStats[slot].fields;

    row[0] = atoi(CG_Argv(2));    // 0x00 selfKills
    row[1] = atoi(CG_Argv(3));    // 0x04 teamKills
    row[2] = atoi(CG_Argv(4));    // 0x08 teamKillDeaths
    row[7] = atoi(CG_Argv(5));    // 0x1c damageDone
    row[8] = atoi(CG_Argv(6));    // 0x20 damageReceived
    row[9] = atoi(CG_Argv(7));    // 0x24 pickups: red armour
    row[11] = atoi(CG_Argv(8));   // 0x2c pickups: yellow armour
    row[13] = atoi(CG_Argv(9));   // 0x34 pickups: green armour
    row[15] = atoi(CG_Argv(10));  // 0x3c pickups: mega health
    row[17] = atoi(CG_Argv(11));  // 0x44 pickups: quad
    row[18] = atoi(CG_Argv(12));  // 0x48 pickups: battlesuit
}

/*
=================
CG_ParseTeamStats_CA

Clan Arena extended stats. Wire: "castats <client> <damageDone>
<damageReceived>" followed by 15 weapon pairs, each "<accuracy> <kills>" for
weapon index k = 0..14 (0 = gauntlet .. 14 = HMG). 33 args after the verb.
Binary quirk carried into the store: for each weapon the accuracy is
the first arg of the pair and lands at the higher offset (0x9c + k*4), while
the kill count is the second arg and lands at the lower offset (0x5c + k*4).
=================
*/
// Address: 0x10046b00
void CG_ParseTeamStats_CA(void) {
    int slot;
    int* row;
    int k;

    slot = atoi(CG_Argv(1));
    row = cg.teamStats[slot].fields;

    row[7] = atoi(CG_Argv(2));  // 0x1c damageDone
    row[8] = atoi(CG_Argv(3));  // 0x20 damageReceived

    // 15 weapon pairs, args 4..33
    for (k = 0; k < 15; k++) {
        row[39 + k] = atoi(CG_Argv(4 + k * 2));  // 0x9c + k*4  accuracy
        row[23 + k] = atoi(CG_Argv(5 + k * 2));  // 0x5c + k*4  kills
    }
}

/*
=================
CG_ParseTeamStats_CTF

CTF / 1FCTF / Harvester / Domination / A&D extended stats. Wire: "ctfstats
<client> <selfKills> <damageDone> <damageReceived> <ra> <ya> <ga> <mh> <quad>
<bs> <regen> <haste> <invis>".
=================
*/
// Address: 0x10047400
void CG_ParseTeamStats_CTF(void) {
    int slot;
    int* row;

    slot = atoi(CG_Argv(1));
    row = cg.teamStats[slot].fields;

    row[0] = atoi(CG_Argv(2));    // 0x00 selfKills
    row[7] = atoi(CG_Argv(3));    // 0x1c damageDone
    row[8] = atoi(CG_Argv(4));    // 0x20 damageReceived
    row[9] = atoi(CG_Argv(5));    // 0x24 pickups: red armour
    row[11] = atoi(CG_Argv(6));   // 0x2c pickups: yellow armour
    row[13] = atoi(CG_Argv(7));   // 0x34 pickups: green armour
    row[15] = atoi(CG_Argv(8));   // 0x3c pickups: mega health
    row[17] = atoi(CG_Argv(9));   // 0x44 pickups: quad
    row[18] = atoi(CG_Argv(10));  // 0x48 pickups: battlesuit
    row[19] = atoi(CG_Argv(11));  // 0x4c pickups: regen
    row[20] = atoi(CG_Argv(12));  // 0x50 pickups: haste
    row[21] = atoi(CG_Argv(13));  // 0x54 pickups: invisibility
}
