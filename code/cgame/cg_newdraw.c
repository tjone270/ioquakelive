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

static void CG_DrawBlueFlagStatus(rectDef_t* rect, qhandle_t shader) {
    if (cgs.gametype != GT_CTF && cgs.gametype != GT_1FCTF) {
        if (cgs.gametype == GT_HARVESTER) {
            vec4_t color = {0, 0, 1, 1};
            trap_R_SetColor(color);
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.blueCubeIcon);
            trap_R_SetColor(NULL);
        }
        return;
    }
    if (shader) {
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
    } else {
        gitem_t* item = BG_FindItemForPowerup(PW_BLUEFLAG);
        if (item) {
            vec4_t color = {0, 0, 1, 1};
            trap_R_SetColor(color);
            if (cgs.blueflag >= 0 && cgs.blueflag <= 2) {
                CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.flagShaders[cgs.blueflag]);
            } else {
                CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.flagShaders[0]);
            }
            trap_R_SetColor(NULL);
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

static void CG_DrawRedFlagStatus(rectDef_t* rect, qhandle_t shader) {
    if (cgs.gametype != GT_CTF && cgs.gametype != GT_1FCTF) {
        if (cgs.gametype == GT_HARVESTER) {
            vec4_t color = {1, 0, 0, 1};
            trap_R_SetColor(color);
            CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.redCubeIcon);
            trap_R_SetColor(NULL);
        }
        return;
    }
    if (shader) {
        CG_DrawPic(rect->x, rect->y, rect->w, rect->h, shader);
    } else {
        gitem_t* item = BG_FindItemForPowerup(PW_REDFLAG);
        if (item) {
            vec4_t color = {1, 0, 0, 1};
            trap_R_SetColor(color);
            if (cgs.redflag >= 0 && cgs.redflag <= 2) {
                CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.flagShaders[cgs.redflag]);
            } else {
                CG_DrawPic(rect->x, rect->y, rect->w, rect->h, cgs.media.flagShaders[0]);
            }
            trap_R_SetColor(NULL);
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
        if (cgs.gametype == GT_TOURNAMENT) {
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

static void CG_DrawAreaChat(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader) {
    CG_OwnerDrawText(rect->x, rect->y, scale, color, teamChat2, 0, 0, 0);
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
    int len, count;
    vec4_t newColor;
    glyphInfo_t* glyph;
    if (text) {
        const char* s = text;
        float max = *maxX;
        float useScale;
        fontInfo_t* font = &cgDC.Assets.textFont;
        if (scale <= cg_smallFont.value) {
            font = &cgDC.Assets.smallFont;
        } else if (scale > cg_bigFont.value) {
            font = &cgDC.Assets.bigFont;
        }
        useScale = scale * font->glyphScale;
        trap_R_SetColor(color);
        len = strlen(text);
        if (limit > 0 && len > limit) {
            len = limit;
        }
        count = 0;
        while (s && *s && count < len) {
            glyph = &font->glyphs[*s & 255];
            if (Q_IsColorString(s)) {
                memcpy(newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
                newColor[3] = color[3];
                trap_R_SetColor(newColor);
                s += 2;
                continue;
            } else {
                float yadj = useScale * glyph->top;
                if (CG_OwnerDrawTextWidth(s, scale, 1) + x > max) {
                    *maxX = 0;
                    break;
                }
                CG_Text_PaintChar(x, y - yadj,
                                  glyph->imageWidth,
                                  glyph->imageHeight,
                                  useScale,
                                  glyph->s,
                                  glyph->t,
                                  glyph->s2,
                                  glyph->t2,
                                  glyph->glyph);
                x += (glyph->xSkip * useScale) + adjust;
                *maxX = x;
                count++;
                s++;
            }
        }
        trap_R_SetColor(NULL);
    }
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

void CG_DrawTeamSpectators(rectDef_t* rect, float scale, vec4_t color, qhandle_t shader) {
    if (cg.spectatorLen) {
        float maxX;

        if (cg.spectatorWidth == -1) {
            cg.spectatorWidth = 0;
            cg.spectatorPaintX = rect->x + 1;
            cg.spectatorPaintX2 = -1;
        }

        if (cg.spectatorOffset > cg.spectatorLen) {
            cg.spectatorOffset = 0;
            cg.spectatorPaintX = rect->x + 1;
            cg.spectatorPaintX2 = -1;
        }

        if (cg.time > cg.spectatorTime) {
            cg.spectatorTime = cg.time + 10;
            if (cg.spectatorPaintX <= rect->x + 2) {
                if (cg.spectatorOffset < cg.spectatorLen) {
                    cg.spectatorPaintX += CG_OwnerDrawTextWidth(&cg.spectatorList[cg.spectatorOffset], scale, 1) - 1;
                    cg.spectatorOffset++;
                } else {
                    cg.spectatorOffset = 0;
                    if (cg.spectatorPaintX2 >= 0) {
                        cg.spectatorPaintX = cg.spectatorPaintX2;
                    } else {
                        cg.spectatorPaintX = rect->x + rect->w - 2;
                    }
                    cg.spectatorPaintX2 = -1;
                }
            } else {
                cg.spectatorPaintX--;
                if (cg.spectatorPaintX2 >= 0) {
                    cg.spectatorPaintX2--;
                }
            }
        }

        maxX = rect->x + rect->w - 2;
        CG_Text_Paint_Limit(&maxX, cg.spectatorPaintX, rect->y + rect->h - 3, scale, color, &cg.spectatorList[cg.spectatorOffset], 0, 0);
        if (cg.spectatorPaintX2 >= 0) {
            float maxX2 = rect->x + rect->w - 2;
            CG_Text_Paint_Limit(&maxX2, cg.spectatorPaintX2, rect->y + rect->h - 3, scale, color, cg.spectatorList, 0, cg.spectatorOffset);
        }
        if (cg.spectatorOffset && maxX > 0) {
            // if we have an offset ( we are skipping the first part of the string ) and we fit the string
            if (cg.spectatorPaintX2 == -1) {
                cg.spectatorPaintX2 = rect->x + rect->w - 2;
            }
        } else {
            cg.spectatorPaintX2 = -1;
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

static void CG_DrawPlayerCounts(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s;
    int count = 0, maxPlayers;
    int i;
    const char *info = CG_ConfigString(CS_SERVERINFO);

    // [QL] count all valid clients (including spectators, matching binary behavior)
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (cgs.clientinfo[i].infoValid) {
            count++;
        }
    }

    maxPlayers = atoi(Info_ValueForKey(info, "sv_maxclients"));
    if (maxPlayers <= 0) {
        maxPlayers = MAX_CLIENTS;
    }

    s = va("%d/%d players", count, maxPlayers);
    CG_OwnerDrawText(rect->x - (CG_OwnerDrawTextWidth(s, scale, 0) / 2), rect->y, scale, color, s, 0, 0, textStyle);
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

static void CG_DrawTeamPlayerCount(rectDef_t *rect, float scale, vec4_t color, int textStyle, int team) {
    int count = 0, i;
    for (i = 0; i < cg.numScores; i++) {
        if (cg.scores[i].team == team) count++;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%d", count), 0, 0, textStyle);
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
        va("%s - %s - %s", state, CG_GameTypeString(), cgs.mapname), 0, 0, textStyle);
}

// [QL] Match end condition
static void CG_DrawMatchEndCondition(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *s = "";
    if (cgs.fraglimit && cgs.timelimit) {
        s = va("First to %d frags or %d minutes", cgs.fraglimit, cgs.timelimit);
    } else if (cgs.fraglimit) {
        s = va("First to %d frags", cgs.fraglimit);
    } else if (cgs.timelimit) {
        s = va("%d minute time limit", cgs.timelimit);
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
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
        int roundTime = atoi(CG_ConfigString(CS_ROUND_START_TIME));
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

// [QL] Follow player name
static void CG_DrawFollowPlayerName(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (cg.snap && cg.snap->ps.pm_flags & PMF_FOLLOW) {
        int clientNum = cg.snap->ps.clientNum;
        if (clientNum >= 0 && clientNum < MAX_CLIENTS && cgs.clientinfo[clientNum].infoValid) {
            CG_OwnerDrawText(rect->x, rect->y, scale, color,
                va("Following - %s", cgs.clientinfo[clientNum].name), 0, 0, textStyle);
        }
    }
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
static void CG_DrawConfigStringValue(rectDef_t *rect, float scale, vec4_t color, int textStyle, int csNum) {
    const char *s = CG_ConfigString(csNum);
    if (s && s[0]) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
    }
}

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
    if (cg.race.active) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, "CURRENT RUN", 0, 0, textStyle);
    } else if (cg.race.finishTime) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, "LAST TIME", 0, 0, textStyle);
    }
}

static void CG_DrawRaceTimes(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int ms;
    if (cg.race.active && cg.race.startTime) {
        ms = cg.time - cg.race.startTime;
    } else if (cg.race.finishTime) {
        ms = cg.race.finishTime;
    } else {
        return;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color,
        va("%d:%02d.%03d", ms / 60000, (ms / 1000) % 60, ms % 1000), 0, 0, textStyle);
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
static void CG_DrawServerSettings(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    const char *info = CG_ConfigString(CS_SERVERINFO);
    const char *sv = Info_ValueForKey(info, "sv_hostname");
    if (sv && sv[0]) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, sv, 0, 0, textStyle);
    }
}

// [QL] Vote display helpers
static void CG_DrawVoteMapShot(rectDef_t *rect, int index, qhandle_t shader) {
    const char *info = CG_ConfigString(CS_ROTATIONMAPS);
    const char *mapname;
    qhandle_t pic;
    char key[8];

    Com_sprintf(key, sizeof(key), "map_%d", index);
    mapname = Info_ValueForKey(info, key);
    if (mapname && mapname[0]) {
        pic = trap_R_RegisterShaderNoMip(va("levelshots/%s", mapname));
        if (!pic) pic = trap_R_RegisterShaderNoMip("levelshots/preview/default");
        if (pic) CG_DrawPic(rect->x, rect->y, rect->w, rect->h, pic);
    }
}

static void CG_DrawVoteMapName(rectDef_t *rect, float scale, vec4_t color, int textStyle, int index) {
    const char *info = CG_ConfigString(CS_ROTATIONMAPS);
    const char *title;
    char key[16];

    Com_sprintf(key, sizeof(key), "title_%d", index);
    title = Info_ValueForKey(info, key);
    if (!title || !title[0]) {
        Com_sprintf(key, sizeof(key), "map_%d", index);
        title = Info_ValueForKey(info, key);
    }
    if (title && title[0]) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, title, 0, 0, textStyle);
    }
}

static void CG_DrawVoteGameType(rectDef_t *rect, float scale, vec4_t color, int textStyle, int index) {
    const char *info = CG_ConfigString(CS_ROTATIONMAPS);
    const char *gt;
    char key[8];

    Com_sprintf(key, sizeof(key), "gt_%d", index);
    gt = Info_ValueForKey(info, key);
    if (gt && gt[0]) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, gt, 0, 0, textStyle);
    }
}

static void CG_DrawVoteCount(rectDef_t *rect, float scale, vec4_t color, int textStyle, int index) {
    const char *info = CG_ConfigString(CS_ROTATIONVOTES);
    const char *count;
    char key[4];

    Com_sprintf(key, sizeof(key), "%d", index);
    count = Info_ValueForKey(info, key);
    if (count && count[0]) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, va("Votes: %s", count), 0, 0, textStyle);
    }
}

static void CG_DrawVoteTimer(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    if (cgs.voteTime) {
        int sec = (cgs.voteTime + 20000 - cg.time) / 1000;
        if (sec > 0) {
            CG_OwnerDrawText(rect->x, rect->y, scale, color,
                va("Voting ends in %d seconds.", sec), 0, 0, textStyle);
        } else {
            CG_OwnerDrawText(rect->x, rect->y, scale, color,
                "Voting has ended.", 0, 0, textStyle);
        }
    }
}

// [QL] Timeout count display
static void CG_DrawTimeoutCount(rectDef_t *rect, float scale, vec4_t color, int textStyle, int csIndex) {
    const char *s = CG_ConfigString(csIndex);
    if (s && s[0]) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
    }
}

// [QL] Duel player stat rendering - handles CG_1ST_PLYR_* and CG_2ND_PLYR_* owner draws
// playerIndex: 0 = 1st player, 1 = 2nd player
// ownerDraw: the specific stat ID to render
static void CG_DrawDuelPlayerStat(rectDef_t *rect, float scale, vec4_t color, int textStyle, int playerIndex, int ownerDraw) {
    int base = (playerIndex == 0) ? CG_1ST_PLYR : CG_2ND_PLYR;
    int offset = ownerDraw - base;
    duelScore_t *ds;
    int clientNum;
    const char *s = "";

    if (!cg.duelScoresValid) {
        // Fallback to basic score data
        clientNum = (playerIndex == 0) ? cg.duelPlayer1 : cg.duelPlayer2;
        if (clientNum < 0 || clientNum >= MAX_CLIENTS) return;

        switch (offset) {
            case 0:  // CG_xST_PLYR (player model/entity - no-op in text context)
                break;
            case 1:  // READY
                break;
            case 2:  // SCORE
                s = va("%d", cgs.clientinfo[clientNum].score);
                break;
            case 7:  // PING
                break;
            case 8:  // WINS
                s = va("%d", cgs.clientinfo[clientNum].wins);
                break;
            default:
                break;
        }
        if (s[0]) CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
        return;
    }

    ds = &cg.duelScores[playerIndex];

    switch (offset) {
        case 0:  break;  // CG_xST_PLYR - player model, handled by feeder/model rendering
        case 1:  // READY
            if (cg.warmup) {
                // Check scoreFlags for ready status
                s = (ds->score) ? "READY" : "";
            }
            break;
        case 2:  s = va("%d", ds->score); break;   // SCORE
        case 3:  s = va("%d", ds->kills); break;   // FRAGS
        case 4:  s = va("%d", ds->deaths); break;  // DEATHS
        case 5:  s = va("%d", ds->damage); break;  // DMG
        case 6:  s = va("%d:%02d", ds->time / 60, ds->time % 60); break;  // TIME
        case 7:  s = va("%d", ds->ping); break;    // PING
        case 8:  // WINS
            clientNum = ds->clientNum;
            if (clientNum >= 0 && clientNum < MAX_CLIENTS)
                s = va("%d", cgs.clientinfo[clientNum].wins);
            break;
        case 9:  s = va("%d%%", ds->accuracy); break;  // ACC
        case 10: break;  // FLAG - flag icon (no-op)
        case 11: break;  // AVATAR - player head model (no-op for text)
        case 12: // TIMEOUT_COUNT
            s = CG_ConfigString(playerIndex == 0 ? CS_TIMEOUTS_RED : CS_TIMEOUTS_BLUE);
            break;
        case 13: // HEALTH_ARMOR - combined display
            break;
        // Per-weapon frags: offsets 14-26 (G,MG,SG,GL,RL,LG,RG,PG,BFG,CG,NG,PL,HMG)
        case 14: case 15: case 16: case 17: case 18: case 19: case 20:
        case 21: case 22: case 23: case 24: case 25: case 26: {
            // Weapon indices: G=1,MG=2,SG=3,GL=4,RL=5,LG=6,RG=7,PG=8,BFG=9,CG=11,NG=12,PL=13,HMG=14
            static const int wpMap[] = {1,2,3,4,5,6,7,8,9,11,12,13,14};
            int wpIdx = offset - 14;
            if (wpIdx >= 0 && wpIdx < 13 && wpMap[wpIdx] < MAX_WEAPONS) {
                s = va("%d", ds->weaponStats[wpMap[wpIdx]].kills);
            }
            break;
        }
        // Per-weapon hits: offsets 27-38 (MG,SG,GL,RL,LG,RG,PG,BFG,CG,NG,PL,HMG)
        case 27: case 28: case 29: case 30: case 31: case 32: case 33:
        case 34: case 35: case 36: case 37: case 38: {
            static const int wpMap[] = {2,3,4,5,6,7,8,9,11,12,13,14};
            int wpIdx = offset - 27;
            if (wpIdx >= 0 && wpIdx < 12 && wpMap[wpIdx] < MAX_WEAPONS) {
                s = va("%d", ds->weaponStats[wpMap[wpIdx]].hits);
            }
            break;
        }
        // Per-weapon shots: offsets 39-50
        case 39: case 40: case 41: case 42: case 43: case 44: case 45:
        case 46: case 47: case 48: case 49: case 50: {
            static const int wpMap[] = {2,3,4,5,6,7,8,9,11,12,13,14};
            int wpIdx = offset - 39;
            if (wpIdx >= 0 && wpIdx < 12 && wpMap[wpIdx] < MAX_WEAPONS) {
                s = va("%d", ds->weaponStats[wpMap[wpIdx]].atts);
            }
            break;
        }
        // Per-weapon damage: offsets 51-63 (G,MG,SG,GL,RL,LG,RG,PG,BFG,CG,NG,PL,HMG)
        case 51: case 52: case 53: case 54: case 55: case 56: case 57:
        case 58: case 59: case 60: case 61: case 62: case 63: {
            static const int wpMap[] = {1,2,3,4,5,6,7,8,9,11,12,13,14};
            int wpIdx = offset - 51;
            if (wpIdx >= 0 && wpIdx < 13 && wpMap[wpIdx] < MAX_WEAPONS) {
                s = va("%d", ds->weaponStats[wpMap[wpIdx]].damage);
            }
            break;
        }
        // Per-weapon accuracy: offsets 64-75 (MG,SG,GL,RL,LG,RG,PG,BFG,CG,NG,PL,HMG)
        case 64: case 65: case 66: case 67: case 68: case 69: case 70:
        case 71: case 72: case 73: case 74: case 75: {
            static const int wpMap[] = {2,3,4,5,6,7,8,9,11,12,13,14};
            int wpIdx = offset - 64;
            if (wpIdx >= 0 && wpIdx < 12 && wpMap[wpIdx] < MAX_WEAPONS) {
                s = va("%d%%", ds->weaponStats[wpMap[wpIdx]].accuracy);
            }
            break;
        }
        // Item pickups: offsets 76-80
        case 76: // PICKUPS (total)
            s = va("%d", ds->redArmorPickups + ds->yellowArmorPickups + ds->greenArmorPickups + ds->megaHealthPickups);
            break;
        case 77: s = va("%d", ds->redArmorPickups); break;     // PICKUPS_RA
        case 78: s = va("%d", ds->yellowArmorPickups); break;  // PICKUPS_YA
        case 79: s = va("%d", ds->greenArmorPickups); break;   // PICKUPS_GA
        case 80: s = va("%d", ds->megaHealthPickups); break;   // PICKUPS_MH
        // Avg pickup times: offsets 81-84
        case 81: s = va("%.1f", ds->redArmorTime); break;
        case 82: s = va("%.1f", ds->yellowArmorTime); break;
        case 83: s = va("%.1f", ds->greenArmorTime); break;
        case 84: s = va("%.1f", ds->megaHealthTime); break;
        // Awards: offsets 85-89
        case 85: s = va("%d", ds->awardExcellent); break;     // EXCELLENT
        case 86: s = va("%d", ds->awardImpressive); break;    // IMPRESSIVE
        case 87: s = va("%d", ds->awardHumiliation); break;   // HUMILIATION
        case 88: break;  // PR (premium rating - not available)
        case 89: break;  // TIER (premium tier - not available)
        default: break;
    }

    if (s[0]) {
        CG_OwnerDrawText(rect->x, rect->y, scale, color, s, 0, 0, textStyle);
    }
}

// [QL] Team pickup stats display
static void CG_DrawTeamPickupStat(rectDef_t *rect, float scale, vec4_t color, int textStyle, int ownerDraw) {
    int val = 0;
    if (!cg.teamPickups.valid) return;

    switch (ownerDraw) {
        case CG_RED_TEAM_PICKUPS_RA: val = cg.teamPickups.rra; break;
        case CG_RED_TEAM_PICKUPS_YA: val = cg.teamPickups.rya; break;
        case CG_RED_TEAM_PICKUPS_GA: val = cg.teamPickups.rga; break;
        case CG_RED_TEAM_PICKUPS_MH: val = cg.teamPickups.rmh; break;
        case CG_RED_TEAM_PICKUPS_QUAD: val = cg.teamPickups.rquad; break;
        case CG_RED_TEAM_PICKUPS_BS: val = cg.teamPickups.rbs; break;
        case CG_RED_TEAM_TIMEHELD_QUAD: val = cg.teamPickups.rquadTime; break;
        case CG_RED_TEAM_TIMEHELD_BS: val = cg.teamPickups.rbsTime; break;
        case CG_BLUE_TEAM_PICKUPS_RA: val = cg.teamPickups.bra; break;
        case CG_BLUE_TEAM_PICKUPS_YA: val = cg.teamPickups.bya; break;
        case CG_BLUE_TEAM_PICKUPS_GA: val = cg.teamPickups.bga; break;
        case CG_BLUE_TEAM_PICKUPS_MH: val = cg.teamPickups.bmh; break;
        case CG_BLUE_TEAM_PICKUPS_QUAD: val = cg.teamPickups.bquad; break;
        case CG_BLUE_TEAM_PICKUPS_BS: val = cg.teamPickups.bbs; break;
        case CG_BLUE_TEAM_TIMEHELD_QUAD: val = cg.teamPickups.bquadTime; break;
        case CG_BLUE_TEAM_TIMEHELD_BS: val = cg.teamPickups.bbsTime; break;
        default: return;
    }
    CG_OwnerDrawText(rect->x, rect->y, scale, color, va("%d", val), 0, 0, textStyle);
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

        // draw square icon: w×w size, spaced vertically by rect->h
        CG_DrawPic(rect->x, rect->h * count + rect->y, rect->w, rect->w, icon);
        count++;
    }

    trap_R_SetColor(NULL);
}

// [QL] CG_ACC_VERTICAL (ownerdraw 98): draw per-weapon accuracy percentages vertically
// Binary: FUN_10035b10 - same loop as CG_WP_VERTICAL, draws "%i%%" text at each slot
static void CG_DrawAccuracyVertical(rectDef_t *rect, float scale, vec4_t color, int textStyle) {
    int i;
    int count = 0;

    trap_R_SetColor(color);

    for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
        if (i == WP_BFG) {
            continue;
        }

        if (cg_weapons[i].weaponIcon == 0) {
            continue;
        }

        if (cg.accuracyStats.valid && cg.accuracyStats.accuracy[i] > 0) {
            const char *s = va("%i%%", cg.accuracyStats.accuracy[i]);
            CG_OwnerDrawText(rect->x, rect->h * count + rect->y, scale, color, s, 0, 0, textStyle);
        }
        count++;
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

    switch (ownerDraw) {
        case CG_PLAYER_ARMOR_ICON:
            CG_DrawPlayerArmorIcon(&rect, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYER_ARMOR_ICON2D:
            CG_DrawPlayerArmorIcon(&rect, qtrue);
            break;
        case CG_PLAYER_ARMOR_VALUE:
            CG_DrawPlayerArmorValue(&rect, scale, color, shader, textStyle, align);
            break;
        case CG_PLAYER_AMMO_ICON:
            CG_DrawPlayerAmmoIcon(&rect, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYER_AMMO_ICON2D:
            CG_DrawPlayerAmmoIcon(&rect, qtrue);
            break;
        case CG_PLAYER_AMMO_VALUE:
            CG_DrawPlayerAmmoValue(&rect, scale, color, shader, textStyle, align);
            break;
        case CG_PLAYER_HEAD:
            CG_DrawPlayerHead(&rect, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYER_ITEM:
            CG_DrawPlayerItem(&rect, scale, ownerDrawFlags & CG_SHOW_2DONLY);
            break;
        case CG_PLAYER_SCORE:
            CG_DrawPlayerScore(&rect, scale, color, shader, textStyle);
            break;
        case CG_PLAYER_HEALTH:
            CG_DrawPlayerHealth(&rect, scale, color, shader, textStyle, align);
            break;
        case CG_RED_SCORE:
            CG_DrawRedScore(&rect, scale, color, shader, textStyle);
            break;
        case CG_BLUE_SCORE:
            CG_DrawBlueScore(&rect, scale, color, shader, textStyle);
            break;
        case CG_BLUE_FLAGSTATUS:
            CG_DrawBlueFlagStatus(&rect, shader);
            break;
        case CG_RED_FLAGSTATUS:
            CG_DrawRedFlagStatus(&rect, shader);
            break;
        case CG_HARVESTER_SKULLS:
            CG_HarvesterSkulls(&rect, scale, color, qfalse, textStyle);
            break;
        case CG_HARVESTER_SKULLS2D:
            CG_HarvesterSkulls(&rect, scale, color, qtrue, textStyle);
            break;
        case CG_ONEFLAG_STATUS:
            CG_OneFlagStatus(&rect);
            break;
        case CG_TEAM_COLOR:
            CG_DrawTeamColor(&rect, color);
            break;
        case CG_CTF_POWERUP:
            CG_DrawCTFPowerUp(&rect);
            break;
        case CG_AREA_POWERUP:
            CG_DrawAreaPowerUp(&rect, align, special, scale, color);
            break;
        case CG_PLAYER_HASFLAG:
            CG_DrawPlayerHasFlag(&rect, qfalse);
            break;
        case CG_PLAYER_HASFLAG2D:
            CG_DrawPlayerHasFlag(&rect, qtrue);
            break;
        case CG_GAME_TYPE:
            CG_DrawGameType(&rect, scale, color, shader, textStyle);
            break;
        case CG_GAME_STATUS:
            CG_DrawGameStatus(&rect, scale, color, shader, textStyle);
            break;
        case CG_KILLER:
            CG_DrawKiller(&rect, scale, color, shader, textStyle);
            break;
        case CG_ACCURACY:
        case CG_ASSISTS:
        case CG_DEFEND:
        case CG_EXCELLENT:
        case CG_IMPRESSIVE:
        case CG_PERFECT:
        case CG_GAUNTLET:
        case CG_CAPTURES:
            CG_DrawMedal(ownerDraw, &rect, scale, color, shader);
            break;
        case CG_SPECTATORS:
            CG_DrawTeamSpectators(&rect, scale, color, shader);
            break;
        case CG_CAPFRAGLIMIT:
            CG_DrawCapFragLimit(&rect, scale, color, shader, textStyle);
            break;
        case CG_1STPLACE:
            CG_Draw1stPlace(&rect, scale, color, shader, textStyle);
            break;
        case CG_2NDPLACE:
            CG_Draw2ndPlace(&rect, scale, color, shader, textStyle);
            break;

        // [QL] scoreboard owner draws
        case CG_MAP_NAME:
            CG_DrawMapName(&rect, scale, color, textStyle);
            break;
        case CG_LEVELTIMER:
            CG_DrawLevelTimer(&rect, scale, color, textStyle);
            break;
        case CG_PLAYER_COUNTS:
            CG_DrawPlayerCounts(&rect, scale, color, textStyle);
            break;
        case CG_SELECTED_PLYR_TEAM_COLOR:
            CG_DrawSelectedPlayerTeamColor(&rect);
            break;
        case CG_SELECTED_PLYR_ACCURACY:
            CG_DrawSelectedPlayerAccuracy(&rect, scale, color, textStyle);
            break;
        case CG_PLYR_BEST_WEAPON_NAME:
            CG_DrawBestWeaponName(&rect, scale, color, textStyle);
            break;
        case CG_RED_PLAYER_COUNT:
            CG_DrawTeamPlayerCount(&rect, scale, color, textStyle, TEAM_RED);
            break;
        case CG_BLUE_PLAYER_COUNT:
            CG_DrawTeamPlayerCount(&rect, scale, color, textStyle, TEAM_BLUE);
            break;
        case CG_AREA_NEW_CHAT:
            CG_DrawAreaChat(&rect, scale, color, shader);
            break;
        // [QL] game info owner draws
        case CG_SERVER_SETTINGS:
            CG_DrawServerSettings(&rect, scale, color, textStyle);
            break;
        case CG_STARTING_WEAPONS:
            CG_DrawStartingWeapons(&rect, scale, color, textStyle);
            break;
        case CG_GAME_LIMIT:
            CG_DrawGameLimit(&rect, scale, color, textStyle);
            break;
        case CG_GAME_TYPE_ICON:
            CG_DrawGameTypeIcon(&rect);
            break;
        case CG_GAME_TYPE_MAP:
            CG_DrawMatchDetails(&rect, scale, color, textStyle);
            break;
        case CG_MATCH_DETAILS:
            CG_DrawMatchDetails(&rect, scale, color, textStyle);
            break;
        case CG_MATCH_END_CONDITION:
            CG_DrawMatchEndCondition(&rect, scale, color, textStyle);
            break;
        case CG_MATCH_STATUS:
            CG_DrawMatchStatus(&rect, scale, color, textStyle);
            break;
        case CG_MATCH_STATE:
            // [QL] force Handel Gothic (fontIndex 1) - pak00 menu omits font directive
            cg_currentFontIndex = fontIndex ? fontIndex : 1;
            CG_DrawMatchState(&rect, scale, color, textStyle);
            break;
        case CG_MATCH_WINNER:
            CG_DrawMatchWinner(&rect, scale, color, textStyle);
            break;
        case CG_ROUND:
            CG_DrawRound(&rect, scale, color, textStyle);
            break;
        case CG_ROUNDTIMER:
            CG_DrawRoundTimer(&rect, scale, color, textStyle);
            break;
        case CG_OVERTIME:
            CG_DrawOvertime(&rect, scale, color, textStyle);
            break;
        case CG_LOCALTIME:
            CG_DrawLocalTime(&rect, scale, color, textStyle);
            break;

        // [QL] follow/spec
        case CG_FOLLOW_PLAYER_NAME:
        case CG_FOLLOW_PLAYER_NAME_EX:
            CG_DrawFollowPlayerName(&rect, scale, color, textStyle);
            break;
        case CG_SPEC_MESSAGES:
            break;  // spectator messages - no standard data source

        // [QL] team names
        case CG_RED_NAME:
            CG_DrawRedName(&rect, scale, color, textStyle);
            break;
        case CG_BLUE_NAME:
            CG_DrawBlueName(&rect, scale, color, textStyle);
            break;
        case CG_RED_AVG_PING:
            CG_DrawTeamAvgPing(&rect, scale, color, textStyle, TEAM_RED);
            break;
        case CG_BLUE_AVG_PING:
            CG_DrawTeamAvgPing(&rect, scale, color, textStyle, TEAM_BLUE);
            break;

        // [QL] health/armor bars (image-masked with texture coordinate cropping)
        case CG_PLAYER_HEALTH_BAR_100:
            CG_DrawHealthBar100(&rect, shader);
            break;
        case CG_PLAYER_HEALTH_BAR_200:
            CG_DrawHealthBar200(&rect, shader);
            break;
        case CG_PLAYER_ARMOR_BAR_100:
            CG_DrawArmorBar100(&rect, shader);
            break;
        case CG_PLAYER_ARMOR_BAR_200:
            CG_DrawArmorBar200(&rect, shader);
            break;

        // [QL] colorized displays
        case CG_HEALTH_COLORIZED:
            CG_DrawHealthColorized(&rect, scale, color, textStyle);
            break;
        case CG_TEAM_COLORIZED:
            CG_DrawTeamColorized(&rect, color, shader);
            break;
        case CG_ARMORTIERED_COLORIZED:
            CG_DrawArmorTieredColorized(&rect, color, shader);
            break;
        case CG_TEAM_PLYR_COUNT:
            CG_DrawTeamPlyrCount(&rect, scale, color, textStyle);
            break;
        case CG_ENEMY_PLYR_COUNT:
            CG_DrawEnemyPlyrCount(&rect, scale, color, textStyle);
            break;

        // [QL] race mode
        case CG_RACE_STATUS:
            CG_DrawRaceStatus(&rect, scale, color, textStyle);
            break;
        case CG_RACE_TIMES:
            CG_DrawRaceTimes(&rect, scale, color, textStyle);
            break;
        case CG_SPEEDOMETER:
            CG_DrawSpeedometer(&rect, scale, color, textStyle);
            break;

        // [QL] place scores
        case CG_1ST_PLACE_SCORE:
            CG_DrawPlaceScore(&rect, scale, color, textStyle, 1);
            break;
        case CG_2ND_PLACE_SCORE:
            CG_DrawPlaceScore(&rect, scale, color, textStyle, 2);
            break;
        case CG_PLYR_END_GAME_SCORE:
            CG_DrawPlayerEndGameScore(&rect, scale, color, textStyle);
            break;

        // [QL] configstring-based award displays
        case CG_MOST_DAMAGEDEALT_PLYR:
            CG_DrawConfigStringValue(&rect, scale, color, textStyle, CS_MOST_DAMAGEDEALT_PLYR);
            break;
        case CG_MOST_ACCURATE_PLYR:
            CG_DrawConfigStringValue(&rect, scale, color, textStyle, CS_MOST_ACCURATE_PLYR);
            break;
        case CG_BEST_ITEMCONTROL_PLYR:
            CG_DrawConfigStringValue(&rect, scale, color, textStyle, CS_BEST_ITEMCONTROL_PLYR);
            break;
        case CG_MOST_VALUABLE_OFFENSIVE_PLYR:
            CG_DrawConfigStringValue(&rect, scale, color, textStyle, CS_MOST_VALUABLE_OFFENSIVE_PLYR);
            break;
        case CG_MOST_VALUABLE_DEFENSIVE_PLYR:
            CG_DrawConfigStringValue(&rect, scale, color, textStyle, CS_MOST_VALUABLE_DEFENSIVE_PLYR);
            break;
        case CG_MOST_VALUABLE_PLYR:
            CG_DrawConfigStringValue(&rect, scale, color, textStyle, CS_MOST_VALUABLE_PLYR);
            break;

        // [QL] vote system
        case CG_VOTESHOT1:
            CG_DrawVoteMapShot(&rect, 0, shader);
            break;
        case CG_VOTESHOT2:
            CG_DrawVoteMapShot(&rect, 1, shader);
            break;
        case CG_VOTESHOT3:
            CG_DrawVoteMapShot(&rect, 2, shader);
            break;
        case CG_VOTENAME1:
            CG_DrawVoteMapName(&rect, scale, color, textStyle, 0);
            break;
        case CG_VOTENAME2:
            CG_DrawVoteMapName(&rect, scale, color, textStyle, 1);
            break;
        case CG_VOTENAME3:
            CG_DrawVoteMapName(&rect, scale, color, textStyle, 2);
            break;
        case CG_VOTEGAMETYPE1:
            CG_DrawVoteGameType(&rect, scale, color, textStyle, 0);
            break;
        case CG_VOTEGAMETYPE2:
            CG_DrawVoteGameType(&rect, scale, color, textStyle, 1);
            break;
        case CG_VOTEGAMETYPE3:
            CG_DrawVoteGameType(&rect, scale, color, textStyle, 2);
            break;
        case CG_VOTECOUNT1:
            CG_DrawVoteCount(&rect, scale, color, textStyle, 0);
            break;
        case CG_VOTECOUNT2:
            CG_DrawVoteCount(&rect, scale, color, textStyle, 1);
            break;
        case CG_VOTECOUNT3:
            CG_DrawVoteCount(&rect, scale, color, textStyle, 2);
            break;
        case CG_VOTETIMER:
            CG_DrawVoteTimer(&rect, scale, color, textStyle);
            break;
        case CG_VOTEMAP1:
        case CG_VOTEMAP2:
        case CG_VOTEMAP3:
            break;  // map preview images handled by VOTESHOT

        // [QL] timeout counts (now functional)
        case CG_RED_TIMEOUT_COUNT:
            CG_DrawTimeoutCount(&rect, scale, color, textStyle, CS_TIMEOUTS_RED);
            break;
        case CG_BLUE_TIMEOUT_COUNT:
            CG_DrawTimeoutCount(&rect, scale, color, textStyle, CS_TIMEOUTS_BLUE);
            break;

        // [QL] duel player stats (1st player: IDs 103-192, 2nd player: IDs 193-282)
        case CG_1ST_PLYR: case CG_1ST_PLYR_READY: case CG_1ST_PLYR_SCORE:
        case CG_1ST_PLYR_FRAGS: case CG_1ST_PLYR_DEATHS: case CG_1ST_PLYR_DMG:
        case CG_1ST_PLYR_TIME: case CG_1ST_PLYR_PING: case CG_1ST_PLYR_WINS:
        case CG_1ST_PLYR_ACC: case CG_1ST_PLYR_FLAG: case CG_1ST_PLYR_AVATAR:
        case CG_1ST_PLYR_TIMEOUT_COUNT: case CG_1ST_PLYR_HEALTH_ARMOR:
        case CG_1ST_PLYR_FRAGS_G: case CG_1ST_PLYR_FRAGS_MG: case CG_1ST_PLYR_FRAGS_SG:
        case CG_1ST_PLYR_FRAGS_GL: case CG_1ST_PLYR_FRAGS_RL: case CG_1ST_PLYR_FRAGS_LG:
        case CG_1ST_PLYR_FRAGS_RG: case CG_1ST_PLYR_FRAGS_PG: case CG_1ST_PLYR_FRAGS_BFG:
        case CG_1ST_PLYR_FRAGS_CG: case CG_1ST_PLYR_FRAGS_NG: case CG_1ST_PLYR_FRAGS_PL:
        case CG_1ST_PLYR_FRAGS_HMG:
        case CG_1ST_PLYR_HITS_MG: case CG_1ST_PLYR_HITS_SG: case CG_1ST_PLYR_HITS_GL:
        case CG_1ST_PLYR_HITS_RL: case CG_1ST_PLYR_HITS_LG: case CG_1ST_PLYR_HITS_RG:
        case CG_1ST_PLYR_HITS_PG: case CG_1ST_PLYR_HITS_BFG: case CG_1ST_PLYR_HITS_CG:
        case CG_1ST_PLYR_HITS_NG: case CG_1ST_PLYR_HITS_PL: case CG_1ST_PLYR_HITS_HMG:
        case CG_1ST_PLYR_SHOTS_MG: case CG_1ST_PLYR_SHOTS_SG: case CG_1ST_PLYR_SHOTS_GL:
        case CG_1ST_PLYR_SHOTS_RL: case CG_1ST_PLYR_SHOTS_LG: case CG_1ST_PLYR_SHOTS_RG:
        case CG_1ST_PLYR_SHOTS_PG: case CG_1ST_PLYR_SHOTS_BFG: case CG_1ST_PLYR_SHOTS_CG:
        case CG_1ST_PLYR_SHOTS_NG: case CG_1ST_PLYR_SHOTS_PL: case CG_1ST_PLYR_SHOTS_HMG:
        case CG_1ST_PLYR_DMG_G: case CG_1ST_PLYR_DMG_MG: case CG_1ST_PLYR_DMG_SG:
        case CG_1ST_PLYR_DMG_GL: case CG_1ST_PLYR_DMG_RL: case CG_1ST_PLYR_DMG_LG:
        case CG_1ST_PLYR_DMG_RG: case CG_1ST_PLYR_DMG_PG: case CG_1ST_PLYR_DMG_BFG:
        case CG_1ST_PLYR_DMG_CG: case CG_1ST_PLYR_DMG_NG: case CG_1ST_PLYR_DMG_PL:
        case CG_1ST_PLYR_DMG_HMG:
        case CG_1ST_PLYR_ACC_MG: case CG_1ST_PLYR_ACC_SG: case CG_1ST_PLYR_ACC_GL:
        case CG_1ST_PLYR_ACC_RL: case CG_1ST_PLYR_ACC_LG: case CG_1ST_PLYR_ACC_RG:
        case CG_1ST_PLYR_ACC_PG: case CG_1ST_PLYR_ACC_BFG: case CG_1ST_PLYR_ACC_CG:
        case CG_1ST_PLYR_ACC_NG: case CG_1ST_PLYR_ACC_PL: case CG_1ST_PLYR_ACC_HMG:
        case CG_1ST_PLYR_PICKUPS: case CG_1ST_PLYR_PICKUPS_RA: case CG_1ST_PLYR_PICKUPS_YA:
        case CG_1ST_PLYR_PICKUPS_GA: case CG_1ST_PLYR_PICKUPS_MH:
        case CG_1ST_PLYR_AVG_PICKUP_TIME_RA: case CG_1ST_PLYR_AVG_PICKUP_TIME_YA:
        case CG_1ST_PLYR_AVG_PICKUP_TIME_GA: case CG_1ST_PLYR_AVG_PICKUP_TIME_MH:
        case CG_1ST_PLYR_EXCELLENT: case CG_1ST_PLYR_IMPRESSIVE:
        case CG_1ST_PLYR_HUMILIATION: case CG_1ST_PLYR_PR: case CG_1ST_PLYR_TIER:
            CG_DrawDuelPlayerStat(&rect, scale, color, textStyle, 0, ownerDraw);
            break;

        case CG_2ND_PLYR: case CG_2ND_PLYR_READY: case CG_2ND_PLYR_SCORE:
        case CG_2ND_PLYR_FRAGS: case CG_2ND_PLYR_DEATHS: case CG_2ND_PLYR_DMG:
        case CG_2ND_PLYR_TIME: case CG_2ND_PLYR_PING: case CG_2ND_PLYR_WINS:
        case CG_2ND_PLYR_ACC: case CG_2ND_PLYR_FLAG: case CG_2ND_PLYR_AVATAR:
        case CG_2ND_PLYR_TIMEOUT_COUNT: case CG_2ND_PLYR_HEALTH_ARMOR:
        case CG_2ND_PLYR_FRAGS_G: case CG_2ND_PLYR_FRAGS_MG: case CG_2ND_PLYR_FRAGS_SG:
        case CG_2ND_PLYR_FRAGS_GL: case CG_2ND_PLYR_FRAGS_RL: case CG_2ND_PLYR_FRAGS_LG:
        case CG_2ND_PLYR_FRAGS_RG: case CG_2ND_PLYR_FRAGS_PG: case CG_2ND_PLYR_FRAGS_BFG:
        case CG_2ND_PLYR_FRAGS_CG: case CG_2ND_PLYR_FRAGS_NG: case CG_2ND_PLYR_FRAGS_PL:
        case CG_2ND_PLYR_FRAGS_HMG:
        case CG_2ND_PLYR_HITS_MG: case CG_2ND_PLYR_HITS_SG: case CG_2ND_PLYR_HITS_GL:
        case CG_2ND_PLYR_HITS_RL: case CG_2ND_PLYR_HITS_LG: case CG_2ND_PLYR_HITS_RG:
        case CG_2ND_PLYR_HITS_PG: case CG_2ND_PLYR_HITS_BFG: case CG_2ND_PLYR_HITS_CG:
        case CG_2ND_PLYR_HITS_NG: case CG_2ND_PLYR_HITS_PL: case CG_2ND_PLYR_HITS_HMG:
        case CG_2ND_PLYR_SHOTS_MG: case CG_2ND_PLYR_SHOTS_SG: case CG_2ND_PLYR_SHOTS_GL:
        case CG_2ND_PLYR_SHOTS_RL: case CG_2ND_PLYR_SHOTS_LG: case CG_2ND_PLYR_SHOTS_RG:
        case CG_2ND_PLYR_SHOTS_PG: case CG_2ND_PLYR_SHOTS_BFG: case CG_2ND_PLYR_SHOTS_CG:
        case CG_2ND_PLYR_SHOTS_NG: case CG_2ND_PLYR_SHOTS_PL: case CG_2ND_PLYR_SHOTS_HMG:
        case CG_2ND_PLYR_DMG_G: case CG_2ND_PLYR_DMG_MG: case CG_2ND_PLYR_DMG_SG:
        case CG_2ND_PLYR_DMG_GL: case CG_2ND_PLYR_DMG_RL: case CG_2ND_PLYR_DMG_LG:
        case CG_2ND_PLYR_DMG_RG: case CG_2ND_PLYR_DMG_PG: case CG_2ND_PLYR_DMG_BFG:
        case CG_2ND_PLYR_DMG_CG: case CG_2ND_PLYR_DMG_NG: case CG_2ND_PLYR_DMG_PL:
        case CG_2ND_PLYR_DMG_HMG:
        case CG_2ND_PLYR_ACC_MG: case CG_2ND_PLYR_ACC_SG: case CG_2ND_PLYR_ACC_GL:
        case CG_2ND_PLYR_ACC_RL: case CG_2ND_PLYR_ACC_LG: case CG_2ND_PLYR_ACC_RG:
        case CG_2ND_PLYR_ACC_PG: case CG_2ND_PLYR_ACC_BFG: case CG_2ND_PLYR_ACC_CG:
        case CG_2ND_PLYR_ACC_NG: case CG_2ND_PLYR_ACC_PL: case CG_2ND_PLYR_ACC_HMG:
        case CG_2ND_PLYR_PICKUPS: case CG_2ND_PLYR_PICKUPS_RA: case CG_2ND_PLYR_PICKUPS_YA:
        case CG_2ND_PLYR_PICKUPS_GA: case CG_2ND_PLYR_PICKUPS_MH:
        case CG_2ND_PLYR_AVG_PICKUP_TIME_RA: case CG_2ND_PLYR_AVG_PICKUP_TIME_YA:
        case CG_2ND_PLYR_AVG_PICKUP_TIME_GA: case CG_2ND_PLYR_AVG_PICKUP_TIME_MH:
        case CG_2ND_PLYR_EXCELLENT: case CG_2ND_PLYR_IMPRESSIVE:
        case CG_2ND_PLYR_HUMILIATION: case CG_2ND_PLYR_PR: case CG_2ND_PLYR_TIER:
            CG_DrawDuelPlayerStat(&rect, scale, color, textStyle, 1, ownerDraw);
            break;

        // [QL] team pickup stats
        case CG_RED_TEAM_MAP_PICKUPS:
        case CG_RED_TEAM_PICKUPS_RA: case CG_RED_TEAM_PICKUPS_YA:
        case CG_RED_TEAM_PICKUPS_GA: case CG_RED_TEAM_PICKUPS_MH:
        case CG_RED_TEAM_PICKUPS_QUAD: case CG_RED_TEAM_PICKUPS_BS:
        case CG_RED_TEAM_TIMEHELD_QUAD: case CG_RED_TEAM_TIMEHELD_BS:
        case CG_BLUE_TEAM_MAP_PICKUPS:
        case CG_BLUE_TEAM_PICKUPS_RA: case CG_BLUE_TEAM_PICKUPS_YA:
        case CG_BLUE_TEAM_PICKUPS_GA: case CG_BLUE_TEAM_PICKUPS_MH:
        case CG_BLUE_TEAM_PICKUPS_QUAD: case CG_BLUE_TEAM_PICKUPS_BS:
        case CG_BLUE_TEAM_TIMEHELD_QUAD: case CG_BLUE_TEAM_TIMEHELD_BS:
            CG_DrawTeamPickupStat(&rect, scale, color, textStyle, ownerDraw);
            break;

        // [QL] remaining team-specific owner draws (flag/clan/pickups not yet tracked)
        case CG_RED_OWNED_FLAGS: case CG_RED_BASESTATUS:
        case CG_RED_CLAN_PLYRS:
        case CG_RED_TEAM_PICKUPS_FLAG: case CG_RED_TEAM_PICKUPS_MEDKIT:
        case CG_RED_TEAM_PICKUPS_REGEN: case CG_RED_TEAM_PICKUPS_HASTE:
        case CG_RED_TEAM_PICKUPS_INVIS:
        case CG_RED_TEAM_TIMEHELD_FLAG: case CG_RED_TEAM_TIMEHELD_REGEN:
        case CG_RED_TEAM_TIMEHELD_HASTE: case CG_RED_TEAM_TIMEHELD_INVIS:
        case CG_BLUE_OWNED_FLAGS: case CG_BLUE_BASESTATUS:
        case CG_BLUE_CLAN_PLYRS:
        case CG_BLUE_TEAM_PICKUPS_FLAG: case CG_BLUE_TEAM_PICKUPS_MEDKIT:
        case CG_BLUE_TEAM_PICKUPS_REGEN: case CG_BLUE_TEAM_PICKUPS_HASTE:
        case CG_BLUE_TEAM_PICKUPS_INVIS:
        case CG_BLUE_TEAM_TIMEHELD_FLAG: case CG_BLUE_TEAM_TIMEHELD_REGEN:
        case CG_BLUE_TEAM_TIMEHELD_HASTE: case CG_BLUE_TEAM_TIMEHELD_INVIS:
            break;  // no data source for these yet

        // [QL] misc owner draws - no-op or not applicable
        case CG_PLAYER_OBIT:
            CG_DrawPlayerObit(&rect, scale, color, textStyle);
            break;
        case CG_WP_VERTICAL:
            CG_DrawWeaponVertical(&rect, color);
            break;
        case CG_ACC_VERTICAL:
            CG_DrawAccuracyVertical(&rect, scale, color, textStyle);
            break;
        case CG_PLAYERMODEL:
        case CG_1STPLACE_PLYR_MODEL: case CG_1STPLACE_PLYR_MODEL_ACTIVE:
        case CG_PLAYER_HASKEY:
        {
            int keys = cg.snap->ps.stats[STAT_KEY];
            if (keys) {
                float xOffset = 0;
                int i;
                for (i = 0; i < 3; i++) {
                    int keyBit = (1 << i);  // KEY_SILVER=1, KEY_GOLD=2, KEY_MASTER=4
                    if (keys & keyBit) {
                        int n;
                        for (n = 1; n < bg_numItems; n++) {
                            if (bg_itemlist[n].giType == IT_KEY && bg_itemlist[n].giTag == keyBit) {
                                CG_RegisterItemVisuals(n);
                                CG_DrawPic(rect.x + xOffset, rect.y, rect.w, rect.h, cg_items[n].icon);
                                xOffset += rect.w * 0.5f;
                                break;
                            }
                        }
                    }
                }
            }
            break;
        }
        case CG_FLAG_STATUS:
        case CG_COMBOKILLS: case CG_RAMPAGES: case CG_MIDAIRS:
            break;
        case UI_ADVERT:
        {
            // [QL] draw ad content image - menu provides defaultContent path via shader param
            qhandle_t adShader = trap_R_RegisterShaderNoMip("textures/ad_content/ad2x1");
            if (adShader) {
                CG_DrawPic(rect.x, rect.y, rect.w, rect.h, adShader);
            }
            break;
        }
        default:
            break;
    }
}

void CG_MouseEvent(int x, int y) {
    int n;

    if ((cg.predictedPlayerState.pm_type == PM_NORMAL || cg.predictedPlayerState.pm_type == PM_SPECTATOR) && cg.showScores == qfalse) {
        trap_Key_SetCatcher(0);
        return;
    }

    cgs.cursorX += x;
    if (cgs.cursorX < 0)
        cgs.cursorX = 0;
    else if (cgs.cursorX > SCREEN_WIDTH)
        cgs.cursorX = SCREEN_WIDTH;

    cgs.cursorY += y;
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
        Display_MouseMove(cgs.capturedItem, x, y);
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

void CG_KeyEvent(int key, qboolean down) {
    if (!down) {
        return;
    }

    if (cg.predictedPlayerState.pm_type == PM_NORMAL || (cg.predictedPlayerState.pm_type == PM_SPECTATOR && cg.showScores == qfalse)) {
        CG_EventHandling(CGAME_EVENT_NONE);
        trap_Key_SetCatcher(0);
        return;
    }

    // if (key == trap_Key_GetKey("teamMenu") || !Display_CaptureItem(cgs.cursorX, cgs.cursorY)) {
    //  if we see this then we should always be visible
    //  CG_EventHandling(CGAME_EVENT_NONE);
    //  trap_Key_SetCatcher(0);
    //}

    Display_HandleKey(key, down, cgs.cursorX, cgs.cursorY);

    if (cgs.capturedItem) {
        cgs.capturedItem = NULL;
    } else {
        if (key == K_MOUSE2 && down) {
            cgs.capturedItem = Display_CaptureItem(cgs.cursorX, cgs.cursorY);
        }
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
