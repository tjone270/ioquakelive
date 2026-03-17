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
// cg_consolecmds.c -- text commands typed in at the local console, or
// executed by a key binding

#include "cg_local.h"
#include "../ui/ui_shared.h"
extern menuDef_t* menuScoreboard;
extern menuDef_t* menuEndScoreboard;

void CG_TargetCommand_f(void) {
    int targetNum;
    char test[4];

    targetNum = CG_CrosshairPlayer();
    if (targetNum == -1) {
        return;
    }

    trap_Argv(1, test, 4);
    trap_SendClientCommand(va("gc %i %i", targetNum, atoi(test)));
}

/*
=================
CG_SizeUp_f

Keybinding command
=================
*/
static void CG_SizeUp_f(void) {
    trap_Cvar_Set("cg_viewsize", va("%i", (int)(cg_viewsize.integer + 10)));
}

/*
=================
CG_SizeDown_f

Keybinding command
=================
*/
static void CG_SizeDown_f(void) {
    trap_Cvar_Set("cg_viewsize", va("%i", (int)(cg_viewsize.integer - 10)));
}

/*
=============
CG_Viewpos_f

Debugging command to print the current position
=============
*/
static void CG_Viewpos_f(void) {
    CG_Printf("(%i %i %i) : %i\n", (int)cg.refdef.vieworg[0],
              (int)cg.refdef.vieworg[1], (int)cg.refdef.vieworg[2],
              (int)cg.refdefViewAngles[YAW]);
}

// [QL] Binary-matched: simplified from Q3 version
static void CG_ScoresDown_f(void) {
    CG_BuildSpectatorString();
    if (cg.scoresRequestTime + 2000 < cg.time) {
        cg.scoresRequestTime = cg.time;
        trap_SendClientCommand("score");
        if (cg.showScores) {
            return;
        }
    }
    cg.showScores = qtrue;
}

// [QL] Binary-matched: no scoreFadeTime
static void CG_ScoresUp_f(void) {
    if (cg.showScores) {
        cg.showScores = qfalse;
    }
}

void Menu_Reset(void);  // FIXME: add to right include file

static void CG_LoadHud_f(void) {
    char buff[1024];
    const char* hudSet;
    memset(buff, 0, sizeof(buff));

    String_Init();
    Menu_Reset();

    trap_Cvar_VariableStringBuffer("cg_hudFiles", buff, sizeof(buff));
    hudSet = buff;
    if (hudSet[0] == '\0') {
        hudSet = "ui/hud.txt";
    }

    CG_LoadMenus(hudSet);

    // [QL] Reload extra menus from pak00 (same list as CG_RegisterGraphics)
    CG_ParseMenu("ui/intro.menu");
    CG_ParseMenu("ui/ingamescorenoteam.menu");
    CG_ParseMenu("ui/ingamescoreteam.menu");
    CG_ParseMenu("ui/endscorenoteam.menu");
    CG_ParseMenu("ui/endscoreteam.menu");
    CG_ParseMenu("ui/spectator.menu");
    CG_ParseMenu("ui/spectator_follow.menu");
    CG_ParseMenu("ui/comp_spectator.menu");
    CG_ParseMenu("ui/comp_spectator_follow.menu");
    CG_ParseMenu("ui/ingamestats.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ffa.menu");
    CG_ParseMenu("ui/ingame_scoreboard_duel.menu");
    CG_ParseMenu("ui/ingame_scoreboard_tdm.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ctf.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ca.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ft.menu");
    CG_ParseMenu("ui/ingame_scoreboard_dom.menu");
    CG_ParseMenu("ui/ingame_scoreboard_ad.menu");
    CG_ParseMenu("ui/ingame_scoreboard_rr.menu");
    CG_ParseMenu("ui/ingame_scoreboard_har.menu");
    CG_ParseMenu("ui/ingame_scoreboard_1fctf.menu");
    CG_ParseMenu("ui/ingame_scoreboard_race.menu");
    CG_ParseMenu("ui/end_scoreboard_ffa.menu");
    CG_ParseMenu("ui/end_scoreboard_duel.menu");
    CG_ParseMenu("ui/end_scoreboard_tdm.menu");
    CG_ParseMenu("ui/end_scoreboard_ctf.menu");
    CG_ParseMenu("ui/end_scoreboard_ca.menu");
    CG_ParseMenu("ui/end_scoreboard_ft.menu");
    CG_ParseMenu("ui/end_scoreboard_dom.menu");
    CG_ParseMenu("ui/end_scoreboard_ad.menu");
    CG_ParseMenu("ui/end_scoreboard_rr.menu");
    CG_ParseMenu("ui/end_scoreboard_har.menu");
    CG_ParseMenu("ui/end_scoreboard_1fctf.menu");
    CG_ParseMenu("ui/end_scoreboard_race.menu");

    menuScoreboard = NULL;
    menuEndScoreboard = NULL;
}

static void CG_scrollScoresDown_f(void) {
    if (menuScoreboard && cg.scoreBoardShowing) {
        Menu_ScrollFeeder(menuScoreboard, FEEDER_SCOREBOARD, qtrue);
        Menu_ScrollFeeder(menuScoreboard, FEEDER_REDTEAM_LIST, qtrue);
        Menu_ScrollFeeder(menuScoreboard, FEEDER_BLUETEAM_LIST, qtrue);
    }
}

static void CG_scrollScoresUp_f(void) {
    if (menuScoreboard && cg.scoreBoardShowing) {
        Menu_ScrollFeeder(menuScoreboard, FEEDER_SCOREBOARD, qfalse);
        Menu_ScrollFeeder(menuScoreboard, FEEDER_REDTEAM_LIST, qfalse);
        Menu_ScrollFeeder(menuScoreboard, FEEDER_BLUETEAM_LIST, qfalse);
    }
}

static void CG_TellTarget_f(void) {
    int clientNum;
    char command[128];
    char message[128];

    clientNum = CG_CrosshairPlayer();
    if (clientNum == -1) {
        return;
    }

    trap_Args(message, 128);
    Com_sprintf(command, 128, "tell %i %s", clientNum, message);
    trap_SendClientCommand(command);
}

static void CG_TellAttacker_f(void) {
    int clientNum;
    char command[128];
    char message[128];

    clientNum = CG_LastAttacker();
    if (clientNum == -1) {
        return;
    }

    trap_Args(message, 128);
    Com_sprintf(command, 128, "tell %i %s", clientNum, message);
    trap_SendClientCommand(command);
}

static void CG_VoiceTellTarget_f(void) {
    int clientNum;
    char command[128];
    char message[128];

    clientNum = CG_CrosshairPlayer();
    if (clientNum == -1) {
        return;
    }

    trap_Args(message, 128);
    Com_sprintf(command, 128, "vtell %i %s", clientNum, message);
    trap_SendClientCommand(command);
}

static void CG_VoiceTellAttacker_f(void) {
    int clientNum;
    char command[128];
    char message[128];

    clientNum = CG_LastAttacker();
    if (clientNum == -1) {
        return;
    }

    trap_Args(message, 128);
    Com_sprintf(command, 128, "vtell %i %s", clientNum, message);
    trap_SendClientCommand(command);
}

static void CG_NextTeamMember_f(void) {
    CG_SelectNextPlayer();
}

static void CG_PrevTeamMember_f(void) {
    CG_SelectPrevPlayer();
}

// ASS U ME's enumeration order as far as task specific orders, OFFENSE is zero, CAMP is last
//
static void CG_NextOrder_f(void) {
    clientInfo_t* ci = cgs.clientinfo + cg.snap->ps.clientNum;
    if (ci) {
        if (!ci->teamLeader && sortedTeamPlayers[cg_currentSelectedPlayer.integer] != cg.snap->ps.clientNum) {
            return;
        }
    }
    if (cgs.currentOrder < TEAMTASK_CAMP) {
        cgs.currentOrder++;

        if (cgs.currentOrder == TEAMTASK_RETRIEVE) {
            if (!CG_OtherTeamHasFlag()) {
                cgs.currentOrder++;
            }
        }

        if (cgs.currentOrder == TEAMTASK_ESCORT) {
            if (!CG_YourTeamHasFlag()) {
                cgs.currentOrder++;
            }
        }

    } else {
        cgs.currentOrder = TEAMTASK_OFFENSE;
    }
    cgs.orderPending = qtrue;
    cgs.orderTime = cg.time + 3000;
}

static void CG_ConfirmOrder_f(void) {
    trap_SendConsoleCommand(va("cmd vtell %d %s\n", cgs.acceptLeader, VOICECHAT_YES));
    trap_SendConsoleCommand("+button5; wait; -button5");
    if (cg.time < cgs.acceptOrderTime) {
        trap_SendClientCommand(va("teamtask %d\n", cgs.acceptTask));
        cgs.acceptOrderTime = 0;
    }
}

static void CG_DenyOrder_f(void) {
    trap_SendConsoleCommand(va("cmd vtell %d %s\n", cgs.acceptLeader, VOICECHAT_NO));
    trap_SendConsoleCommand("+button6; wait; -button6");
    if (cg.time < cgs.acceptOrderTime) {
        cgs.acceptOrderTime = 0;
    }
}

static void CG_TaskOffense_f(void) {
    if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF) {
        trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONGETFLAG));
    } else {
        trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONOFFENSE));
    }
    trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_OFFENSE));
}

static void CG_TaskDefense_f(void) {
    trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONDEFENSE));
    trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_DEFENSE));
}

static void CG_TaskPatrol_f(void) {
    trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONPATROL));
    trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_PATROL));
}

static void CG_TaskCamp_f(void) {
    trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONCAMPING));
    trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_CAMP));
}

static void CG_TaskFollow_f(void) {
    trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONFOLLOW));
    trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_FOLLOW));
}

static void CG_TaskRetrieve_f(void) {
    trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONRETURNFLAG));
    trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_RETRIEVE));
}

static void CG_TaskEscort_f(void) {
    trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_ONFOLLOWCARRIER));
    trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_ESCORT));
}

static void CG_TaskOwnFlag_f(void) {
    trap_SendConsoleCommand(va("cmd vsay_team %s\n", VOICECHAT_IHAVEFLAG));
}

static void CG_TauntKillInsult_f(void) {
    trap_SendConsoleCommand("cmd vsay kill_insult\n");
}

static void CG_TauntPraise_f(void) {
    trap_SendConsoleCommand("cmd vsay praise\n");
}

static void CG_TauntTaunt_f(void) {
    trap_SendConsoleCommand("cmd vtaunt\n");
}

static void CG_TauntDeathInsult_f(void) {
    trap_SendConsoleCommand("cmd vsay death_insult\n");
}

static void CG_TauntGauntlet_f(void) {
    trap_SendConsoleCommand("cmd vsay kill_gauntlet\n");
}

static void CG_TaskSuicide_f(void) {
    int clientNum;
    char command[128];

    clientNum = CG_CrosshairPlayer();
    if (clientNum == -1) {
        return;
    }

    Com_sprintf(command, 128, "tell %i suicide", clientNum);
    trap_SendClientCommand(command);
}

static void CG_StartOrbit_f(void) {
    char var[MAX_TOKEN_CHARS];

    trap_Cvar_VariableStringBuffer("developer", var, sizeof(var));
    if (!atoi(var)) {
        return;
    }
    if (cg_cameraOrbit.value != 0) {
        trap_Cvar_Set("cg_cameraOrbit", "0");
        trap_Cvar_Set("cg_thirdPerson", "0");
    } else {
        trap_Cvar_Set("cg_cameraOrbit", "5");
        trap_Cvar_Set("cg_thirdPerson", "1");
        trap_Cvar_Set("cg_thirdPersonAngle", "0");
        trap_Cvar_Set("cg_thirdPersonRange", "100");
    }
}

// =====================================================================
// [QL] Drop commands
// =====================================================================

static void CG_DropFlag_f(void) {
    if (cgs.gametype != GT_CTF && cgs.gametype != GT_1FCTF && cgs.gametype != GT_HARVESTER) {
        CG_Printf("DropFlag is not available in non-flag gametypes.\n");
        return;
    }
    trap_SendClientCommand("dropflag");
}

static void CG_DropPowerup_f(void) {
    if ((int)cgs.gametype < GT_TEAM) {
        CG_Printf("DropPowerup is not available in non-team gametypes.\n");
        return;
    }
    if (cgs.gametype == GT_CA || cgs.gametype == GT_DOMINATION || cgs.gametype == GT_HARVESTER ||
        cgs.gametype == GT_RACE) {
        CG_Printf("DropPowerup is not available in %s.\n", gametypeDisplayNames[cgs.gametype]);
        return;
    }
    if (cgs.dmflags & DF_INSTAGIB) {
        CG_Printf("DropPowerup is not available in InstaGib.\n");
        return;
    }
    trap_SendClientCommand("droppowerup");
}

static void CG_DropRune_f(void) {
    if (cgs.gametype == GT_RACE) {
        CG_Printf("DropRune not available in %s.\n", gametypeDisplayNames[GT_RACE]);
        return;
    }
    trap_SendClientCommand("droprune");
}

static void CG_DropWeapon_f(void) {
    if ((int)cgs.gametype < GT_TEAM) {
        CG_Printf("DropWeapon is not available in non-team gametypes.\n");
        return;
    }
    if (cgs.gametype == GT_CA || cgs.gametype == GT_DOMINATION || cgs.gametype == GT_HARVESTER ||
        cgs.gametype == GT_RACE) {
        CG_Printf("DropWeapon is not available in %s.\n", gametypeDisplayNames[cgs.gametype]);
        return;
    }
    if (cgs.dmflags & DF_INSTAGIB) {
        CG_Printf("DropWeapon is not available in InstaGib.\n");
        return;
    }
    trap_SendClientCommand("dropweapon");
}

// =====================================================================
// [QL] Readyup, team, forfeit, ragequit
// =====================================================================

static void CG_Readyup_f(void) {
    if ((cg.warmup != 0 && cg.snap != NULL && cg.snap->ps.pm_type != PM_DEAD) ||
        cg.snap->ps.pm_type == PM_INTERMISSION) {
        trap_SendClientCommand("readyup");
    }
}

static void CG_Team_f(void) {
    char message[128];
    char command[128];

    trap_Args(message, 128);
    Com_sprintf(command, 128, "team %s", message);
    trap_SendClientCommand(command);
    CG_CloseMenus();
}

static void CG_Forfeit_f(void) {
    if (cgs.gametype != GT_FFA && cgs.gametype != GT_RACE && cgs.gametype != GT_RR) {
        trap_SendClientCommand("forfeit");
        return;
    }
    if ((int)cgs.gametype < GT_RR + 1) {
        CG_Printf("Forfeit is not available in %s.\n", gametypeDisplayNames[cgs.gametype]);
    } else {
        CG_Printf("Forfeit is not available in %s.\n", "Unknown Gametype");
    }
}

static void CG_Ragequit_f(void) {
    trap_SendClientCommand("ragequit");
    cg.disconnectRequest = 2;
}

// =====================================================================
// [QL] Color commands
// =====================================================================

// [QL] Color wheel: 26 evenly-spaced hue colors for 'a'-'z'.
// Binary has these as BSS arrays populated at runtime.
// g_colorWheel = packed 0xRRGGBBAA, g_colorWheelNormalized = vec3_t RGB [0,1].
unsigned int g_colorWheel[26];
vec3_t g_colorWheelNormalized[26];

void CG_InitColorWheel(void) {
    int i;

    for (i = 0; i < 26; i++) {
        float hue = i * 360.0f / 26.0f;
        float r, g, b;
        float f, q, t;
        int sector;

        sector = (int)(hue / 60.0f);
        f = hue / 60.0f - sector;
        q = 1.0f - f;
        t = f;

        switch (sector) {
        case 0:  r = 1.0f; g = t;    b = 0.0f; break;
        case 1:  r = q;    g = 1.0f; b = 0.0f; break;
        case 2:  r = 0.0f; g = 1.0f; b = t;    break;
        case 3:  r = 0.0f; g = q;    b = 1.0f; break;
        case 4:  r = t;    g = 0.0f; b = 1.0f; break;
        case 5:  r = 1.0f; g = 0.0f; b = q;    break;
        default: r = 1.0f; g = 0.0f; b = 0.0f; break;
        }

        g_colorWheelNormalized[i][0] = r;
        g_colorWheelNormalized[i][1] = g;
        g_colorWheelNormalized[i][2] = b;

        g_colorWheel[i] = ((unsigned int)(r * 255.0f + 0.5f) << 24)
                        | ((unsigned int)(g * 255.0f + 0.5f) << 16)
                        | ((unsigned int)(b * 255.0f + 0.5f) << 8)
                        | 0xFF;
    }
}

static void CG_SetEnemyTeamColor(const char *prefix) {
    char args[128];
    char colorHex[128];
    int len;
    int i;

    trap_Args(args, 128);

    if (args[0] == '\0') {
        char current[128];
        memset(current, 0, sizeof(current));
        trap_Cvar_VariableStringBuffer(va("cg_%scolor", prefix), current, 127);
        CG_Printf("Current %s color: %s\n", prefix, current);
        return;
    }

    len = strlen(args);
    if (len > 3) {
        len = 3;
    }

    // Set the base color cvar
    trap_Cvar_Set(va("cg_%scolor", prefix), args);

    // Set head/upper/lower colors from individual characters
    for (i = 0; i < len; i++) {
        if (args[i] != '\0') {
            int ch = (args[i] >= 'A' && args[i] <= 'Z') ? args[i] + 32 : args[i];
            int idx = ch - 'a';
            if (idx < 0 || idx > 25) {
                idx = 0;
            }

            Com_sprintf(colorHex, sizeof(colorHex), "0x%08x", g_colorWheel[idx]);

            if (i == 0) {
                trap_Cvar_Set(va("cg_%sHeadColor", prefix), colorHex);
                trap_Cvar_Set(va("cg_%sUpperColor", prefix), colorHex);
                trap_Cvar_Set(va("cg_%sLowerColor", prefix), colorHex);
            } else if (i == 1) {
                trap_Cvar_Set(va("cg_%sUpperColor", prefix), colorHex);
                trap_Cvar_Set(va("cg_%sLowerColor", prefix), colorHex);
            } else if (i == 2) {
                trap_Cvar_Set(va("cg_%sHeadColor", prefix), colorHex);
                trap_Cvar_Set(va("cg_%sLowerColor", prefix), colorHex);
            }
        }
    }
}

static void CG_SetTeamColor_f(void) {
    CG_SetEnemyTeamColor("team");
}

static void CG_SetEnemyColor_f(void) {
    CG_SetEnemyTeamColor("enemy");
}

// =====================================================================
// [QL] Chat history toggles
// =====================================================================

static void CG_ChatDown_f(void) {
    if (!cg.chatHistoryShowing) {
        cg.chatHistoryShowing = qtrue;
    }
}

static void CG_ChatUp_f(void) {
    if (cg.chatHistoryShowing) {
        cg.chatHistoryShowing = qfalse;
    }
}

static void CG_ToggleChatHistory_f(void) {
    cg.chatHistoryShowing = !cg.chatHistoryShowing;
}

// =====================================================================
// [QL] Print, kill, clientmute
// =====================================================================

// [QL] Chat system - ring buffer matching binary at 0x10006910
void CG_ClearChat(void) {
    memset(cg.chatLines, 0, sizeof(cg.chatLines));
    memset(&cg.currentChatLine, 0, sizeof(cg.currentChatLine));
    cg.chatIndex = MAX_CHAT_LINES - 1;
}

void CG_AddChat(const char *text, int teamOnly, int extraTime) {
    int len;

    len = strlen(text);

    if (cg.currentChatLine.startTime != 0) {
        // Push current line into ring buffer
        cg.chatIndex = (cg.chatIndex + 1) % MAX_CHAT_LINES;
        memcpy(&cg.chatLines[cg.chatIndex], &cg.currentChatLine, sizeof(cg.currentChatLine));
    }

    cg.currentChatLine.startTime = cg.time;
    cg.currentChatLine.endTime = cg.time + 2000 + extraTime;
    cg.currentChatLine.teamOnly = teamOnly;

    Q_strncpyz(cg.currentChatLine.text, text,
               len < CHAT_LINE_TEXT ? len + 1 : CHAT_LINE_TEXT);

    // Also print to console
    CG_Printf("%s", text);
}

// [QL] Draw chat overlay - binary at 0x10006A10
// chatRect defaults to bottom-left area (x=1, y=350, h=120 from bottom)
void CG_DrawChat(void) {
    int maxLines;
    float y;
    float x;
    float scale;
    int i;

    // Default chat area: bottom-left
    x = 1.0f;
    y = 478.0f;     // near bottom of 480-high screen
    scale = 0.18f;   // default chat text scale

    maxLines = 8;    // default visible history lines

    // Draw the current (newest) chat line if active
    if (cg.currentChatLine.startTime != 0) {
        if (cg.currentChatLine.endTime < cg.time) {
            // Expire: push into ring buffer
            cg.chatIndex = (cg.chatIndex + 1) % MAX_CHAT_LINES;
            memcpy(&cg.chatLines[cg.chatIndex], &cg.currentChatLine,
                   sizeof(cg.currentChatLine));
            memset(&cg.currentChatLine, 0, sizeof(cg.currentChatLine));
        } else {
            CG_SetWidescreen(WIDESCREEN_LEFT);
            CG_DrawText(x, y, 1, scale, colorWhite,
                        cg.currentChatLine.text, 0, 256, 3);
            CG_SetWidescreen(WIDESCREEN_STRETCH);
        }
    }

    // Determine how many history lines to show
    if (cg.snap->ps.pm_type == PM_INTERMISSION) {
        maxLines = 4;
    } else if (cg.showScores) {
        maxLines = 2;
    }

    // Draw chat history lines (when +chat held or scoreboard up)
    if ((cg.chatHistoryShowing || cg.showScores) && maxLines > 0) {
        i = cg.chatIndex;
        do {
            y -= 13.0f;
            if (cg.chatLines[i].text[0]) {
                CG_SetWidescreen(WIDESCREEN_LEFT);
                CG_DrawText(x, y, 1, scale, colorWhite,
                            cg.chatLines[i].text, 0, 256, 3);
                CG_SetWidescreen(WIDESCREEN_STRETCH);
            }
            i--;
            if (i < 0) {
                i = MAX_CHAT_LINES - 1;
            }
            maxLines--;
        } while (maxLines != 0);
    }
}

static void CG_Print_f(void) {
    char text[1024];
    char arg[MAX_TOKEN_CHARS];
    int i;
    int argc;

    text[0] = '\0';

    argc = trap_Argc();
    for (i = 1; i < argc; i++) {
        trap_Argv(i, arg, sizeof(arg));
        if (strlen(text) + strlen(arg) + 2 < sizeof(text)) {
            Q_strcat(text, sizeof(text), arg);
            Q_strcat(text, sizeof(text), " ");
        }
    }
    CG_AddChat(text, 0, 0);
}

static void CG_Kill_f(void) {
    cg.killRequested = qtrue;
    trap_SendClientCommand("kill");
}

// [QL] clientmute - mute/unmute a client's voice by number
// Binary: cgamex86.dll 0x10007E90
static void CG_ClientMute_f(void) {
    int clientNum;
    char arg[MAX_TOKEN_CHARS];

    if (trap_Argc() < 2) {
        CG_Printf("Usage: clientmute <clientnum>\n");
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    clientNum = atoi(arg);

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        CG_Printf("Invalid client number.\n");
        return;
    }

    if (!cgs.clientinfo[clientNum].infoValid) {
        CG_Printf("Client %d not connected.\n", clientNum);
        return;
    }

    trap_S_MuteClient(clientNum, qtrue);
    CG_Printf("Client %d muted.\n", clientNum);
}

// [QL] Binary-matched acc/pstats handlers with spectator check and request throttling
static void CG_AccDown_f(void) {
    if (cg.snap->ps.pm_type != PM_SPECTATOR || (cg.snap->ps.pm_flags & PMF_FOLLOW)) {
        if (cg.accRequestTime + 1000 < cg.time) {
            cg.accRequestTime = cg.time;
            trap_SendClientCommand("acc");
        }
        cg.accShowing = qtrue;
    }
}

static void CG_AccUp_f(void) {
    if (cg.accShowing) {
        cg.accShowing = qfalse;
    }
}

static void CG_PStatsDown_f(void) {
    if (cg.snap->ps.pm_type != PM_SPECTATOR || (cg.snap->ps.pm_flags & PMF_FOLLOW)) {
        if (cg.pstatsRequestTime + 1000 < cg.time) {
            cg.pstatsRequestTime = cg.time;
            trap_SendClientCommand("pstats");
        }
        cg.pstatsShowing = qtrue;
    }
}

static void CG_PStatsUp_f(void) {
    if (cg.pstatsShowing) {
        cg.pstatsShowing = qfalse;
    }
}

typedef struct {
    char* cmd;
    void (*function)(void);
} consoleCommand_t;

// [QL] Command table - matches cgamex86.dll binary at 0x10078DC0 (57 entries)
static consoleCommand_t commands[] = {
    {"viewpos", CG_Viewpos_f},
    {"+scores", CG_ScoresDown_f},
    {"-scores", CG_ScoresUp_f},
    {"+acc", CG_AccDown_f},
    {"-acc", CG_AccUp_f},
    {"+pstats", CG_PStatsDown_f},
    {"-pstats", CG_PStatsUp_f},
    {"nextframe", CG_TestModelNextFrame_f},
    {"prevframe", CG_TestModelPrevFrame_f},
    {"sizeup", CG_SizeUp_f},
    {"sizedown", CG_SizeDown_f},
    {"weapnext", CG_NextWeapon_f},
    {"weapprev", CG_PrevWeapon_f},
    {"weapon", CG_Weapon_f},
    {"tell_target", CG_TellTarget_f},
    {"tell_attacker", CG_TellAttacker_f},
    {"vtell_target", CG_VoiceTellTarget_f},
    {"vtell_attacker", CG_VoiceTellAttacker_f},
    {"tcmd", CG_TargetCommand_f},
    {"loadhud", CG_LoadHud_f},
    {"nextTeamMember", CG_NextTeamMember_f},
    {"prevTeamMember", CG_PrevTeamMember_f},
    {"nextOrder", CG_NextOrder_f},
    {"confirmOrder", CG_ConfirmOrder_f},
    {"denyOrder", CG_DenyOrder_f},
    {"taskOffense", CG_TaskOffense_f},
    {"taskDefense", CG_TaskDefense_f},
    {"taskPatrol", CG_TaskPatrol_f},
    {"taskCamp", CG_TaskCamp_f},
    {"taskFollow", CG_TaskFollow_f},
    {"taskRetrieve", CG_TaskRetrieve_f},
    {"taskEscort", CG_TaskEscort_f},
    {"taskSuicide", CG_TaskSuicide_f},
    {"taskOwnFlag", CG_TaskOwnFlag_f},
    {"tauntKillInsult", CG_TauntKillInsult_f},
    {"tauntPraise", CG_TauntPraise_f},
    {"tauntTaunt", CG_TauntTaunt_f},
    {"tauntDeathInsult", CG_TauntDeathInsult_f},
    {"tauntGauntlet", CG_TauntGauntlet_f},
    {"startOrbit", CG_StartOrbit_f},
    {"loaddeferred", CG_LoadDeferredPlayers},
    // [QL] new commands
    {"dropflag", CG_DropFlag_f},
    {"droppowerup", CG_DropPowerup_f},
    {"droprune", CG_DropRune_f},
    {"dropweapon", CG_DropWeapon_f},
    {"+chat", CG_ChatDown_f},
    {"-chat", CG_ChatUp_f},
    {"readyup", CG_Readyup_f},
    {"team", CG_Team_f},
    {"togglechathistory", CG_ToggleChatHistory_f},
    {"forfeit", CG_Forfeit_f},
    {"ragequit", CG_Ragequit_f},
    {"setteamcolor", CG_SetTeamColor_f},
    {"setenemycolor", CG_SetEnemyColor_f},
    {"print", CG_Print_f},
    {"kill", CG_Kill_f},
    {"clientmute", CG_ClientMute_f},
};

/*
=================
CG_ConsoleCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
qboolean CG_ConsoleCommand(void) {
    const char* cmd;
    int i;

    cmd = CG_Argv(0);

    for (i = 0; i < ARRAY_LEN(commands); i++) {
        if (!Q_stricmp(cmd, commands[i].cmd)) {
            commands[i].function();
            return qtrue;
        }
    }

    return qfalse;
}

/*
=================
CG_InitConsoleCommands

Let the client system know about all of our commands
so it can perform tab completion
=================
*/
void CG_InitConsoleCommands(void) {
    int i;

    for (i = 0; i < ARRAY_LEN(commands); i++) {
        trap_AddCommand(commands[i].cmd);
    }

    // Server-forwarded commands (alphabetical, matches QL binary)
    trap_AddCommand("abort");
    trap_AddCommand("addadmin");
    trap_AddCommand("addbot");
    trap_AddCommand("addmod");
    trap_AddCommand("addscore");
    trap_AddCommand("addteamscore");
    trap_AddCommand("allready");
    trap_AddCommand("ban");
    trap_AddCommand("callvote");
    trap_AddCommand("demote");
    trap_AddCommand("dropflag");
    trap_AddCommand("droppowerup");
    trap_AddCommand("droprune");
    trap_AddCommand("dropweapon");
    trap_AddCommand("follow");
    trap_AddCommand("forfeit");
    trap_AddCommand("give");
    trap_AddCommand("god");
    trap_AddCommand("kill");
    trap_AddCommand("levelshot");
    trap_AddCommand("listaccess");
    trap_AddCommand("loaddeferred");
    trap_AddCommand("lock");
    trap_AddCommand("mute");
    trap_AddCommand("notarget");
    trap_AddCommand("noclip");
    trap_AddCommand("opsay");
    trap_AddCommand("pause");
    trap_AddCommand("players");
    trap_AddCommand("put");
    trap_AddCommand("ragequit");
    trap_AddCommand("rcon");
    trap_AddCommand("reload_access");
    trap_AddCommand("say");
    trap_AddCommand("say_team");
    trap_AddCommand("setmatchtime");
    trap_AddCommand("setviewpos");
    trap_AddCommand("spec");
    trap_AddCommand("team");
    trap_AddCommand("tell");
    trap_AddCommand("tempban");
    trap_AddCommand("timein");
    trap_AddCommand("timeout");
    trap_AddCommand("unban");
    trap_AddCommand("unlock");
    trap_AddCommand("unmute");
    trap_AddCommand("unpause");
    trap_AddCommand("vote");
}
