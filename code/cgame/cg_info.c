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
// cg_info.c -- display information while data is being loading

#include "cg_local.h"
#include "../../ui/menudef.h"

#define MAX_LOADING_PLAYER_ICONS 16
#define MAX_LOADING_ITEM_ICONS 26

// [QL] loading screen progress has 4 stages
#define NUM_LOADING_STAGES 4

static int loadingPlayerIconCount;
static int loadingItemIconCount;
static qhandle_t loadingPlayerIcons[MAX_LOADING_PLAYER_ICONS];
static qhandle_t loadingItemIcons[MAX_LOADING_ITEM_ICONS];

// [QL] use shared gametypeDisplayNames[] from bg_misc.c

/*
======================
CG_LoadingString

======================
*/
void CG_LoadingString(const char* s) {
    Q_strncpyz(cg.infoScreenText, s, sizeof(cg.infoScreenText));

    trap_UpdateScreen();
}

/*
===================
CG_LoadingItem
===================
*/
void CG_LoadingItem(int itemNum) {
    gitem_t* item;

    item = &bg_itemlist[itemNum];

    if (item->icon && loadingItemIconCount < MAX_LOADING_ITEM_ICONS) {
        loadingItemIcons[loadingItemIconCount++] = trap_R_RegisterShaderNoMip(item->icon);
    }

    CG_LoadingString(item->pickup_name);
}

/*
===================
CG_LoadingClient
===================
*/
void CG_LoadingClient(int clientNum) {
    const char* info;
    char* skin;
    char personality[MAX_QPATH];
    char model[MAX_QPATH];
    char iconName[MAX_QPATH];

    info = CG_ConfigString(CS_PLAYERS + clientNum);

    if (loadingPlayerIconCount < MAX_LOADING_PLAYER_ICONS) {
        Q_strncpyz(model, Info_ValueForKey(info, "model"), sizeof(model));
        skin = strrchr(model, '/');
        if (skin) {
            *skin++ = '\0';
        } else {
            skin = "default";
        }

        Com_sprintf(iconName, MAX_QPATH, "models/players/%s/icon_%s.tga", model, skin);

        loadingPlayerIcons[loadingPlayerIconCount] = trap_R_RegisterShaderNoMip(iconName);
        if (!loadingPlayerIcons[loadingPlayerIconCount]) {
            Com_sprintf(iconName, MAX_QPATH, "models/players/%s/icon_%s.tga", DEFAULT_MODEL, "default");
            loadingPlayerIcons[loadingPlayerIconCount] = trap_R_RegisterShaderNoMip(iconName);
        }
        if (loadingPlayerIcons[loadingPlayerIconCount]) {
            loadingPlayerIconCount++;
        }
    }

    Q_strncpyz(personality, Info_ValueForKey(info, "n"), sizeof(personality));

    CG_LoadingString(personality);
}

/*
====================
CG_DrawInformation

[QL] Rewritten to match Quake Live binary loading screen.
Draws levelshot, QL branding, gametype, loading text, and progress bar.
====================
*/
void CG_DrawInformation(void) {
    const char *s;
    const char *info;
    const char *sAuthor, *sAuthor2;
    const char *gametypeName;
    qhandle_t levelshot;
    int i;
    int w;
    float s1, s2;

    // [QL] colors from binary
    static vec4_t colorBarFilled   = { 0.375f, 0.125f, 0.125f, 1.0f  };  // dark red, full alpha
    static vec4_t colorBarEmpty    = { 0.0f,   0.0f,   0.0f,   0.75f };  // black semi-transparent
    static vec4_t colorBarBg       = { 0.375f, 0.125f, 0.125f, 0.75f };  // dark red, semi-transparent

    // [QL] compute aspect-correct texture coords for levelshot (1920x1080 source)
    s1 = ((1920.0f - (1080.0f / (float)cgs.glconfig.vidHeight) * (float)cgs.glconfig.vidWidth)
          / 1920.0f) * 0.5f;
    s2 = 1.0f - s1;

    info = CG_ConfigString(CS_SERVERINFO);

    // register levelshot
    s = Info_ValueForKey(info, "mapname");
    levelshot = trap_R_RegisterShaderNoMip(va("levelshots/%s.tga", s));
    if (!levelshot) {
        levelshot = trap_R_RegisterShaderNoMip("menu/art/unknownmap");
    }
    trap_R_SetColor(NULL);

    // [QL] draw the levelshot fullscreen with aspect-correct texture coords
    trap_R_DrawStretchPic(0, 0, (float)cgs.glconfig.vidWidth, (float)cgs.glconfig.vidHeight,
                          s1, 0, s2, 1.0f, levelshot);

    // [QL] left-anchored elements: top bar, logos, bottom bar, map name, author
    CG_SetWidescreen(WIDESCREEN_LEFT);

    // top bar: dark red semi-transparent background across full width
    CG_FillRect(-400, 0, 1440, 64, colorBarBg);

    // logo background (512x128 source, drawn at 256x64 = 4:1 ratio preserved)
    if (cgs.media.logoBackgroundShader) {
        CG_DrawPic(0, 0, 256, 64, cgs.media.logoBackgroundShader);
    }

    // QL logo (512x128 source, drawn at 256x64)
    if (cgs.media.qlLogoShader) {
        CG_DrawPic(0, 0, 256, 64, cgs.media.qlLogoShader);
    }

    // [QL] bottom bar: black semi-transparent covering y=400..480
    CG_FillRect(-400, 400, 1440, 80, colorBarEmpty);

    // [QL] map display name (lower-left, bigFont via scale 0.8)
    s = CG_ConfigString(CS_MESSAGE);
    CG_Text_Paint(8, 435, 0.8f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

    // [QL] author/author2 below map name (textFont = NotoSans via fontIndex 0)
    sAuthor = CG_ConfigString(CS_AUTHOR);
    CG_DrawText_DC(8, 455, 0.25f, colorWhite, sAuthor, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE, 0);

    sAuthor2 = CG_ConfigString(CS_AUTHOR2);
    CG_DrawText_DC(8, 470, 0.25f, colorWhite, sAuthor2, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE, 0);

    // [QL] right-anchored elements: gametype background and name
    CG_SetWidescreen(WIDESCREEN_RIGHT);

    // gametype background (right side of top bar, 512x128 source at 256x64)
    if (cgs.media.gtBackgroundShader) {
        CG_DrawPic(384, 0, 256, 64, cgs.media.gtBackgroundShader);
    }

    // [QL] gametype name (right-aligned in top bar, bigFont via scale 0.8)
    if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
        gametypeName = gametypeDisplayNames[cgs.gametype];
    } else {
        gametypeName = "Unknown Gametype";
    }
    w = CG_Text_Width(gametypeName, 0.8f, 0);
    CG_Text_Paint(620 - w, 45, 0.8f, colorWhite, gametypeName, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

    // [QL] center-anchored elements: loading text and progress boxes
    CG_SetWidescreen(WIDESCREEN_CENTER);

    // [QL] loading text (centered, textFont via scale 0.3)
    if (cg.infoScreenText[0]) {
        s = va("Loading %s", cg.infoScreenText);
    } else {
        s = "Awaiting Snapshot";
    }
    w = CG_Text_Width(s, 0.3f, 0);
    CG_Text_Paint(320 - w / 2, 368, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

    // [QL] progress boxes: 4 boxes, 8x8 each, 16px stride, y=380
    for (i = 0; i < NUM_LOADING_STAGES; i++) {
        if (cg.loadingStage > i) {
            CG_FillRect(292 + i * 16, 380, 8, 8, colorBarFilled);
        } else {
            CG_FillRect(292 + i * 16, 380, 8, 8, colorBarEmpty);
        }
    }

    // [QL] reset widescreen mode
    CG_SetWidescreen(WIDESCREEN_STRETCH);
}
