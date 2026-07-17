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
#include "../ui/ui_shared.h"  // [QL] Menus_ActivateByName (voice-chat menu)

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

// [QL] Mirror CS_DISABLE_LOADOUT bitmask into the per-weapon
// cg_disableLoadout_* cvars queried by ui/ingame_about.menu + ui/intro.menu.
// Cvar names match cgamex86.dll string table (0x1006bb04 area).
static const char* cg_disableLoadoutCvarNames[WP_NUM_WEAPONS] = {
    NULL,                     // WP_NONE
    "cg_disableLoadout_g",    // WP_GAUNTLET
    "cg_disableLoadout_mg",   // WP_MACHINEGUN
    "cg_disableLoadout_sg",   // WP_SHOTGUN
    "cg_disableLoadout_gl",   // WP_GRENADE_LAUNCHER
    "cg_disableLoadout_rl",   // WP_ROCKET_LAUNCHER
    "cg_disableLoadout_lg",   // WP_LIGHTNING
    "cg_disableLoadout_rg",   // WP_RAILGUN
    "cg_disableLoadout_pg",   // WP_PLASMAGUN
    "cg_disableLoadout_bfg",  // WP_BFG
    "cg_disableLoadout_gh",   // WP_GRAPPLING_HOOK
    "cg_disableLoadout_ng",   // WP_NAILGUN
    "cg_disableLoadout_pl",   // WP_PROX_LAUNCHER
    "cg_disableLoadout_cg",   // WP_CHAINGUN
    "cg_disableLoadout_hmg",  // WP_HMG
};

static void CG_ParseDisableLoadout(const char* s) {
    int mask = atoi(s);
    int i;
    for (i = WP_GAUNTLET; i < WP_NUM_WEAPONS; i++) {
        if (cg_disableLoadoutCvarNames[i]) {
            trap_Cvar_Set(cg_disableLoadoutCvarNames[i],
                          (mask & (1 << i)) ? "1" : "0");
        }
    }
}

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
        // binary 0x10045090: the field after bestWeapon is the best weapon accuracy
        // (score_t+0xe0), not a powerup mask; powerup icons come from entity state
        sp->bestWeaponAccuracy = atoi(CG_Argv(idx++));
        sp->damageDone = atoi(CG_Argv(idx++));
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
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

[QL] scores_ca: 16 fields per player (no team header)
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

[QL] scores_ft: same team header as TDM (28 fields) + 17 fields per player (includes thaws)
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
        // binary 0x100476a0: after gauntlet come thaws, tks, tkd (score_t
        // +0xc0/+0xd4/+0xd8), then damage and the alive flag
        sp->thaws = atoi(CG_Argv(i++));
        sp->tks = atoi(CG_Argv(i++));
        sp->tkd = atoi(CG_Argv(i++));
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

[QL] scores_rr: 19 fields per player (includes roundScore)
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
        // [QL] binary CG_ParseScores_RR (0x10047de0): damageDone is argv 14
        // (right after bestWeaponAccuracy), NOT last. The award/pickup block
        // follows it. Reading impressive here shifted every field by one.
        sp->damageDone = atoi(CG_Argv(idx++));
        sp->impressiveCount = atoi(CG_Argv(idx++));
        sp->excellentCount = atoi(CG_Argv(idx++));
        sp->guantletCount = atoi(CG_Argv(idx++));
        sp->defendCount = atoi(CG_Argv(idx++));
        sp->assistCount = atoi(CG_Argv(idx++));
        sp->perfect = atoi(CG_Argv(idx++));
        sp->captures = atoi(CG_Argv(idx++));
        sp->alive = atoi(CG_Argv(idx++));
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
CG_ParseScores

[QL] bare "scores" verb -> binary CG_ParseScores_Generic (0x100481f0).
18 fields per player like FFA, but two per-player fields (argv +4,+5) are unused
and there is no powerups field. Dispatched by CG_ServerCommand (0x1004ba75).
Format: numScores teamScore0 teamScore1 [client score ping time _ _ accuracy
impressive excellent gauntlet defend assist perfect captures alive frags deaths
bestWeapon] ...
=================
*/
static void CG_ParseScores(void) {
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
        idx += 2;  // two unused per-player fields (binary skips argv +4,+5)
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
        sp->net = sp->frags - sp->deaths;

        if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
            sp->client = 0;
        }
        cgs.clientinfo[sp->client].score = sp->score;
        sp->team = cgs.clientinfo[sp->client].team;
    }
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

    // [QL] binary CG_ParseTeamInfo (0x100487b0, Ghidra-mislabelled
    // CG_ParseAccuracyFromCS): QL's "tinfo" is NOT Q3's 6-fields-per-player
    // overlay. The server (qagame SendTeamInfo, "tinfo %i %s" with a " %i" per
    // player) sends only the sorted client-number list; teammate
    // health/armour/location come through the snapshot, not here. So this is a
    // flat list: argv(1) = count, argv(2..count+1) = client numbers.
    numSortedTeamPlayers = atoi(CG_Argv(1));
    if (numSortedTeamPlayers < 0 || numSortedTeamPlayers > TEAM_MAXOVERLAY) {
        CG_Error("CG_ParseTeamInfo: numSortedTeamPlayers out of range (%d)",
                 numSortedTeamPlayers);
        return;
    }

    for (i = 0; i < numSortedTeamPlayers; i++) {
        client = atoi(CG_Argv(i + 2));
        if (client < 0 || client >= MAX_CLIENTS) {
            CG_Error("CG_ParseTeamInfo: bad client number: %d", client);
            return;
        }
        sortedTeamPlayers[i] = client;
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

    // [QL] copy g_loadout from serverinfo -> cg_loadout ROM cvar
    trap_Cvar_Set("cg_loadout", Info_ValueForKey(info, "g_loadout"));

    mapname = Info_ValueForKey(info, "mapname");
    Com_sprintf(cgs.mapname, sizeof(cgs.mapname), "maps/%s.bsp", mapname);

    // [QL] g_voteFlags -> set voting disabled cvars for UI
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
        if (gt < GT_TEAM || gt == GT_DUEL || gt == GT_RR) {
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
CG_ParseWarmupInfo

[QL] cgamex86.dll: CG_ParseWarmupInfo @ 0x10048c80.  Parses the round-based
warmup countdown from CS_ROUND_WARMUP.  The binary reads two extra per-team
counts and bumps the countdown for gametype 11 (GT_AD).
================
*/
static void CG_ParseWarmupInfo(void) {
    const char* info;
    int warmup;

    info = CG_ConfigString(CS_ROUND_WARMUP);

    warmup = atoi(Info_ValueForKey(info, "time"));
    cg.warmupCount = atoi(Info_ValueForKey(info, "count"));

    if (cgs.gametype == GT_AD) {
        cg.warmupFreezeCount_red = atoi(Info_ValueForKey(info, "redCount"));
        cg.warmupFreezeCount_blue = atoi(Info_ValueForKey(info, "blueCount"));
        cg.warmupCount++;
    }

    cg.warmup = warmup;
}

/*
================
CG_ParseArmorTiered

[QL] cgamex86.dll: CG_ParseArmorTiered @ 0x10048df0.  Reads the tiered-armor
flag from CS_ARMORINFO and mirrors it into the cg_armorTiered cvar.
================
*/
static void CG_ParseArmorTiered(void) {
    const char* info;

    info = CG_ConfigString(CS_ARMORINFO);
    // [QL] key is "armor_tiered" (verified against a live QL server demo), not "t"
    cgs.armorTiered = atoi(Info_ValueForKey(info, "armor_tiered"));
    trap_Cvar_Set("cg_armorTiered", va("%d", cgs.armorTiered));
}

/*
================
CG_ParseConfigParams

[QL] cgamex86.dll: CG_ParseConfigParams @ 0x10048e40. Reads the head/model scale factors
and g_allowCustomHeadmodels from CS_PLAYERINFO, then recomputes model scales. Verified
against a live QL server demo: CS_PLAYERINFO carries only these numeric params - the
model/head OVERRIDE strings are applied server-side and are NOT in this configstring, so
they are read but normally resolve empty. The scales keep sane defaults (matching the
server cvar defaults 1.0/1.0/1.1) when the server did not send CS_PLAYERINFO, otherwise
CG_CalcModelScale would scale every model to zero.
================
*/
void CG_ParseConfigParams(void) {
    const char* info;
    const char* val;

    info = CG_ConfigString(CS_PLAYERINFO);

    Q_strncpyz(cgs.playermodelOverride, Info_ValueForKey(info, "g_playermodelOverride"),
               sizeof(cgs.playermodelOverride));
    Q_strncpyz(cgs.playerheadmodelOverride, Info_ValueForKey(info, "g_playerheadmodelOverride"),
               sizeof(cgs.playerheadmodelOverride));
    cgs.allowCustomHeadmodels = atoi(Info_ValueForKey(info, "g_allowCustomHeadmodels"));

    val = Info_ValueForKey(info, "g_playerheadScale");       cgs.playerheadScale = *val ? atof(val) : 1.0f;
    val = Info_ValueForKey(info, "g_playerheadScaleOffset");  cgs.playerheadScaleOffset = *val ? atof(val) : 1.0f;
    val = Info_ValueForKey(info, "g_playerModelScale");       cgs.playerModelScale = *val ? atof(val) : 1.1f;

    CG_UpdateAllModelScales();
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
    // [QL] custom-settings bitmask (drives the RR-infected bones model), wallbang depth,
    // and the capsule-hull flag for client-side hit prediction
    cgs.customSettings = atoi(CG_ConfigString(CS_CUSTOM_SETTINGS));
    cgs.dmgThroughDepth = atof(CG_ConfigString(CS_DMGTHROUGHDEPTH));
    cgs.playerCylinders = atoi(CG_ConfigString(CS_PLAYER_CYLINDERS));
    // [QL] pause/timeout: freezeEnd is the frozen match-clock base (CG_GetLevelTimerMsec),
    // pauseEnd is the resume/countdown target (CG_DrawTimeout)
    cgs.freezeEnd = atoi(CG_ConfigString(CS_PAUSE_START_TIME));
    cgs.pauseEnd = atoi(CG_ConfigString(CS_PAUSE_END_TIME));
    // [QL] CG_SetConfigValues (binary 0x10049420) seeds only practice (DAT_10a3ff30)
    // and freecam (DAT_10a5fd0c) at init; allReadyTime/roundStartTime are NOT read
    // here by the binary (they arrive later via CG_ConfigStringModified).
    cgs.practice = atoi(CG_ConfigString(CS_PRACTICE));
    cgs.freecam = atoi(CG_ConfigString(CS_FREECAM));

    // [QL] parse pmove parameters for client-side prediction
    CG_ParsePmoveParams();
    // [QL] parse player model/head overrides + scales from CS_PLAYERINFO
    CG_ParseConfigParams();
    if (cgs.gametype == GT_CTF) {
        s = CG_ConfigString(CS_FLAGSTATUS);
        cgs.redflag = s[0] - '0';
        cgs.blueflag = s[1] - '0';
    } else if (cgs.gametype == GT_1FCTF) {
        s = CG_ConfigString(CS_FLAGSTATUS);
        cgs.flagStatus = s[0] - '0';
    }
    cg.warmup = atoi(Info_ValueForKey(CG_ConfigString(CS_WARMUP), "time"));
    // [QL] seed cg_disableLoadout_* cvars from CS_DISABLE_LOADOUT bitmask
    CG_ParseDisableLoadout(CG_ConfigString(CS_DISABLE_LOADOUT));
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
        // [QL] binary CG_ConfigStringModified (0x10049980): CS_ROUND_START_TIME
        // (0x296) writes the round start time to DAT_10a403dc. The prior handler
        // dropped the time and only latched roundStarted; keep roundStarted for
        // the ioquakelive round state and record the time the binary stores.
        cgs.roundStartTime = atoi(str);
        if (cgs.roundStartTime) {
            cgs.roundStarted = qtrue;
        }
    } else if (num == CS_ROUND_WARMUP) {
        CG_ParseWarmupInfo();
    } else if (num == CS_TEAMCOUNT_RED) {
        cgs.teamCountRed = atoi(str);
    } else if (num == CS_TEAMCOUNT_BLUE) {
        cgs.teamCountBlue = atoi(str);
    } else if (num == CS_ARMORINFO) {
        CG_ParseArmorTiered();
    } else if (num == CS_PLAYERINFO) {
        CG_ParseConfigParams();
    } else if (num == CS_ROTATIONMAPS) {
        // [QL] map vote info - parse map names for vote display
        cg.mapVoteActive = (str[0] != '\0');
    } else if (num == CS_SHADERSTATE) {
        CG_ShaderStateChanged();
    } else if (num == CS_PMOVEINFO) {
        CG_ParsePmoveParams();
    } else if (num == CS_CUSTOM_SETTINGS) {
        cgs.customSettings = atoi(str);
    } else if (num == CS_DMGTHROUGHDEPTH) {
        cgs.dmgThroughDepth = atof(str);
    } else if (num == CS_PLAYER_CYLINDERS) {
        cgs.playerCylinders = atoi(str);
    } else if (num == CS_PAUSE_START_TIME) {
        cgs.freezeEnd = atoi(str);
    } else if (num == CS_PAUSE_END_TIME) {
        cgs.pauseEnd = atoi(str);
    } else if (num == CS_PRACTICE) {
        // [QL] practice-mode flag (binary 0x10049980 case 0x29b -> DAT_10a3ff30)
        cgs.practice = atoi(str);
    } else if (num == CS_FREECAM) {
        // [QL] team-spectator free-camera flag (case 0x29c -> DAT_10a5fd0c)
        cgs.freecam = atoi(str);
    } else if (num == CS_ALLREADY_TIME) {
        // [QL] all-ready countdown target time (case 0x2c4 -> DAT_10a403d8)
        cgs.allReadyTime = atoi(str);
    } else if (num == CS_DISABLE_LOADOUT) {
        CG_ParseDisableLoadout(str);
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
===============================================================================

[QL] CLIENT-SIDE VOICE-CHAT RECEIVE SYSTEM

Ported from cgamex86.dll.  Quake Live kept the
Quake 3 voice-chat plumbing but ships no loader for the parsed voice data:
nothing in the binary ever writes the voiceFiles[] table (every xref to its base
0x107d98c0 is a loop bound or the gender read), and there is no CG_LoadVoiceChats
in the binary at all.  At runtime the tables therefore stay empty and
CG_GetVoiceChat never matches, so the whole receive path is inert - but the code
paths are all present in the binary and reproduced here.

QL's storage does NOT match ioquake3's voiceChatList_t, so the file-static
structs below are sized to the binary exactly:
  - voiceFiles[8]       at 0x107d98c0, stride 0x45148  (the parsed voice lists)
  - voiceChatLists[64]  at 0x107d87c0, stride 0x44     (model/skin -> file cache)
  - voiceChatBuffer[32] at 0x10a02300, stride 0x138    (the received ring buffer)
The public prototypes in cg_local.h are kept unchanged.  CG_VoiceChatListForClient
resolves the "current" list; the binary passes that pointer to CG_GetVoiceChat in
EBX, which the fixed 3-arg prototype cannot carry, so it is bridged through the
file-static cg_currentVoiceChatList.

===============================================================================
*/

#define MAX_VOICEFILESIZE 16384  // 0x4000 - max .vc file size the parser accepts
#define MAX_VOICECHATS    64     // voice commands per file / name-cache slots
#define MAX_VOICESOUNDS   64     // random sound variants per voice command
#define MAX_CHATSIZE      64     // per-variant chat display string

// One parsed voice command.  Binary stride 0x1144.
typedef struct {
    char        id[64];                                // +0x000 command name
    int         numSounds;                             // +0x040 variant count
    sfxHandle_t sounds[MAX_VOICESOUNDS];               // +0x044 sound handles
    char        chats[MAX_VOICESOUNDS][MAX_CHATSIZE];  // +0x144 chat strings
} qlVoiceChat_t;

// One parsed voice file.  Binary stride 0x45148.
typedef struct {
    char          name[64];                    // +0x00000 list name
    int           gender;                      // +0x00040
    int           numVoiceChats;               // +0x00044
    qlVoiceChat_t voiceChats[MAX_VOICECHATS];  // +0x00048
} qlVoiceFile_t;

// Maps a client model/skin string -> loaded voiceFiles[] slot.  Binary stride
// 0x44 (matches cg_local.h's voiceChatList_t, but kept file-local).
typedef struct {
    char name[64];
    int  index;
} qlVoiceNameCache_t;

static qlVoiceFile_t       voiceFiles[MAX_VOICEFILES];            // 0x107d98c0
static qlVoiceNameCache_t  voiceChatLists[MAX_VOICECHATS];        // 0x107d87c0
static bufferedVoiceChat_t voiceChatBuffer[MAX_VOICECHATBUFFER];  // 0x10a02300
static qlVoiceFile_t*      cg_currentVoiceChatList;               // binary EBX bridge

// [QL] gate cvars: defined in cg_main.c, not extern'd in cg_local.h.  Positive
// sense (enable flags) - the opposite polarity to Q3's cg_noVoiceChats/Text.
extern vmCvar_t cg_playVoiceChats;  // DAT_10a62acc - sound gate
extern vmCvar_t cg_showVoiceText;   // DAT_10abb2ec - text gate

/*
=================
CG_LoadVoiceChats

[QL] No standalone loader exists in cgamex86.dll (voiceFiles[] is never
populated - QL ships no .vc assets).  Reset the lookup state to a known-empty
baseline so the receive path behaves exactly like the shipped, inert binary.
=================
*/
void CG_LoadVoiceChats(void) {
    memset(voiceChatLists, 0, sizeof(voiceChatLists));
    memset(voiceFiles, 0, sizeof(voiceFiles));
    cg_currentVoiceChatList = NULL;
}

/*
=================
CG_ParseVoiceChats

Address: 0x1004a560

[QL] NOT ioquake3's parser.  QL's version only opens the file, reads it (max
0x4000 bytes), pulls the first token (the list name) and scans the already
loaded voiceFiles[] table for a name match, returning that slot index or -1.
The sound/message table is never loaded by any function in the binary.
=================
*/
int CG_ParseVoiceChats(const char* filename) {
    fileHandle_t f;
    int len;
    char buf[MAX_VOICEFILESIZE];
    char* p;
    const char* token;
    int i;

    len = trap_FS_FOpenFile(filename, &f, FS_READ);
    if (!f) {
        return -1;
    }
    if (len >= (int)sizeof(buf)) {
        CG_Printf("^1voice chat file too large: %s is %i, max allowed is %i\n",
                  filename, len, MAX_VOICEFILESIZE);
        trap_FS_FCloseFile(f);
        return -1;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = '\0';
    trap_FS_FCloseFile(f);

    p = buf;
    token = COM_ParseExt(&p, qtrue);
    if (!token || !token[0]) {
        return -1;
    }

    // scan the loaded voice-file table for a matching list name
    for (i = 0; i < MAX_VOICEFILES; i++) {
        if (!Q_stricmpn(token, voiceFiles[i].name, sizeof(voiceFiles[i].name))) {
            return i;
        }
    }
    return -1;
}

/*
=================
CG_GetVoiceChat

Address: 0x1004a6a0

[QL] Look up cmd in the current voice list (passed in EBX by the binary, bridged
here via cg_currentVoiceChatList), pick a random sound/chat variant, and return
whether a match was found.
=================
*/
qboolean CG_GetVoiceChat(const char* cmd, sfxHandle_t* snd, const char** chat) {
    qlVoiceFile_t* list = cg_currentVoiceChatList;
    int i, rnd;

    if (!list) {
        return qfalse;  // not in the binary; guards the EBX-bridge global
    }

    for (i = 0; i < list->numVoiceChats; i++) {
        if (cmd == NULL) {
            continue;
        }
        // binary uses Q_stricmpn(cmd, id, 99999), i.e. a full case-insensitive compare
        if (!Q_stricmp(cmd, list->voiceChats[i].id)) {
            // binary: (int)((rand() & 0x7fff) / 32768.0 * numSounds).  Reproduce
            // the /0x8000 exactly rather than ioquakelive's random() (which uses
            // /0x7fff and could index numSounds out of bounds).
            rnd = (int)(((float)(rand() & 0x7fff) / 32768.0f) * list->voiceChats[i].numSounds);
            *snd = list->voiceChats[i].sounds[rnd];
            *chat = list->voiceChats[i].chats[rnd];
            return qtrue;
        }
    }
    return qfalse;
}

/*
=================
CG_VoiceChatListForClient

Address: 0x1004a740

[QL] Resolve the voice list for a client from its forced head model/skin
(clientinfo +0x254 / +0x294), caching the model/skin -> file-slot mapping.
The binary returns the resolved &voiceFiles[index] in EAX; here it is stored in
cg_currentVoiceChatList (the header prototype is void).
=================
*/
void CG_VoiceChatListForClient(int clientNum) {
    clientInfo_t* ci;
    char vchatName[64];
    char vchatFile[64];
    int pass;
    int i, j;
    int vchatIndex;
    const char* name;
    int searchGender;

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        clientNum = 0;
    }
    ci = &cgs.clientinfo[clientNum];

    for (pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            // model/skin-specific voice file
            if (ci->forcedHeadModel[0] == '*') {
                Com_sprintf(vchatName, sizeof(vchatName), "%s/%s",
                            ci->forcedHeadModel + 1, ci->forcedHeadSkin);
            } else {
                Com_sprintf(vchatName, sizeof(vchatName), "%s/%s",
                            ci->forcedHeadModel, ci->forcedHeadSkin);
            }
        } else {
            // fallback: model name only
            if (ci->forcedHeadModel[0] == '*') {
                name = ci->forcedHeadModel + 1;
            } else {
                name = ci->forcedHeadModel;
            }
            Com_sprintf(vchatName, sizeof(vchatName), "%s", name);
        }

        // already cached?
        for (i = 0; i < MAX_VOICECHATS; i++) {
            if (voiceChatLists[i].name[0] &&
                !Q_stricmp(vchatName, voiceChatLists[i].name)) {
                cg_currentVoiceChatList = &voiceFiles[voiceChatLists[i].index];
                return;
            }
        }

        // first empty cache slot: try to load
        for (i = 0; i < MAX_VOICECHATS; i++) {
            if (!voiceChatLists[i].name[0]) {
                Com_sprintf(vchatFile, sizeof(vchatFile), "scripts/%s.vc", vchatName);
                vchatIndex = CG_ParseVoiceChats(vchatFile);
                if (vchatIndex != -1) {
                    Q_strncpyz(voiceChatLists[i].name, vchatName,
                               sizeof(voiceChatLists[i].name));
                    voiceChatLists[i].index = vchatIndex;
                    cg_currentVoiceChatList = &voiceFiles[vchatIndex];
                    return;
                }
                break;
            }
        }
    }

    // gender fallback: first try the client's gender, then gender 0 (default)
    searchGender = ci->gender;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < MAX_VOICEFILES; i++) {
            if (voiceFiles[i].name[0] && voiceFiles[i].gender == searchGender) {
                for (j = 0; j < MAX_VOICECHATS; j++) {
                    if (!voiceChatLists[j].name[0]) {
                        Q_strncpyz(voiceChatLists[j].name, vchatName,
                                   sizeof(voiceChatLists[j].name));
                        voiceChatLists[j].index = i;
                        break;
                    }
                }
                cg_currentVoiceChatList = &voiceFiles[i];
                return;
            }
        }
        if (searchGender == 0) {
            break;
        }
        searchGender = 0;
    }

    // last resort: default to voice file 0, recording into an empty cache slot
    for (i = 0; i < MAX_VOICECHATS; i++) {
        if (!voiceChatLists[i].name[0]) {
            Q_strncpyz(voiceChatLists[i].name, vchatName,
                       sizeof(voiceChatLists[i].name));
            voiceChatLists[i].index = 0;
            break;
        }
    }
    cg_currentVoiceChatList = &voiceFiles[0];
}

/*
=================
CG_PlayVoiceChat

Address: 0x1004aac0

[QL] Play one buffered voice chat.  Gates (Ghidra-verified branch sense):
  - cg.intermissionStarted != 1  (DAT_10a6f8b8) to proceed at all
  - cg_playVoiceChats.integer     (DAT_10a62acc, != 0) plays the sound on CHAN_VOICE
  - cg_showVoiceText.integer      (DAT_10abb2ec, != 0) prints the team-chat text
These QL cvars are positive-sense (enable flags), the opposite polarity to Q3's
cg_noVoiceChats / cg_noVoiceText.
=================
*/
void CG_PlayVoiceChat(bufferedVoiceChat_t* vchat) {
    int orderTask;

    if (cg.intermissionStarted == 1) {
        return;
    }

    if (cg_playVoiceChats.integer) {
        trap_S_StartLocalSound(vchat->snd, CHAN_VOICE);  // syscall +0x9c, channel 3

        if (vchat->clientNum != cg.snap->ps.clientNum) {
            orderTask = CG_ValidOrder(vchat->cmd);
            if (orderTask > 0) {
                cgs.acceptOrderTime = cg.time + 5000;
                Q_strncpyz(cgs.acceptVoice, vchat->cmd, sizeof(cgs.acceptVoice));
                cgs.acceptTask = orderTask;
                cgs.currentOrder = vchat->clientNum;
            }
            Menus_ActivateByName("voiceMenu");
            cg.voiceTime = cg.time;
        }
    }

    if (vchat->voiceOnly == qfalse && cg_showVoiceText.integer) {
        CG_AddToTeamChat(vchat->message);
        CG_Printf("%s\n", vchat->message);
    }

    // mark the current out-slot as consumed
    voiceChatBuffer[cg.voiceChatBufferOut].snd = 0;
}

/*
=================
CG_AddBufferedVoiceChat

Address: 0x1004ac30
=================
*/
void CG_AddBufferedVoiceChat(bufferedVoiceChat_t* vchat) {
    if (cg.intermissionStarted == 1) {
        return;
    }

    memcpy(&voiceChatBuffer[cg.voiceChatBufferIn], vchat, sizeof(bufferedVoiceChat_t));
    cg.voiceChatBufferIn = (cg.voiceChatBufferIn + 1) % MAX_VOICECHATBUFFER;

    if (cg.voiceChatBufferIn == cg.voiceChatBufferOut) {
        CG_PlayVoiceChat(&voiceChatBuffer[cg.voiceChatBufferOut]);
        cg.voiceChatBufferOut++;
    }
}

/*
=================
CG_PlayBufferedVoiceChats

Address: 0x1004abc0
=================
*/
void CG_PlayBufferedVoiceChats(void) {
    if (cg.voiceChatTime < cg.time &&
        cg.voiceChatBufferOut != cg.voiceChatBufferIn &&
        voiceChatBuffer[cg.voiceChatBufferOut].snd != 0) {
        CG_PlayVoiceChat(&voiceChatBuffer[cg.voiceChatBufferOut]);
        cg.voiceChatBufferOut = (cg.voiceChatBufferOut + 1) % MAX_VOICECHATBUFFER;
        cg.voiceChatTime = cg.time + 1000;
    }
}

/*
=================
CG_VoiceChatLocal

Address: 0x1004ac90

[QL] Entry point for an incoming voice chat (driven by entity events via
cg_event.c).  Resolves the client's list, looks up the command, formats the
display line "(name): ^5<chat>" and buffers it.
=================
*/
void CG_VoiceChatLocal(int clientNum, const char* cmd) {
    bufferedVoiceChat_t vchat;
    sfxHandle_t snd;
    const char* chatStr;

    if (cg.intermissionStarted == 1) {
        return;
    }

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        clientNum = 0;
    }
    cgs.currentVoiceClient = clientNum;  // DAT_10a5f2c0

    CG_VoiceChatListForClient(clientNum);

    if (!CG_GetVoiceChat(cmd, &snd, &chatStr)) {
        return;
    }

    vchat.clientNum = clientNum;
    vchat.snd = snd;
    vchat.voiceOnly = qfalse;
    Q_strncpyz(vchat.cmd, cmd, sizeof(vchat.cmd));
    Com_sprintf(vchat.message, sizeof(vchat.message), "(%s): ^5%s",
                cgs.clientinfo[clientNum].name, chatStr);

    CG_AddBufferedVoiceChat(&vchat);
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
    if (cg.warmup == 0 /* && cgs.gametype == GT_DUEL */) {
        trap_S_StartLocalSound(cgs.media.countFightSound, CHAN_ANNOUNCER);
        CG_CenterPrint("FIGHT!", 120, GIANTCHAR_WIDTH * 2);
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
void CG_ParseTeamStats(void) {
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
void CG_ParseAccuracy(void) {
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
CG_DuelScoreToScore

[QL] Project a parsed duel-score record onto the generic score_t slot the
scoreboard/selection code reads. score_t is a cgame-internal layout, so only the
meaningful fields carry across. The binary parses argv into cg.scores[] first and
then remaps into cg.duelScores[]; here the duel record is filled directly and
mirrored back into cg.scores[] for the common scoreboard path.
=================
*/
static void CG_DuelScoreToScore(score_t *sp, const duelScore_t *ds) {
    sp->client = ds->clientNum;
    if (sp->client < 0 || sp->client >= MAX_CLIENTS) {
        sp->client = 0;
    }
    sp->score = ds->score;
    sp->ping = ds->ping;
    sp->time = ds->time;
    sp->frags = ds->kills;
    sp->deaths = ds->deaths;
    sp->accuracy = ds->accuracy;
    sp->bestWeapon = ds->bestWeapon;
    sp->damageDone = ds->damage;
    sp->perfect = ds->perfect;
    sp->impressiveCount = ds->awardImpressive;
    sp->excellentCount = ds->awardExcellent;
    sp->guantletCount = ds->awardHumiliation;
    sp->net = ds->kills - ds->deaths;
    sp->team = cgs.clientinfo[sp->client].team;
}

/*
=================
CG_ParseDuelScores

[QL] Parse duel-specific score data with per-weapon stats.
Binary CG_ParseScores_Duel (0x10045440): argv(1) is the player count and player
records start at argv(2). The binary parses each record into cg.scores[] and then
remaps into cg.duelScores[]; here cg.duelScores[] is filled directly (21 base +
14x5 weapon fields per player) and each is mirrored into cg.scores[]. At
intermission with a single reported duelist, the cached first duelist is
re-injected as an extra scoreboard entry (spectator merge).
=================
*/
void CG_ParseDuelScores(void) {
    int j, idx;
    int player;
    int slot;

    cg.numScores = atoi(CG_Argv(1));
    if (cg.numScores > MAX_CLIENTS) {
        cg.numScores = MAX_CLIENTS;
    }

    memset(cg.scores, 0, sizeof(cg.scores));

    // [QL] the binary keeps cg.duelScores across parses in live play so the first
    // duelist stays cached for the intermission merge below; only demo playback
    // (DAT_10ab8f4c) clears the cache each parse.
    if (cg.demoPlayback) {
        memset(cg.duelScores, 0, sizeof(cg.duelScores));
        cg.duelScores[0].clientNum = -1;
        cg.duelScores[1].clientNum = -1;
    }

    idx = 2;
    slot = 0;
    for (player = 0; player < cg.numScores; player++) {
        duelScore_t *ds;

        // [QL] intermission spectator merge (binary gate: intermissionStarted==1
        // && numScores<2): if the single reported duelist differs from the cached
        // first duelist, re-inject the cached record into the scoreboard and shift
        // the reported player to the next slot so cg.duelScores[0] stays cached.
        if (cg.intermissionStarted == 1 && cg.numScores < 2 &&
            atoi(CG_Argv(2)) != cg.duelScores[0].clientNum) {
            if (slot < MAX_CLIENTS) {
                CG_DuelScoreToScore(&cg.scores[slot], &cg.duelScores[0]);
            }
            slot++;
        }

        // cg.duelScores only holds 2 records; the reported player lands in slot 1
        // when the merge above consumed slot 0.
        ds = &cg.duelScores[slot < 2 ? slot : 1];
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
        // [QL] binary reads exactly 14 (0xe) weapon groups per duel player,
        // NOT MAX_WEAPONS. Reading 16 over-runs into the next player's record.
        for (j = 0; j < 14; j++) {
            if (idx >= trap_Argc()) break;
            ds->weaponStats[j].hits = atoi(CG_Argv(idx++));
            ds->weaponStats[j].atts = atoi(CG_Argv(idx++));
            ds->weaponStats[j].accuracy = atoi(CG_Argv(idx++));
            ds->weaponStats[j].damage = atoi(CG_Argv(idx++));
            ds->weaponStats[j].kills = atoi(CG_Argv(idx++));
        }

        if (slot < MAX_CLIENTS) {
            CG_DuelScoreToScore(&cg.scores[slot], ds);
        }
        slot++;
    }

    cg.duelScoresValid = qtrue;
    CG_SetScoreSelection(NULL);
}

/*
=================
CG_InitScores

[QL] cgamex86.dll: CG_InitScores @ 0x100475f0.  Parser for the A&D
round-overlay scoreboard.  BOTH the "scores_ad" verb and its "adscores"
alias dispatch here (verified in CG_ServerCommand @ 0x1004adc0); this is a
dedicated 22-integer format and must NOT be routed through the 34-field CTF
parser.  Wire:
  scores_ad <s1>..<s20> <redTeamScore> <blueTeamScore>
argv(1..20) fill cg.adScores[0..19] (memset to 0 first), argv(21)/argv(22)
fill cg.teamScores[0] (red) / cg.teamScores[1] (blue).
=================
*/
void CG_InitScores(void) {
    int i;

    memset(cg.adScores, 0, sizeof(cg.adScores));
    cg.teamScores[0] = 0;
    cg.teamScores[1] = 0;

    for (i = 0; i < 20; i++) {
        cg.adScores[i] = atoi(CG_Argv(i + 1));
    }
    cg.teamScores[0] = atoi(CG_Argv(21));
    cg.teamScores[1] = atoi(CG_Argv(22));
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
        // [QL] binary CG_ServerCommand (0x1004b065): "print" also feeds the text
        // to CG_AddChat so server prints land in the chat/notify area.
        CG_AddChat(CG_Argv(1), 0, 0);
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
        // [QL] the server relay prefixes the payload with the sender clientNum as
        // "%02d " (g_cmds.c G_SayTo); skip it, the remainder is the display text.
        {
            const char* p = CG_Argv(1);
            while (*p >= '0' && *p <= '9') { p++; }
            if (*p == ' ') { p++; }
            Q_strncpyz(text, p, MAX_SAY_TEXT);
        }
        CG_RemoveChatEscapeChar(text);
        CG_AddChat(text, 0, 0);
        return;
    }

    // [QL] bchat - broadcast chat with on-screen duration. The binary
    // (CG_ServerCommand 0x1004b255) copies argv(1) verbatim (no clientNum
    // prefix skip, unlike chat/tchat) and passes atoi(argv(2))*1000 as the
    // CG_AddChat duration.
    if (!strcmp(cmd, "bchat")) {
        trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
        Q_strncpyz(text, CG_Argv(1), MAX_SAY_TEXT);
        CG_RemoveChatEscapeChar(text);
        CG_AddChat(text, 0, atoi(CG_Argv(2)) * 1000);
        return;
    }

    // [QL] clearChat - clear chat display
    if (!strcmp(cmd, "clearChat")) {
        memset(cgs.teamChatMsgs, 0, sizeof(cgs.teamChatMsgs));
        cgs.teamChatPos = 0;
        CG_ClearChat();
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
        // [QL] skip the leading "%02d " sender-clientNum prefix added by the relay.
        {
            const char* p = CG_Argv(1);
            while (*p >= '0' && *p <= '9') { p++; }
            if (*p == ' ') { p++; }
            Q_strncpyz(text, p, MAX_SAY_TEXT);
        }
        CG_RemoveChatEscapeChar(text);
        CG_AddToTeamChat(text);
        CG_AddChat(text, 1, 0);
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
    if (!strcmp(cmd, "scores_ctf")) {
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
    // [QL] bare "scores" -> generic 18-field parser (binary 0x1004ba75)
    if (!strcmp(cmd, "scores")) {
        CG_ParseScores();
        return;
    }

    // [QL] per-gametype extended team stats (one command per client; arg1 = slot).
    // Verified: CG_ServerCommand @ 0x1004adc0 routes each verb to a distinct parser.
    if (!strcmp(cmd, "tdmstats")) {
        CG_ParseTeamStats_TDM();
        return;
    }
    if (!strcmp(cmd, "castats")) {
        CG_ParseTeamStats_CA();
        return;
    }
    if (!strcmp(cmd, "ctfstats")) {
        CG_ParseTeamStats_CTF();
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
    // [QL] scores_ad / adscores: dedicated 22-int A&D overlay parser.
    // Verified: CG_ServerCommand @ 0x1004adc0 dispatches BOTH verbs to
    // CG_InitScores @ 0x100475f0 (NOT the 34-field CTF format). Closes the
    // prior mis-route of scores_ad -> CG_ParseScores_Ctf.
    if (!strcmp(cmd, "scores_ad") || !strcmp(cmd, "adscores")) {
        CG_InitScores();
        return;
    }
    if (!strcmp(cmd, "pstats")) {
        return;  // pstats: consumed silently (no cgame handler exists in the binary)
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
        cg.race.nextNextCheckpointEnt = -1;
        cg.race.currentCheckpointEnt = -1;
        return;
    }

    // [QL] complaint system (prefix match). binary CG_ServerCommand (0x1004adc0):
    // gated on !cg.demoPlayback (DAT_10ab8f4c). complaintClient = atoi(argv(1))
    // (DAT_10a5fdc8); complaintEndTime = cg.time + (client < 0 ? 10000 : 15000)
    // (DAT_10a5fdcc; the negative code is a "no complaint available" status).
    if (!Q_stricmpn(cmd, "complaint", 9)) {
        if (!cg.demoPlayback) {
            cg.complaintClient = atoi(CG_Argv(1));
            cg.complaintEndTime = cg.time + (cg.complaintClient < 0 ? 10000 : 15000);
        }
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
