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
// cg_servercmds.c -- reliably sequenced text commands sent by the server
// these are processed at snapshot transition time, so there will definitely
// be a valid snapshot this frame

#include "cg_local.h"
#include "../../ui/menudef.h"

typedef struct {
    const char* order;
    int taskNum;
} orderTask_t;

static const orderTask_t validOrders[] = {
    {VOICECHAT_GETFLAG, TEAMTASK_OFFENSE},
    {VOICECHAT_OFFENSE, TEAMTASK_OFFENSE},
    {VOICECHAT_DEFEND, TEAMTASK_DEFENSE},
    {VOICECHAT_DEFENDFLAG, TEAMTASK_DEFENSE},
    {VOICECHAT_PATROL, TEAMTASK_PATROL},
    {VOICECHAT_CAMP, TEAMTASK_CAMP},
    {VOICECHAT_FOLLOWME, TEAMTASK_FOLLOW},
    {VOICECHAT_RETURNFLAG, TEAMTASK_RETRIEVE},
    {VOICECHAT_FOLLOWFLAGCARRIER, TEAMTASK_ESCORT}};

static const int numValidOrders = ARRAY_LEN(validOrders);

static int CG_ValidOrder(const char* p) {
    int i;
    for (i = 0; i < numValidOrders; i++) {
        if (Q_stricmp(p, validOrders[i].order) == 0) {
            return validOrders[i].taskNum;
        }
    }
    return -1;
}

/*
=================
CG_ParseScores_Ffa

[QL] scores_ffa: 18 fields per player
Format: numPlayers teamScore0 teamScore1 [client score ping time accuracy impressive excellent gauntlet defend assist perfect captures alive frags deaths bestWeapon powerups damageDone] ...
=================
*/
static void CG_ParseScores_Ffa(void) {
    int i, idx;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(2));
    cg.teamScores[1] = atoi(CG_Argv(3));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (i = 0; i < cg.numScores; i++) {
        score_t *sp = &cg.scores[i];
        idx = i * 18 + 4;
        sp->client = atoi(CG_Argv(idx++));
        sp->score = atoi(CG_Argv(idx++));
        sp->ping = atoi(CG_Argv(idx++));
        sp->time = atoi(CG_Argv(idx++));
        sp->accuracy = atoi(CG_Argv(idx++));
        sp->impressiveCount = atoi(CG_Argv(idx++));
        sp->excellentCount = atoi(CG_Argv(idx++));
        sp->guantletCount = atoi(CG_Argv(idx++));
        sp->defendCount = atoi(CG_Argv(idx++));
        sp->assistCount = atoi(CG_Argv(idx++));
        sp->perfect = atoi(CG_Argv(idx++));
        sp->captures = atoi(CG_Argv(idx++));
        sp->alive = atoi(CG_Argv(idx++));
        sp->frags = atoi(CG_Argv(idx++));
        sp->deaths = atoi(CG_Argv(idx++));
        sp->bestWeapon = atoi(CG_Argv(idx++));
        sp->powerUps = atoi(CG_Argv(idx++));
        sp->damageDone = atoi(CG_Argv(idx++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        cgs.clientinfo[sp->client].powerups = sp->powerUps;
        sp->team = cgs.clientinfo[sp->client].team;
    }
    CG_SetScoreSelection(NULL);
}

/*
=================
CG_ParseScores_Tdm

[QL] scores_tdm: 28-field team header + 16 fields per player
=================
*/
static void CG_ParseScores_Tdm(void) {
    int i, n;
    tdmScoreHeader_t *h = &cg.tdmScoreHeader;

    i = 1;
    h->rra = atoi(CG_Argv(i++));    h->rya = atoi(CG_Argv(i++));
    h->rga = atoi(CG_Argv(i++));    h->rmh = atoi(CG_Argv(i++));
    h->rquad = atoi(CG_Argv(i++));  h->rbs = atoi(CG_Argv(i++));
    h->rregen = atoi(CG_Argv(i++)); h->rhaste = atoi(CG_Argv(i++));
    h->rinvis = atoi(CG_Argv(i++));
    h->rquadTime = atoi(CG_Argv(i++)); h->rbsTime = atoi(CG_Argv(i++));
    h->rregenTime = atoi(CG_Argv(i++)); h->rhasteTime = atoi(CG_Argv(i++));
    h->rinvisTime = atoi(CG_Argv(i++));
    h->bra = atoi(CG_Argv(i++));    h->bya = atoi(CG_Argv(i++));
    h->bga = atoi(CG_Argv(i++));    h->bmh = atoi(CG_Argv(i++));
    h->bquad = atoi(CG_Argv(i++));  h->bbs = atoi(CG_Argv(i++));
    h->bregen = atoi(CG_Argv(i++)); h->bhaste = atoi(CG_Argv(i++));
    h->binvis = atoi(CG_Argv(i++));
    h->bquadTime = atoi(CG_Argv(i++)); h->bbsTime = atoi(CG_Argv(i++));
    h->bregenTime = atoi(CG_Argv(i++)); h->bhasteTime = atoi(CG_Argv(i++));
    h->binvisTime = atoi(CG_Argv(i++));
    h->valid = qtrue;

    cg.numScores = atoi(CG_Argv(i++));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(i++));
    cg.teamScores[1] = atoi(CG_Argv(i++));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->team = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));
        sp->frags = atoi(CG_Argv(i++));
        sp->deaths = atoi(CG_Argv(i++));
        sp->accuracy = atoi(CG_Argv(i++));
        sp->bestWeapon = atoi(CG_Argv(i++));
        sp->impressiveCount = atoi(CG_Argv(i++));
        sp->excellentCount = atoi(CG_Argv(i++));
        sp->guantletCount = atoi(CG_Argv(i++));
        sp->tks = atoi(CG_Argv(i++));
        sp->tkd = atoi(CG_Argv(i++));
        sp->damageDone = atoi(CG_Argv(i++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        cgs.clientinfo[sp->client].team = sp->team;
    }
    cg.numScores = n;
    CG_SetScoreSelection(NULL);
}

/*
=================
CG_ParseScores_Ca

[QL] scores_ca: 17 fields per player (no team header)
=================
*/
static void CG_ParseScores_Ca(void) {
    int i, n;

    i = 1;
    cg.numScores = atoi(CG_Argv(i++));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(i++));
    cg.teamScores[1] = atoi(CG_Argv(i++));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->team = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));
        sp->frags = atoi(CG_Argv(i++));
        sp->deaths = atoi(CG_Argv(i++));
        sp->accuracy = atoi(CG_Argv(i++));
        sp->bestWeapon = atoi(CG_Argv(i++));
        sp->bestWeaponAccuracy = atoi(CG_Argv(i++));
        sp->damageDone = atoi(CG_Argv(i++));
        sp->impressiveCount = atoi(CG_Argv(i++));
        sp->excellentCount = atoi(CG_Argv(i++));
        sp->guantletCount = atoi(CG_Argv(i++));
        sp->perfect = atoi(CG_Argv(i++));
        sp->alive = atoi(CG_Argv(i++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        cgs.clientinfo[sp->client].team = sp->team;
    }
    cg.numScores = n;
    CG_SetScoreSelection(NULL);
}

/*
=================
CG_ParseScores_Ctf

[QL] scores_ctf: 34-field team header + 19 fields per player
=================
*/
static void CG_ParseScores_Ctf(void) {
    int i, n;
    ctfScoreHeader_t *h = &cg.ctfScoreHeader;

    i = 1;
    h->rra = atoi(CG_Argv(i++));    h->rya = atoi(CG_Argv(i++));
    h->rga = atoi(CG_Argv(i++));    h->rmh = atoi(CG_Argv(i++));
    h->rquad = atoi(CG_Argv(i++));  h->rbs = atoi(CG_Argv(i++));
    h->rregen = atoi(CG_Argv(i++)); h->rhaste = atoi(CG_Argv(i++));
    h->rinvis = atoi(CG_Argv(i++)); h->rflag = atoi(CG_Argv(i++));
    h->rmedkit = atoi(CG_Argv(i++));
    h->rquadTime = atoi(CG_Argv(i++)); h->rbsTime = atoi(CG_Argv(i++));
    h->rregenTime = atoi(CG_Argv(i++)); h->rhasteTime = atoi(CG_Argv(i++));
    h->rinvisTime = atoi(CG_Argv(i++)); h->rflagTime = atoi(CG_Argv(i++));
    h->bra = atoi(CG_Argv(i++));    h->bya = atoi(CG_Argv(i++));
    h->bga = atoi(CG_Argv(i++));    h->bmh = atoi(CG_Argv(i++));
    h->bquad = atoi(CG_Argv(i++));  h->bbs = atoi(CG_Argv(i++));
    h->bregen = atoi(CG_Argv(i++)); h->bhaste = atoi(CG_Argv(i++));
    h->binvis = atoi(CG_Argv(i++)); h->bflag = atoi(CG_Argv(i++));
    h->bmedkit = atoi(CG_Argv(i++));
    h->bquadTime = atoi(CG_Argv(i++)); h->bbsTime = atoi(CG_Argv(i++));
    h->bregenTime = atoi(CG_Argv(i++)); h->bhasteTime = atoi(CG_Argv(i++));
    h->binvisTime = atoi(CG_Argv(i++)); h->bflagTime = atoi(CG_Argv(i++));
    h->valid = qtrue;

    cg.numScores = atoi(CG_Argv(i++));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(i++));
    cg.teamScores[1] = atoi(CG_Argv(i++));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->team = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));
        sp->frags = atoi(CG_Argv(i++));
        sp->deaths = atoi(CG_Argv(i++));
        sp->accuracy = atoi(CG_Argv(i++));
        sp->bestWeapon = atoi(CG_Argv(i++));
        sp->impressiveCount = atoi(CG_Argv(i++));
        sp->excellentCount = atoi(CG_Argv(i++));
        sp->guantletCount = atoi(CG_Argv(i++));
        sp->defendCount = atoi(CG_Argv(i++));
        sp->assistCount = atoi(CG_Argv(i++));
        sp->captures = atoi(CG_Argv(i++));
        sp->perfect = atoi(CG_Argv(i++));
        sp->alive = atoi(CG_Argv(i++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        cgs.clientinfo[sp->client].team = sp->team;
    }
    cg.numScores = n;
    CG_SetScoreSelection(NULL);
}

/*
=================
CG_ParseScores_Ft

[QL] scores_ft: same team header as TDM (28 fields) + 18 fields per player (includes thaws)
=================
*/
static void CG_ParseScores_Ft(void) {
    int i, n;
    tdmScoreHeader_t *h = &cg.tdmScoreHeader;

    i = 1;
    h->rra = atoi(CG_Argv(i++));    h->rya = atoi(CG_Argv(i++));
    h->rga = atoi(CG_Argv(i++));    h->rmh = atoi(CG_Argv(i++));
    h->rquad = atoi(CG_Argv(i++));  h->rbs = atoi(CG_Argv(i++));
    h->rregen = atoi(CG_Argv(i++)); h->rhaste = atoi(CG_Argv(i++));
    h->rinvis = atoi(CG_Argv(i++));
    h->rquadTime = atoi(CG_Argv(i++)); h->rbsTime = atoi(CG_Argv(i++));
    h->rregenTime = atoi(CG_Argv(i++)); h->rhasteTime = atoi(CG_Argv(i++));
    h->rinvisTime = atoi(CG_Argv(i++));
    h->bra = atoi(CG_Argv(i++));    h->bya = atoi(CG_Argv(i++));
    h->bga = atoi(CG_Argv(i++));    h->bmh = atoi(CG_Argv(i++));
    h->bquad = atoi(CG_Argv(i++));  h->bbs = atoi(CG_Argv(i++));
    h->bregen = atoi(CG_Argv(i++)); h->bhaste = atoi(CG_Argv(i++));
    h->binvis = atoi(CG_Argv(i++));
    h->bquadTime = atoi(CG_Argv(i++)); h->bbsTime = atoi(CG_Argv(i++));
    h->bregenTime = atoi(CG_Argv(i++)); h->bhasteTime = atoi(CG_Argv(i++));
    h->binvisTime = atoi(CG_Argv(i++));
    h->valid = qtrue;

    cg.numScores = atoi(CG_Argv(i++));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(i++));
    cg.teamScores[1] = atoi(CG_Argv(i++));

    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->team = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));
        sp->frags = atoi(CG_Argv(i++));
        sp->deaths = atoi(CG_Argv(i++));
        sp->accuracy = atoi(CG_Argv(i++));
        sp->bestWeapon = atoi(CG_Argv(i++));
        sp->impressiveCount = atoi(CG_Argv(i++));
        sp->excellentCount = atoi(CG_Argv(i++));
        sp->guantletCount = atoi(CG_Argv(i++));
        sp->thaws = atoi(CG_Argv(i++));
        sp->tkd = atoi(CG_Argv(i++));
        i++;  // unknown field
        sp->damageDone = atoi(CG_Argv(i++));
        sp->alive = atoi(CG_Argv(i++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        cgs.clientinfo[sp->client].team = sp->team;
    }
    cg.numScores = n;
    CG_SetScoreSelection(NULL);
}

/*
=================
CG_ParseScores_Rr

[QL] scores_rr: 20 fields per player (includes roundScore)
=================
*/
static void CG_ParseScores_Rr(void) {
    int n, idx;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(2));
    cg.teamScores[1] = atoi(CG_Argv(3));

    idx = 4;
    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores; n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(idx++));
        sp->score = atoi(CG_Argv(idx++));
        sp->roundScore = atoi(CG_Argv(idx++));
        sp->ping = atoi(CG_Argv(idx++));
        sp->time = atoi(CG_Argv(idx++));
        sp->frags = atoi(CG_Argv(idx++));
        sp->deaths = atoi(CG_Argv(idx++));
        sp->accuracy = atoi(CG_Argv(idx++));
        sp->bestWeapon = atoi(CG_Argv(idx++));
        sp->bestWeaponAccuracy = atoi(CG_Argv(idx++));
        sp->impressiveCount = atoi(CG_Argv(idx++));
        sp->excellentCount = atoi(CG_Argv(idx++));
        sp->guantletCount = atoi(CG_Argv(idx++));
        sp->defendCount = atoi(CG_Argv(idx++));
        sp->assistCount = atoi(CG_Argv(idx++));
        sp->perfect = atoi(CG_Argv(idx++));
        sp->captures = atoi(CG_Argv(idx++));
        sp->alive = atoi(CG_Argv(idx++));
        sp->damageDone = atoi(CG_Argv(idx++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        sp->team = cgs.clientinfo[sp->client].team;
    }
    cg.numScores = n;
    CG_SetScoreSelection(NULL);
}

/*
=================
CG_ParseScores_Race

[QL] scores_race: 4 fields per player (minimal)
=================
*/
static void CG_ParseScores_Race(void) {
    int i, n;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }

    i = 2;
    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        sp->team = cgs.clientinfo[sp->client].team;
    }
    cg.numScores = n;
    CG_SetScoreSelection(NULL);
}

/*
=================
CG_ParseSmScores

[QL] smscores: compact 8-field per player format
=================
*/
static void CG_ParseSmScores(void) {
    int i, n;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }
    cg.teamScores[0] = atoi(CG_Argv(2));
    cg.teamScores[1] = atoi(CG_Argv(3));

    i = 4;
    memset(cg.scores, 0, sizeof(cg.scores));
    for (n = 0; n < cg.numScores && *CG_Argv(i); n++) {
        score_t *sp = &cg.scores[n];
        sp->client = atoi(CG_Argv(i++));
        sp->score = atoi(CG_Argv(i++));
        sp->ping = atoi(CG_Argv(i++));
        sp->time = atoi(CG_Argv(i++));
        i++;  // unknown
        i++;  // unknown
        sp->frags = atoi(CG_Argv(i++));
        sp->deaths = atoi(CG_Argv(i++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        sp->team = cgs.clientinfo[sp->client].team;
    }
    cg.numScores = n;
    CG_SetScoreSelection(NULL);
}

/*
=================
CG_ParseTeamInfo

=================
*/
static void CG_ParseTeamInfo(void) {
    int i;
    int client;

    numSortedTeamPlayers = atoi(CG_Argv(1));
    if (numSortedTeamPlayers < 0 || numSortedTeamPlayers > TEAM_MAXOVERLAY) {
        CG_Error("CG_ParseTeamInfo: numSortedTeamPlayers out of range (%d)",
                 numSortedTeamPlayers);
        return;
    }

    for (i = 0; i < numSortedTeamPlayers; i++) {
        client = atoi(CG_Argv(i * 6 + 2));
        if (client < 0 || client >= MAX_CLIENTS) {
            CG_Error("CG_ParseTeamInfo: bad client number: %d", client);
            return;
        }

        sortedTeamPlayers[i] = client;

        cgs.clientinfo[client].location = atoi(CG_Argv(i * 6 + 3));
        cgs.clientinfo[client].health = atoi(CG_Argv(i * 6 + 4));
        cgs.clientinfo[client].armor = atoi(CG_Argv(i * 6 + 5));
        cgs.clientinfo[client].curWeapon = atoi(CG_Argv(i * 6 + 6));
        cgs.clientinfo[client].powerups = atoi(CG_Argv(i * 6 + 7));
    }
}

/*
================
CG_ParseServerinfo

This is called explicitly when the gamestate is first received,
and whenever the server updates any serverinfo flagged cvars
================
*/
void CG_ParseServerinfo(void) {
    const char* info;
    const char* mapname;

    info = CG_ConfigString(CS_SERVERINFO);

    // [QL] matches binary CG_ParseServerinfo field order
    cgs.gametype = atoi(Info_ValueForKey(info, "g_gametype"));
    trap_Cvar_Set("cg_gametype", va("%i", cgs.gametype));
    cgs.teamsize = atoi(Info_ValueForKey(info, "teamsize"));
    cgs.teamSizeMin = atoi(Info_ValueForKey(info, "g_teamSizeMin"));
    cgs.teamForceBalance = atoi(Info_ValueForKey(info, "g_teamForceBalance"));
    cgs.dmflags = atoi(Info_ValueForKey(info, "dmflags"));
    cgs.fraglimit = atoi(Info_ValueForKey(info, "fraglimit"));
    cgs.capturelimit = atoi(Info_ValueForKey(info, "capturelimit"));
    cgs.scorelimit = atoi(Info_ValueForKey(info, "scorelimit"));
    cgs.mercylimit = atoi(Info_ValueForKey(info, "mercylimit"));
    cgs.timelimit = atoi(Info_ValueForKey(info, "timelimit"));
    cgs.roundlimit = atoi(Info_ValueForKey(info, "roundlimit"));
    cgs.roundtimelimit = atoi(Info_ValueForKey(info, "roundtimelimit"));
    cgs.roundWarmupDelay = atoi(Info_ValueForKey(info, "g_roundWarmupDelay"));
    cgs.startingHealth = atoi(Info_ValueForKey(info, "g_startingHealth"));
    cgs.adCaptureScoreBonus = atoi(Info_ValueForKey(info, "g_adCaptureScoreBonus"));
    cgs.adElimScoreBonus = atoi(Info_ValueForKey(info, "g_adElimScoreBonus"));
    cgs.adtouchScoreBonus = atoi(Info_ValueForKey(info, "g_adtouchScoreBonus"));
    cgs.freezeRoundDelay = atoi(Info_ValueForKey(info, "g_freezeRoundDelay"));
    cgs.maxclients = atoi(Info_ValueForKey(info, "sv_maxclients"));
    cgs.timeoutCount = atoi(Info_ValueForKey(info, "g_timeoutCount"));
    cgs.timelimit_overtime = atoi(Info_ValueForKey(info, "g_overtime"));
    cgs.itemHeight = atoi(Info_ValueForKey(info, "g_itemHeight"));
    cgs.gravity = atoi(Info_ValueForKey(info, "g_gravity"));
    cgs.weaponRespawn = atoi(Info_ValueForKey(info, "g_weaponRespawn"));
    cgs.itemTimers = atoi(Info_ValueForKey(info, "g_itemTimers"));
    cgs.quadDamageFactor = atoi(Info_ValueForKey(info, "g_quadDamageFactor"));

    // [QL] copy g_loadout from serverinfo → cg_loadout ROM cvar
    trap_Cvar_Set("cg_loadout", Info_ValueForKey(info, "g_loadout"));

    mapname = Info_ValueForKey(info, "mapname");
    Com_sprintf(cgs.mapname, sizeof(cgs.mapname), "maps/%s.bsp", mapname);

    // [QL] g_voteFlags → set voting disabled cvars for UI
    cgs.voteFlags = atoi(Info_ValueForKey(info, "g_voteFlags"));
    if (cgs.voteFlags & (VF_MAP | VF_NEXTMAP)) {
        trap_Cvar_Set("ui_mapVotingDisabled", "1");
    } else {
        trap_Cvar_Set("ui_mapVotingDisabled", "0");
    }
    if ((cgs.voteFlags & VF_ENDMAP_VOTING) || cgs.localServer) {
        trap_Cvar_Set("ui_endMapVotingDisabled", "1");
    } else {
        trap_Cvar_Set("ui_endMapVotingDisabled", "0");
    }
    if (cgs.voteFlags & VF_GAMETYPE) {
        trap_Cvar_Set("ui_gameTypeVotingDisabled", "1");
    } else {
        trap_Cvar_Set("ui_gameTypeVotingDisabled", "0");
    }

    Q_strncpyz(cgs.redTeam, Info_ValueForKey(info, "g_redTeam"), sizeof(cgs.redTeam));
    Q_strncpyz(cgs.blueTeam, Info_ValueForKey(info, "g_blueTeam"), sizeof(cgs.blueTeam));
}

/*
==================
CG_ParseWarmup
==================
*/
static void CG_ParseWarmup(void) {
    const char* info;
    const char* s;
    int warmup;

    info = CG_ConfigString(CS_WARMUP);

    warmup = atoi(Info_ValueForKey(info, "time"));

    // Parse optional gametype override
    s = Info_ValueForKey(info, "g_gametype");
    if (*s) {
        cg.warmupGametype = atoi(s);
    } else {
        cg.warmupGametype = -1;
    }

    cg.warmupCount = -1;

    if (warmup == 0 && cg.warmup) {
        // match started - no sound
    } else if (warmup > 0 && cg.warmup <= 0) {
        // countdown started
        int gt = (cg.warmupGametype >= 0) ? cg.warmupGametype : cgs.gametype;
        if (gt < GT_TEAM || gt == GT_TOURNAMENT || gt == GT_RR) {
            trap_S_StartLocalSound(cgs.media.countPrepareSound, CHAN_ANNOUNCER);
        } else {
            trap_S_StartLocalSound(cgs.media.countPrepareTeamSound, CHAN_ANNOUNCER);
        }
    }

    cg.warmup = warmup;

    trap_Cvar_Set("ui_warmup", va("%i", warmup));
}

/*
================
CG_ParsePmoveParams

[QL] Parse pmove_* parameters from CS_PMOVEINFO configstring.
Called at init and whenever CS_PMOVEINFO changes, so client-side
prediction uses the same physics parameters as the server.
================
*/
void CG_ParsePmoveParams(void) {
    const char *info;
    const char *val;

    info = CG_ConfigString(CS_PMOVEINFO);
    if (!info[0]) {
        return;
    }

    val = Info_ValueForKey(info, "pmove_AirAccel");
    if (*val) PM_SetAirAccel(atof(val));

    val = Info_ValueForKey(info, "pmove_AirStepFriction");
    if (*val) PM_SetAirStepFriction(atof(val));

    val = Info_ValueForKey(info, "pmove_AirSteps");
    if (*val) PM_SetAirSteps(atoi(val));

    val = Info_ValueForKey(info, "pmove_AirStopAccel");
    if (*val) PM_SetAirStopAccel(atof(val));

    val = Info_ValueForKey(info, "pmove_AirControl");
    if (*val) PM_SetAirControl(atof(val));

    val = Info_ValueForKey(info, "pmove_AutoHop");
    if (*val) PM_SetAutoHop(atoi(val));

    val = Info_ValueForKey(info, "pmove_BunnyHop");
    if (*val) PM_SetBunnyHop(atoi(val));

    val = Info_ValueForKey(info, "pmove_ChainJump");
    if (*val) PM_SetChainJump(atoi(val));

    val = Info_ValueForKey(info, "pmove_ChainJumpVelocity");
    if (*val) PM_SetChainJumpVelocity(atof(val));

    val = Info_ValueForKey(info, "pmove_CircleStrafeFriction");
    if (*val) PM_SetCircleStrafeFriction(atof(val));

    val = Info_ValueForKey(info, "pmove_CrouchSlideFriction");
    if (*val) PM_SetCrouchSlideFriction(atof(val));

    val = Info_ValueForKey(info, "pmove_CrouchSlideTime");
    if (*val) PM_SetCrouchSlideTime(atoi(val));

    val = Info_ValueForKey(info, "pmove_CrouchSlide");
    if (*val) PM_SetCrouchSlide(atoi(val));

    val = Info_ValueForKey(info, "pmove_CrouchStepJump");
    if (*val) PM_SetCrouchStepJump(atoi(val));

    val = Info_ValueForKey(info, "pmove_DoubleJump");
    if (*val) PM_SetDoubleJump(atoi(val));

    val = Info_ValueForKey(info, "pmove_JumpTimeDeltaMin");
    if (*val) PM_SetJumpTimeDeltaMin(atof(val));

    val = Info_ValueForKey(info, "pmove_JumpVelocity");
    if (*val) PM_SetJumpVelocity(atof(val));

    val = Info_ValueForKey(info, "pmove_JumpVelocityMax");
    if (*val) PM_SetJumpVelocityMax(atof(val));

    val = Info_ValueForKey(info, "pmove_JumpVelocityScaleAdd");
    if (*val) PM_SetJumpVelocityScaleAdd(atof(val));

    val = Info_ValueForKey(info, "pmove_JumpVelocityTimeThreshold");
    if (*val) PM_SetJumpVelocityTimeThreshold(atof(val));

    val = Info_ValueForKey(info, "pmove_JumpVelocityTimeThresholdOffset");
    if (*val) PM_SetJumpVelocityTimeThresholdOffset(atof(val));

    val = Info_ValueForKey(info, "pmove_noPlayerClip");
    if (*val) PM_SetNoPlayerClip(atoi(val));

    val = Info_ValueForKey(info, "pmove_RampJump");
    if (*val) PM_SetRampJump(atoi(val));

    val = Info_ValueForKey(info, "pmove_RampJumpScale");
    if (*val) PM_SetRampJumpScale(atof(val));

    val = Info_ValueForKey(info, "pmove_StepHeight");
    if (*val) PM_SetStepHeight(atof(val));

    val = Info_ValueForKey(info, "pmove_StepJump");
    if (*val) PM_SetStepJump(atoi(val));

    val = Info_ValueForKey(info, "pmove_StepJumpVelocity");
    if (*val) PM_SetStepJumpVelocity(atof(val));

    val = Info_ValueForKey(info, "pmove_StrafeAccel");
    if (*val) PM_SetStrafeAccel(atof(val));

    val = Info_ValueForKey(info, "pmove_velocity_gh");
    if (*val) PM_SetVelocityGH(atof(val));

    val = Info_ValueForKey(info, "pmove_WalkAccel");
    if (*val) PM_SetWalkAccel(atof(val));

    val = Info_ValueForKey(info, "pmove_WalkFriction");
    if (*val) PM_SetWalkFriction(atof(val));

    val = Info_ValueForKey(info, "pmove_WaterSwimScale");
    if (*val) PM_SetWaterSwimScale(atof(val));

    val = Info_ValueForKey(info, "pmove_WaterWadeScale");
    if (*val) PM_SetWaterWadeScale(atof(val));

    val = Info_ValueForKey(info, "pmove_WeaponRaiseTime");
    if (*val) PM_SetWeaponRaiseTime(atoi(val));

    val = Info_ValueForKey(info, "pmove_WeaponDropTime");
    if (*val) PM_SetWeaponDropTime(atoi(val));

    val = Info_ValueForKey(info, "pmove_WishSpeed");
    if (*val) PM_SetWishSpeed(atof(val));
}

/*
================
CG_SetConfigValues

Called on load to set the initial values from configure strings
================
*/
void CG_SetConfigValues(void) {
    const char* s;

    cgs.scores1 = atoi(CG_ConfigString(CS_SCORES1));
    cgs.scores2 = atoi(CG_ConfigString(CS_SCORES2));
    cgs.levelStartTime = atoi(CG_ConfigString(CS_LEVEL_START_TIME));

    // [QL] parse pmove parameters for client-side prediction
    CG_ParsePmoveParams();
    if (cgs.gametype == GT_CTF) {
        s = CG_ConfigString(CS_FLAGSTATUS);
        cgs.redflag = s[0] - '0';
        cgs.blueflag = s[1] - '0';
    } else if (cgs.gametype == GT_1FCTF) {
        s = CG_ConfigString(CS_FLAGSTATUS);
        cgs.flagStatus = s[0] - '0';
    }
    cg.warmup = atoi(Info_ValueForKey(CG_ConfigString(CS_WARMUP), "time"));
}

/*
=====================
CG_ShaderStateChanged
=====================
*/
void CG_ShaderStateChanged(void) {
    char originalShader[MAX_QPATH];
    char newShader[MAX_QPATH];
    char timeOffset[16];
    const char* o;
    char *n, *t;

    o = CG_ConfigString(CS_SHADERSTATE);
    while (o && *o) {
        n = strstr(o, "=");
        if (n && *n) {
            strncpy(originalShader, o, n - o);
            originalShader[n - o] = 0;
            n++;
            t = strstr(n, ":");
            if (t && *t) {
                strncpy(newShader, n, t - n);
                newShader[t - n] = 0;
            } else {
                break;
            }
            t++;
            o = strstr(t, "@");
            if (o) {
                strncpy(timeOffset, t, o - t);
                timeOffset[o - t] = 0;
                o++;
                trap_R_RemapShader(originalShader, newShader, timeOffset);
            }
        } else {
            break;
        }
    }
}

/*
================
CG_ConfigStringModified

================
*/
static void CG_ConfigStringModified(void) {
    const char* str;
    int num;

    num = atoi(CG_Argv(1));

    // get the gamestate from the client system, which will have the
    // new configstring already integrated
    trap_GetGameState(&cgs.gameState);

    // look up the individual string that was modified
    str = CG_ConfigString(num);

    // do something with it if necessary
    if (num == CS_MUSIC) {
        CG_StartMusic();
    } else if (num == CS_SERVERINFO) {
        CG_ParseServerinfo();
    } else if (num == CS_WARMUP) {
        CG_ParseWarmup();
    } else if (num == CS_SCORES1) {
        cgs.scores1 = atoi(str);
    } else if (num == CS_SCORES2) {
        cgs.scores2 = atoi(str);
    } else if (num == CS_LEVEL_START_TIME) {
        cgs.levelStartTime = atoi(str);
    } else if (num == CS_VOTE_TIME) {
        cgs.voteTime = atoi(str);
        cgs.voteModified = qtrue;
        // [QL] set ui_voteactive cvar for menu visibility
        if (cgs.voteTime && !cgs.localServer) {
            trap_Cvar_Set("ui_voteactive", "1");
        } else {
            trap_Cvar_Set("ui_voteactive", "0");
        }
    } else if (num == CS_VOTE_YES) {
        cgs.voteYes = atoi(str);
        cgs.voteModified = qtrue;
    } else if (num == CS_VOTE_NO) {
        cgs.voteNo = atoi(str);
        cgs.voteModified = qtrue;
    } else if (num == CS_VOTE_STRING) {
        Q_strncpyz(cgs.voteString, str, sizeof(cgs.voteString));
        trap_Cvar_Set("ui_votestring", cgs.voteString);
        if (cgs.voteString[0]) {
            trap_S_StartLocalSound(cgs.media.voteNow, CHAN_ANNOUNCER);
        }
    } else if (num == CS_INTERMISSION) {
        cg.intermissionStarted = atoi(str);
        // [QL] notify UI about intermission state
        if (cg.intermissionStarted == 1) {
            trap_Cvar_Set("ui_intermission", "1");
        }
    } else if (num >= CS_MODELS && num < CS_MODELS + MAX_MODELS) {
        cgs.gameModels[num - CS_MODELS] = trap_R_RegisterModel(str);
    } else if (num >= CS_SOUNDS && num < CS_SOUNDS + MAX_SOUNDS) {
        if (str[0] != '*') {  // player specific sounds don't register here
            cgs.gameSounds[num - CS_SOUNDS] = trap_S_RegisterSound(str, qfalse);
        }
    } else if (num >= CS_PLAYERS && num < CS_PLAYERS + MAX_CLIENTS) {
        CG_NewClientInfo(num - CS_PLAYERS);
        CG_BuildSpectatorString();
    } else if (num == CS_FLAGSTATUS) {
        if (cgs.gametype == GT_CTF) {
            // format is rb where its red/blue, 0 is at base, 1 is taken, 2 is dropped
            cgs.redflag = str[0] - '0';
            cgs.blueflag = str[1] - '0';
        } else if (cgs.gametype == GT_1FCTF) {
            cgs.flagStatus = str[0] - '0';
        }
    } else if (num == CS_CLIENTNUM1STPLAYER) {
        cgs.clientNum1stPlayer = atoi(str);
        cg.duelPlayer1 = cgs.clientNum1stPlayer;
    } else if (num == CS_CLIENTNUM2NDPLAYER) {
        cgs.clientNum2ndPlayer = atoi(str);
        cg.duelPlayer2 = cgs.clientNum2ndPlayer;
    } else if (num == CS_ROUND_START_TIME) {
        if (atoi(str)) {
            cgs.roundStarted = qtrue;
        }
    } else if (num == CS_ROTATIONMAPS) {
        // [QL] map vote info - parse map names for vote display
        cg.mapVoteActive = (str[0] != '\0');
    } else if (num == CS_SHADERSTATE) {
        CG_ShaderStateChanged();
    } else if (num == CS_PMOVEINFO) {
        CG_ParsePmoveParams();
    }
}

/*
=======================
CG_AddToTeamChat

=======================
*/
static void CG_AddToTeamChat(const char* str) {
    int len;
    char *p, *ls;
    int lastcolor;
    int chatHeight;

    if (cg_teamChatHeight.integer < TEAMCHAT_HEIGHT) {
        chatHeight = cg_teamChatHeight.integer;
    } else {
        chatHeight = TEAMCHAT_HEIGHT;
    }

    if (chatHeight <= 0 || cg_teamChatTime.integer <= 0) {
        // team chat disabled, dump into normal chat
        cgs.teamChatPos = cgs.teamLastChatPos = 0;
        return;
    }

    len = 0;

    p = cgs.teamChatMsgs[cgs.teamChatPos % chatHeight];
    *p = 0;

    lastcolor = '7';

    ls = NULL;
    while (*str) {
        if (len > TEAMCHAT_WIDTH - 1) {
            if (ls) {
                str -= (p - ls);
                str++;
                p -= (p - ls);
            }
            *p = 0;

            cgs.teamChatMsgTimes[cgs.teamChatPos % chatHeight] = cg.time;

            cgs.teamChatPos++;
            p = cgs.teamChatMsgs[cgs.teamChatPos % chatHeight];
            *p = 0;
            *p++ = Q_COLOR_ESCAPE;
            *p++ = lastcolor;
            len = 0;
            ls = NULL;
        }

        if (Q_IsColorString(str)) {
            *p++ = *str++;
            lastcolor = *str;
            *p++ = *str++;
            continue;
        }
        if (*str == ' ') {
            ls = p;
        }
        *p++ = *str++;
        len++;
    }
    *p = 0;

    cgs.teamChatMsgTimes[cgs.teamChatPos % chatHeight] = cg.time;
    cgs.teamChatPos++;

    if (cgs.teamChatPos - cgs.teamLastChatPos > chatHeight)
        cgs.teamLastChatPos = cgs.teamChatPos - chatHeight;
}

/*
===============
CG_MapRestart

The server has issued a map_restart, so the next snapshot
is completely new and should not be interpolated to.

A tournement restart will clear everything, but doesn't
require a reload of all the media
===============
*/
static void CG_MapRestart(void) {
    if (cg_showmiss.integer) {
        CG_Printf("CG_MapRestart\n");
    }

    CG_InitLocalEntities();
    CG_InitMarkPolys();
    CG_ClearParticles();

    // make sure the "3 frags left" warnings play again
    cg.fraglimitWarnings = 0;

    cg.timelimitWarnings = 0;
    cg.rewardTime = 0;
    cg.rewardStack = 0;
    cg.intermissionStarted = qfalse;
    cg.levelShot = qfalse;

    cgs.voteTime = 0;

    cg.mapRestart = qtrue;

    CG_StartMusic();

    trap_S_ClearLoopingSounds(qtrue);

    // we really should clear more parts of cg here and stop sounds

    // play the "fight" sound if this is a restart without warmup
    if (cg.warmup == 0 /* && cgs.gametype == GT_TOURNAMENT */) {
        trap_S_StartLocalSound(cgs.media.countFightSound, CHAN_ANNOUNCER);
        CG_CenterPrint("FIGHT!", 120, GIANTCHAR_WIDTH * 2);
    }
    if (cg_singlePlayerActive.integer) {
        trap_Cvar_Set("ui_matchStartTime", va("%i", cg.time));
        if (cg_recordSPDemo.integer && *cg_recordSPDemoName.string) {
            trap_SendConsoleCommand(va("set g_synchronousclients 1 ; record %s \n", cg_recordSPDemoName.string));
        }
    }
    trap_Cvar_Set("cg_thirdPerson", "0");
}

/*
=================
CG_RemoveChatEscapeChar
=================
*/
static void CG_RemoveChatEscapeChar(char* text) {
    int i, l;

    l = 0;
    for (i = 0; text[i]; i++) {
        if (text[i] == '\x19')
            continue;
        text[l++] = text[i];
    }
    text[l] = '\0';
}

/*
=================
CG_ParseTeamStats

[QL] Parse team stats (tdmstats/castats/ctfstats) - item pickup counts
=================
*/
static void CG_ParseTeamStats(void) {
    int i = 1;
    cg.teamPickups.rra = atoi(CG_Argv(i++));
    cg.teamPickups.rya = atoi(CG_Argv(i++));
    cg.teamPickups.rga = atoi(CG_Argv(i++));
    cg.teamPickups.rmh = atoi(CG_Argv(i++));
    cg.teamPickups.rquad = atoi(CG_Argv(i++));
    cg.teamPickups.rbs = atoi(CG_Argv(i++));
    cg.teamPickups.rquadTime = atoi(CG_Argv(i++));
    cg.teamPickups.rbsTime = atoi(CG_Argv(i++));
    cg.teamPickups.bra = atoi(CG_Argv(i++));
    cg.teamPickups.bya = atoi(CG_Argv(i++));
    cg.teamPickups.bga = atoi(CG_Argv(i++));
    cg.teamPickups.bmh = atoi(CG_Argv(i++));
    cg.teamPickups.bquad = atoi(CG_Argv(i++));
    cg.teamPickups.bbs = atoi(CG_Argv(i++));
    cg.teamPickups.bquadTime = atoi(CG_Argv(i++));
    cg.teamPickups.bbsTime = atoi(CG_Argv(i++));
    cg.teamPickups.valid = qtrue;
}

/*
=================
CG_ParseAccuracy

[QL] Parse accuracy stats (per-weapon accuracy percentages)
=================
*/
static void CG_ParseAccuracy(void) {
    int i;
    for (i = 0; i < MAX_WEAPONS && i < trap_Argc() - 1; i++) {
        cg.accuracyStats.accuracy[i] = atoi(CG_Argv(i + 1));
    }
    cg.accuracyStats.time = cg.time;
    cg.accuracyStats.clientNum = cg.snap ? cg.snap->ps.clientNum : 0;
    cg.accuracyStats.valid = qtrue;
}

/*
=================
CG_ParseDuelScores

[QL] Parse duel-specific score data with per-weapon stats
=================
*/
static void CG_ParseDuelScores(void) {
    int i, j, idx;

    idx = 1;
    for (i = 0; i < 2; i++) {
        duelScore_t *ds = &cg.duelScores[i];
        ds->clientNum = atoi(CG_Argv(idx++));
        ds->score = atoi(CG_Argv(idx++));
        ds->ping = atoi(CG_Argv(idx++));
        ds->time = atoi(CG_Argv(idx++));
        ds->kills = atoi(CG_Argv(idx++));
        ds->deaths = atoi(CG_Argv(idx++));
        ds->accuracy = atoi(CG_Argv(idx++));
        ds->bestWeapon = atoi(CG_Argv(idx++));
        ds->damage = atoi(CG_Argv(idx++));
        ds->awardImpressive = atoi(CG_Argv(idx++));
        ds->awardExcellent = atoi(CG_Argv(idx++));
        ds->awardHumiliation = atoi(CG_Argv(idx++));
        ds->perfect = atoi(CG_Argv(idx++));
        ds->redArmorPickups = atoi(CG_Argv(idx++));
        ds->redArmorTime = atof(CG_Argv(idx++));
        ds->yellowArmorPickups = atoi(CG_Argv(idx++));
        ds->yellowArmorTime = atof(CG_Argv(idx++));
        ds->greenArmorPickups = atoi(CG_Argv(idx++));
        ds->greenArmorTime = atof(CG_Argv(idx++));
        ds->megaHealthPickups = atoi(CG_Argv(idx++));
        ds->megaHealthTime = atof(CG_Argv(idx++));
        for (j = 0; j < MAX_WEAPONS; j++) {
            if (idx >= trap_Argc()) break;
            ds->weaponStats[j].hits = atoi(CG_Argv(idx++));
            ds->weaponStats[j].atts = atoi(CG_Argv(idx++));
            ds->weaponStats[j].accuracy = atoi(CG_Argv(idx++));
            ds->weaponStats[j].damage = atoi(CG_Argv(idx++));
            ds->weaponStats[j].kills = atoi(CG_Argv(idx++));
        }
    }
    cg.duelScoresValid = qtrue;
}

/*
=================
CG_ServerCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
static void CG_ServerCommand(void) {
    const char* cmd;
    char text[MAX_SAY_TEXT];

    cmd = CG_Argv(0);

    if (!cmd[0]) {
        // server claimed the command
        return;
    }

    if (!strcmp(cmd, "cs")) {
        CG_ConfigStringModified();
        return;
    }

    // [QL] screenshot/record commands from server
    if (!strcmp(cmd, "screenshot")) {
        trap_SendConsoleCommand("screenshot\n");
        return;
    }
    if (!strcmp(cmd, "record")) {
        trap_SendConsoleCommand("record\n");
        return;
    }
    if (!strcmp(cmd, "stoprecord")) {
        trap_SendConsoleCommand("stoprecord\n");
        return;
    }

    if (!strcmp(cmd, "cp")) {
        CG_CenterPrint(CG_Argv(1), SCREEN_HEIGHT * 0.30, BIGCHAR_WIDTH);
        return;
    }

    // [QL] pcp - center print that also prints to console
    if (!strcmp(cmd, "pcp")) {
        CG_CenterPrint(CG_Argv(1), SCREEN_HEIGHT * 0.30, BIGCHAR_WIDTH);
        CG_Printf("%s\n", CG_Argv(1));
        return;
    }

    if (!strcmp(cmd, "print")) {
        CG_Printf("%s", CG_Argv(1));
        cmd = CG_Argv(1);
        if (!Q_stricmpn(cmd, "vote failed", 11) || !Q_stricmpn(cmd, "team vote failed", 16)) {
            trap_S_StartLocalSound(cgs.media.voteFailed, CHAN_ANNOUNCER);
        } else if (!Q_stricmpn(cmd, "vote passed", 11) || !Q_stricmpn(cmd, "team vote passed", 16)) {
            trap_S_StartLocalSound(cgs.media.votePassed, CHAN_ANNOUNCER);
        }
        return;
    }

    if (!strcmp(cmd, "chat")) {
        if (cgs.gametype >= GT_TEAM && cg_teamChatsOnly.integer) {
            return;
        }
        // QL binary: cg_chatbeep.integer gates the chat sound (vmCvar 0x10A6A9E0)
        if (cg_chatbeep.integer) {
            trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
        }
        Q_strncpyz(text, CG_Argv(1), MAX_SAY_TEXT);
        CG_RemoveChatEscapeChar(text);
        CG_Printf("%s\n", text);
        return;
    }

    // [QL] bchat - broadcast chat with on-screen duration
    if (!strcmp(cmd, "bchat")) {
        trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
        Q_strncpyz(text, CG_Argv(1), MAX_SAY_TEXT);
        CG_RemoveChatEscapeChar(text);
        CG_Printf("%s\n", text);
        return;
    }

    // [QL] clearChat - clear chat display
    if (!strcmp(cmd, "clearChat")) {
        memset(cgs.teamChatMsgs, 0, sizeof(cgs.teamChatMsgs));
        cgs.teamChatPos = 0;
        return;
    }

    // [QL] playSound - server-triggered sound effect
    if (!strcmp(cmd, "playSound")) {
        if (trap_Argc() > 1) {
            sfxHandle_t sfx = trap_S_RegisterSound(CG_Argv(1), qfalse);
            trap_S_StartLocalSound(sfx, CHAN_AUTO);
        }
        return;
    }

    // [QL] playMusic / stopMusic - server-controlled music
    if (!strcmp(cmd, "playMusic")) {
        trap_S_StartBackgroundTrack(CG_Argv(1), CG_Argv(2));
        return;
    }
    if (!strcmp(cmd, "stopMusic")) {
        trap_S_StartBackgroundTrack("", "");
        return;
    }

    // [QL] clearSounds - stop all looping sounds
    if (!strcmp(cmd, "clearSounds")) {
        trap_S_ClearLoopingSounds(qtrue);
        return;
    }

    if (!strcmp(cmd, "tchat")) {
        // QL binary: cg_chatbeep.integer gates the chat sound
        if (cg_chatbeep.integer) {
            trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
        }
        Q_strncpyz(text, CG_Argv(1), MAX_SAY_TEXT);
        CG_RemoveChatEscapeChar(text);
        CG_AddToTeamChat(text);
        CG_Printf("%s\n", text);
        return;
    }

    // [QL] priv - client privileges for conditional UI display, etc
    if (!strcmp(cmd, "priv")) {
        trap_Cvar_Set("ui_priv", CG_Argv(1));
        return;
    }

    // [QL] per-gametype scoreboard commands
    if (!strcmp(cmd, "scores_ffa")) {
        CG_ParseScores_Ffa();
        return;
    }
    if (!strcmp(cmd, "scores_tdm")) {
        CG_ParseScores_Tdm();
        return;
    }
    if (!strcmp(cmd, "scores_ca")) {
        CG_ParseScores_Ca();
        return;
    }
    if (!strcmp(cmd, "scores_ctf") || !strcmp(cmd, "scores_ad")) {
        CG_ParseScores_Ctf();
        return;
    }
    if (!strcmp(cmd, "scores_ft")) {
        CG_ParseScores_Ft();
        return;
    }
    if (!strcmp(cmd, "scores_rr")) {
        CG_ParseScores_Rr();
        return;
    }
    if (!strcmp(cmd, "scores_race")) {
        CG_ParseScores_Race();
        return;
    }
    if (!strcmp(cmd, "smscores")) {
        CG_ParseSmScores();
        return;
    }

    // [QL] per-gametype stats commands
    if (!strcmp(cmd, "tdmstats") || !strcmp(cmd, "castats") || !strcmp(cmd, "ctfstats")) {
        CG_ParseTeamStats();
        return;
    }
    if (!strcmp(cmd, "acc")) {
        CG_ParseAccuracy();
        return;
    }
    if (!strcmp(cmd, "scores_duel")) {
        CG_ParseDuelScores();
        return;
    }
    if (!strcmp(cmd, "adscores") || !strcmp(cmd, "pstats")) {
        return;  // AD scores and pstats: consume silently (no UI display needed)
    }

    if (!strcmp(cmd, "tinfo")) {
        CG_ParseTeamInfo();
        return;
    }

    // [QL] race mode commands
    if (!strcmp(cmd, "race_info")) {
        cg.race.totalCheckpoints = atoi(CG_Argv(1));
        cg.race.bestTime = atoi(CG_Argv(2));
        if (trap_Argc() > 3) {
            cg.race.bestSplit = atoi(CG_Argv(3));
        }
        return;
    }
    if (!strcmp(cmd, "race_init")) {
        // reset race state
        memset(&cg.race, 0, sizeof(cg.race));
        cg.race.nextCheckpointEnt = -1;
        cg.race.currentCheckpointEnt = -1;
        return;
    }

    // [QL] complaint system (prefix match)
    if (!Q_stricmpn(cmd, "complaint", 9)) {
        return;
    }

    // [QL] vote UI control
    if (!Q_stricmpn(cmd, "enable_vote_ui", 14)) {
        trap_Cvar_Set("ui_voteactive", "1");
        return;
    }
    if (!Q_stricmpn(cmd, "disable_vote_ui", 15)) {
        trap_Cvar_Set("ui_voteactive", "0");
        return;
    }

    if (!strcmp(cmd, "map_restart")) {
        CG_MapRestart();
        return;
    }

    if (Q_stricmp(cmd, "remapShader") == 0) {
        if (trap_Argc() == 4) {
            char shader1[MAX_QPATH];
            char shader2[MAX_QPATH];
            char shader3[MAX_QPATH];

            Q_strncpyz(shader1, CG_Argv(1), sizeof(shader1));
            Q_strncpyz(shader2, CG_Argv(2), sizeof(shader2));
            Q_strncpyz(shader3, CG_Argv(3), sizeof(shader3));

            trap_R_RemapShader(shader1, shader2, shader3);
        }
        return;
    }

    // loaddeferred can be both a servercmd and a consolecmd
    if (!strcmp(cmd, "loaddefered") || !strcmp(cmd, "loaddeferred")) {  // [QL] accept both spellings
        CG_LoadDeferredPlayers();
        return;
    }

    // clientLevelShot is sent before taking a special screenshot for
    // the menu system during development
    if (!strcmp(cmd, "clientLevelShot")) {
        cg.levelShot = qtrue;
        return;
    }

    CG_Printf("Unknown client game command: %s\n", cmd);
}

/*
====================
CG_ExecuteNewServerCommands

Execute all of the server commands that were received along
with this this snapshot.
====================
*/
void CG_ExecuteNewServerCommands(int latestSequence) {
    while (cgs.serverCommandSequence < latestSequence) {
        if (trap_GetServerCommand(++cgs.serverCommandSequence)) {
            CG_ServerCommand();
        }
    }
}
