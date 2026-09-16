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

#include "cg_local.h"
#include "../ui/ui_shared.h"

extern displayContextDef_t cgDC;

// [QL] Current fontIndex for owner-draw text rendering.
// Set by CG_OwnerDraw before dispatching to individual draw functions.
static int cg_currentFontIndex = 0;

// [QL] Owner-draw text wrappers - use CG_DrawText (bulk widescreen + fontIndex)
// instead of CG_Text_Paint (per-character widescreen, scale-based font).
static void CG_OwnerDrawText(float x, float y, float scale, vec4_t color,
                              const char *text, float adjust, int limit, int style) {
    CG_DrawText(x, y, cg_currentFontIndex, scale, color, text, adjust, limit, style);
}

static int CG_OwnerDrawTextWidth(const char *text, float scale, int limit) {
    return CG_DrawTextWidth(text, scale, limit, cg_currentFontIndex);
}

// set in CG_ParseTeamInfo

// static int sortedTeamPlayers[TEAM_MAXOVERLAY];
// static int numSortedTeamPlayers;
int drawTeamOverlayModificationCount = -1;

// static char systemChat[256];
// static char teamChat1[256];
// static char teamChat2[256];

void CG_InitTeamChat(void) {
    memset(teamChat1, 0, sizeof(teamChat1));
    memset(teamChat2, 0, sizeof(teamChat2));
    memset(systemChat, 0, sizeof(systemChat));
}

void CG_SetPrintString(int type, const char* p) {
    if (type == SYSTEM_PRINT) {
        strcpy(systemChat, p);
    } else {
        strcpy(teamChat2, teamChat1);
        strcpy(teamChat1, p);
    }
}

void CG_CheckOrderPending(void) {
    if (cgs.gametype < GT_CTF) {
        return;
    }
    if (cgs.orderPending) {
        // clientInfo_t *ci = cgs.clientinfo + sortedTeamPlayers[cg_currentSelectedPlayer.integer];
        const char *p1, *p2, *b;
        p1 = p2 = b = NULL;
        switch (cgs.currentOrder) {
            case TEAMTASK_OFFENSE:
                p1 = VOICECHAT_ONOFFENSE;
                p2 = VOICECHAT_OFFENSE;
                b = "+button7; wait; -button7";
                break;
            case TEAMTASK_DEFENSE:
                p1 = VOICECHAT_ONDEFENSE;
                p2 = VOICECHAT_DEFEND;
                b = "+button8; wait; -button8";
                break;
            case TEAMTASK_PATROL:
                p1 = VOICECHAT_ONPATROL;
                p2 = VOICECHAT_PATROL;
                b = "+button9; wait; -button9";
                break;
            case TEAMTASK_FOLLOW:
                p1 = VOICECHAT_ONFOLLOW;
                p2 = VOICECHAT_FOLLOWME;
                b = "+button10; wait; -button10";
                break;
            case TEAMTASK_CAMP:
                p1 = VOICECHAT_ONCAMPING;
                p2 = VOICECHAT_CAMP;
                break;
            case TEAMTASK_RETRIEVE:
                p1 = VOICECHAT_ONGETFLAG;
                p2 = VOICECHAT_RETURNFLAG;
                break;
            case TEAMTASK_ESCORT:
                p1 = VOICECHAT_ONFOLLOWCARRIER;
                p2 = VOICECHAT_FOLLOWFLAGCARRIER;
                break;
        }

        if (cg_currentSelectedPlayer.integer == numSortedTeamPlayers) {
            // to everyone
            trap_SendConsoleCommand(va("cmd vsay_team %s\n", p2));
        } else {
            // for the player self
            if (sortedTeamPlayers[cg_currentSelectedPlayer.integer] == cg.snap->ps.clientNum && p1) {
                trap_SendConsoleCommand(va("teamtask %i\n", cgs.currentOrder));
                // trap_SendConsoleCommand(va("cmd say_team %s\n", p2));
                trap_SendConsoleCommand(va("cmd vsay_team %s\n", p1));
            } else if (p2) {
                // trap_SendConsoleCommand(va("cmd say_team %s, %s\n", ci->name,p));
                trap_SendConsoleCommand(va("cmd vtell %d %s\n", sortedTeamPlayers[cg_currentSelectedPlayer.integer], p2));
            }
        }
        if (b) {
            trap_SendConsoleCommand(b);
        }
        cgs.orderPending = qfalse;
    }
}

static void CG_SetSelectedPlayerName(void) {
    if (cg_currentSelectedPlayer.integer >= 0 && cg_currentSelectedPlayer.integer < numSortedTeamPlayers) {
        clientInfo_t* ci = cgs.clientinfo + sortedTeamPlayers[cg_currentSelectedPlayer.integer];
        if (ci) {
            trap_Cvar_Set("cg_selectedPlayerName", ci->name);
            trap_Cvar_Set("cg_selectedPlayer", va("%d", sortedTeamPlayers[cg_currentSelectedPlayer.integer]));
            cgs.currentOrder = ci->teamTask;
        }
    } else {
        trap_Cvar_Set("cg_selectedPlayerName", "Everyone");
    }
}
int CG_GetSelectedPlayer(void) {
    if (cg_currentSelectedPlayer.integer < 0 || cg_currentSelectedPlayer.integer >= numSortedTeamPlayers) {
        cg_currentSelectedPlayer.integer = 0;
    }
    return cg_currentSelectedPlayer.integer;
}

void CG_SelectNextPlayer(void) {
    CG_CheckOrderPending();
    if (cg_currentSelectedPlayer.integer >= 0 && cg_currentSelectedPlayer.integer < numSortedTeamPlayers) {
        cg_currentSelectedPlayer.integer++;
    } else {
        cg_currentSelectedPlayer.integer = 0;
    }
    CG_SetSelectedPlayerName();
}

void CG_SelectPrevPlayer(void) {
    CG_CheckOrderPending();
    if (cg_currentSelectedPlayer.integer > 0 && cg_currentSelectedPlayer.integer <= numSortedTeamPlayers) {
        cg_currentSelectedPlayer.integer--;
    } else {
        cg_currentSelectedPlayer.integer = numSortedTeamPlayers;
    }
    CG_SetSelectedPlayerName();
}

static void CG_DrawPlayerArmorIcon(rectDef_t* rect, qboolean draw2D) {
    vec3_t angles;
    vec3_t origin;

    if (cg_drawStatus.integer == 0) {
        return;
    }

    if (draw2D || (!cg_draw3dIcons.integer && cg_drawIcons.integer)) {
        CG_DrawPic(rect->x, rect->y + rect->h / 2 + 1, rect->w, rect->h, cgs.media.armorIcon);
    } else if (cg_draw3dIcons.integer) {
        VectorClear(angles);
        origin[0] = 90;
        origin[1] = 0;
        origin[2] = -10;
        angles[YAW] = (cg.time & 2047) * 360 / 2048.0f;
        CG_Draw3DModel(rect->x, rect->y, rect->w, rect->h, cgs.media.armorModel, 0, origin, angles);
    }
}

static void CG_DrawPlayerArmorValue(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle, int align) {
    char num[16];
    int value;
    float tx, ty;
    playerState_t* ps;

    ps = &cg.snap->ps;

    value = ps->stats[STAT_ARMOR];

    if (shader) {
        trap_R_SetColor(color);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
        trap_R_SetColor(NULL);
    } else {
        Com_sprintf(num, sizeof(num), "%i", value);
        tx = rect->x;
        if (align == ITEM_ALIGN_CENTER) {
            tx -= CG_OwnerDrawTextWidth(num, scale, 0) * 0.5f;
        } else if (align == ITEM_ALIGN_RIGHT) {
            tx -= CG_OwnerDrawTextWidth(num, scale, 0);
        }
        ty = rect->y + scale * 48.0f - 1.0f;
        CG_OwnerDrawText(tx, ty, scale, color, num, 0, 0, textStyle);
    }
}

static void CG_DrawPlayerAmmoIcon(rectDef_t* rect, qboolean draw2D) {
    centity_t* cent;
    vec3_t angles;
    vec3_t origin;

    cent = &cg_entities[cg.snap->ps.clientNum];

    if (draw2D || (!cg_draw3dIcons.integer && cg_drawIcons.integer)) {
        qhandle_t icon;
        icon = cg_weapons[cg.predictedPlayerState.weapon].ammoIcon;
        if (icon) {
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, icon);
        }
    } else if (cg_draw3dIcons.integer) {
        if (cent->currentState.weapon && cg_weapons[cent->currentState.weapon].ammoModel) {
            VectorClear(angles);
            origin[0] = 70;
            origin[1] = 0;
            origin[2] = 0;
            angles[YAW] = 90 + 20 * sin(cg.time / 1000.0);
            CG_Draw3DModel(rect->x, rect->y, rect->w, rect->h, cg_weapons[cent->currentState.weapon].ammoModel, 0, origin, angles);
        }
    }
}

static void CG_DrawPlayerAmmoValue(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle, int align) {
    char num[16];
    int value;
    float tx, ty;
    int weapon;
    playerState_t* ps;

    ps = &cg.snap->ps;
    weapon = cg_entities[ps->clientNum].currentState.weapon;

    // Weapons with no ammo display (gauntlet, nailgun have no ammo concept)
    if (weapon == WP_NONE || weapon == WP_GAUNTLET || weapon == WP_NAILGUN) {
        return;
    }

    value = ps->ammo[weapon];
    if (value >= 0) {
        if (shader) {
            trap_R_SetColor(color);
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
            trap_R_SetColor(NULL);
        } else {
            Com_sprintf(num, sizeof(num), "%i", value);
            tx = rect->x;
            if (align == ITEM_ALIGN_CENTER) {
                tx -= CG_OwnerDrawTextWidth(num, scale, 0) * 0.5f;
            } else if (align == ITEM_ALIGN_RIGHT) {
                tx -= CG_OwnerDrawTextWidth(num, scale, 0);
            }
            ty = rect->y + scale * 48.0f - 1.0f;
            CG_OwnerDrawText(tx, ty, scale, color, num, 0, 0, textStyle);
        }
    } else if (value == -1) {
        // Infinite ammo - draw infinity symbol icon (square, sized by rect height)
        tx = rect->x;
        if (align == ITEM_ALIGN_CENTER) {
            tx -= rect->h * 0.5f;
        } else if (align == ITEM_ALIGN_RIGHT) {
            tx -= rect->h;
        }
        trap_R_SetColor(color);
        CG_DrawPic(tx, rect->y, rect->h, rect->h, cgs.media.infiniteAmmoShader);
        trap_R_SetColor(NULL);
    }
}

static void CG_DrawPlayerHead(rectDef_t* rect, qboolean draw2D) {
    vec3_t angles;
    float size, stretch;
    float frac;
    float x = rect->x;

    VectorClear(angles);

    if (cg.damageTime && cg.time - cg.damageTime < DAMAGE_TIME) {
        frac = (float)(cg.time - cg.damageTime) / DAMAGE_TIME;
        size = rect->w * 1.25 * (1.5 - frac * 0.5);

        stretch = size - rect->w * 1.25;
        // kick in the direction of damage
        x -= stretch * 0.5 + cg.damageX * stretch * 0.5;

        cg.headStartYaw = 180 + cg.damageX * 45;

        cg.headEndYaw = 180 + 20 * cos(crandom() * M_PI);
        cg.headEndPitch = 5 * cos(crandom() * M_PI);

        cg.headStartTime = cg.time;
        cg.headEndTime = cg.time + 100 + random() * 2000;
    } else {
        if (cg.time >= cg.headEndTime) {
            // select a new head angle
            cg.headStartYaw = cg.headEndYaw;
            cg.headStartPitch = cg.headEndPitch;
            cg.headStartTime = cg.headEndTime;
            cg.headEndTime = cg.time + 100 + random() * 2000;

            cg.headEndYaw = 180 + 20 * cos(crandom() * M_PI);
            cg.headEndPitch = 5 * cos(crandom() * M_PI);
        }
    }

    // if the server was frozen for a while we may have a bad head start time
    if (cg.headStartTime > cg.time) {
        cg.headStartTime = cg.time;
    }

    frac = (cg.time - cg.headStartTime) / (float)(cg.headEndTime - cg.headStartTime);
    frac = frac * frac * (3 - 2 * frac);
    angles[YAW] = cg.headStartYaw + (cg.headEndYaw - cg.headStartYaw) * frac;
    angles[PITCH] = cg.headStartPitch + (cg.headEndPitch - cg.headStartPitch) * frac;

    CG_DrawHead(x, rect->y, rect->w, rect->h, cg.snap->ps.clientNum, angles);
}

static void CG_DrawSelectedPlayerHealth(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    clientInfo_t* ci;
    int value;
    char num[16];

    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        if (shader) {
            trap_R_SetColor(color);
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
            trap_R_SetColor(NULL);
        } else {
            Com_sprintf(num, sizeof(num), "%i", ci->health);
            value = CG_OwnerDrawTextWidth(num, scale, 0);
            CG_OwnerDrawText(rect->x + (rect->w - value) / 2, rect->y, scale, color, num, 0, 0, textStyle);
        }
    }
}

static void CG_DrawSelectedPlayerArmor(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    clientInfo_t* ci;
    int value;
    char num[16];
    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        if (ci->armor > 0) {
            if (shader) {
                trap_R_SetColor(color);
                CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
                trap_R_SetColor(NULL);
            } else {
                Com_sprintf(num, sizeof(num), "%i", ci->armor);
                value = CG_OwnerDrawTextWidth(num, scale, 0);
                CG_OwnerDrawText(rect->x + (rect->w - value) / 2, rect->y, scale, color, num, 0, 0, textStyle);
            }
        }
    }
}

qhandle_t CG_StatusHandle(int task) {
    qhandle_t h;
    switch (task) {
        case TEAMTASK_OFFENSE:
            h = cgs.media.assaultShader;
            break;
        case TEAMTASK_DEFENSE:
            h = cgs.media.defendShader;
            break;
        case TEAMTASK_PATROL:
            h = cgs.media.patrolShader;
            break;
        case TEAMTASK_FOLLOW:
            h = cgs.media.followShader;
            break;
        case TEAMTASK_CAMP:
            h = cgs.media.campShader;
            break;
        case TEAMTASK_RETRIEVE:
            h = cgs.media.retrieveShader;
            break;
        case TEAMTASK_ESCORT:
            h = cgs.media.escortShader;
            break;
        default:
            h = cgs.media.assaultShader;
            break;
    }
    return h;
}

static void CG_DrawSelectedPlayerStatus(rectDef_t* rect) {
    clientInfo_t* ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        qhandle_t h;
        if (cgs.orderPending) {
            // blink the icon
            if (cg.time > cgs.orderTime - 2500 && (cg.time >> 9) & 1) {
                return;
            }
            h = CG_StatusHandle(cgs.currentOrder);
        } else {
            h = CG_StatusHandle(ci->teamTask);
        }
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, h);
    }
}

static void CG_DrawPlayerStatus(rectDef_t* rect) {
    clientInfo_t* ci = &cgs.clientinfo[cg.snap->ps.clientNum];
    if (ci) {
        qhandle_t h = CG_StatusHandle(ci->teamTask);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, h);
    }
}

static void CG_DrawSelectedPlayerName(rectDef_t* rect, float scale, vec4_t color, qboolean voice, int textStyle) {
    clientInfo_t* ci;
    ci = cgs.clientinfo + ((voice) ? cgs.currentVoiceClient : sortedTeamPlayers[CG_GetSelectedPlayer()]);
    if (ci) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, ci->name, 0, 0, textStyle);
    }
}

static void CG_DrawSelectedPlayerLocation(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    clientInfo_t* ci;
    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        const char* p = CG_ConfigString(CS_LOCATIONS + ci->location);
        if (!p || !*p) {
            p = "unknown";
        }
        CG_OwnerDrawText(rect->x, rect->y, scale, color, p, 0, 0, textStyle);
    }
}

static void CG_DrawPlayerLocation(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    clientInfo_t* ci = &cgs.clientinfo[cg.snap->ps.clientNum];
    if (ci) {
        const char* p = CG_ConfigString(CS_LOCATIONS + ci->location);
        if (!p || !*p) {
            p = "unknown";
        }
        CG_OwnerDrawText(rect->x, rect->y, scale, color, p, 0, 0, textStyle);
    }
}

static void CG_DrawSelectedPlayerWeapon(rectDef_t* rect) {
    clientInfo_t* ci;

    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        if (cg_weapons[ci->curWeapon].weaponIcon) {
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cg_weapons[ci->curWeapon].weaponIcon);
        } else {
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.deferShader);
        }
    }
}

static void CG_DrawPlayerScore(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    char num[16];
    int value = cg.snap->ps.persistant[PERS_SCORE];

    if (shader) {
        trap_R_SetColor(color);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
        trap_R_SetColor(NULL);
    } else {
        Com_sprintf(num, sizeof(num), "%i", value);
        value = CG_OwnerDrawTextWidth(num, scale, 0);
        CG_OwnerDrawText(rect->x + (rect->w - value) / 2, rect->y, scale, color, num, 0, 0, textStyle);
    }
}

static void CG_DrawPlayerItem(rectDef_t* rect, float scale, qboolean draw2D) {
    int value;
    vec3_t origin, angles;

    value = cg.snap->ps.stats[STAT_HOLDABLE_ITEM];
    if (value) {
        CG_RegisterItemVisuals(value);

        if (qtrue) {
            CG_RegisterItemVisuals(value);
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cg_items[value].icon);
        } else {
            VectorClear(angles);
            origin[0] = 90;
            origin[1] = 0;
            origin[2] = -10;
            angles[YAW] = (cg.time & 2047) * 360 / 2048.0;
            CG_Draw3DModel(rect->x, rect->y, rect->w, rect->h, cg_items[value].models[0], 0, origin, angles);
        }
    }
}

static void CG_DrawSelectedPlayerPowerup(rectDef_t* rect, qboolean draw2D) {
    clientInfo_t* ci;
    int j;
    float x, y;

    ci = cgs.clientinfo + sortedTeamPlayers[CG_GetSelectedPlayer()];
    if (ci) {
        x = rect->x;
        y = rect->y;

        for (j = 0; j < PW_NUM_POWERUPS; j++) {
            if (ci->powerups & (1 << j)) {
                gitem_t* item;
                item = BG_FindItemForPowerup(j);
                if (item) {
                    CG_DrawPic(x, y, rect->w, rect->h, trap_R_RegisterShader(item->icon));
                    return;
                }
            }
        }
    }
}

static void CG_DrawSelectedPlayerHead(rectDef_t* rect, qboolean draw2D, qboolean voice) {
    clipHandle_t cm;
    clientInfo_t* ci;
    float len;
    vec3_t origin;
    vec3_t mins, maxs, angles;

    ci = cgs.clientinfo + ((voice) ? cgs.currentVoiceClient : sortedTeamPlayers[CG_GetSelectedPlayer()]);

    if (ci) {
        if (cg_draw3dIcons.integer) {
            cm = ci->headModel;
            if (!cm) {
                return;
            }

            // offset the origin y and z to center the head
            trap_R_ModelBounds(cm, mins, maxs);

            origin[2] = -0.5 * (mins[2] + maxs[2]);
            origin[1] = 0.5 * (mins[1] + maxs[1]);

            // calculate distance so the head nearly fills the box
            // assume heads are taller than wide
            len = 0.7 * (maxs[2] - mins[2]);
            origin[0] = len / 0.268;  // len / tan( fov/2 )

            // allow per-model tweaking
            VectorAdd(origin, ci->headOffset, origin);

            angles[PITCH] = 0;
            angles[YAW] = 180;
            angles[ROLL] = 0;

            CG_Draw3DModel(rect->x, rect->y, rect->w, rect->h, ci->headModel, ci->headSkin, origin, angles);
        } else if (cg_drawIcons.integer) {
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, ci->modelIcon);
        }

        // if they are deferred, draw a cross out
        if (ci->deferred) {
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.deferShader);
        }
    }
}

static void CG_DrawPlayerHealth(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle, int align) {
    playerState_t* ps;
    int value;
    float tx, ty;
    char num[16];

    ps = &cg.snap->ps;

    value = ps->stats[STAT_HEALTH];

    if (shader) {
        trap_R_SetColor(color);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
        trap_R_SetColor(NULL);
    } else {
        Com_sprintf(num, sizeof(num), "%i", value);
        tx = rect->x;
        if (align == ITEM_ALIGN_CENTER) {
            tx -= CG_OwnerDrawTextWidth(num, scale, 0) * 0.5f;
        } else if (align == ITEM_ALIGN_RIGHT) {
            tx -= CG_OwnerDrawTextWidth(num, scale, 0);
        }
        ty = rect->y + scale * 48.0f - 1.0f;
        CG_OwnerDrawText(tx, ty, scale, color, num, 0, 0, textStyle);
    }
}

static void CG_DrawRedScore(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    int value;
    char num[16];
    if (cgs.scores1 == SCORE_NOT_PRESENT) {
        Com_sprintf(num, sizeof(num), "-");
    } else {
        Com_sprintf(num, sizeof(num), "%i", cgs.scores1);
    }
    value = CG_OwnerDrawTextWidth(num, scale, 0);
    CG_OwnerDrawText(rect->x + rect->w - value, rect->y, scale, color, num, 0, 0, textStyle);
}

static void CG_DrawBlueScore(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    int value;
    char num[16];

    if (cgs.scores2 == SCORE_NOT_PRESENT) {
        Com_sprintf(num, sizeof(num), "-");
    } else {
        Com_sprintf(num, sizeof(num), "%i", cgs.scores2);
    }
    value = CG_OwnerDrawTextWidth(num, scale, 0);
    CG_OwnerDrawText(rect->x + rect->w - value, rect->y, scale, color, num, 0, 0, textStyle);
}

static void CG_DrawBlueFlagName(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    int i;
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_RED && cgs.clientinfo[i].powerups & (1 << PW_BLUEFLAG)) {
            CG_OwnerDrawText(rect->x, rect->y, scale, color, cgs.clientinfo[i].name, 0, 0, textStyle);
            return;
        }
    }
}

static void CG_DrawBlueFlagHead(rectDef_t* rect) {
    int i;
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_RED && cgs.clientinfo[i].powerups & (1 << PW_BLUEFLAG)) {
            vec3_t angles;
            VectorClear(angles);
            angles[YAW] = 180 + 20 * sin(cg.time / 650.0);
            ;
            CG_DrawHead(rect->x, rect->y, rect->w, rect->h, 0, angles);
            return;
        }
    }
}

static void CG_DrawRedFlagName(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    int i;
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_BLUE && cgs.clientinfo[i].powerups & (1 << PW_REDFLAG)) {
            CG_OwnerDrawText(rect->x, rect->y, scale, color, cgs.clientinfo[i].name, 0, 0, textStyle);
            return;
        }
    }
}

static void CG_DrawRedFlagHead(rectDef_t* rect) {
    int i;
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_BLUE && cgs.clientinfo[i].powerups & (1 << PW_REDFLAG)) {
            vec3_t angles;
            VectorClear(angles);
            angles[YAW] = 180 + 20 * sin(cg.time / 650.0);
            ;
            CG_DrawHead(rect->x, rect->y, rect->w, rect->h, 0, angles);
            return;
        }
    }
}

static void CG_HarvesterSkulls(rectDef_t* rect, float scale, vec4_t color, qboolean force2D, int textStyle) {
    char num[16];
    vec3_t origin, angles;
    qhandle_t handle;
    int value = cg.snap->ps.generic1;

    if (cgs.gametype != GT_HARVESTER) {
        return;
    }

    if (value > 99) {
        value = 99;
    }

    Com_sprintf(num, sizeof(num), "%i", value);
    value = CG_OwnerDrawTextWidth(num, scale, 0);
    CG_OwnerDrawText(rect->x + (rect->w - value), rect->y, scale, color, num, 0, 0, textStyle);

    if (cg_drawIcons.integer) {
        if (!force2D && cg_draw3dIcons.integer) {
            VectorClear(angles);
            origin[0] = 90;
            origin[1] = 0;
            origin[2] = -10;
            angles[YAW] = (cg.time & 2047) * 360 / 2048.0;
            if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
                handle = cgs.media.redCubeModel;
            } else {
                handle = cgs.media.blueCubeModel;
            }
            CG_Draw3DModel(rect->x, rect->y, 35, 35, handle, 0, origin, angles);
        } else {
            if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
                handle = cgs.media.redCubeIcon;
            } else {
                handle = cgs.media.blueCubeIcon;
            }
            CG_DrawPic(rect->x + 3, rect->y + 16, 20, 20, handle);
        }
    }
}

static void CG_OneFlagStatus(rectDef_t* rect) {
    if (cgs.gametype != GT_1FCTF) {
        return;
    } else {
        gitem_t* item = BG_FindItemForPowerup(PW_NEUTRALFLAG);
        if (item) {
            if (cgs.flagStatus >= 0 && cgs.flagStatus <= 4) {
                vec4_t color = {1, 1, 1, 1};
                int index = 0;
                if (cgs.flagStatus == FLAG_TAKEN_RED) {
                    color[1] = color[2] = 0;
                    index = 1;
                } else if (cgs.flagStatus == FLAG_TAKEN_BLUE) {
                    color[0] = color[1] = 0;
                    index = 1;
                } else if (cgs.flagStatus == FLAG_DROPPED) {
                    index = 2;
                }
                trap_R_SetColor(color);
                CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.flagShaders[index]);
            }
        }
    }
}

static void CG_DrawCTFPowerUp(rectDef_t* rect) {
    int value;

    if (cgs.gametype < GT_CTF) {
        return;
    }
    value = cg.snap->ps.stats[STAT_PERSISTANT_POWERUP];
    if (value) {
        CG_RegisterItemVisuals(value);
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cg_items[value].icon);
    }
}

static void CG_DrawTeamColor(rectDef_t* rect, vec4_t color) {
    CG_DrawTeamBackground(rect->x, rect->y, rect->w, rect->h, color[3], cg.snap->ps.persistant[PERS_TEAM]);
}

static void CG_DrawAreaPowerUp(rectDef_t* rect, int align, float special, float scale, vec4_t color) {
    char num[16];
    int sorted[MAX_POWERUPS];
    int sortedTime[MAX_POWERUPS];
    int i, j, k;
    int active;
    playerState_t* ps;
    int t;
    gitem_t* item;
    float f;
    rectDef_t r2;
    float* inc;
    r2.x = rect->x;
    r2.y = rect->y;
    r2.w = rect->w;
    r2.h = rect->h;

    inc = (align == HUD_VERTICAL) ? &r2.y : &r2.x;

    ps = &cg.snap->ps;

    if (ps->stats[STAT_HEALTH] <= 0) {
        return;
    }

    // sort the list by time remaining
    active = 0;
    for (i = 0; i < MAX_POWERUPS; i++) {
        if (!ps->powerups[i]) {
            continue;
        }

        // ZOID--don't draw if the power up has unlimited time
        // This is true of the CTF flags
        if (ps->powerups[i] == INT_MAX) {
            continue;
        }

        t = ps->powerups[i] - cg.time;
        if (t <= 0) {
            continue;
        }

        // insert into the list
        for (j = 0; j < active; j++) {
            if (sortedTime[j] >= t) {
                for (k = active - 1; k >= j; k--) {
                    sorted[k + 1] = sorted[k];
                    sortedTime[k + 1] = sortedTime[k];
                }
                break;
            }
        }
        sorted[j] = i;
        sortedTime[j] = t;
        active++;
    }

    // draw the icons and timers
    for (i = 0; i < active; i++) {
        item = BG_FindItemForPowerup(sorted[i]);

        if (item) {
            t = ps->powerups[sorted[i]];
            if (t - cg.time >= POWERUP_BLINKS * POWERUP_BLINK_TIME) {
                trap_R_SetColor(NULL);
            } else {
                vec4_t modulate;

                f = (float)(t - cg.time) / POWERUP_BLINK_TIME;
                f -= (int)f;
                modulate[0] = modulate[1] = modulate[2] = modulate[3] = f;
                trap_R_SetColor(modulate);
            }

            CG_DrawPic(r2.x, r2.y, r2.w * .75, r2.h, trap_R_RegisterShader(item->icon));

            Com_sprintf(num, sizeof(num), "%i", sortedTime[i] / 1000);
            CG_OwnerDrawText(r2.x + (r2.w * .75) + 3, r2.y + r2.h, scale, color, num, 0, 0, 0);
            *inc += r2.w + special;
        }
    }
    trap_R_SetColor(NULL);
}

float CG_GetValue(int ownerDraw) {
    centity_t* cent;
    playerState_t* ps;

    cent = &cg_entities[cg.snap->ps.clientNum];
    ps = &cg.snap->ps;

    switch (ownerDraw) {
        case CG_PLAYER_ARMOR_VALUE:
            return ps->stats[STAT_ARMOR];
            break;
        case CG_PLAYER_AMMO_VALUE:
            if (cent->currentState.weapon) {
                return ps->ammo[cent->currentState.weapon];
            }
            break;
        case CG_PLAYER_SCORE:
            return cg.snap->ps.persistant[PERS_SCORE];
            break;
        case CG_PLAYER_HEALTH:
            return ps->stats[STAT_HEALTH];
            break;
        case CG_RED_SCORE:
            return cgs.scores1;
            break;
        case CG_BLUE_SCORE:
            return cgs.scores2;
            break;
        default:
            break;
    }
    return -1;
}

qboolean CG_OtherTeamHasFlag(void) {
    if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF) {
        int team = cg.snap->ps.persistant[PERS_TEAM];
        if (cgs.gametype == GT_1FCTF) {
            if (team == TEAM_RED && cgs.flagStatus == FLAG_TAKEN_BLUE) {
                return qtrue;
            } else if (team == TEAM_BLUE && cgs.flagStatus == FLAG_TAKEN_RED) {
                return qtrue;
            } else {
                return qfalse;
            }
        } else {
            if (team == TEAM_RED && cgs.redflag == FLAG_TAKEN) {
                return qtrue;
            } else if (team == TEAM_BLUE && cgs.blueflag == FLAG_TAKEN) {
                return qtrue;
            } else {
                return qfalse;
            }
        }
    }
    return qfalse;
}

qboolean CG_YourTeamHasFlag(void) {
    if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF) {
        int team = cg.snap->ps.persistant[PERS_TEAM];
        if (cgs.gametype == GT_1FCTF) {
            if (team == TEAM_RED && cgs.flagStatus == FLAG_TAKEN_RED) {
                return qtrue;
            } else if (team == TEAM_BLUE && cgs.flagStatus == FLAG_TAKEN_BLUE) {
                return qtrue;
            } else {
                return qfalse;
            }
        } else {
            if (team == TEAM_RED && cgs.blueflag == FLAG_TAKEN) {
                return qtrue;
            } else if (team == TEAM_BLUE && cgs.redflag == FLAG_TAKEN) {
                return qtrue;
            } else {
                return qfalse;
            }
        }
    }
    return qfalse;
}

// THINKABOUTME: should these be exclusive or inclusive..
//
qboolean CG_OwnerDrawVisible(int flags, int flags2) {
    if (flags & CG_SHOW_TEAMINFO) {
        return (cg_currentSelectedPlayer.integer == numSortedTeamPlayers);
    }

    if (flags & CG_SHOW_NOTEAMINFO) {
        return !(cg_currentSelectedPlayer.integer == numSortedTeamPlayers);
    }

    if (flags & CG_SHOW_OTHERTEAMHASFLAG) {
        return CG_OtherTeamHasFlag();
    }

    if (flags & CG_SHOW_YOURTEAMHASENEMYFLAG) {
        return CG_YourTeamHasFlag();
    }

    if (flags & (CG_SHOW_BLUE_TEAM_HAS_REDFLAG | CG_SHOW_RED_TEAM_HAS_BLUEFLAG)) {
        if (flags & CG_SHOW_BLUE_TEAM_HAS_REDFLAG && (cgs.redflag == FLAG_TAKEN || cgs.flagStatus == FLAG_TAKEN_RED)) {
            return qtrue;
        } else if (flags & CG_SHOW_RED_TEAM_HAS_BLUEFLAG && (cgs.blueflag == FLAG_TAKEN || cgs.flagStatus == FLAG_TAKEN_BLUE)) {
            return qtrue;
        }
        return qfalse;
    }

    if (flags & CG_SHOW_ANYTEAMGAME) {
        if (cgs.gametype >= GT_TEAM) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_ANYNONTEAMGAME) {
        if (cgs.gametype < GT_TEAM) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_HARVESTER) {
        if (cgs.gametype == GT_HARVESTER) {
            return qtrue;
        } else {
            return qfalse;
        }
    }

    if (flags & CG_SHOW_ONEFLAG) {
        if (cgs.gametype == GT_1FCTF) {
            return qtrue;
        } else {
            return qfalse;
        }
    }

    if (flags & CG_SHOW_CTF) {
        if (cgs.gametype == GT_CTF) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_OBELISK) {
        if (cgs.gametype == GT_OBELISK) {
            return qtrue;
        } else {
            return qfalse;
        }
    }

    if (flags & CG_SHOW_HEALTHCRITICAL) {
        if (cg.snap->ps.stats[STAT_HEALTH] < 25) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_IF_PLAYER_HAS_FLAG) {
        if (cg.snap->ps.powerups[PW_REDFLAG] || cg.snap->ps.powerups[PW_BLUEFLAG] || cg.snap->ps.powerups[PW_NEUTRALFLAG]) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_IF_CHAT_VISIBLE) {
        return qtrue;  // chat area always available on scoreboard
    }

    // [QL] Message presence indicator (for chat balloon HUD icon)
    if (flags & CG_SHOW_IF_MSG_PRESENT) {
        if (cgs.teamChatMsgTimes[0] > 0 && cg.time - cgs.teamChatMsgTimes[0] < 8000) {
            return qtrue;
        }
        return qfalse;
    }

    if (flags & CG_SHOW_INTERMISSION) {
        if (cg.snap->ps.pm_type == PM_INTERMISSION) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_NOTINTERMISSION) {
        if (cg.snap->ps.pm_type != PM_INTERMISSION) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_IF_NOT_WARMUP) {
        if (!cg.warmup) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_IF_WARMUP) {
        if (cg.warmup) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_DUEL) {
        if (cgs.gametype == GT_DUEL) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_CLAN_ARENA) {
        if (cgs.gametype == GT_CA) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_DOMINATION) {
        if (cgs.gametype == GT_DOMINATION) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_IF_BLUE_IS_FIRST_PLACE) {
        if (cgs.scores2 > cgs.scores1) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_IF_RED_IS_FIRST_PLACE) {
        if (cgs.scores1 > cgs.scores2) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_IF_PLYR_IS_FIRST_PLACE) {
        if (cg.snap && cg.snap->ps.persistant[PERS_RANK] == 0) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_IF_PLYR_IS_NOT_FIRST_PLACE) {
        if (cg.snap && cg.snap->ps.persistant[PERS_RANK] != 0) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_IF_PLYR_IS_ON_RED) {
        if (cg.snap && cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_IF_PLYR_IS_ON_BLUE) {
        if (cg.snap && cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
            return qtrue;
        }
    }

    if (flags & CG_SHOW_PLAYERS_REMAINING) {
        // For CA/FT - show if players remain (always true during gameplay)
        if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE) {
            return qtrue;
        }
    }

    // [QL] ownerDrawFlags2 - used by intro.menu for loadout visibility
    if (flags2) {
        if (flags2 & CG_SHOW_IF_LOADOUT_ENABLED) {
            if (cg_loadout.integer != 0) {
                return qtrue;
            }
            return qfalse;
        }
        if (flags2 & CG_SHOW_IF_LOADOUT_DISABLED) {
            if (cg_loadout.integer == 0) {
                return qtrue;
            }
            return qfalse;
        }
    }

    return qfalse;
}

static void CG_DrawPlayerHasFlag(rectDef_t* rect, qboolean force2D) {
    int adj = (force2D) ? 0 : 2;
    if (cg.predictedPlayerState.powerups[PW_REDFLAG]) {
        CG_DrawFlagModel(rect->x + adj, rect->y + adj, rect->w - adj, rect->h - adj, TEAM_RED, force2D);
    } else if (cg.predictedPlayerState.powerups[PW_BLUEFLAG]) {
        CG_DrawFlagModel(rect->x + adj, rect->y + adj, rect->w - adj, rect->h - adj, TEAM_BLUE, force2D);
    } else if (cg.predictedPlayerState.powerups[PW_NEUTRALFLAG]) {
        CG_DrawFlagModel(rect->x + adj, rect->y + adj, rect->w - adj, rect->h - adj, TEAM_FREE, force2D);
    }
}

static void CG_DrawAreaSystemChat(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, systemChat, 0, 0, 0);
}

static void CG_DrawAreaTeamChat(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, teamChat1, 0, 0, 0);
}

const char* CG_GetKillerText(void) {
    const char* s = "";
    if (cg.killerName[0]) {
        s = va("Fragged by %s", cg.killerName);
    }
    return s;
}

static void CG_DrawKiller(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    // fragged by ... line
    if (cg.killerName[0]) {
        int x = rect->x + rect->w / 2;
        CG_OwnerDrawText(x - (CG_OwnerDrawTextWidth(CG_GetKillerText(), scale, 0) / 2), rect->y, scale, color, CG_GetKillerText(), 0, 0, textStyle);
    }
}

static void CG_DrawCapFragLimit(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    const char* s = va("%2i", ((cgs.gametype >= GT_CTF) ? cgs.capturelimit : cgs.fraglimit));
    CG_OwnerDrawText(rect->x - (CG_OwnerDrawTextWidth(s, scale, 0) / 2), rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_Draw1stPlace(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    if (cgs.scores1 != SCORE_NOT_PRESENT) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%2i", cgs.scores1), 0, 0, textStyle);
    }
}

static void CG_Draw2ndPlace(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    if (cgs.scores2 != SCORE_NOT_PRESENT) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%2i", cgs.scores2), 0, 0, textStyle);
    }
}

// [QL] Binary: FUN_10034b30 - game status text for CG_GAME_STATUS ownerDraw
const char* CG_GetGameStatusText(void) {
    const char* s = "";

    // [QL] Race returns empty (binary-verified)
    if (cgs.gametype == GT_RACE) {
        return s;
    }

    // [QL] FFA/Duel/RR - individual placement
    if (cgs.gametype < GT_TEAM || cgs.gametype == GT_RR) {
        if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
            s = va("%s place with %i", CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1), cg.snap->ps.persistant[PERS_SCORE]);
        }
    } else {
        // Team games
        if (cg.teamScores[0] == cg.teamScores[1]) {
            s = va("Teams are tied at %i", cg.teamScores[0]);
        } else if (cg.teamScores[0] >= cg.teamScores[1]) {
            s = va("Red leads Blue, %i to %i", cg.teamScores[0], cg.teamScores[1]);
        } else {
            s = va("Blue leads Red, %i to %i", cg.teamScores[1], cg.teamScores[0]);
        }
    }
    return s;
}

// [QL] Binary: FUN_10034a00 - match status text for CG_MATCH_STATUS ownerDraw
// Returns "MATCH WARMUP/IN PROGRESS/SUMMARY" + score details
const char* CG_GetMatchStatusText(void) {
    const char* prefix;

    if (cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION) {
        prefix = "MATCH SUMMARY";
    } else if (cg.warmup) {
        prefix = "MATCH WARMUP";
    } else {
        prefix = "MATCH IN PROGRESS";
    }

    if (cgs.gametype == GT_RACE) {
        return prefix;
    }

    // FFA/Duel/RR - individual placement
    if (cgs.gametype < GT_TEAM || cgs.gametype == GT_RR) {
        if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
            return va("%s - %s place with %i", prefix,
                CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1),
                cg.snap->ps.persistant[PERS_SCORE]);
        }
        return prefix;
    }

    // Team games
    if (cg.teamScores[0] == cg.teamScores[1]) {
        return va("%s - Teams are tied at %i", prefix, cg.teamScores[0]);
    } else if (cg.teamScores[0] > cg.teamScores[1]) {
        return va("%s - ^1Red^7 leads ^4Blue^7, %i to %i", prefix,
            cg.teamScores[0], cg.teamScores[1]);
    } else {
        return va("%s - ^4Blue^7 leads ^1Red^7, %i to %i", prefix,
            cg.teamScores[1], cg.teamScores[0]);
    }
}

static void CG_DrawGameStatus(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    CG_OwnerDrawText(rect->x, rect->y + rect->h, scale, color, CG_GetGameStatusText(), 0, 0, textStyle);
}

const char* CG_GameTypeString(void) {
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        return gametypeDisplayNames[cgs.gametype];
    }
    return "Unknown Gametype";
}

static void CG_DrawGameType(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, CG_GameTypeString(), 0, 0, textStyle);
}

static void CG_Text_Paint_Limit(float* maxX, float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit) {
    // [QL] draw text truncated to fit within *maxX (a 640-space x bound), report
    // the resulting right edge. The old per-glyph pixel clip is now a proportional
    // character-count fit against the engine glyph atlas, close enough for the
    // owner-draw layout callers that read *maxX.
    int w640;
    float avail = *maxX - x;
    (void)adjust;

    if (!text || !*text)
        return;

    w640 = CG_Text_Width(text, scale, limit);
    if (avail > 0.0f && (float)w640 > avail) {
        int full = (limit > 0) ? limit : (int)strlen(text);
        int fit = (int)((float)full * (avail / (float)w640));
        if (fit < 0)
            fit = 0;
        limit = fit;
        w640 = CG_Text_Width(text, scale, limit);
    }

    CG_Text_Paint(x, y, scale, color, text, 0, limit, 0);
    *maxX = x + (float)w640;
}

#define PIC_WIDTH 12

void CG_DrawNewTeamInfo(rectDef_t* rect, float text_x, float text_y, float scale, vec4_t color, qhandle_t shader) {
    int xx;
    float y;
    int i, j, len, count;
    const char* p;
    vec4_t hcolor;
    float pwidth, lwidth, maxx, leftOver;
    clientInfo_t* ci;
    gitem_t* item;
    qhandle_t h;

    // max player name width
    pwidth = 0;
    count = (numSortedTeamPlayers > 8) ? 8 : numSortedTeamPlayers;
    for (i = 0; i < count; i++) {
        ci = cgs.clientinfo + sortedTeamPlayers[i];
        if (ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
            len = CG_OwnerDrawTextWidth(ci->name, scale, 0);
            if (len > pwidth)
                pwidth = len;
        }
    }

    // max location name width
    lwidth = 0;
    for (i = 1; i < MAX_LOCATIONS; i++) {
        p = CG_ConfigString(CS_LOCATIONS + i);
        if (p && *p) {
            len = CG_OwnerDrawTextWidth(p, scale, 0);
            if (len > lwidth)
                lwidth = len;
        }
    }

    y = rect->y;

    for (i = 0; i < count; i++) {
        ci = cgs.clientinfo + sortedTeamPlayers[i];
        if (ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
            xx = rect->x + 1;
            for (j = 0; j <= PW_NUM_POWERUPS; j++) {
                if (ci->powerups & (1 << j)) {
                    item = BG_FindItemForPowerup(j);

                    if (item) {
                        CG_DrawPic(xx, y, PIC_WIDTH, PIC_WIDTH, trap_R_RegisterShader(item->icon));
                        xx += PIC_WIDTH;
                    }
                }
            }

            // FIXME: max of 3 powerups shown properly
            xx = rect->x + (PIC_WIDTH * 3) + 2;

            CG_GetColorForHealth(ci->health, ci->armor, hcolor);
            trap_R_SetColor(hcolor);
            CG_DrawPic(xx, y + 1, PIC_WIDTH - 2, PIC_WIDTH - 2, cgs.media.heartShader);

            // Com_sprintf (st, sizeof(st), "%3i %3i", ci->health,	ci->armor);
            // CG_OwnerDrawText(xx, y + text_y, scale, hcolor, st, 0, 0);

            // draw weapon icon
            xx += PIC_WIDTH + 1;

            trap_R_SetColor(NULL);
            if (cgs.orderPending) {
                // blink the icon
                if (cg.time > cgs.orderTime - 2500 && (cg.time >> 9) & 1) {
                    h = 0;
                } else {
                    h = CG_StatusHandle(cgs.currentOrder);
                }
            } else {
                h = CG_StatusHandle(ci->teamTask);
            }

            if (h) {
                CG_DrawPic(xx, y, PIC_WIDTH, PIC_WIDTH, h);
            }

            xx += PIC_WIDTH + 1;

            leftOver = rect->w - xx;
            maxx = xx + leftOver / 3;

            CG_Text_Paint_Limit(&maxx, xx, y + text_y, scale, color, ci->name, 0, 0);

            p = CG_ConfigString(CS_LOCATIONS + ci->location);
            if (!p || !*p) {
                p = "unknown";
            }

            xx += leftOver / 3 + 2;
            maxx = rect->w - 4;

            CG_Text_Paint_Limit(&maxx, xx, y + text_y, scale, color, p, 0, 0);
            y += text_y + 2;
            if (y + text_y + 2 > rect->y + rect->h) {
                break;
            }
        }
    }
}

void CG_DrawMedal(int ownerDraw, rectDef_t* rect, float scale, vec4_t color, qhandle_t shader) {
    score_t* score = &cg.scores[cg.selectedScore];
    float value = 0;
    char* text = NULL;
    color[3] = 0.25;

    switch (ownerDraw) {
        case CG_ACCURACY:
            value = score->accuracy;
            break;
        case CG_ASSISTS:
            value = score->assistCount;
            break;
        case CG_DEFEND:
            value = score->defendCount;
            break;
        case CG_EXCELLENT:
            value = score->excellentCount;
            break;
        case CG_IMPRESSIVE:
            value = score->impressiveCount;
            break;
        case CG_PERFECT:
            value = score->perfect;
            break;
        case CG_GAUNTLET:
            value = score->guantletCount;
            break;
        case CG_CAPTURES:
            value = score->captures;
            break;
    }

    if (value > 0) {
        if (ownerDraw != CG_PERFECT) {
            if (ownerDraw == CG_ACCURACY) {
                text = va("%i%%", (int)value);
                if (value > 50) {
                    color[3] = 1.0;
                }
            } else {
                text = va("%i", (int)value);
                color[3] = 1.0;
            }
        } else {
            if (value) {
                color[3] = 1.0;
            }
            text = "Wow";
        }
    }

    trap_R_SetColor(color);
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);

    if (text) {
        color[3] = 1.0;
        CG_OwnerDrawText(rect->x + rect->w + 2, rect->y + rect->h - 4, scale * 1.2f, color, text, 0, 0, 0);
    }
    trap_R_SetColor(NULL);
}

// [QL] scoreboard owner draws

static void CG_DrawMapName(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, CG_ConfigString(CS_MESSAGE), 0, 0, textStyle);
}

static void CG_DrawLevelTimer(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int msec, mins, secs;
    const char *s;

    if (cgs.timelimit > 0) {
        msec = (cgs.timelimit * 60000) - (cg.time - cgs.levelStartTime);
        if (msec < 0) msec = 0;
    } else {
        msec = cg.time - cgs.levelStartTime;
    }

    secs = msec / 1000;
    mins = secs / 60;
    secs %= 60;
    s = va("%d:%02d", mins, secs);
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawPlayerCounts(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    const char *s;
    int count = 0, maxPlayers;
    int i;
    float x = rect->x;

    // [QL] count all valid clients (including spectators, matching binary behavior)
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid) {
            count++;
        }
    }

    maxPlayers = cgs.maxclients;
    if (cgs.gametype != GT_DUEL && cgs.teamsize > 0) {
        maxPlayers = cgs.teamsize;
        if (cgs.gametype >= GT_TEAM) {
            maxPlayers = cgs.teamsize * 2;
            if (cgs.maxclients < cgs.teamsize * 2) {
                maxPlayers = cgs.maxclients;
            }
        }
    }

    s = va("%d/%d Players", count, maxPlayers);
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_OwnerDrawTextWidth(s, scale, 0) / 2;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_OwnerDrawTextWidth(s, scale, 0);
    }
    CG_OwnerDrawText(x, rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawSelectedPlayerTeamColor(rectDef_t *rect) {
    vec4_t teamColor;

    if (cg.selectedScore >= 0 && cg.selectedScore < cg.numScores) {
        switch (cg.scores[cg.selectedScore].team) {
            case TEAM_RED:
                teamColor[0] = 1; teamColor[1] = 0.2f; teamColor[2] = 0.2f; teamColor[3] = 0.3f;
                break;
            case TEAM_BLUE:
                teamColor[0] = 0.2f; teamColor[1] = 0.2f; teamColor[2] = 1; teamColor[3] = 0.3f;
                break;
            default:
                teamColor[0] = 0.5f; teamColor[1] = 0.5f; teamColor[2] = 0.5f; teamColor[3] = 0.3f;
                break;
        }
        CG_FillRect(rect->x, rect->y, rect->w, rect->h, teamColor);
    }
}

static void CG_DrawSelectedPlayerAccuracy(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s;
    if (cg.selectedScore >= 0 && cg.selectedScore < cg.numScores) {
        s = va("%d%%", cg.scores[cg.selectedScore].accuracy);
        CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
    }
}

static void CG_DrawBestWeaponName(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (cg.selectedScore >= 0 && cg.selectedScore < cg.numScores) {
        int bw = cg.scores[cg.selectedScore].bestWeapon;
        if (bw > 0 && bw < WP_NUM_WEAPONS) {
            gitem_t *item = BG_FindItemForWeapon(bw);
            if (item) {
                CG_OwnerDrawText(rect->x, rect->y, scale, color, item->pickup_name, 0, 0, textStyle);
            }
        }
    }
}

// [QL] Game limit display (frag/cap/round/score limit based on gametype)
static void CG_DrawGameLimit(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s;
    if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE || cgs.gametype == GT_RR) {
        s = va("Round Limit: %d", cgs.roundlimit);
    } else if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF || cgs.gametype == GT_HARVESTER) {
        s = va("Cap Limit: %d", cgs.capturelimit);
    } else if (cgs.gametype == GT_DOMINATION) {
        s = va("Score Limit: %d", cgs.capturelimit);
    } else {
        if (cgs.fraglimit) {
            s = va("Frag Limit: %d", cgs.fraglimit);
        } else {
            s = va("Time Limit: %d", cgs.timelimit);
        }
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

// [QL] Match details: "Game State - Gametype - Map"
static void CG_DrawMatchDetails(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *state;
    if (cg.warmup) {
        state = "Warmup";
    } else if (cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION) {
        state = "Match Complete";
    } else {
        state = "In Progress";
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color,
        va("%s - %s - %s", state, CG_GameTypeString(), CG_ConfigString(CS_MESSAGE)), 0, 0, textStyle);
}

// [QL] Match status: "MATCH WARMUP/IN PROGRESS/SUMMARY" + score details (binary-verified)
static void CG_DrawMatchStatus(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, CG_GetMatchStatusText(), 0, 0, textStyle);
}

// [QL] Round number display for CA/FT
static void CG_DrawRound(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE || cgs.gametype == GT_RR) {
        if (cg.warmup) {
            CG_OwnerDrawText(rect->x, rect->y, scale, color, "Warmup", 0, 0, textStyle);
        } else {
            CG_OwnerDrawText(rect->x, rect->y, scale, color,
                va("Round %d", cgs.roundNum ? cgs.roundNum : 1), 0, 0, textStyle);
        }
    }
}

// [QL] Round timer for CA/FT
static void CG_DrawRoundTimer(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (cgs.roundStarted && cgs.roundtimelimit) {
        int roundTime = cgs.roundStartTime;  // [QL] now cached (was atoi'd inline each frame)
        if (roundTime) {
            int elapsed = (cg.time - roundTime) / 1000;
            int remaining = cgs.roundtimelimit - elapsed;
            if (remaining >= 0 && remaining <= 30) {
                CG_OwnerDrawText(rect->x, rect->y, scale, color,
                    va("%d", remaining), 0, 0, textStyle);
            }
        }
    }
}

// [QL] Overtime display
static void CG_DrawOvertime(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (!cg.warmup && cgs.timelimit && cgs.timelimit_overtime) {
        int elapsed = (cg.time - cgs.levelStartTime) / 1000;
        int regulationTime = cgs.timelimit * 60;
        if (elapsed > regulationTime) {
            int otPeriods = (elapsed - regulationTime) / cgs.timelimit_overtime + 1;
            if (otPeriods < 2) {
                CG_OwnerDrawText(rect->x, rect->y, scale, color, "Overtime", 0, 0, textStyle);
            } else {
                CG_OwnerDrawText(rect->x, rect->y, scale, color,
                    va("Overtime x%d", otPeriods), 0, 0, textStyle);
            }
        }
    }
}

// [QL] Local time display (12-hour, right-aligned)
static void CG_DrawLocalTime(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    qtime_t ct;
    int hour12;
    const char *s;
    int tw;

    trap_RealTime(&ct);
    hour12 = ct.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    s = va("%d:%02d %s", hour12, ct.tm_min, ct.tm_hour >= 12 ? "pm" : "am");

    // Right-align: rect->x is the right edge reference
    tw = CG_OwnerDrawTextWidth(s, scale, 0);
    CG_OwnerDrawText(rect->x - tw, rect->y, scale, color, s, 0, 0, textStyle);
}

// [QL] Match state: WARMUP / IN PROGRESS / SUMMARY
static void CG_DrawMatchState(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s;
    if (cg.warmup) {
        s = "MATCH WARMUP";
    } else if (cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION) {
        s = "MATCH SUMMARY";
    } else {
        s = "MATCH IN PROGRESS";
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

// [QL] Match winner display
static void CG_DrawMatchWinner(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s = "";
    if (cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION) {
        if (cgs.gametype >= GT_TEAM) {
            if (cgs.scores1 > cgs.scores2) s = "Red Wins!";
            else if (cgs.scores2 > cgs.scores1) s = "Blue Wins!";
            else s = "Tie Game!";
        } else {
            s = CG_ConfigString(CS_SCORES1PLAYER);
        }
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

static void CG_DrawFollowPlayerName(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (cg.selectedScore >= 0 && cg.selectedScore < cg.numScores) {
        int client = cg.scores[cg.selectedScore].client;
        if (client >= 0 && client < MAX_CLIENTS && cgs.clientinfo[client].infoValid) {
            CG_OwnerDrawText(rect->x, rect->y, scale, color,
                cgs.clientinfo[client].name, 0, 0, textStyle);
        }
    }
}

static void CG_DrawFollow(rectDef_t *rect, float scale, vec4_t color, int textStyle,
                          int align, int ownerDraw) {
    const char *s;
    float x = rect->x;
    float *drawColor = color;
    int clientNum;

    if (!cg.snap) {
        return;
    }
    clientNum = cg.snap->ps.clientNum;
    if (clientNum < 0 || clientNum >= MAX_CLIENTS || !cgs.clientinfo[clientNum].infoValid) {
        return;
    }

    if (cgs.gametype > GT_RACE) {
        drawColor = CG_TeamColor(cgs.clientinfo[clientNum].team);
    }

    s = va("%s%s", (ownerDraw == CG_FOLLOW_PLAYER_NAME) ? "Following - " : "",
           cgs.clientinfo[clientNum].name);

    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(s, scale, 0, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(s, scale, 0, 0);
    }
    CG_OwnerDrawText(x, rect->y, scale, drawColor, s, 0, 0, textStyle);
}

// [QL] Team name display
static void CG_DrawRedName(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color,
        cgs.redTeam[0] ? cgs.redTeam : "RED", 0, 0, textStyle);
}

static void CG_DrawBlueName(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color,
        cgs.blueTeam[0] ? cgs.blueTeam : "BLUE", 0, 0, textStyle);
}

// [QL] Team average ping
static void CG_DrawTeamAvgPing(rectDef_t *rect, float scale, vec4_t color, int textStyle, int team) {
    int i, count = 0, total = 0;
    for (i = 0; i < cg.numScores; i++) {
        if (cg.scores[i].team == team && cg.scores[i].ping > 0) {
            total += cg.scores[i].ping;
            count++;
        }
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color,
        va("%d", count ? total / count : 0), 0, 0, textStyle);
}

// [QL] Configstring-based owner draw (MVP, most damage, most accurate, etc.)
// [QL] 1st/2nd place score (numeric score value)
static void CG_DrawPlaceScore(rectDef_t *rect, float scale, vec4_t color, int textStyle, int place) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color,
        va("%d", place == 1 ? cgs.scores1 : cgs.scores2), 0, 0, textStyle);
}

// [QL] Player end-game score
static void CG_DrawPlayerEndGameScore(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (cg.snap) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color,
            va("%d", cg.snap->ps.persistant[PERS_SCORE]), 0, 0, textStyle);
    }
}

// [QL] Team color for HUD bars (from binary - team-specific tinting)
static const float *CG_HudBarTeamColor(void) {
    static vec4_t colorFree = { 1.0f, 0.8f, 0.2f, 1.0f };
    static vec4_t colorRed  = { 1.0f, 0.2f, 0.1f, 1.0f };
    static vec4_t colorBlue = { 0.2f, 0.4f, 1.0f, 1.0f };
    static vec4_t colorSpec = { 0.75f, 0.75f, 0.75f, 1.0f };
    static vec4_t colorDef  = { 1.0f, 1.0f, 1.0f, 1.0f };

    switch (cg.snap->ps.persistant[PERS_TEAM]) {
    case TEAM_FREE:      return colorFree;
    case TEAM_RED:       return colorRed;
    case TEAM_BLUE:      return colorBlue;
    case TEAM_SPECTATOR: return colorSpec;
    default:             return colorDef;
    }
}

// [QL] Health bar 100: fills LEFT-to-RIGHT, clamped to maxHealth
// Uses texture coordinate cropping (s2 = health/maxHealth) for shaped alpha mask
static void CG_DrawHealthBar100(rectDef_t *rect, qhandle_t shader) {
    float health, maxHealth, scaledWidth, s2;
    float x, y, w, h;

    if (!cg.snap) return;

    maxHealth = (float)cg.snap->ps.stats[STAT_MAX_HEALTH];
    if (maxHealth <= 0) return;
    health = (float)cg.snap->ps.stats[STAT_HEALTH];
    if (maxHealth < health) health = maxHealth;
    if (health <= 0) return;

    scaledWidth = health * (rect->w / maxHealth);
    s2 = scaledWidth / rect->w;

    trap_R_SetColor(CG_HudBarTeamColor());
    x = rect->x; y = rect->y; w = scaledWidth; h = rect->h;
    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, 0, 0, s2, 1.0f, shader);
    trap_R_SetColor(NULL);
}

// [QL] Health bar 200: fills BOTTOM-to-TOP for overhealth (health > maxHealth)
// Height scaled by 0.6875 factor, texture t1 cropped from top
static void CG_DrawHealthBar200(rectDef_t *rect, qhandle_t shader) {
    float maxHealth, overhealth, scaledHeight, t1;
    float x, y, w, h;

    if (!cg.snap) return;

    maxHealth = (float)cg.snap->ps.stats[STAT_MAX_HEALTH];
    if (maxHealth <= 0) return;
    overhealth = (float)(cg.snap->ps.stats[STAT_HEALTH] - cg.snap->ps.stats[STAT_MAX_HEALTH]);
    if (overhealth <= 0) return;
    if (maxHealth < overhealth) overhealth = maxHealth;

    scaledHeight = overhealth * ((rect->h * 0.6875f) / maxHealth);
    t1 = 1.0f - scaledHeight / rect->h;

    trap_R_SetColor(CG_HudBarTeamColor());
    x = rect->x; y = rect->y + rect->h - scaledHeight; w = rect->w; h = scaledHeight;
    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, 0, t1, 1.0f, 1.0f, shader);
    trap_R_SetColor(NULL);
}

// [QL] Armor bar 100: fills RIGHT-to-LEFT, clamped to 100
// Texture s1 cropped from left, position offset to right edge
static void CG_DrawArmorBar100(rectDef_t *rect, qhandle_t shader) {
    float armor, scaledWidth, s1;
    float x, y, w, h;

    if (!cg.snap) return;

    armor = (float)cg.snap->ps.stats[STAT_ARMOR];
    if (armor <= 0) return;
    if (armor > 100.0f) armor = 100.0f;

    scaledWidth = armor * (rect->w / 100.0f);
    s1 = 1.0f - scaledWidth / rect->w;

    trap_R_SetColor(CG_HudBarTeamColor());
    x = rect->x + rect->w - scaledWidth; y = rect->y; w = scaledWidth; h = rect->h;
    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, s1, 0, 1.0f, 1.0f, shader);
    trap_R_SetColor(NULL);
}

// [QL] Armor bar 200: fills BOTTOM-to-TOP for overarmor (armor > 100)
// Height scaled by 0.6875 factor, texture t1 cropped from top
static void CG_DrawArmorBar200(rectDef_t *rect, qhandle_t shader) {
    float overarmor, scaledHeight, t1;
    float x, y, w, h;

    if (!cg.snap) return;

    overarmor = (float)(cg.snap->ps.stats[STAT_ARMOR] - 100);
    if (overarmor <= 0) return;
    if (overarmor > 100.0f) overarmor = 100.0f;

    scaledHeight = overarmor * ((rect->h * 0.6875f) / 100.0f);
    t1 = 1.0f - scaledHeight / rect->h;

    trap_R_SetColor(CG_HudBarTeamColor());
    x = rect->x; y = rect->y + rect->h - scaledHeight; w = rect->w; h = scaledHeight;
    CG_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, 0, t1, 1.0f, 1.0f, shader);
    trap_R_SetColor(NULL);
}

// [QL] Race status and times
static void CG_DrawRaceStatus(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    // [QL binary-verified] CG_DrawRespawnMessage case 0x34 (Address: 0x1002f1f0)
    // Draws at rect->x, rect->y directly - menu alignment handles positioning
    const char *s;

    // Respawn hint, shown while no run is in progress. Binary looks up the key
    // bound to the respawn ('kill') command and prompts to bind it if unbound.
    if (!cg.race.active) {
        int keynum = trap_Key_GetKey("kill");
        const char *hint;
        if (keynum > 0) {
            char keyName[32];
            trap_Key_KeynumToStringBuf(keynum, keyName, sizeof(keyName));
            hint = va("Press %s to respawn.", keyName);
        } else {
            hint = "Bind 'kill' to respawn";
        }
        CG_OwnerDrawText(rect->x + 19.0f, rect->y + 27.0f, 0.176f, colorWhite,
                         hint, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
    }

    if (cg.race.active) {
        s = "CURRENT RUN";
    } else if (cg.race.finishTime) {
        s = "LAST TIME";
    } else {
        return;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, colorYellow, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
}

static void CG_DrawRaceTimes(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    // [QL binary-verified] CG_DrawRespawnMessage case 0x35
    // Draws at rect->x, rect->y directly - menu alignment handles positioning
    int ms;
    const char *s;
    if (cg.race.active && cg.race.startTime) {
        ms = cg.time - cg.race.startTime;
    } else if (cg.race.finishTime) {
        ms = cg.race.finishTime;
    } else {
        return;
    }
    s = va("%d:%02d.%03d", ms / 60000, (ms / 1000) % 60, ms % 1000);
    CG_OwnerDrawText(rect->x, rect->y, scale, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
}

// [QL] Speedometer ownerdraw - text-only display from speed history
// Used by menu system (mode 4 text-only, or any menu ownerdraw referencing CG_SPEEDOMETER)
static void CG_DrawSpeedometer(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int speedInt;
    const char *text;

    if (!cg.snap) return;

    speedInt = (int)cg.speedHistory[cg.speedHistoryIndex];
    text = va("%d", speedInt);
    CG_OwnerDrawText(rect->x, rect->y, scale, color, text, 0, 0, textStyle);
}

// [QL] Team-colorized health display
static void CG_DrawHealthColorized(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    vec4_t hcolor;
    int health;
    if (!cg.snap) return;
    health = cg.snap->ps.stats[STAT_HEALTH];
    if (health > 100) {
        hcolor[0] = 1.0f; hcolor[1] = 1.0f; hcolor[2] = 1.0f; hcolor[3] = 1.0f;
    } else if (health > 50) {
        hcolor[0] = 0.0f; hcolor[1] = 1.0f; hcolor[2] = 0.0f; hcolor[3] = 1.0f;
    } else if (health > 25) {
        hcolor[0] = 1.0f; hcolor[1] = 1.0f; hcolor[2] = 0.0f; hcolor[3] = 1.0f;
    } else {
        hcolor[0] = 1.0f; hcolor[1] = 0.0f; hcolor[2] = 0.0f; hcolor[3] = 1.0f;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, hcolor, va("%d", health), 0, 0, textStyle);
}

// [QL] Team/enemy player count
static void CG_DrawTeamPlyrCount(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int count = 0, i, myTeam;
    if (!cg.snap) return;
    myTeam = cg.snap->ps.persistant[PERS_TEAM];
    for (i = 0; i < cg.numScores; i++) {
        if (cg.scores[i].team == myTeam) count++;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%d", count), 0, 0, textStyle);
}

static void CG_DrawEnemyPlyrCount(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int count = 0, i, myTeam, enemyTeam;
    if (!cg.snap) return;
    myTeam = cg.snap->ps.persistant[PERS_TEAM];
    enemyTeam = (myTeam == TEAM_RED) ? TEAM_BLUE : TEAM_RED;
    for (i = 0; i < cg.numScores; i++) {
        if (cg.scores[i].team == enemyTeam) count++;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%d", count), 0, 0, textStyle);
}

// [QL] Starting weapons display
static void CG_DrawStartingWeapons(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s = CG_ConfigString(CS_STARTING_WEAPONS);
    if (s && s[0]) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
    }
}

// [QL] Server settings display
// [QL] Vote display helpers. All the text drawers position by the item's align
// themselves (centre subtracts half the text width, right the full width).
static void CG_DrawVoteText(rectDef_t *rect, float yOfs, int fontIndex, float scale,
                            vec4_t color, int textStyle, int align, const char *s) {
    float x = rect->x;

    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(s, scale, 0, fontIndex) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(s, scale, 0, fontIndex);
    }
    CG_DrawText(x, rect->y + yOfs, fontIndex, scale, color, s, 0, 0, textStyle);
}

// binary 0x10035790: always draws; an empty slot falls back to the "default" shot
static void CG_DrawVoteMapShot(rectDef_t *rect, int index, qhandle_t shader) {
    const char *info = CG_ConfigString(CS_ROTATIONMAPS);
    const char *mapname;
    char key[8];

    Com_sprintf(key, sizeof(key), "map_%d", index);
    mapname = Info_ValueForKey(info, key);
    if (!mapname || !mapname[0]) {
        mapname = "default";
    }
    CG_DrawPic(rect->x, rect->y, rect->w, rect->h,
               trap_R_RegisterShaderNoMip(va("levelshots/preview/%s", mapname)));
}

// binary 0x100355d0 (was mislabelled as duel player info): the raw map name
static void CG_DrawVoteMap(rectDef_t *rect, int fontIndex, float scale, vec4_t color,
                           int textStyle, int align, int index) {
    const char *info = CG_ConfigString(CS_ROTATIONMAPS);
    const char *mapname;
    char key[8];

    Com_sprintf(key, sizeof(key), "map_%d", index);
    mapname = Info_ValueForKey(info, key);
    if (mapname && mapname[0]) {
        CG_DrawVoteText(rect, 0, fontIndex, scale, color, textStyle, align, mapname);
    }
}

// binary 0x10035660: the display title only, no map-name fallback
static void CG_DrawVoteMapName(rectDef_t *rect, int fontIndex, float scale, vec4_t color,
                               int textStyle, int align, int index) {
    const char *info = CG_ConfigString(CS_ROTATIONMAPS);
    const char *title;
    char key[16];

    Com_sprintf(key, sizeof(key), "title_%d", index);
    title = Info_ValueForKey(info, key);
    if (title && title[0]) {
        CG_DrawVoteText(rect, 0, fontIndex, scale, color, textStyle, align, title);
    }
}

// binary 0x100356f0: gametype label, drawn 8 units above the item rect
static void CG_DrawVoteGameType(rectDef_t *rect, int fontIndex, float scale, vec4_t color,
                                int textStyle, int align, int index) {
    const char *info = CG_ConfigString(CS_ROTATIONMAPS);
    const char *gt;
    char key[8];

    Com_sprintf(key, sizeof(key), "gt_%d", index);
    gt = Info_ValueForKey(info, key);
    if (gt && gt[0]) {
        CG_DrawVoteText(rect, -8, fontIndex, scale, color, textStyle, align, gt);
    }
}

// binary 0x10035920: 20s window from cgs.voteTime, singular form at one second
static void CG_DrawVoteTimer(rectDef_t *rect, int fontIndex, float scale, vec4_t color,
                             int textStyle, int align) {
    const char *s;
    int sec;

    if (!cgs.voteTime) {
        return;
    }
    sec = (cgs.voteTime + 20000 - cg.time) / 1000;
    if (sec < 1) {
        s = "Voting has ended.";
    } else if (sec == 1) {
        s = "Voting ends in 1 second.";
    } else {
        s = va("Voting ends in %d seconds.", sec);
    }
    CG_DrawVoteText(rect, 0, fontIndex, scale, color, textStyle, align, s);
}

// [QL] Timeout count display (binary CG_DrawTimeoutCount 0x10033530): the team's
// remaining timeouts, prefixed "TO:". An empty configstring falls back to the
// server's g_timeoutCount default.
static void CG_DrawTimeoutCount(rectDef_t *rect, float scale, vec4_t color, int textStyle, int team) {
    int count = (team == TEAM_RED) ? cgs.timeoutsRed : cgs.timeoutsBlue;

    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("TO: %d", count), 0, 0, textStyle);
}

// [QL] Duel overlay owner-draws.
// Detailed duel scoreboard, two players side by side. 1st-player slots are ids
// 0x67-0xc0, 2nd-player slots 0xc1-0x11a; the binary picks the player by testing
// the id against the 2nd-player range. Per-player data comes from cg.duelScores[]
// (filled by CG_ParseDuelScores in cg_servercmds.c). Each per-stat draw below is
// a separate function in the binary.

// which duel player an owner-draw id refers to. 2nd player = ids 0xc1..0x11a.
static int CG_DuelPlayerIndex(int ownerDraw) {
    return (ownerDraw >= CG_2ND_PLYR && ownerDraw <= CG_2ND_PLYR_TIER) ? 1 : 0;
}

// binary guard: draw only if the slot has a valid client, or during warmup.
static qboolean CG_DuelStatShown(const duelScore_t *ds) {
    if (!cg.duelScoresValid) {
        return qfalse;
    }
    if (cg.warmup) {
        return qtrue;
    }
    if (ds->clientNum < 0 || ds->clientNum >= MAX_CLIENTS) {
        return qfalse;
    }
    return cgs.clientinfo[ds->clientNum].infoValid;
}

// x offset for the given align; text is measured with the owner-draw font.
static float CG_OwnerDrawAlignX(rectDef_t *rect, const char *s, float scale, int align) {
    float x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_OwnerDrawTextWidth(s, scale, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_OwnerDrawTextWidth(s, scale, 0);
    }
    return x;
}

// [QL] map an owner-draw id to the weapon slot it addresses (binary
// CG_OwnerDrawWeaponIndex). The frags row includes the gauntlet, the other rows
// start at the machinegun. Weapon enum order: G1 MG2 SG3 GL4 RL5 LG6 RG7 PG8 BFG9
// NG11 PL12 CG13 HMG14 (CG/NG/PL slots are 13/11/12, not 11/12/13).
static int CG_OwnerDrawWeaponIndex(int ownerDraw) {
    switch (ownerDraw) {
    case 0x75: case 0x9a: case 0xcf: case 0xf4:
        return 1;
    case 0x76: case 0x82: case 0x8e: case 0x9b: case 0xa7:
    case 0xd0: case 0xdc: case 0xe8: case 0xf5: case 0x101:
        return 2;
    case 0x77: case 0x83: case 0x8f: case 0x9c: case 0xa8:
    case 0xd1: case 0xdd: case 0xe9: case 0xf6: case 0x102:
        return 3;
    case 0x78: case 0x84: case 0x90: case 0x9d: case 0xa9:
    case 0xd2: case 0xde: case 0xea: case 0xf7: case 0x103:
        return 4;
    case 0x79: case 0x85: case 0x91: case 0x9e: case 0xaa:
    case 0xd3: case 0xdf: case 0xeb: case 0xf8: case 0x104:
        return 5;
    case 0x7a: case 0x86: case 0x92: case 0x9f: case 0xab:
    case 0xd4: case 0xe0: case 0xec: case 0xf9: case 0x105:
        return 6;
    case 0x7b: case 0x87: case 0x93: case 0xa0: case 0xac:
    case 0xd5: case 0xe1: case 0xed: case 0xfa: case 0x106:
        return 7;
    case 0x7c: case 0x88: case 0x94: case 0xa1: case 0xad:
    case 0xd6: case 0xe2: case 0xee: case 0xfb: case 0x107:
        return 8;
    case 0x7d: case 0x89: case 0x95: case 0xa2: case 0xae:
    case 0xd7: case 0xe3: case 0xef: case 0xfc: case 0x108:
        return 9;
    case 0x7e: case 0x8a: case 0x96: case 0xa3: case 0xaf:
    case 0xd8: case 0xe4: case 0xf0: case 0xfd: case 0x109:
        return 13;
    case 0x7f: case 0x8b: case 0x97: case 0xa4: case 0xb0:
    case 0xd9: case 0xe5: case 0xf1: case 0xfe: case 0x10a:
        return 11;
    case 0x80: case 0x8c: case 0x98: case 0xa5: case 0xb1:
    case 0xda: case 0xe6: case 0xf2: case 0xff: case 0x10b:
        return 12;
    case 0x81: case 0x8d: case 0x99: case 0xa6: case 0xb2:
    case 0xdb: case 0xe7: case 0xf3: case 0x100: case 0x10c:
        return 14;
    default:
        return 0;
    }
}

// 0x67/0xc1 CG_DrawDuelPlayerName. Binary truncates to fit the plate width; we
// draw the plain client name (with alignment). Skips empty names in warmup.
static void CG_DrawDuelPlayerName(int ownerDraw, rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *name;
    float x;

    if (!cg.duelScoresValid) {
        return;
    }
    if (ds->clientNum < 0 || ds->clientNum >= MAX_CLIENTS) {
        return;
    }
    name = cgs.clientinfo[ds->clientNum].name;
    if (cg.warmup && !name[0]) {
        return;
    }
    x = CG_OwnerDrawAlignX(rect, name, scale, align);
    CG_OwnerDrawText(x, rect->y, scale, color, name, 0, 0, textStyle);
}

// 0x69/0xc3 CG_DrawDuelPlayerScore. Draws the duel score; -9999/-999 draw nothing.
static void CG_DrawDuelPlayerScore(int ownerDraw, rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    int score = cg.duelScores[idx].score;
    const char *s;
    float x;

    if (!cg.duelScoresValid || score == SCORE_NOT_PRESENT || score == -999) {
        return;
    }
    s = va("%i", score);
    x = CG_OwnerDrawAlignX(rect, s, scale, align);
    CG_OwnerDrawText(x, rect->y, scale, color, s, 0, 0, textStyle);
}

// 0x6a/0xc4 CG_DrawDuelPlayerFrags.
static void CG_DrawDuelPlayerFrags(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    if (!CG_DuelStatShown(ds)) return;
    s = va("%i", ds->kills);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

// 0x6b/0xc5 CG_DrawDuelPlayerDeaths.
static void CG_DrawDuelPlayerDeaths(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    if (!CG_DuelStatShown(ds)) return;
    s = va("%i", ds->deaths);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

// 0x6c/0xc6 CG_DrawDuelPlayerDmg.
static void CG_DrawDuelPlayerDmg(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    if (!CG_DuelStatShown(ds)) return;
    s = va("%i", ds->damage);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

// 0x6e/0xc8 CG_DrawDuelPlayerPing. Ping is colour-coded green/yellow/red.
static void CG_DrawDuelPlayerPing(rectDef_t *rect, float scale, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    vec4_t c;
    if (!CG_DuelStatShown(ds)) return;
    if (ds->ping < 41) {
        c[0] = 0; c[1] = 1; c[2] = 0;
    } else if (ds->ping < 81) {
        c[0] = 1; c[1] = 1; c[2] = 0;
    } else {
        c[0] = 1; c[1] = 0; c[2] = 0;
    }
    c[3] = 0.8f;
    s = va("%i", ds->ping);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, c, s, 0, 0, textStyle);
}

// 0x6f/0xc9 CG_DrawDuelPlayerWins. Win count comes from the client info.
static void CG_DrawDuelPlayerWins(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    if (!CG_DuelStatShown(ds)) return;
    if (ds->clientNum < 0 || ds->clientNum >= MAX_CLIENTS) return;
    s = va("%i", cgs.clientinfo[ds->clientNum].wins);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

// 0x70/0xca CG_DrawDuelPlayerAcc.
static void CG_DrawDuelPlayerAcc(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    const char *s;
    if (!CG_DuelStatShown(ds)) return;
    s = va("%i%%", ds->accuracy);
    CG_OwnerDrawText(CG_OwnerDrawAlignX(rect, s, scale, align), rect->y, scale, color, s, 0, 0, textStyle);
}

// 0x71/0xcb CG_DrawDuelPlayerFlag. The per-player flag handle isn't tracked in
// the ioquakelive duel score, so no-op.
static void CG_DrawDuelPlayerFlag(rectDef_t *rect, int ownerDraw) {
    (void)rect; (void)ownerDraw;
}

// 0x72/0xcc CG_DrawDuelPlayerAvatar. Draws the player's head/model icon.
static void CG_DrawDuelPlayerAvatar(rectDef_t *rect, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    int clientNum;
    if (!cg.duelScoresValid) return;
    clientNum = ds->clientNum;
    if (clientNum < 0 || clientNum >= MAX_CLIENTS || !cgs.clientinfo[clientNum].infoValid) return;
    if (cgs.clientinfo[clientNum].modelIcon) {
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.clientinfo[clientNum].modelIcon);
    }
}

// 0x74/0xce CG_DrawDuelHealthArmorBar. The binary reads live health/armor for the
// duel player; we draw a two-part bar from the client info health/armor.
static void CG_DrawDuelHealthArmorBar(rectDef_t *rect, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    clientInfo_t *ci;
    float health, armor, frac;
    vec4_t hcolor;

    if (!cg.duelScoresValid) return;
    if (ds->clientNum < 0 || ds->clientNum >= MAX_CLIENTS) return;
    ci = &cgs.clientinfo[ds->clientNum];
    if (!ci->infoValid) return;

    health = (float)ci->health;
    if (health > 0) {
        if (health > 200) health = 200;
        frac = health / 200.0f;
        CG_GetColorForHealth(ci->health, ci->armor, hcolor);
        trap_R_SetColor(hcolor);
        CG_DrawPic(rect->x, rect->y + 2, rect->w * frac, 8, cgs.media.whiteShader);
        trap_R_SetColor(NULL);
    }
    armor = (float)ci->armor;
    if (armor > 0) {
        vec4_t cyan = { 0, 1, 1, 1 };
        if (armor > 200) armor = 200;
        frac = armor / 200.0f;
        trap_R_SetColor(cyan);
        CG_DrawPic(rect->x, rect->y + 10, rect->w * frac, 4, cgs.media.whiteShader);
        trap_R_SetColor(NULL);
    }
}

// 0x75-0x81/0xcf-0xdb CG_DrawDuelWeaponFrags.
static void CG_DrawDuelWeaponFrags(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    int w = CG_OwnerDrawWeaponIndex(ownerDraw);
    const char *s;
    if (w <= 0 || w >= MAX_WEAPONS) return;
    s = va("%i", cg.duelScores[idx].weaponStats[w].kills);
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

// 0x82-0x8d/0xdc-0xe7 CG_DrawDuelWeaponHits.
static void CG_DrawDuelWeaponHits(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    int w = CG_OwnerDrawWeaponIndex(ownerDraw);
    const char *s;
    if (w <= 0 || w >= MAX_WEAPONS) return;
    s = va("%i", cg.duelScores[idx].weaponStats[w].hits);
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

// 0x8e-0x99/0xe8-0xf3 CG_DrawDuelWeaponShots.
static void CG_DrawDuelWeaponShots(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    int w = CG_OwnerDrawWeaponIndex(ownerDraw);
    const char *s;
    if (w <= 0 || w >= MAX_WEAPONS) return;
    s = va("%i", cg.duelScores[idx].weaponStats[w].atts);
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

// 0x9a-0xa6/0xf4-0x100 CG_DrawDuelWeaponDmg.
static void CG_DrawDuelWeaponDmg(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    int w = CG_OwnerDrawWeaponIndex(ownerDraw);
    const char *s;
    if (w <= 0 || w >= MAX_WEAPONS) return;
    s = va("%i", cg.duelScores[idx].weaponStats[w].damage);
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

// 0xa7-0xb2/0x101-0x10c CG_DrawDuelWeaponAcc. Higher accuracy of the two players
// is highlighted (white).
static void CG_DrawDuelWeaponAcc(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    int w = CG_OwnerDrawWeaponIndex(ownerDraw);
    const char *s;
    vec4_t c;
    if (w <= 0 || w >= MAX_WEAPONS) return;
    Vector4Copy(color, c);
    if (cg.duelScores[idx ^ 1].weaponStats[w].accuracy < cg.duelScores[idx].weaponStats[w].accuracy) {
        c[0] = c[1] = c[2] = 1.0f; c[3] = 0.8f;
    }
    s = va("%i%%", cg.duelScores[idx].weaponStats[w].accuracy);
    CG_OwnerDrawText(rect->x, rect->y, scale, c, s, 0, 0, textStyle);
}

// pickup slot data for CG_DrawDuelPlayerPickups / accuracy: count + avg time.
static void CG_DuelPickupSlot(duelScore_t *ds, int slot, int *count, float *avgTime) {
    switch (slot) {
    case 0: *count = ds->redArmorPickups;    *avgTime = ds->redArmorTime;    break;
    case 1: *count = ds->yellowArmorPickups; *avgTime = ds->yellowArmorTime; break;
    case 2: *count = ds->greenArmorPickups;  *avgTime = ds->greenArmorTime;  break;
    default:*count = ds->megaHealthPickups;  *avgTime = ds->megaHealthTime;  break;
    }
}

// 0xb3-0xb7/0x10d-0x111 CG_DrawDuelPlayerPickups. 0xb3 draws the full RA/YA/GA/MH
// row (icon + count + avg time); the others draw a single count.
static void CG_DrawDuelPlayerPickups(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    int base = (idx == 0) ? CG_1ST_PLYR_PICKUPS : CG_2ND_PLYR_PICKUPS;
    int off = ownerDraw - base;
    int count;
    float avgTime;
    int i;
    float x, y;

    if (off == 0) {
        // full pickup row: count + avg time per slot (RA/YA/GA/MH item icons are
        // not registered in this media set, so only the numbers are drawn).
        x = rect->x;
        y = rect->y;
        for (i = 0; i < 4; i++) {
            CG_DuelPickupSlot(ds, i, &count, &avgTime);
            if (!count) continue;
            CG_OwnerDrawText(x + 15, y + 15, scale, color, va("%i", count), 0, 0, textStyle);
            CG_OwnerDrawText(x + 30, y + 15, scale, color, va("%3.2f", avgTime), 0, 0, textStyle);
            x += 45;
        }
        return;
    }

    if (off < 1 || off > 4) return;
    CG_DuelPickupSlot(ds, off - 1, &count, &avgTime);
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%i", count), 0, 0, textStyle);
}

// 0xb8-0xbb/0x112-0x115 CG_DrawDuelAccuracy. Avg pickup time for RA/YA/GA/MH, only
// shown when the matching pickup count is non-zero.
static void CG_DrawDuelAccuracy(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    int base = (idx == 0) ? CG_1ST_PLYR_AVG_PICKUP_TIME_RA : CG_2ND_PLYR_AVG_PICKUP_TIME_RA;
    int slot = ownerDraw - base;
    int count;
    float avgTime;
    if (slot < 0 || slot > 3) return;
    CG_DuelPickupSlot(ds, slot, &count, &avgTime);
    if (!count) return;
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%3.2f", avgTime), 0, 0, textStyle);
}

// 0xbc-0xbe/0x116-0x118 CG_DrawDuelKDR. Excellent / Impressive / Humiliation counts.
static void CG_DrawDuelKDR(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int idx = CG_DuelPlayerIndex(ownerDraw);
    duelScore_t *ds = &cg.duelScores[idx];
    int base = (idx == 0) ? CG_1ST_PLYR_EXCELLENT : CG_2ND_PLYR_EXCELLENT;
    int off = ownerDraw - base;
    int val;
    if (!CG_DuelStatShown(ds)) return;
    switch (off) {
    case 0: val = ds->awardExcellent; break;    // EXCELLENT
    case 1: val = ds->awardImpressive; break;   // IMPRESSIVE
    case 2: val = ds->awardHumiliation; break;  // HUMILIATION
    default: return;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%i", val), 0, 0, textStyle);
}

// [QL] Team pickup stat columns (binary CG_OwnerDraw_StatColumn1 / StatColumn2).
// Column 1 = pickup counts, column 2 = time-held values. Data comes from
// cg.teamPickups; slots without a tracked field draw nothing.
static void CG_OwnerDraw_StatColumn1(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int val = 0;
    if (!cg.teamPickups.valid) return;
    switch (ownerDraw) {
        case CG_RED_TEAM_PICKUPS_RA:   val = cg.teamPickups.rra; break;
        case CG_RED_TEAM_PICKUPS_YA:   val = cg.teamPickups.rya; break;
        case CG_RED_TEAM_PICKUPS_GA:   val = cg.teamPickups.rga; break;
        case CG_RED_TEAM_PICKUPS_MH:   val = cg.teamPickups.rmh; break;
        case CG_RED_TEAM_PICKUPS_QUAD: val = cg.teamPickups.rquad; break;
        case CG_RED_TEAM_PICKUPS_BS:   val = cg.teamPickups.rbs; break;
        case CG_BLUE_TEAM_PICKUPS_RA:  val = cg.teamPickups.bra; break;
        case CG_BLUE_TEAM_PICKUPS_YA:  val = cg.teamPickups.bya; break;
        case CG_BLUE_TEAM_PICKUPS_GA:  val = cg.teamPickups.bga; break;
        case CG_BLUE_TEAM_PICKUPS_MH:  val = cg.teamPickups.bmh; break;
        case CG_BLUE_TEAM_PICKUPS_QUAD:val = cg.teamPickups.bquad; break;
        case CG_BLUE_TEAM_PICKUPS_BS:  val = cg.teamPickups.bbs; break;
        default: return;  // map-pickups header / flag / medkit / regen / haste / invis: no data
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%i", val), 0, 0, textStyle);
}

static void CG_OwnerDraw_StatColumn2(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int val = 0;
    if (!cg.teamPickups.valid) return;
    switch (ownerDraw) {
        case CG_RED_TEAM_TIMEHELD_QUAD:  val = cg.teamPickups.rquadTime; break;
        case CG_RED_TEAM_TIMEHELD_BS:    val = cg.teamPickups.rbsTime; break;
        case CG_BLUE_TEAM_TIMEHELD_QUAD: val = cg.teamPickups.bquadTime; break;
        case CG_BLUE_TEAM_TIMEHELD_BS:   val = cg.teamPickups.bbsTime; break;
        default: return;  // flag / regen / haste / invis time-held: no data
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%i", val), 0, 0, textStyle);
}

// [QL] Draw gametype icon for the current gametype
static void CG_DrawGameTypeIcon(rectDef_t *rect) {
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        qhandle_t icon = cgs.media.gametypeIcon[cgs.gametype];
        if (icon) {
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, icon);
        }
    }
}

// [QL] Colorize based on player's team
static void CG_DrawTeamColorized(rectDef_t *rect, vec4_t color, qhandle_t shader) {
    vec4_t teamColor;

    if (cgs.gametype >= GT_TEAM) {
        if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED) {
            teamColor[0] = 1.0f; teamColor[1] = 0.2f; teamColor[2] = 0.2f; teamColor[3] = color[3];
        } else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
            teamColor[0] = 0.2f; teamColor[1] = 0.2f; teamColor[2] = 1.0f; teamColor[3] = color[3];
        } else {
            Vector4Copy(color, teamColor);
        }
    } else {
        Vector4Copy(color, teamColor);
    }

    trap_R_SetColor(teamColor);
    if (shader) {
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
    } else {
        CG_FillRect(rect->x, rect->y, rect->w, rect->h, teamColor);
    }
    trap_R_SetColor(NULL);
}

// [QL] Colorize armor icon based on armor tier (green/yellow/red)
static void CG_DrawArmorTieredColorized(rectDef_t *rect, vec4_t color, qhandle_t shader) {
    int armor = cg.snap->ps.stats[STAT_ARMOR];
    vec4_t tierColor;

    if (armor > 100) {
        tierColor[0] = 1.0f; tierColor[1] = 0.2f; tierColor[2] = 0.2f; tierColor[3] = color[3];
    } else if (armor > 50) {
        tierColor[0] = 1.0f; tierColor[1] = 1.0f; tierColor[2] = 0.2f; tierColor[3] = color[3];
    } else {
        tierColor[0] = 0.2f; tierColor[1] = 1.0f; tierColor[2] = 0.2f; tierColor[3] = color[3];
    }

    trap_R_SetColor(tierColor);
    if (shader) {
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
    } else {
        CG_FillRect(rect->x, rect->y, rect->w, rect->h, tierColor);
    }
    trap_R_SetColor(NULL);
}

// [QL] Team colors for obituary names (binary: DAT_10078610-50)
static vec3_t obitTeamColors[] = {
    { 1.0f, 1.0f, 1.0f },       // TEAM_FREE - white
    { 1.0f, 0.5f, 0.5f },       // TEAM_RED
    { 0.5f, 0.75f, 1.0f },      // TEAM_BLUE
    { 0.85f, 0.85f, 0.85f },    // TEAM_SPECTATOR - light grey
    { 1.0f, 0.8f, 0.2f }        // default - orange/gold
};

// [QL] Draw obituary / kill feed (binary: FUN_1002e9b0)
// Renders: [attacker name] [weapon icon] [victim name] per line
// For world/suicide: just [victim name] with skull icon
static void CG_DrawPlayerObit(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int i;
    float y;
    int iconSize;
    int textHeight;

    // Expire old entries (>2000ms)
    for (i = 0; i < cg.obituaryCount; ) {
        if (cg.obituaries[i].active &&
            cg.obituaries[i].time > 0 &&
            cg.time - cg.obituaries[i].time > 2000) {
            // Shift remaining entries down
            int j;
            for (j = i; j < cg.obituaryCount - 1; j++) {
                cg.obituaries[j] = cg.obituaries[j + 1];
            }
            cg.obituaryCount--;
            memset(&cg.obituaries[cg.obituaryCount], 0, sizeof(cg.obituaries[0]));
        } else {
            i++;
        }
    }

    y = (float)(int)rect->y;

    for (i = 0; i < cg.obituaryCount; i++) {
        vec4_t drawColor;
        float alpha;
        int dt;
        int tw;
        float x;
        vec3_t *teamColor;

        if (!cg.obituaries[i].active || cg.obituaries[i].time == 0) {
            continue;
        }

        dt = cg.time - cg.obituaries[i].time;
        if (dt >= 2000) {
            continue;
        }

        // Fade out in last 200ms (binary: alpha = remaining/200.0)
        if (2000 - dt < 200) {
            alpha = (float)(2000 - dt) / 200.0f;
        } else {
            alpha = 1.0f;
        }

        // Compute icon size from text height at this scale
        textHeight = CG_Text_Height("A", scale, 0);
        iconSize = textHeight + 2;

        // Reset text alignment
        trap_R_SetColor(NULL);

        x = rect->x;
        tw = 0;

        if (cg.obituaries[i].hasAttacker) {
            // Draw attacker name with team color
            int team = cg.obituaries[i].attackerTeam;
            if (team < 0 || team > 3) team = 4; // default orange
            teamColor = &obitTeamColors[team];
            drawColor[0] = (*teamColor)[0];
            drawColor[1] = (*teamColor)[1];
            drawColor[2] = (*teamColor)[2];
            drawColor[3] = alpha;

            CG_OwnerDrawText(x, y, scale, drawColor, cg.obituaries[i].attackerName, 0, 0, 0);
            tw = CG_OwnerDrawTextWidth(cg.obituaries[i].attackerName, scale, 0);

            trap_R_SetColor(NULL);
        }

        // Draw weapon/skull icon
        if (cg.obituaries[i].weaponIcon) {
            drawColor[0] = 1.0f;
            drawColor[1] = 1.0f;
            drawColor[2] = 1.0f;
            drawColor[3] = alpha;
            trap_R_SetColor(drawColor);

            CG_DrawPic(x + (float)tw + (float)(iconSize / 2),
                        y - (float)iconSize,
                        (float)iconSize, (float)iconSize,
                        cg.obituaries[i].weaponIcon);

            trap_R_SetColor(NULL);
            tw += iconSize * 2;
        }

        // Draw victim name with team color
        {
            int team = cg.obituaries[i].victimTeam;
            if (team < 0 || team > 3) team = 4;
            teamColor = &obitTeamColors[team];
            drawColor[0] = (*teamColor)[0];
            drawColor[1] = (*teamColor)[1];
            drawColor[2] = (*teamColor)[2];
            drawColor[3] = alpha;

            CG_OwnerDrawText(x + (float)tw, y, scale, drawColor, cg.obituaries[i].victimName, 0, 0, 0);
        }

        y += (float)(iconSize + 2);
    }
}

// [QL] Draw vertical weapon bar (binary-accurate: FUN_10035a10)
// Draws weapon icons vertically. rect->w = icon size (square), rect->h = vertical spacing.
// Skips gauntlet (WP_GAUNTLET) and BFG (WP_BFG). No selection highlight, no ammo counts.
static void CG_DrawWeaponVertical(rectDef_t *rect, vec4_t color) {
    int i;
    int count = 0;

    trap_R_SetColor(color);

    // iterate weapons 2 (machinegun) through WP_NUM_WEAPONS-1, skip BFG
    for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
        qhandle_t icon;

        if (i == WP_BFG) {
            continue;  // binary skips weapon 10 (BFG)
        }

        icon = cg_weapons[i].weaponIcon;
        if (icon == 0) {
            continue;  // skip unregistered weapons
        }

        // draw square icon: wxw size, spaced vertically by rect->h
        CG_DrawPic(rect->x, rect->h * count + rect->y, rect->w, rect->w, icon);
        count++;
    }

    trap_R_SetColor(NULL);
}

// [QL] CG_ACC_VERTICAL (ownerdraw 98): draw per-weapon accuracy percentages vertically
// Binary: FUN_10035b10 - weapons 2..14 skipping the grapple, always drawn (the
// server's "acc" reply seeds every slot), unregistered icons excepted
static void CG_DrawAccuracyVertical(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int i;
    int count = 0;

    trap_R_SetColor(color);

    for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
        if (i == WP_GRAPPLING_HOOK) {
            continue;
        }

        if (cg_weapons[i].weaponIcon == 0) {
            continue;
        }

        CG_OwnerDrawText(rect->x, rect->h * count + rect->y, scale, color,
                         va("%i%%", cg.accuracyStats.accuracy[i]), 0, 0, textStyle);
        count++;
    }

    trap_R_SetColor(NULL);
}

// [QL] Full gametype names, indexed by cgs.gametype (binary table PTR_s_Free_For_All_10079028).
static const char *CG_GameTypeFullName(void) {
    static const char *names[] = {
        "Free For All",       // GT_FFA 0
        "Duel",               // GT_DUEL 1
        "Race",               // GT_RACE 2
        "Team Deathmatch",    // GT_TEAM 3
        "Clan Arena",         // GT_CA 4
        "Capture the Flag",   // GT_CTF 5
        "One Flag CTF",       // GT_1FCTF 6
        "Overload",           // GT_OBELISK 7
        "Harvester",          // GT_HARVESTER 8
        "Freeze Tag",         // GT_FREEZE 9
        "Domination",         // GT_DOMINATION 10
        "Attack and Defend",  // GT_AD 11
        "Red Rover"           // GT_RR 12
    };
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        return names[cgs.gametype];
    }
    return "Unknown Gametype";
}

// [QL] CG_DrawGameTypeMap - "Gametype Fullname - Map"
// Address: 0x100344b0
static void CG_DrawGameTypeMap(rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    const char *s = va("%s - %s", CG_GameTypeFullName(), CG_ConfigString(CS_MESSAGE));
    float x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(s, scale, 0, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(s, scale, 0, 0);
    }
    CG_DrawText(x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

// [QL] CG_DrawWinCondition - gametype-driven win-condition string
// Address: 0x10034280
static void CG_DrawWinCondition(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s;

    if (cgs.gametype == GT_RACE) {
        s = "Fastest race time within the time limit";
    } else if (cgs.timelimit == 0 ||
               (cg.time - cgs.levelStartTime) < cgs.timelimit * 60000) {
        // still within regulation time
        if (cgs.gametype == GT_CTF) {
            // binary: DAT_10a3ff38 is the capture/mercy limit; if unset or neither
            // team has reached it the mercy rule wording is shown.
            if (cgs.capturelimit == 0 ||
                (cgs.scores1 < cgs.capturelimit && cgs.scores2 < cgs.capturelimit)) {
                s = "First to reach the mercy limit";
            } else {
                s = "First to reach the capture limit";
            }
        } else if (cgs.gametype == GT_CA) {
            s = "First to reach the round limit";
        } else if (cgs.gametype == GT_DOMINATION || cgs.gametype == GT_AD) {
            s = "First to reach the score limit";
        } else {
            s = "Highest score at the end of the game";
        }
    } else if (cgs.gametype == GT_CTF) {
        s = "Most flag captures within the time limit";
    } else if (cgs.gametype == GT_CA) {
        s = "Most rounds won within the time limit";
    } else {
        s = "Highest score within the time limit";
    }
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

// [QL] CG_DrawTeamAliveCount - per-team alive count for DOM/AD overlay
// Address: 0x100337a0  (binary guard: gametype == GT_DOMINATION || GT_AD)
static void CG_DrawTeamAliveCount(rectDef_t *rect, int team, float scale, vec4_t color, int textStyle) {
    const char *s = va("%d", cgs.teamAliveCount[team]);
    CG_DrawText(rect->x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

// [QL] CG_DrawPlayerCount2 - team / total player count with gametype-specific wording
// Address: 0x100333c0
static void CG_DrawPlayerCount2(rectDef_t *rect, int team, float scale, vec4_t color, int textStyle, int align) {
    int i, count = 0;
    const char *s;
    float x;

    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == team) {
            count++;
        }
    }

    // team gametypes: TEAM, CA, CTF, 1FCTF, HARVESTER, DOM, AD (binary excludes OBELISK/FREEZE/RR)
    if (cgs.gametype == GT_TEAM || cgs.gametype == GT_CA || cgs.gametype == GT_CTF ||
        cgs.gametype == GT_1FCTF || cgs.gametype == GT_HARVESTER ||
        cgs.gametype == GT_DOMINATION || cgs.gametype == GT_AD) {
        if (cgs.teamsize < 1) {
            s = va("%d", count);
        } else {
            s = va("(%d/%d)", count, cgs.teamsize);
        }
    } else if (cgs.teamsize < 1 || cgs.maxclients < cgs.teamsize * 2) {
        s = va("%d Player%s", count, (count == 1) ? "" : "s");
    } else {
        s = va("%d/%d Players", count, cgs.teamsize);
    }

    x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(s, scale, 0, 0) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(s, scale, 0, 0);
    }
    CG_DrawText(x, rect->y, 0, scale, color, s, 0, 0, textStyle);
}

// [QL] CG_DrawSpectators - paginated spectator name list on the scoreboards
// Address: 0x100351a0.  Draws as many entries as fit the rect (10px gaps) along the
// rect's bottom edge, then advances to the next page every 4000ms and wraps.
static void CG_DrawSpectators(rectDef_t *rect, int fontIndex, float scale, vec4_t color) {
    int i, shown = 0;
    float x, used = 0.0f;
    int count = cg.spectatorCount;

    // count how many names (from the page offset) fit within rect->w
    for (i = cg.spectatorPageOffset; i < count; i++) {
        float w = (float)CG_DrawTextWidth(cg.spectatorNames[i], scale, 0, fontIndex);
        if (rect->w < used + w) {
            break;
        }
        used += w + 10.0f;
        shown++;
    }

    // draw the visible page
    x = rect->x;
    for (i = cg.spectatorPageOffset; i < cg.spectatorPageOffset + shown && i < count; i++) {
        float w = (float)CG_DrawTextWidth(cg.spectatorNames[i], scale, 0, fontIndex);
        CG_DrawText(x, (rect->y + rect->h) - 3.0f, fontIndex, scale, color,
                    cg.spectatorNames[i], 0, 0, 0);
        x += w + 10.0f;
    }

    // advance to the next page on the 4000ms timer
    if (cg.spectatorPageTime < cg.time) {
        cg.spectatorPageOffset += shown;
        cg.spectatorPageTime = cg.time + 4000;
        if (cg.spectatorPageOffset >= count) {
            cg.spectatorPageOffset = 0;
        }
    }
}

// [QL] CG_DrawVotes - map-rotation vote tally for a vote slot
// Address: 0x10035820.  ownerDraw is CG_VOTECOUNT1..3.
static void CG_DrawVotes(rectDef_t *rect, int ownerDraw, int fontIndex, float scale, vec4_t color, int textStyle, int align) {
    const char *info = CG_ConfigString(CS_ROTATIONVOTES);
    const char *count;
    const char *s;
    char key[4];
    float x;

    Com_sprintf(key, sizeof(key), "%d", ownerDraw - CG_VOTECOUNT1);
    count = Info_ValueForKey(info, key);
    if (!count || !count[0]) {
        return;
    }
    s = va("Votes: %s", count);

    x = rect->x;
    if (align == ITEM_ALIGN_CENTER) {
        x -= CG_DrawTextWidth(s, scale, 0, fontIndex) * 0.5f;
    } else if (align == ITEM_ALIGN_RIGHT) {
        x -= CG_DrawTextWidth(s, scale, 0, fontIndex);
    }
    CG_DrawText(x, rect->y, fontIndex, scale, color, s, 0, 0, textStyle);
}

// [QL] CG_DrawFlagStatus - team flag / base status icon
// Address: 0x10030a80.  Uses cgs.media.flagStatusHandles[iconIndex + team*4].
// NOTE: flagStatusHandles is registered in cg_main.c (CG_RegisterGraphics, not owned
// by this area). Until registered the handles are 0 and CG_DrawPic is a no-op.
static void CG_DrawFlagStatus(rectDef_t *rect, int ownerDraw) {
    int team, iconIndex, flagState;
    qboolean isBase;
    gitem_t *item;

    // red for RED_FLAGSTATUS/RED_BASESTATUS, else blue
    team = (ownerDraw == CG_RED_FLAGSTATUS || ownerDraw == CG_RED_BASESTATUS) ? 1 : 2;
    isBase = (ownerDraw == CG_RED_BASESTATUS || ownerDraw == CG_BLUE_BASESTATUS);

    switch (cgs.gametype) {
        case GT_CTF: case GT_1FCTF: case GT_OBELISK:
        case GT_HARVESTER: case GT_DOMINATION: case GT_AD:
            break;
        default:
            return;
    }

    item = BG_FindItemForPowerup(cgs.gametype == GT_1FCTF ? PW_NEUTRALFLAG
                                 : (team == 1 ? PW_REDFLAG : PW_BLUEFLAG));
    if (!item) {
        return;
    }

    trap_R_SetColor(colorWhite);

    flagState = (team == 1) ? cgs.redflag : cgs.blueflag;

    if (cgs.gametype == GT_1FCTF) {
        // neutral-flag icon (index 3); shown only when this team carries it
        if (team == 1) {
            if (cgs.flagStatus != FLAG_TAKEN_RED) { trap_R_SetColor(NULL); return; }
        } else {
            if (cgs.flagStatus != FLAG_TAKEN_BLUE) { trap_R_SetColor(NULL); return; }
        }
        iconIndex = 3;
        team = 0;
    } else if (flagState == FLAG_ATBASE) {
        iconIndex = 0;
    } else if (flagState == FLAG_TAKEN || isBase) {
        iconIndex = 1;
    } else if (flagState == FLAG_DROPPED) {
        iconIndex = 2;
    } else {
        iconIndex = 0;
    }

    CG_DrawPic(rect->x, rect->y, rect->w, rect->h,
               cgs.media.flagStatusHandles[iconIndex + team * 4]);
    trap_R_SetColor(NULL);
}

// [QL] CG_DrawFlagStatusBar - flag-carrier proximity bar (CG_FLAG_STATUS)
// Address: 0x10030240.
// The real body reads six flag-carrier world positions from a server info string
// (Info_ValueForKey + atof) and plots carrier head icons along a horizontal bar by
// normalised distance to each flag. The info-string key names are not recoverable
// from the stripped binary and the qagame emitter that writes them is not yet ported,
// so the carrier plotting can't be reproduced faithfully. The gametype/flag guards
// and the two background half-shaders are kept, carrier plotting is stubbed.
static void CG_DrawFlagStatusBar(rectDef_t *rect) {
    switch (cgs.gametype) {
        case GT_CTF: case GT_OBELISK: case GT_HARVESTER:
        case GT_DOMINATION: case GT_AD:
            break;
        default:
            return;   // binary also excludes GT_1FCTF (case 6)
    }
    if (!cgs.media.flagStatusBarLeft || !cgs.media.flagStatusBarRight) {
        return;
    }
    if (!cgs.redflag && !cgs.blueflag) {
        return;
    }
    // Background halves (carrier icons require the un-ported flag-position feed).
    CG_DrawPic(rect->x - rect->w * 0.5f, rect->y, rect->w * 0.5f, rect->h,
               cgs.media.flagStatusBarLeft);
    CG_DrawPic(rect->x, rect->y, rect->w * 0.5f, rect->h,
               cgs.media.flagStatusBarRight);
}

// [QL] CG_DrawPlayerModel / CG_DrawSelectedPlayerModel
// Addresses: 0x10034980 / 0x10034900.
// These point at CG_Draw3DPlayerModel (cg_players.c, 0x10008c40) which renders a full
// legs+torso+head player model auto-framed inside the box, matching the binary.
static void CG_DrawPlayerModel(rectDef_t *rect) {
    int clientNum = cg.snap ? cg.snap->ps.clientNum : cg.clientNum;

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        return;
    }
    CG_Draw3DPlayerModel(rect->x, rect->y, rect->w, rect->h, clientNum, 0);
}

static void CG_DrawSelectedPlayerModel(rectDef_t *rect) {
    int clientNum = cg.duelPlayer1;

    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        clientNum = cg.snap ? cg.snap->ps.clientNum : cg.clientNum;
    }
    if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
        return;
    }
    CG_Draw3DPlayerModel(rect->x, rect->y, rect->w, rect->h, clientNum, 0);
}

// [QL] CG_DrawPlayerStatusLeft / CG_DrawPlayerStatusRight - duel LEADS/TRAILS/TIED plate
// Addresses: 0x10037ba0 / 0x10037d60.
// Draws a status plate shader (cgs.media.duelStatus*) plus the status word. The plate
// shaders are registered in cg_main.c; until then only the text renders.
static void CG_DrawDuelStatusPlate(rectDef_t *rect, int selfIdx, qboolean rightSide) {
    duelScore_t *self, *other;
    const char *text;
    qhandle_t plate;

    if (!cg.duelScoresValid) {
        return;
    }
    self  = &cg.duelScores[selfIdx];
    other = &cg.duelScores[selfIdx ^ 1];

    if (cg.warmup) {
        text  = "READY";
        plate = rightSide ? cgs.media.duelStatusReady_right : cgs.media.duelStatusReady;
    } else if (other->score < self->score) {
        text  = "LEADS";
        plate = rightSide ? cgs.media.duelStatusLeads_right : cgs.media.duelStatusLeads;
    } else if (self->score < other->score) {
        text  = "TRAILS";
        plate = rightSide ? cgs.media.duelStatusTrails_right : cgs.media.duelStatusTrails;
    } else {
        text  = "TIED";
        plate = rightSide ? cgs.media.duelStatusTied_right : cgs.media.duelStatusTied;
    }

    CG_DrawPic(rect->x, rect->y, rect->w, rect->h, plate);
    CG_DrawText(rect->x + 16.0f, rect->y + 16.0f, 0, 0.16f, colorWhite, text, 0, 0, 3);
}

static void CG_DrawPlayerStatusLeft(rectDef_t *rect) {
    CG_DrawDuelStatusPlate(rect, 0, qfalse);
}

static void CG_DrawPlayerStatusRight(rectDef_t *rect) {
    CG_DrawDuelStatusPlate(rect, 1, qtrue);
}

// [QL] CG_DrawModifiers (id 1, binary 0x1003a1c0). Lists the active server
// modifiers one per line. The bits come from cgs.customSettings (CS 704), the
// same 28-bit map the server accumulates; the quad factor and gravity lines read
// their live values from the serverinfo, and the tiered-armor line comes from
// CS_ARMORINFO. The trailing "MODIFIED WEAPONS" block (weapon-modification bits
// 0x1fff) is intentionally left as the summary line without the per-weapon icon
// grid.
static void CG_DrawModifiers(rectDef_t *rect, float scale, vec4_t color) {
    const char *info = CG_ConfigString(CS_SERVERINFO);
    int bits = cgs.customSettings;
    float y = rect->y;
    int v;

#define MOD_LINE(text) do { \
        CG_OwnerDrawText(rect->x, y, scale, color, (text), 0, 0, 0); \
        y += 12.0f; \
    } while (0)

    if (bits & 0x2000)  MOD_LINE("AIR CONTROL");
    if (bits & 0x4000)  MOD_LINE("RAMP JUMPING");
    if (cgs.armorTiered) MOD_LINE("TIERED ARMOR");
    if (bits & 0x10000) MOD_LINE("MODIFIED WEAPON SWITCH");

    {
        const char *s = Info_ValueForKey(info, "g_quadDamageFactor");
        v = atoi(s);
        if (s[0] && v != 3) {
            MOD_LINE(va("%ix QUAD", v));
        }
    }
    if (bits & 0x8000)  MOD_LINE("MODIFIED PHYSICS");

    {
        const char *s = Info_ValueForKey(info, "g_gravity");
        v = atoi(s);
        if (s[0] && v != 800) {
            MOD_LINE(va("GRAVITY %i", v));
        }
    }

    if (bits & 0x20000)  MOD_LINE("INSTAGIB");
    if (bits & 0x40000)  MOD_LINE("QUAD HOG");
    if (bits & 0x80000)  MOD_LINE("REGEN HEALTH");
    if ((bits & 0x100000) && !(bits & 0x20000)) MOD_LINE("DROP DAMAGED HEALTH");
    if (bits & 0x200000) MOD_LINE("VAMPIRIC DAMAGE");
    if (bits & 0x400000) MOD_LINE("MODIFIED ITEM SPAWNING");
    if (bits & 0x800000) MOD_LINE("HEADSHOTS ENABLED");
    if (bits & 0x1000000) MOD_LINE("RAIL JUMPING");
    if (bits & 0x1fff)   MOD_LINE("MODIFIED WEAPONS");

#undef MOD_LINE
}

// [QL] CG_DrawWeaponHorizontal (id 2). Row of owned weapon icons, then the
// selected weapon after a gap.
static void CG_DrawWeaponHorizontal(rectDef_t *rect, vec4_t color) {
    int i, weapons, cur;
    float x = rect->x;

    weapons = cg.snap->ps.stats[STAT_WEAPONS];
    for (i = WP_GAUNTLET; i < WP_NUM_WEAPONS; i++) {
        qhandle_t icon;
        if (!(weapons & (1 << i))) {
            continue;
        }
        CG_RegisterWeapon(i);
        icon = cg_weapons[i].weaponIcon;
        if (!icon) {
            continue;
        }
        trap_R_SetColor(colorWhite);
        CG_DrawPic(x, rect->y, rect->w, rect->h, icon);
        x += rect->w * 1.5f;
        trap_R_SetColor(NULL);
    }

    cur = cg.weaponSelect;
    if (cur > 0) {
        qhandle_t icon;
        if (cur < 1 || cur > 14) {
            cur = 14;
        }
        CG_RegisterWeapon(cur);
        icon = cg_weapons[cur].weaponIcon;
        if (icon) {
            trap_R_SetColor(colorWhite);
            CG_DrawPic(x + rect->w, rect->y, rect->w, rect->h, icon);
            trap_R_SetColor(NULL);
        }
    }
}

// [QL] CG_DrawScoreByOwnerDraw (0x33 player score, 0x53 1st place, 0x56 2nd place).
// Race formats the value as a lap time; -9999 draws nothing.
static void CG_DrawScoreByOwnerDraw(int ownerDraw, rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    int score;
    char buf[16];
    float x;

    if (ownerDraw == CG_PLAYER_SCORE) {
        if (cgs.gametype == GT_RACE) {
            score = cgs.clientinfo[cg.snap->ps.clientNum].score;
        } else {
            score = cg.snap->ps.persistant[PERS_SCORE];
        }
    } else if (ownerDraw == CG_1STPLACE) {
        score = cgs.scores1;
    } else if (ownerDraw == CG_2NDPLACE) {
        score = cgs.scores2;
    } else {
        return;
    }

    if (score == SCORE_NOT_PRESENT) {
        return;
    }

    if (cgs.gametype == GT_RACE) {
        if (score != 0x7fffffff && score >= 0) {
            Q_strncpyz(buf, CG_FormatRaceTime(score), sizeof(buf));
        } else {
            Q_strncpyz(buf, "-", sizeof(buf));
        }
    } else {
        Com_sprintf(buf, sizeof(buf), "%i", score);
    }

    x = CG_OwnerDrawAlignX(rect, buf, scale, align);
    CG_OwnerDrawText(x, rect->y, scale, color, buf, 0, 0, textStyle);
}

// [QL] CG_DrawScoreboardPlayerHead (0x4b-0x50). Draws the head icon of each
// end-of-match award player. The binary keeps the award clientNums in dedicated
// globals; ioquakelive does not track them, so the client is read as a leading
// clientNum from the award configstring. No valid client draws nothing.
static void CG_DrawScoreboardPlayerHead(rectDef_t *rect, int ownerDraw) {
    int csIndex, clientNum;
    const char *s;

    // [QL] binary CG_DrawScoreboardPlayerHead (0x1003a0d0) sources each head's
    // clientNum from these configstrings (via DAT_10a5fdb0..c4), which don't line up
    // with the same-named award CS constants. The values below are the binary's.
    switch (ownerDraw) {
        case CG_MOST_VALUABLE_OFFENSIVE_PLYR: csIndex = 697; break;  // 0x4b DAT_10a5fdbc
        case CG_MOST_VALUABLE_DEFENSIVE_PLYR: csIndex = 698; break;  // 0x4c DAT_10a5fdc0
        case CG_MOST_VALUABLE_PLYR:           csIndex = 699; break;  // 0x4d DAT_10a5fdc4
        case CG_BEST_ITEMCONTROL_PLYR:        csIndex = 696; break;  // 0x4e DAT_10a5fdb8
        case CG_MOST_ACCURATE_PLYR:           csIndex = 693; break;  // 0x4f DAT_10a5fdb4
        case CG_MOST_DAMAGEDEALT_PLYR:        csIndex = 692; break;  // 0x50 DAT_10a5fdb0
        default: return;
    }

    s = CG_ConfigString(csIndex);
    if (!s || !s[0]) {
        return;
    }
    clientNum = atoi(s);
    if (clientNum < 0 || clientNum >= MAX_CLIENTS || !cgs.clientinfo[clientNum].infoValid) {
        return;
    }
    if (cgs.clientinfo[clientNum].modelIcon) {
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.clientinfo[clientNum].modelIcon);
    }
}

// [QL] CG_DrawEndGameResult (0x5a). Intermission summary line for the local
// player. Team games report captures/assists/defends before falling back to the
// score; FFA/Duel report finishing place. Race and spectators draw nothing.
static void CG_DrawEndGameResult(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    playerState_t *ps = &cg.snap->ps;
    int score = ps->persistant[PERS_SCORE];
    const char *s;

    if (cgs.gametype == GT_RACE) {
        return;
    }
    if (ps->persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        return;
    }

    if (cgs.gametype < GT_TEAM) {
        if (score == SCORE_NOT_PRESENT) {
            s = "";
        } else if (score == -999) {
            s = "You forfeited the match.";
        } else {
            // binary uses the score value for the place string; we use rank,
            // matching CG_GetGameStatusText.
            s = va("You finished %s with a score of %d",
                   CG_PlaceString(ps->persistant[PERS_RANK] + 1), score);
        }
    } else {
        int captures = ps->persistant[PERS_CAPTURES];
        int assists  = ps->persistant[PERS_ASSIST_COUNT];
        int defends  = ps->persistant[PERS_DEFEND_COUNT];

        if (captures > 0) {
            const char *plural = (captures == 1) ? "" : "s";
            if (cgs.gametype == GT_HARVESTER) {
                s = va("You captured %d skull%s.", captures, plural);
            } else {
                s = va("You had %d flag capture%s.", captures, plural);
            }
        } else if (assists > 0) {
            s = va("You had %d assist%s.", assists, (assists == 1) ? "" : "s");
        } else if (defends > 0) {
            s = va("You had %d defend%s.", defends, (defends == 1) ? "" : "s");
        } else if (score == SCORE_NOT_PRESENT) {
            s = "";
        } else if (score == -999) {
            s = "You forfeited the match.";
        } else {
            s = va("You finished with a score of %d.", score);
        }
    }

    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
}

// [QL] CG_DrawOpponentScore (0x64/0x65, round-based only). 0x64 draws your team's
// round score, 0x65 the opposing team's.
static void CG_DrawOpponentScore(int useOwnTeam, rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int team = cg.snap->ps.persistant[PERS_TEAM];
    int score;

    if (!useOwnTeam) {
        if (team == TEAM_RED) {
            team = TEAM_BLUE;
        } else if (team == TEAM_BLUE) {
            team = TEAM_RED;
        }
    }
    score = (team == TEAM_RED) ? cgs.scores1 : cgs.scores2;
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%i", score), 0, 0, textStyle);
}

// [QL] CG_DrawTeamScore (0x11c/0x138/0x122/0x13d). Numeric team score.
// Binary shows scores1 only for id 0x11c (CG_RED_SCORE); every other selector
// shows scores2. -9999/-999 draw nothing.
static void CG_DrawTeamScore(int which, rectDef_t *rect, float scale, vec4_t color, int textStyle, int align) {
    int score = (which == CG_RED_SCORE) ? cgs.scores1 : cgs.scores2;
    const char *s;
    float x;

    if (score == SCORE_NOT_PRESENT || score == -999) {
        return;
    }
    s = va("%i", score);
    x = CG_OwnerDrawAlignX(rect, s, scale, align);
    CG_OwnerDrawText(x, rect->y, scale, color, s, 0, 0, textStyle);
}

// [QL] CG_DrawWeaponIcon (0x225). Draws a weapon icon shader, gated by the
// owner-draw visibility flags.
static void CG_DrawWeaponIcon(rectDef_t *rect, vec4_t color, qhandle_t iconShader, int ownerDrawFlags) {
    if (ownerDrawFlags && !CG_OwnerDrawVisible(ownerDrawFlags, 0)) {
        return;
    }
    trap_R_SetColor(color);
    if (iconShader) {
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, iconShader);
    }
    trap_R_SetColor(NULL);
}

//
void CG_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle, int fontIndex) {
    rectDef_t rect;

    if (cg_drawStatus.integer == 0) {
        return;
    }

    // if (ownerDrawFlags != 0 && !CG_OwnerDrawVisible(ownerDrawFlags)) {
    //	return;
    // }

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

    cg_currentFontIndex = fontIndex;

    // Dispatch mirrors the binary CG_OwnerDraw switch (cgamex86.dll 0x1003b0f0).
    // Case labels use the pak00 menudef constants; hex ids match the binary. Some
    // menudef labels are stale (QL repurposed the slot), so the comment names the
    // binary behaviour where they differ.
    switch (ownerDraw) {
        case CG_SERVER_SETTINGS:            // 1  CG_DrawModifiers
            CG_DrawModifiers(&rect, scale, color);
            break;
        case CG_STARTING_WEAPONS:           // 2  CG_DrawWeaponHorizontal
            CG_DrawWeaponHorizontal(&rect, color);
            break;
        case CG_GAME_LIMIT:                 // 3  CG_DrawFragLimit
            CG_DrawGameLimit(&rect, scale, color, textStyle);
            break;
        case CG_GAME_TYPE:                  // 4
            CG_DrawGameType(&rect, scale, color, shader, textStyle);
            break;
        case CG_GAME_TYPE_ICON:             // 5
            CG_DrawGameTypeIcon(&rect);
            break;
        case CG_GAME_TYPE_MAP:              // 6
            CG_DrawGameTypeMap(&rect, scale, color, textStyle, align);
            break;
        case CG_GAME_STATUS:                // 7
            CG_DrawGameStatus(&rect, scale, color, shader, textStyle);
            break;
        case CG_MATCH_DETAILS:              // 8
            CG_DrawMatchDetails(&rect, scale, color, textStyle);
            break;
        case CG_MATCH_END_CONDITION:        // 9  CG_DrawWinCondition
            CG_DrawWinCondition(&rect, scale, color, textStyle);
            break;
        case CG_MATCH_STATUS:               // 0xa
            CG_DrawMatchStatus(&rect, scale, color, textStyle);
            break;
        case CG_CAPFRAGLIMIT:               // 0xb
            CG_DrawCapFragLimit(&rect, scale, color, shader, textStyle);
            break;
        case CG_LEVELTIMER:                 // 0xc  CG_DrawRoundTimer(level timer)
            CG_DrawLevelTimer(&rect, scale, color, textStyle);
            break;
        case CG_ROUND:                      // 0xd  CG_DrawWarmup (gt CA/FT/AD/RR)
            if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE ||
                cgs.gametype == GT_AD || cgs.gametype == GT_RR) {
                CG_DrawRound(&rect, scale, color, textStyle);
            }
            break;
        case CG_ROUNDTIMER:                 // 0xe
            CG_DrawRoundTimer(&rect, scale, color, textStyle);
            break;
        case CG_OVERTIME:                   // 0xf
            CG_DrawOvertime(&rect, scale, color, textStyle);
            break;
        case CG_LOCALTIME:                  // 0x10
            CG_DrawLocalTime(&rect, scale, color, textStyle);
            break;
        case CG_PLAYER_COUNTS:              // 0x11  CG_DrawPlayerCount
            CG_DrawPlayerCounts(&rect, scale, color, textStyle, align);
            break;
        case CG_MAP_NAME:                   // 0x12
            CG_DrawMapName(&rect, scale, color, textStyle);
            break;
        case CG_VOTEGAMETYPE1:              // 0x13-0x15  CG_DrawVoteGameType
            CG_DrawVoteGameType(&rect, fontIndex, scale, color, textStyle, align, 0);
            break;
        case CG_VOTEGAMETYPE2:
            CG_DrawVoteGameType(&rect, fontIndex, scale, color, textStyle, align, 1);
            break;
        case CG_VOTEGAMETYPE3:
            CG_DrawVoteGameType(&rect, fontIndex, scale, color, textStyle, align, 2);
            break;
        case CG_VOTEMAP1:                   // 0x16-0x18  raw map names (binary 0x100355d0)
            CG_DrawVoteMap(&rect, fontIndex, scale, color, textStyle, align, 0);
            break;
        case CG_VOTEMAP2:
            CG_DrawVoteMap(&rect, fontIndex, scale, color, textStyle, align, 1);
            break;
        case CG_VOTEMAP3:
            CG_DrawVoteMap(&rect, fontIndex, scale, color, textStyle, align, 2);
            break;
        case CG_VOTESHOT1:                  // 0x19-0x1b  CG_DrawVoteMapShot
            CG_DrawVoteMapShot(&rect, 0, shader);
            break;
        case CG_VOTESHOT2:
            CG_DrawVoteMapShot(&rect, 1, shader);
            break;
        case CG_VOTESHOT3:
            CG_DrawVoteMapShot(&rect, 2, shader);
            break;
        case CG_VOTENAME1:                  // 0x1c-0x1e  map titles (binary 0x10035660)
            CG_DrawVoteMapName(&rect, fontIndex, scale, color, textStyle, align, 0);
            break;
        case CG_VOTENAME2:
            CG_DrawVoteMapName(&rect, fontIndex, scale, color, textStyle, align, 1);
            break;
        case CG_VOTENAME3:
            CG_DrawVoteMapName(&rect, fontIndex, scale, color, textStyle, align, 2);
            break;
        case CG_VOTECOUNT1:                 // 0x1f-0x21
        case CG_VOTECOUNT2:
        case CG_VOTECOUNT3:
            CG_DrawVotes(&rect, ownerDraw, fontIndex, scale, color, textStyle, align);
            break;
        case CG_VOTETIMER:                  // 0x22
            // binary falls through to the respawn message here; kept separate.
            CG_DrawVoteTimer(&rect, fontIndex, scale, color, textStyle, align);
            break;
        case CG_SPEC_MESSAGES:              // 0x23  CG_DrawSpectator (binary 0x10034d70)
            CG_DrawSpectator(&rect);
            break;
        case CG_PLAYER_HEAD:                // 0x24
            CG_DrawPlayerHead(&rect, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYERMODEL:                // 0x25
            CG_DrawPlayerModel(&rect);
            break;
        case CG_PLAYER_ARMOR_ICON:          // 0x26
            CG_DrawPlayerArmorIcon(&rect, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYER_ARMOR_ICON2D:        // 0x27
            CG_DrawPlayerArmorIcon(&rect, qtrue);
            break;
        case CG_PLAYER_ARMOR_VALUE:         // 0x28  (binary symbol is CG_DrawPlayerScore
            CG_DrawPlayerArmorValue(&rect, scale, color, shader, textStyle, align); // but it reads STAT_ARMOR)
            break;
        case CG_PLAYER_ARMOR_BAR_100:       // 0x29
            CG_DrawArmorBar100(&rect, shader);
            break;
        case CG_PLAYER_ARMOR_BAR_200:       // 0x2a
            CG_DrawArmorBar200(&rect, shader);
            break;
        case CG_ARMORTIERED_COLORIZED:      // 0x2b
            CG_DrawArmorTieredColorized(&rect, color, shader);
            break;
        case CG_PLAYER_HEALTH:              // 0x2c
            CG_DrawPlayerHealth(&rect, scale, color, shader, textStyle, align);
            break;
        case CG_PLAYER_HEALTH_BAR_100:      // 0x2d
            CG_DrawHealthBar100(&rect, shader);
            break;
        case CG_PLAYER_HEALTH_BAR_200:      // 0x2e
            CG_DrawHealthBar200(&rect, shader);
            break;
        case CG_PLAYER_AMMO_ICON:           // 0x2f
            CG_DrawPlayerAmmoIcon(&rect, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYER_AMMO_ICON2D:         // 0x30
            CG_DrawPlayerAmmoIcon(&rect, qtrue);
            break;
        case CG_PLAYER_AMMO_VALUE:          // 0x31
            CG_DrawPlayerAmmoValue(&rect, scale, color, shader, textStyle, align);
            break;
        case CG_PLAYER_ITEM:                // 0x32
            CG_DrawPlayerItem(&rect, scale, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYER_SCORE:               // 0x33/0x53/0x56  CG_DrawScoreByOwnerDraw
        case CG_1STPLACE:
        case CG_2NDPLACE:
            CG_DrawScoreByOwnerDraw(ownerDraw, &rect, scale, color, textStyle, align);
            break;
        case CG_ONEFLAG_STATUS:             // 0x36
            CG_OneFlagStatus(&rect);
            break;
        case CG_PLAYER_HASFLAG:             // 0x37
        case CG_PLAYER_HASFLAG2D:           // 0x38
            CG_DrawPlayerHasFlag(&rect, ownerDraw == CG_PLAYER_HASFLAG2D);
            break;
        case CG_HARVESTER_SKULLS:           // 0x39  CG_DrawDomStatus (harvester skulls, gt 8)
            CG_HarvesterSkulls(&rect, scale, color, qfalse, textStyle);
            break;
        case CG_HARVESTER_SKULLS2D:         // 0x3a
            CG_HarvesterSkulls(&rect, scale, color, qtrue, textStyle);
            break;
        case CG_PLAYER_HASKEY:              // 0x3b  CG_DrawCTFPowerUp (QL repurposed the slot)
            CG_DrawCTFPowerUp(&rect);
            break;
        case CG_CTF_POWERUP:                // 0x3c  CG_DrawSelectedPlayerWeapon
            CG_DrawSelectedPlayerWeapon(&rect);
            break;
        case CG_AREA_POWERUP:               // 0x3d
            CG_DrawAreaPowerUp(&rect, align, special, scale, color);
            break;
        case CG_TEAM_COLOR:                 // 0x3e  CG_DrawTeamBackground
            CG_DrawTeamColor(&rect, color);
            break;
        case CG_KILLER:                     // 0x3f
            CG_DrawKiller(&rect, scale, color, shader, textStyle);
            break;
        case CG_ACCURACY:                   // 0x40-0x42,0x44-0x47,0x4a  medals
        case CG_ASSISTS:
        case CG_CAPTURES:
        case CG_DEFEND:
        case CG_EXCELLENT:
        case CG_GAUNTLET:
        case CG_IMPRESSIVE:
        case CG_PERFECT:
            CG_DrawMedal(ownerDraw, &rect, scale, color, shader);
            break;
        case CG_MOST_VALUABLE_OFFENSIVE_PLYR: // 0x4b-0x50  CG_DrawScoreboardPlayerHead
        case CG_MOST_VALUABLE_DEFENSIVE_PLYR:
        case CG_MOST_VALUABLE_PLYR:
        case CG_BEST_ITEMCONTROL_PLYR:
        case CG_MOST_ACCURATE_PLYR:
        case CG_MOST_DAMAGEDEALT_PLYR:
            CG_DrawScoreboardPlayerHead(&rect, ownerDraw);
            break;
        case CG_SPECTATORS:                 // 0x51  CG_DrawSpectators (binary 0x100351a0)
            CG_DrawSpectators(&rect, fontIndex, scale, color);
            break;
        case CG_MATCH_WINNER:               // 0x52
            CG_DrawMatchWinner(&rect, scale, color, textStyle);
            break;
        case CG_1ST_PLACE_SCORE:            // 0x54  CG_DrawRedScore
            CG_DrawRedScore(&rect, scale, color, shader, textStyle);
            break;
        case CG_1STPLACE_PLYR_MODEL:        // 0x55  CG_DrawSelectedPlayerModel
            CG_DrawSelectedPlayerModel(&rect);
            break;
        case CG_2ND_PLACE_SCORE:            // 0x57  CG_DrawBlueScore
            CG_DrawBlueScore(&rect, scale, color, shader, textStyle);
            break;
        case CG_PLAYER_OBIT:                // 0x58
            CG_DrawPlayerObit(&rect, scale, color, textStyle);
            break;
        case CG_AREA_NEW_CHAT:              // 0x59  CG_DrawChat (binary 0x10006a10)
            CG_DrawChat(&rect, scale);
            break;
        case CG_PLYR_END_GAME_SCORE:        // 0x5a  CG_DrawEndGameResult
            CG_DrawEndGameResult(&rect, scale, color, textStyle);
            break;
        case CG_PLYR_BEST_WEAPON_NAME:      // 0x5b
            CG_DrawBestWeaponName(&rect, scale, color, textStyle);
            break;
        case CG_SELECTED_PLYR_TEAM_COLOR:   // 0x5c
            CG_DrawSelectedPlayerTeamColor(&rect);
            break;
        case CG_SELECTED_PLYR_ACCURACY:     // 0x5d  CG_DrawFollowPlayerName (duel selected name)
            CG_DrawFollowPlayerName(&rect, scale, color, textStyle);
            break;
        case CG_FOLLOW_PLAYER_NAME:         // 0x5e  CG_DrawFollow ("Following - <name>")
        case CG_FOLLOW_PLAYER_NAME_EX:      // 0x5f  CG_DrawFollow (name only)
            CG_DrawFollow(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_SPEEDOMETER:                // 0x60
            CG_DrawSpeedometer(&rect, scale, color, textStyle);
            break;
        case CG_WP_VERTICAL:                // 0x61
            CG_DrawWeaponVertical(&rect, color);
            break;
        case CG_ACC_VERTICAL:               // 0x62
            CG_DrawAccuracyVertical(&rect, scale, color, textStyle);
            break;
        case CG_TEAM_COLORIZED:             // 0x63
            CG_DrawTeamColorized(&rect, color, shader);
            break;
        case CG_TEAM_PLYR_COUNT:            // 0x64  CG_DrawOpponentScore (round-based only)
            if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE ||
                cgs.gametype == GT_AD || cgs.gametype == GT_RR) {
                CG_DrawOpponentScore(1, &rect, scale, color, textStyle);
            }
            break;
        case CG_ENEMY_PLYR_COUNT:           // 0x65
            if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE ||
                cgs.gametype == GT_AD || cgs.gametype == GT_RR) {
                CG_DrawOpponentScore(0, &rect, scale, color, textStyle);
            }
            break;
        case CG_1STPLACE_PLYR_MODEL_ACTIVE: // 0x66  no binary handler
            break;

        // ---- duel overlay: 1st player 0x67-0xc0, 2nd player 0xc1-0x11a ----
        case CG_1ST_PLYR: case CG_2ND_PLYR:                       // 0x67/0xc1
            CG_DrawDuelPlayerName(ownerDraw, &rect, scale, color, textStyle, align);
            break;
        case CG_1ST_PLYR_READY:                                   // 0x68
            CG_DrawPlayerStatusLeft(&rect);
            break;
        case CG_2ND_PLYR_READY:                                   // 0xc2
            CG_DrawPlayerStatusRight(&rect);
            break;
        case CG_1ST_PLYR_SCORE: case CG_2ND_PLYR_SCORE:           // 0x69/0xc3
            CG_DrawDuelPlayerScore(ownerDraw, &rect, scale, color, textStyle, align);
            break;
        case CG_1ST_PLYR_FRAGS: case CG_2ND_PLYR_FRAGS:           // 0x6a/0xc4
            CG_DrawDuelPlayerFrags(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_DEATHS: case CG_2ND_PLYR_DEATHS:         // 0x6b/0xc5
            CG_DrawDuelPlayerDeaths(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_DMG: case CG_2ND_PLYR_DMG:               // 0x6c/0xc6
            CG_DrawDuelPlayerDmg(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_PING: case CG_2ND_PLYR_PING:             // 0x6e/0xc8
            CG_DrawDuelPlayerPing(&rect, scale, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_WINS: case CG_2ND_PLYR_WINS:             // 0x6f/0xc9
            CG_DrawDuelPlayerWins(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_ACC: case CG_2ND_PLYR_ACC:               // 0x70/0xca
            CG_DrawDuelPlayerAcc(&rect, scale, color, textStyle, align, ownerDraw);
            break;
        case CG_1ST_PLYR_FLAG: case CG_2ND_PLYR_FLAG:             // 0x71/0xcb
            CG_DrawDuelPlayerFlag(&rect, ownerDraw);
            break;
        case CG_1ST_PLYR_AVATAR: case CG_2ND_PLYR_AVATAR:         // 0x72/0xcc
            CG_DrawDuelPlayerAvatar(&rect, ownerDraw);
            break;
        case CG_1ST_PLYR_HEALTH_ARMOR: case CG_2ND_PLYR_HEALTH_ARMOR: // 0x74/0xce
            CG_DrawDuelHealthArmorBar(&rect, ownerDraw);
            break;
        case CG_1ST_PLYR_FRAGS_G: case CG_1ST_PLYR_FRAGS_MG: case CG_1ST_PLYR_FRAGS_SG:
        case CG_1ST_PLYR_FRAGS_GL: case CG_1ST_PLYR_FRAGS_RL: case CG_1ST_PLYR_FRAGS_LG:
        case CG_1ST_PLYR_FRAGS_RG: case CG_1ST_PLYR_FRAGS_PG: case CG_1ST_PLYR_FRAGS_BFG:
        case CG_1ST_PLYR_FRAGS_CG: case CG_1ST_PLYR_FRAGS_NG: case CG_1ST_PLYR_FRAGS_PL:
        case CG_1ST_PLYR_FRAGS_HMG:
        case CG_2ND_PLYR_FRAGS_G: case CG_2ND_PLYR_FRAGS_MG: case CG_2ND_PLYR_FRAGS_SG:
        case CG_2ND_PLYR_FRAGS_GL: case CG_2ND_PLYR_FRAGS_RL: case CG_2ND_PLYR_FRAGS_LG:
        case CG_2ND_PLYR_FRAGS_RG: case CG_2ND_PLYR_FRAGS_PG: case CG_2ND_PLYR_FRAGS_BFG:
        case CG_2ND_PLYR_FRAGS_CG: case CG_2ND_PLYR_FRAGS_NG: case CG_2ND_PLYR_FRAGS_PL:
        case CG_2ND_PLYR_FRAGS_HMG:                               // 0x75-0x81/0xcf-0xdb
            CG_DrawDuelWeaponFrags(&rect, scale, color, textStyle, ownerDraw);
            break;
        case CG_1ST_PLYR_HITS_MG: case CG_1ST_PLYR_HITS_SG: case CG_1ST_PLYR_HITS_GL:
        case CG_1ST_PLYR_HITS_RL: case CG_1ST_PLYR_HITS_LG: case CG_1ST_PLYR_HITS_RG:
        case CG_1ST_PLYR_HITS_PG: case CG_1ST_PLYR_HITS_BFG: case CG_1ST_PLYR_HITS_CG:
        case CG_1ST_PLYR_HITS_NG: case CG_1ST_PLYR_HITS_PL: case CG_1ST_PLYR_HITS_HMG:
        case CG_2ND_PLYR_HITS_MG: case CG_2ND_PLYR_HITS_SG: case CG_2ND_PLYR_HITS_GL:
        case CG_2ND_PLYR_HITS_RL: case CG_2ND_PLYR_HITS_LG: case CG_2ND_PLYR_HITS_RG:
        case CG_2ND_PLYR_HITS_PG: case CG_2ND_PLYR_HITS_BFG: case CG_2ND_PLYR_HITS_CG:
        case CG_2ND_PLYR_HITS_NG: case CG_2ND_PLYR_HITS_PL: case CG_2ND_PLYR_HITS_HMG:
                                                                 // 0x82-0x8d/0xdc-0xe7
            CG_DrawDuelWeaponHits(&rect, scale, color, textStyle, ownerDraw);
            break;
        case CG_1ST_PLYR_SHOTS_MG: case CG_1ST_PLYR_SHOTS_SG: case CG_1ST_PLYR_SHOTS_GL:
        case CG_1ST_PLYR_SHOTS_RL: case CG_1ST_PLYR_SHOTS_LG: case CG_1ST_PLYR_SHOTS_RG:
        case CG_1ST_PLYR_SHOTS_PG: case CG_1ST_PLYR_SHOTS_BFG: case CG_1ST_PLYR_SHOTS_CG:
        case CG_1ST_PLYR_SHOTS_NG: case CG_1ST_PLYR_SHOTS_PL: case CG_1ST_PLYR_SHOTS_HMG:
        case CG_2ND_PLYR_SHOTS_MG: case CG_2ND_PLYR_SHOTS_SG: case CG_2ND_PLYR_SHOTS_GL:
        case CG_2ND_PLYR_SHOTS_RL: case CG_2ND_PLYR_SHOTS_LG: case CG_2ND_PLYR_SHOTS_RG:
        case CG_2ND_PLYR_SHOTS_PG: case CG_2ND_PLYR_SHOTS_BFG: case CG_2ND_PLYR_SHOTS_CG:
        case CG_2ND_PLYR_SHOTS_NG: case CG_2ND_PLYR_SHOTS_PL: case CG_2ND_PLYR_SHOTS_HMG:
                                                                 // 0x8e-0x99/0xe8-0xf3
            CG_DrawDuelWeaponShots(&rect, scale, color, textStyle, ownerDraw);
            break;
        case CG_1ST_PLYR_DMG_G: case CG_1ST_PLYR_DMG_MG: case CG_1ST_PLYR_DMG_SG:
        case CG_1ST_PLYR_DMG_GL: case CG_1ST_PLYR_DMG_RL: case CG_1ST_PLYR_DMG_LG:
        case CG_1ST_PLYR_DMG_RG: case CG_1ST_PLYR_DMG_PG: case CG_1ST_PLYR_DMG_BFG:
        case CG_1ST_PLYR_DMG_CG: case CG_1ST_PLYR_DMG_NG: case CG_1ST_PLYR_DMG_PL:
        case CG_1ST_PLYR_DMG_HMG:
        case CG_2ND_PLYR_DMG_G: case CG_2ND_PLYR_DMG_MG: case CG_2ND_PLYR_DMG_SG:
        case CG_2ND_PLYR_DMG_GL: case CG_2ND_PLYR_DMG_RL: case CG_2ND_PLYR_DMG_LG:
        case CG_2ND_PLYR_DMG_RG: case CG_2ND_PLYR_DMG_PG: case CG_2ND_PLYR_DMG_BFG:
        case CG_2ND_PLYR_DMG_CG: case CG_2ND_PLYR_DMG_NG: case CG_2ND_PLYR_DMG_PL:
        case CG_2ND_PLYR_DMG_HMG:                                 // 0x9a-0xa6/0xf4-0x100
            CG_DrawDuelWeaponDmg(&rect, scale, color, textStyle, ownerDraw);
            break;
        case CG_1ST_PLYR_ACC_MG: case CG_1ST_PLYR_ACC_SG: case CG_1ST_PLYR_ACC_GL:
        case CG_1ST_PLYR_ACC_RL: case CG_1ST_PLYR_ACC_LG: case CG_1ST_PLYR_ACC_RG:
        case CG_1ST_PLYR_ACC_PG: case CG_1ST_PLYR_ACC_BFG: case CG_1ST_PLYR_ACC_CG:
        case CG_1ST_PLYR_ACC_NG: case CG_1ST_PLYR_ACC_PL: case CG_1ST_PLYR_ACC_HMG:
        case CG_2ND_PLYR_ACC_MG: case CG_2ND_PLYR_ACC_SG: case CG_2ND_PLYR_ACC_GL:
        case CG_2ND_PLYR_ACC_RL: case CG_2ND_PLYR_ACC_LG: case CG_2ND_PLYR_ACC_RG:
        case CG_2ND_PLYR_ACC_PG: case CG_2ND_PLYR_ACC_BFG: case CG_2ND_PLYR_ACC_CG:
        case CG_2ND_PLYR_ACC_NG: case CG_2ND_PLYR_ACC_PL: case CG_2ND_PLYR_ACC_HMG:
                                                                 // 0xa7-0xb2/0x101-0x10c
            CG_DrawDuelWeaponAcc(&rect, scale, color, textStyle, ownerDraw);
            break;
        case CG_1ST_PLYR_PICKUPS: case CG_1ST_PLYR_PICKUPS_RA: case CG_1ST_PLYR_PICKUPS_YA:
        case CG_1ST_PLYR_PICKUPS_GA: case CG_1ST_PLYR_PICKUPS_MH:
        case CG_2ND_PLYR_PICKUPS: case CG_2ND_PLYR_PICKUPS_RA: case CG_2ND_PLYR_PICKUPS_YA:
        case CG_2ND_PLYR_PICKUPS_GA: case CG_2ND_PLYR_PICKUPS_MH:  // 0xb3-0xb7/0x10d-0x111
            CG_DrawDuelPlayerPickups(&rect, scale, color, textStyle, ownerDraw);
            break;
        case CG_1ST_PLYR_AVG_PICKUP_TIME_RA: case CG_1ST_PLYR_AVG_PICKUP_TIME_YA:
        case CG_1ST_PLYR_AVG_PICKUP_TIME_GA: case CG_1ST_PLYR_AVG_PICKUP_TIME_MH:
        case CG_2ND_PLYR_AVG_PICKUP_TIME_RA: case CG_2ND_PLYR_AVG_PICKUP_TIME_YA:
        case CG_2ND_PLYR_AVG_PICKUP_TIME_GA: case CG_2ND_PLYR_AVG_PICKUP_TIME_MH:
                                                                 // 0xb8-0xbb/0x112-0x115
            CG_DrawDuelAccuracy(&rect, scale, color, textStyle, ownerDraw);
            break;
        case CG_1ST_PLYR_EXCELLENT: case CG_1ST_PLYR_IMPRESSIVE: case CG_1ST_PLYR_HUMILIATION:
        case CG_2ND_PLYR_EXCELLENT: case CG_2ND_PLYR_IMPRESSIVE: case CG_2ND_PLYR_HUMILIATION:
                                                                 // 0xbc-0xbe/0x116-0x118
            CG_DrawDuelKDR(&rect, scale, color, textStyle, ownerDraw);
            break;

        // ---- team header / scoreboard ----
        case CG_RED_FLAGSTATUS:             // 0x11b/0x120/0x136/0x13b  CG_DrawFlagStatus
        case CG_RED_BASESTATUS:
        case CG_BLUE_FLAGSTATUS:
        case CG_BLUE_BASESTATUS:
            CG_DrawFlagStatus(&rect, ownerDraw);
            break;
        case CG_RED_SCORE:                  // 0x11c/0x138  CG_DrawTeamScore
        case CG_BLUE_SCORE:
            CG_DrawTeamScore(ownerDraw, &rect, scale, color, textStyle, align);
            break;
        case CG_RED_NAME:                   // 0x11d/0x137  CG_DrawTeamName
            CG_DrawRedName(&rect, scale, color, textStyle);
            break;
        case CG_BLUE_NAME:
            CG_DrawBlueName(&rect, scale, color, textStyle);
            break;
        case CG_RED_OWNED_FLAGS:            // 0x11e  CG_DrawTeamAliveCount (DOM/AD)
            if (cgs.gametype == GT_DOMINATION || cgs.gametype == GT_AD) {
                CG_DrawTeamAliveCount(&rect, TEAM_RED, scale, color, textStyle);
            }
            break;
        case CG_BLUE_OWNED_FLAGS:           // 0x139
            if (cgs.gametype == GT_DOMINATION || cgs.gametype == GT_AD) {
                CG_DrawTeamAliveCount(&rect, TEAM_BLUE, scale, color, textStyle);
            }
            break;
        case CG_RED_AVG_PING:               // 0x11f/0x13a
            CG_DrawTeamAvgPing(&rect, scale, color, textStyle, TEAM_RED);
            break;
        case CG_BLUE_AVG_PING:
            CG_DrawTeamAvgPing(&rect, scale, color, textStyle, TEAM_BLUE);
            break;
        case CG_RED_PLAYER_COUNT:           // 0x121/0x13c  CG_DrawPlayerCount2
            CG_DrawPlayerCount2(&rect, TEAM_RED, scale, color, textStyle, align);
            break;
        case CG_BLUE_PLAYER_COUNT:
            CG_DrawPlayerCount2(&rect, TEAM_BLUE, scale, color, textStyle, align);
            break;
        case CG_RED_CLAN_PLYRS:             // 0x122  CG_DrawTeamScore(1) (round-based)
            if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE ||
                cgs.gametype == GT_AD || cgs.gametype == GT_RR) {
                CG_DrawTeamScore(1, &rect, scale, color, textStyle, align);
            }
            break;
        case CG_BLUE_CLAN_PLYRS:            // 0x13d  CG_DrawTeamScore(2)
            if (cgs.gametype == GT_CA || cgs.gametype == GT_FREEZE ||
                cgs.gametype == GT_AD || cgs.gametype == GT_RR) {
                CG_DrawTeamScore(2, &rect, scale, color, textStyle, align);
            }
            break;
        case CG_RED_TIMEOUT_COUNT:          // 0x123  CG_DrawTimeoutCount
            CG_DrawTimeoutCount(&rect, scale, color, textStyle, TEAM_RED);
            break;
        case CG_BLUE_TIMEOUT_COUNT:         // 0x13e
            CG_DrawTimeoutCount(&rect, scale, color, textStyle, TEAM_BLUE);
            break;

        // ---- team pickup stat columns (0x124-0x135 red, 0x13f-0x150 blue) ----
        case CG_RED_TEAM_MAP_PICKUPS:       // column 1: pickup counts
        case CG_RED_TEAM_PICKUPS_RA: case CG_RED_TEAM_PICKUPS_YA:
        case CG_RED_TEAM_PICKUPS_GA: case CG_RED_TEAM_PICKUPS_MH:
        case CG_RED_TEAM_PICKUPS_QUAD: case CG_RED_TEAM_PICKUPS_BS:
        case CG_RED_TEAM_PICKUPS_FLAG: case CG_RED_TEAM_PICKUPS_MEDKIT:
        case CG_RED_TEAM_PICKUPS_REGEN: case CG_RED_TEAM_PICKUPS_HASTE:
        case CG_RED_TEAM_PICKUPS_INVIS:
        case CG_BLUE_TEAM_MAP_PICKUPS:
        case CG_BLUE_TEAM_PICKUPS_RA: case CG_BLUE_TEAM_PICKUPS_YA:
        case CG_BLUE_TEAM_PICKUPS_GA: case CG_BLUE_TEAM_PICKUPS_MH:
        case CG_BLUE_TEAM_PICKUPS_QUAD: case CG_BLUE_TEAM_PICKUPS_BS:
        case CG_BLUE_TEAM_PICKUPS_FLAG: case CG_BLUE_TEAM_PICKUPS_MEDKIT:
        case CG_BLUE_TEAM_PICKUPS_REGEN: case CG_BLUE_TEAM_PICKUPS_HASTE:
        case CG_BLUE_TEAM_PICKUPS_INVIS:
            CG_OwnerDraw_StatColumn1(&rect, scale, color, textStyle, ownerDraw);
            break;
        case CG_RED_TEAM_TIMEHELD_QUAD:     // column 2: time-held values
        case CG_RED_TEAM_TIMEHELD_BS:
        case CG_RED_TEAM_TIMEHELD_FLAG: case CG_RED_TEAM_TIMEHELD_REGEN:
        case CG_RED_TEAM_TIMEHELD_HASTE: case CG_RED_TEAM_TIMEHELD_INVIS:
        case CG_BLUE_TEAM_TIMEHELD_QUAD: case CG_BLUE_TEAM_TIMEHELD_BS:
        case CG_BLUE_TEAM_TIMEHELD_FLAG: case CG_BLUE_TEAM_TIMEHELD_REGEN:
        case CG_BLUE_TEAM_TIMEHELD_HASTE: case CG_BLUE_TEAM_TIMEHELD_INVIS:
            CG_OwnerDraw_StatColumn2(&rect, scale, color, textStyle, ownerDraw);
            break;

        case CG_FLAG_STATUS:                // 0x151
            CG_DrawFlagStatusBar(&rect);
            break;
        case CG_HEALTH_COLORIZED:           // 0x152
            CG_DrawHealthColorized(&rect, scale, color, textStyle);
            break;
        case CG_MATCH_STATE:                // 0x153
            // [QL] force Handel Gothic (fontIndex 1), pak00 menu omits the font directive
            cg_currentFontIndex = fontIndex ? fontIndex : 1;
            CG_DrawMatchState(&rect, scale, color, textStyle);
            break;

        // ---- race respawn message (0x34/0x35), split into status + times ----
        case CG_RACE_STATUS:                // 0x34
            CG_DrawRaceStatus(&rect, scale, color, textStyle);
            break;
        case CG_RACE_TIMES:                 // 0x35
            CG_DrawRaceTimes(&rect, scale, color, textStyle);
            break;

        case UI_ADVERT:                     // 0x225  CG_DrawWeaponIcon
            CG_DrawWeaponIcon(&rect, color, shader, ownerDrawFlags);
            break;

        default:
            break;
    }
}

// [QL] binary 0x1003c4f0. The engine feeds absolute cursor coordinates (already scaled
// to the 640x480 virtual screen) while a menu holds KEYCATCH_CGAME, because the shipped
// client shows the OS cursor rather than a rendered bitmap. Store them directly; the
// player sees the system cursor, and cgs.cursorX/Y only drive menu hit-testing. It never
// touches the key catcher here (that would kill the join-game menu mouse).
void CG_MouseEvent(int x, int y) {
    int n;
    int dx = x - cgs.cursorX;  // delta only for captured-item (window drag) moves
    int dy = y - cgs.cursorY;

    cgs.cursorX = x;
    if (cgs.cursorX < 0)
        cgs.cursorX = 0;
    else if (cgs.cursorX > SCREEN_WIDTH)
        cgs.cursorX = SCREEN_WIDTH;

    cgs.cursorY = y;
    if (cgs.cursorY < 0)
        cgs.cursorY = 0;
    else if (cgs.cursorY > SCREEN_HEIGHT)
        cgs.cursorY = SCREEN_HEIGHT;

    n = Display_CursorType(cgs.cursorX, cgs.cursorY);
    cgs.activeCursor = 0;
    if (n == CURSOR_ARROW) {
        cgs.activeCursor = cgs.media.selectCursor;
    } else if (n == CURSOR_SIZER) {
        cgs.activeCursor = cgs.media.sizeCursor;
    }

    if (cgs.capturedItem) {
        Display_MouseMove(cgs.capturedItem, dx, dy);
    } else {
        Display_MouseMove(NULL, cgs.cursorX, cgs.cursorY);
    }
}

/*
==================
CG_HideTeamMenus
==================

*/
void CG_HideTeamMenu(void) {
    Menus_CloseByName("teamMenu");
    Menus_CloseByName("getMenu");
}

/*
==================
CG_ShowTeamMenus
==================

*/
void CG_ShowTeamMenu(void) {
    Menus_OpenByName("teamMenu");
}

/*
==================
CG_EventHandling
==================
 type 0 - no event handling
      1 - team menu
      2 - hud editor

*/
void CG_EventHandling(int type) {
    cgs.eventHandling = type;
    if (type == CGAME_EVENT_NONE) {
        CG_HideTeamMenu();
    } else if (type == CGAME_EVENT_TEAMMENU) {
        // CG_ShowTeamMenu();
    } else if (type == CGAME_EVENT_SCOREBOARD) {
    }
}

// [QL] key handler while the cgame catcher is up (binary 0x1003c6f0). Certain binds
// pass straight through so the scoreboard and join menu never eat them; anything
// else goes to the focused menu.
void CG_KeyEvent(int key, qboolean down) {
    char bind[32];
    menuDef_t* menu;

    if (!down) {
        return;
    }

    bind[0] = '\0';
    trap_Key_GetBindingBuf(key, bind, sizeof(bind));

    if (!Q_stricmpn(bind, "messagemode", 11)) {
        trap_SendConsoleCommand(bind);
        return;
    }
    if (!Q_stricmpn(bind, "screenshot", 10) || !Q_stricmpn(bind, "screenshotJPEG", 14)) {
        trap_SendConsoleCommand(bind);
        return;
    }
    if (!Q_stricmpn(bind, "+voice", 6)) {
        trap_SendConsoleCommand("+voice\n");
        return;
    }
    if (!Q_stricmpn(bind, "+scores", 7)) {
        cg.showScores = qtrue;
        CG_ScoresDown_f();
        return;
    }

    menu = Menu_GetFocused();
    if (menu) {
        Menu_HandleKey(menu, key, down);
    }
}

int CG_ClientNumFromName(const char* p) {
    int i;
    for (i = 0; i < cgs.maxclients; i++) {
        if (cgs.clientinfo[i].infoValid && Q_stricmp(cgs.clientinfo[i].name, p) == 0) {
            return i;
        }
    }
    return -1;
}

void CG_ShowResponseHead(void) {
    float x, y, w, h;

    x = 72;
    y = w = h = 0;
    CG_AdjustFrom640(&x, &y, &w, &h);

    Menus_OpenByName("voiceMenu");
    trap_Cvar_Set("cl_conXOffset", va("%d", (int)x));
    cg.voiceTime = cg.time;
}

void CG_RunMenuScript(char** args) {
}

void CG_ExecuteText(int exec_when, const char* text) {
    (void)exec_when;  // cgame's console-command trap always appends
    trap_SendConsoleCommand(text);
}

void CG_GetTeamColor(vec4_t* color) {
    if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED) {
        (*color)[0] = 1.0f;
        (*color)[3] = 0.25f;
        (*color)[1] = (*color)[2] = 0.0f;
    } else if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE) {
        (*color)[0] = (*color)[1] = 0.0f;
        (*color)[2] = 1.0f;
        (*color)[3] = 0.25f;
    } else {
        (*color)[0] = (*color)[2] = 0.0f;
        (*color)[1] = 0.17f;
        (*color)[3] = 0.25f;
    }
}
