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

#include "ui_local.h"

uiInfo_t uiInfo;

static char* netnames[] = {
    "???",
    "UDP",
    "UDP6"};

static int gamecodetoui[] = {4, 2, 3, 0, 5, 1, 6};
static int uitogamecode[] = {4, 6, 2, 3, 1, 5, 7};

static void UI_FeederSelection(float feederID, int index);
static int UI_MapCountByGameType(qboolean singlePlayer);
static int UI_HeadCountByTeam(void);
static void UI_ParseGameInfo(const char* teamFile);
static void UI_ParseTeamInfo(const char* teamFile);
static const char* UI_SelectedMap(int index, int* actual);
static const char* UI_SelectedHead(int index, int* actual);
static int UI_GetIndexFromSelection(int actual);
static void UI_DrawCinematic(int handle, float x, float y, float w, float h);

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .qvm file
================
*/
vmCvar_t ui_debug;
vmCvar_t ui_initialized;

void _UI_Init(qboolean);
void _UI_Shutdown(void);
void _UI_KeyEvent(int key, qboolean down);
void _UI_MouseEvent(int dx, int dy);
void _UI_Refresh(int realtime);
qboolean _UI_IsFullscreen(void);
Q_EXPORT intptr_t vmMain(int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11) {
    switch (command) {
        case UI_GETAPIVERSION:
            return UI_API_VERSION;

        case UI_INIT:
            _UI_Init(arg0);
            return 0;

        case UI_SHUTDOWN:
            _UI_Shutdown();
            return 0;

        case UI_KEY_EVENT:
            _UI_KeyEvent(arg0, arg1);
            return 0;

        case UI_MOUSE_EVENT:
            _UI_MouseEvent(arg0, arg1);
            return 0;

        case UI_REFRESH:
            _UI_Refresh(arg0);
            return 0;

        case UI_IS_FULLSCREEN:
            return _UI_IsFullscreen();

        case UI_SET_ACTIVE_MENU:
            _UI_SetActiveMenu(arg0);
            return 0;

        case UI_CONSOLE_COMMAND:
            return UI_ConsoleCommand(arg0);

        case UI_DRAW_CONNECT_SCREEN:
            UI_DrawConnectScreen(arg0);
            return 0;
        case UI_HASUNIQUECDKEY:
            return qtrue;
    }

    return -1;
}

void AssetCache(void) {
    uiInfo.uiDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(ASSET_GRADIENTBAR);
    uiInfo.uiDC.Assets.fxBasePic = trap_R_RegisterShaderNoMip(ART_FX_BASE);
    uiInfo.uiDC.Assets.fxPic[0] = trap_R_RegisterShaderNoMip(ART_FX_RED);
    uiInfo.uiDC.Assets.fxPic[1] = trap_R_RegisterShaderNoMip(ART_FX_YELLOW);
    uiInfo.uiDC.Assets.fxPic[2] = trap_R_RegisterShaderNoMip(ART_FX_GREEN);
    uiInfo.uiDC.Assets.fxPic[3] = trap_R_RegisterShaderNoMip(ART_FX_TEAL);
    uiInfo.uiDC.Assets.fxPic[4] = trap_R_RegisterShaderNoMip(ART_FX_BLUE);
    uiInfo.uiDC.Assets.fxPic[5] = trap_R_RegisterShaderNoMip(ART_FX_CYAN);
    uiInfo.uiDC.Assets.fxPic[6] = trap_R_RegisterShaderNoMip(ART_FX_WHITE);
    uiInfo.uiDC.Assets.scrollBar = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR);
    uiInfo.uiDC.Assets.scrollBarArrowDown = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWDOWN);
    uiInfo.uiDC.Assets.scrollBarArrowUp = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWUP);
    uiInfo.uiDC.Assets.scrollBarArrowLeft = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWLEFT);
    uiInfo.uiDC.Assets.scrollBarArrowRight = trap_R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWRIGHT);
    uiInfo.uiDC.Assets.scrollBarThumb = trap_R_RegisterShaderNoMip(ASSET_SCROLL_THUMB);
    uiInfo.uiDC.Assets.sliderBar = trap_R_RegisterShaderNoMip(ASSET_SLIDER_BAR);
    uiInfo.uiDC.Assets.sliderThumb = trap_R_RegisterShaderNoMip(ASSET_SLIDER_THUMB);

    for (int n = 1; n < NUM_CROSSHAIRS; n++) {
        uiInfo.uiDC.Assets.crosshairShader[n] = trap_R_RegisterShaderNoMip(va("gfx/2d/crosshair%i", n));
    }

    uiInfo.newHighScoreSound = trap_S_RegisterSound("sound/vo/new_high_score.ogg", qfalse);
}

void _UI_DrawSides(float x, float y, float w, float h, float size) {
    UI_AdjustFrom640(&x, &y, &w, &h);
    size *= uiInfo.uiDC.xscale;
    trap_R_DrawStretchPic(x, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    trap_R_DrawStretchPic(x + w - size, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

void _UI_DrawTopBottom(float x, float y, float w, float h, float size) {
    UI_AdjustFrom640(&x, &y, &w, &h);
    size *= uiInfo.uiDC.yscale;
    trap_R_DrawStretchPic(x, y, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    trap_R_DrawStretchPic(x, y + h - size, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void _UI_DrawRect(float x, float y, float width, float height, float size, const float* color) {
    trap_R_SetColor(color);

    _UI_DrawTopBottom(x, y, width, height, size);
    _UI_DrawSides(x, y, width, height, size);

    trap_R_SetColor(NULL);
}

int Text_Width(const char* text, float scale, int limit) {
    int count, len;
    float out;
    glyphInfo_t* glyph;
    float useScale;
    const char* s = text;
    fontInfo_t* font = &uiInfo.uiDC.Assets.textFont;
    if (scale <= ui_smallFont.value) {
        font = &uiInfo.uiDC.Assets.smallFont;
    } else if (scale >= ui_bigFont.value) {
        font = &uiInfo.uiDC.Assets.bigFont;
    }
    useScale = scale * font->glyphScale;
    out = 0;
    if (text) {
        len = strlen(text);
        if (limit > 0 && len > limit) {
            len = limit;
        }
        count = 0;
        while (s && *s && count < len) {
            if (Q_IsColorString(s)) {
                s += 2;
                continue;
            } else {
                glyph = &font->glyphs[*s & 255];
                out += glyph->xSkip;
                s++;
                count++;
            }
        }
    }
    return out * useScale;
}

int Text_Height(const char* text, float scale, int limit) {
    int len, count;
    float max;
    glyphInfo_t* glyph;
    float useScale;
    const char* s = text;
    fontInfo_t* font = &uiInfo.uiDC.Assets.textFont;
    if (scale <= ui_smallFont.value) {
        font = &uiInfo.uiDC.Assets.smallFont;
    } else if (scale >= ui_bigFont.value) {
        font = &uiInfo.uiDC.Assets.bigFont;
    }
    useScale = scale * font->glyphScale;
    max = 0;
    if (text) {
        len = strlen(text);
        if (limit > 0 && len > limit) {
            len = limit;
        }
        count = 0;
        while (s && *s && count < len) {
            if (Q_IsColorString(s)) {
                s += 2;
                continue;
            } else {
                glyph = &font->glyphs[*s & 255];
                if (max < glyph->height) {
                    max = glyph->height;
                }
                s++;
                count++;
            }
        }
    }
    return max * useScale;
}

void Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader) {
    float w, h;
    w = width * scale;
    h = height * scale;
    UI_AdjustFrom640(&x, &y, &w, &h);
    trap_R_DrawStretchPic(x, y, w, h, s, t, s2, t2, hShader);
}

void Text_Paint(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style) {
    int len, count;
    vec4_t newColor;
    glyphInfo_t* glyph;
    float useScale;
    fontInfo_t* font = &uiInfo.uiDC.Assets.textFont;
    if (scale <= ui_smallFont.value) {
        font = &uiInfo.uiDC.Assets.smallFont;
    } else if (scale >= ui_bigFont.value) {
        font = &uiInfo.uiDC.Assets.bigFont;
    }
    useScale = scale * font->glyphScale;
    if (text) {
        const char* s = text;
        trap_R_SetColor(color);
        memcpy(&newColor[0], &color[0], sizeof(vec4_t));
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
                if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE) {
                    int ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
                    colorBlack[3] = newColor[3];
                    trap_R_SetColor(colorBlack);
                    Text_PaintChar(x + ofs, y - yadj + ofs,
                                   glyph->imageWidth,
                                   glyph->imageHeight,
                                   useScale,
                                   glyph->s,
                                   glyph->t,
                                   glyph->s2,
                                   glyph->t2,
                                   glyph->glyph);
                    trap_R_SetColor(newColor);
                    colorBlack[3] = 1.0;
                }
                Text_PaintChar(x, y - yadj,
                               glyph->imageWidth,
                               glyph->imageHeight,
                               useScale,
                               glyph->s,
                               glyph->t,
                               glyph->s2,
                               glyph->t2,
                               glyph->glyph);

                x += (glyph->xSkip * useScale) + adjust;
                s++;
                count++;
            }
        }
        trap_R_SetColor(NULL);
    }
}

void Text_PaintWithCursor(float x, float y, float scale, vec4_t color, const char* text, int cursorPos, char cursor, int limit, int style) {
    int len, count;
    vec4_t newColor;
    glyphInfo_t *glyph, *glyph2;
    float yadj;
    float useScale;
    fontInfo_t* font = &uiInfo.uiDC.Assets.textFont;
    if (scale <= ui_smallFont.value) {
        font = &uiInfo.uiDC.Assets.smallFont;
    } else if (scale >= ui_bigFont.value) {
        font = &uiInfo.uiDC.Assets.bigFont;
    }
    useScale = scale * font->glyphScale;
    if (text) {
        const char* s = text;
        trap_R_SetColor(color);
        memcpy(&newColor[0], &color[0], sizeof(vec4_t));
        len = strlen(text);
        if (limit > 0 && len > limit) {
            len = limit;
        }
        count = 0;
        glyph2 = &font->glyphs[cursor & 255];
        while (s && *s && count < len) {
            glyph = &font->glyphs[*s & 255];
            if (Q_IsColorString(s)) {
                memcpy(newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
                newColor[3] = color[3];
                trap_R_SetColor(newColor);
                s += 2;
                continue;
            } else {
                yadj = useScale * glyph->top;
                if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE) {
                    int ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
                    colorBlack[3] = newColor[3];
                    trap_R_SetColor(colorBlack);
                    Text_PaintChar(x + ofs, y - yadj + ofs,
                                   glyph->imageWidth,
                                   glyph->imageHeight,
                                   useScale,
                                   glyph->s,
                                   glyph->t,
                                   glyph->s2,
                                   glyph->t2,
                                   glyph->glyph);
                    colorBlack[3] = 1.0;
                    trap_R_SetColor(newColor);
                }
                Text_PaintChar(x, y - yadj,
                               glyph->imageWidth,
                               glyph->imageHeight,
                               useScale,
                               glyph->s,
                               glyph->t,
                               glyph->s2,
                               glyph->t2,
                               glyph->glyph);

                yadj = useScale * glyph2->top;
                if (count == cursorPos && !((uiInfo.uiDC.realTime / BLINK_DIVISOR) & 1)) {
                    Text_PaintChar(x, y - yadj,
                                   glyph2->imageWidth,
                                   glyph2->imageHeight,
                                   useScale,
                                   glyph2->s,
                                   glyph2->t,
                                   glyph2->s2,
                                   glyph2->t2,
                                   glyph2->glyph);
                }

                x += (glyph->xSkip * useScale);
                s++;
                count++;
            }
        }
        // need to paint cursor at end of text
        if (cursorPos == len && !((uiInfo.uiDC.realTime / BLINK_DIVISOR) & 1)) {
            yadj = useScale * glyph2->top;
            Text_PaintChar(x, y - yadj,
                           glyph2->imageWidth,
                           glyph2->imageHeight,
                           useScale,
                           glyph2->s,
                           glyph2->t,
                           glyph2->s2,
                           glyph2->t2,
                           glyph2->glyph);
        }

        trap_R_SetColor(NULL);
    }
}

static void Text_Paint_Limit(float* maxX, float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit) {
    int len, count;
    vec4_t newColor;
    glyphInfo_t* glyph;
    if (text) {
        const char* s = text;
        float max = *maxX;
        float useScale;
        fontInfo_t* font = &uiInfo.uiDC.Assets.textFont;
        if (scale <= ui_smallFont.value) {
            font = &uiInfo.uiDC.Assets.smallFont;
        } else if (scale > ui_bigFont.value) {
            font = &uiInfo.uiDC.Assets.bigFont;
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
                if (Text_Width(s, scale, 1) + x > max) {
                    *maxX = 0;
                    break;
                }
                Text_PaintChar(x, y - yadj,
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

void UI_ShowPostGame(qboolean newHigh) {
    trap_Cvar_Set("cg_cameraOrbit", "0");
    trap_Cvar_Set("cg_thirdPerson", "0");
    uiInfo.soundHighScore = newHigh;
    _UI_SetActiveMenu(UIMENU_POSTGAME);
}
/*
=================
_UI_Refresh
=================
*/

void UI_DrawCenteredPic(qhandle_t image, int w, int h) {
    int x, y;
    x = (SCREEN_WIDTH - w) / 2;
    y = (SCREEN_HEIGHT - h) / 2;
    UI_DrawHandlePic(x, y, w, h, image);
}

int frameCount = 0;
int startTime;

#define UI_FPS_FRAMES 4
void _UI_Refresh(int realtime) {
    static int index;
    static int previousTimes[UI_FPS_FRAMES];

    uiInfo.uiDC.frameTime = realtime - uiInfo.uiDC.realTime;
    uiInfo.uiDC.realTime = realtime;

    previousTimes[index % UI_FPS_FRAMES] = uiInfo.uiDC.frameTime;
    index++;
    if (index > UI_FPS_FRAMES) {
        int i, total;
        // average multiple frames together to smooth changes out a bit
        total = 0;
        for (i = 0; i < UI_FPS_FRAMES; i++) {
            total += previousTimes[i];
        }
        if (!total) {
            total = 1;
        }
        uiInfo.uiDC.FPS = 1000 * UI_FPS_FRAMES / total;
    }

    UI_UpdateCvars();

    if (Menu_Count() > 0) {
        // paint all the menus
        Menu_PaintAll();
    }

    // draw cursor
    UI_SetColor(NULL);
    if (Menu_Count() > 0 && (trap_Key_GetCatcher() & KEYCATCH_UI)) {
        UI_DrawHandlePic(uiInfo.uiDC.cursorx - 16, uiInfo.uiDC.cursory - 16, 32, 32, uiInfo.uiDC.Assets.cursor);
    }
}

/*
=================
_UI_Shutdown
=================
*/
void _UI_Shutdown(void) {
}

char* defaultMenu = NULL;

char* GetMenuBuffer(const char* filename) {
    int len;
    fileHandle_t f;
    static char buf[MAX_MENUFILE];

    len = trap_FS_FOpenFile(filename, &f, FS_READ);
    if (!f) {
        trap_Print(va(S_COLOR_RED "menu file not found: %s, using default\n", filename));
        return defaultMenu;
    }
    if (len >= MAX_MENUFILE) {
        trap_Print(va(S_COLOR_RED "menu file too large: %s is %i, max allowed is %i\n", filename, len, MAX_MENUFILE));
        trap_FS_FCloseFile(f);
        return defaultMenu;
    }

    trap_FS_Read(buf, len, f);
    buf[len] = 0;
    trap_FS_FCloseFile(f);
    // COM_Compress(buf);
    return buf;
}

qboolean Asset_Parse(int handle) {
    pc_token_t token;
    const char* tempStr;

    if (!trap_PC_ReadToken(handle, &token))
        return qfalse;
    if (Q_stricmp(token.string, "{") != 0) {
        return qfalse;
    }

    while (1) {
        memset(&token, 0, sizeof(pc_token_t));

        if (!trap_PC_ReadToken(handle, &token))
            return qfalse;

        if (Q_stricmp(token.string, "}") == 0) {
            return qtrue;
        }

        // font
        if (Q_stricmp(token.string, "font") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(DEFAULT_SANS_FONT, pointSize, &uiInfo.uiDC.Assets.textFont);
            uiInfo.uiDC.Assets.fontRegistered = qtrue;
            continue;
        }

        if (Q_stricmp(token.string, "smallFont") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(DEFAULT_MONO_FONT, pointSize, &uiInfo.uiDC.Assets.smallFont);
            continue;
        }

        if (Q_stricmp(token.string, "bigFont") == 0) {
            int pointSize;
            if (!PC_String_Parse(handle, &tempStr) || !PC_Int_Parse(handle, &pointSize)) {
                return qfalse;
            }
            trap_R_RegisterFont(DEFAULT_FONT, pointSize, &uiInfo.uiDC.Assets.bigFont);
            continue;
        }

        // gradientbar
        if (Q_stricmp(token.string, "gradientbar") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.gradientBar = trap_R_RegisterShaderNoMip(tempStr);
            continue;
        }

        // enterMenuSound
        if (Q_stricmp(token.string, "menuEnterSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.menuEnterSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        // exitMenuSound
        if (Q_stricmp(token.string, "menuExitSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.menuExitSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        // itemFocusSound
        if (Q_stricmp(token.string, "itemFocusSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.itemFocusSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        // menuBuzzSound
        if (Q_stricmp(token.string, "menuBuzzSound") == 0) {
            if (!PC_String_Parse(handle, &tempStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.menuBuzzSound = trap_S_RegisterSound(tempStr, qfalse);
            continue;
        }

        if (Q_stricmp(token.string, "cursor") == 0) {
            if (!PC_String_Parse(handle, &uiInfo.uiDC.Assets.cursorStr)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.cursor = trap_R_RegisterShaderNoMip(uiInfo.uiDC.Assets.cursorStr);
            continue;
        }

        if (Q_stricmp(token.string, "fadeClamp") == 0) {
            if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.fadeClamp)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "fadeCycle") == 0) {
            if (!PC_Int_Parse(handle, &uiInfo.uiDC.Assets.fadeCycle)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "fadeAmount") == 0) {
            if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.fadeAmount)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowX") == 0) {
            if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.shadowX)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowY") == 0) {
            if (!PC_Float_Parse(handle, &uiInfo.uiDC.Assets.shadowY)) {
                return qfalse;
            }
            continue;
        }

        if (Q_stricmp(token.string, "shadowColor") == 0) {
            if (!PC_Color_Parse(handle, &uiInfo.uiDC.Assets.shadowColor)) {
                return qfalse;
            }
            uiInfo.uiDC.Assets.shadowFadeClamp = uiInfo.uiDC.Assets.shadowColor[3];
            continue;
        }
    }
    return qfalse;
}

void Font_Report(void) {
    int i;
    Com_Printf("Font Info\n");
    Com_Printf("=========\n");
    for (i = 32; i < 96; i++) {
        Com_Printf("  Glyph handle %i: %i\n", i, uiInfo.uiDC.Assets.textFont.glyphs[i].glyph);
    }
}

void UI_Report(void) {
    String_Report();
    Font_Report();
}

void UI_ParseMenu(const char* menuFile) {
    int handle;
    pc_token_t token;

    Com_Printf("Parsing menu file: %s\n", menuFile);

    handle = trap_PC_LoadSource(menuFile);
    if (!handle) {
        return;
    }

    while (1) {
        memset(&token, 0, sizeof(pc_token_t));
        if (!trap_PC_ReadToken(handle, &token)) {
            break;
        }

        if (token.string[0] == '}') {
            break;
        }

        if (Q_stricmp(token.string, "assetGlobalDef") == 0) {
            if (Asset_Parse(handle)) {
                continue;
            } else {
                break;
            }
        }

        if (Q_stricmp(token.string, "menudef") == 0) {
            // start a new menu
            Menu_New(handle);
        }
    }
    trap_PC_FreeSource(handle);
}

qboolean Load_Menu(int handle) {
    pc_token_t token;

    if (!trap_PC_ReadToken(handle, &token))
        return qfalse;
    if (token.string[0] != '{') {
        return qfalse;
    }

    while (1) {
        if (!trap_PC_ReadToken(handle, &token))
            return qfalse;

        if (token.string[0] == 0) {
            return qfalse;
        }

        if (token.string[0] == '}') {
            return qtrue;
        }

        UI_ParseMenu(token.string);
    }
    return qfalse;
}

void UI_LoadMenus(const char* menuFile, qboolean reset) {
    pc_token_t token;
    int handle;
    int start;

    start = trap_Milliseconds();

    handle = trap_PC_LoadSource(menuFile);
    if (!handle) {
        Com_Printf(S_COLOR_YELLOW "menu file not found: %s, using default\n", menuFile);
        handle = trap_PC_LoadSource("ui/menus.txt");
        if (!handle) {
            trap_Error(S_COLOR_RED "default menu file not found: ui/menus.txt, unable to continue!");
        }
    }

    if (reset) {
        Menu_Reset();
    }

    while (1) {
        if (!trap_PC_ReadToken(handle, &token))
            break;
        if (token.string[0] == 0 || token.string[0] == '}') {
            break;
        }

        if (token.string[0] == '}') {
            break;
        }

        if (Q_stricmp(token.string, "loadmenu") == 0) {
            if (Load_Menu(handle)) {
                continue;
            } else {
                break;
            }
        }
    }

    Com_Printf("UI menu load time = %d milli seconds\n", trap_Milliseconds() - start);

    trap_PC_FreeSource(handle);
}

void UI_Load(void) {
    char lastName[1024];
    menuDef_t* menu = Menu_GetFocused();
    char* menuSet = UI_Cvar_VariableString("ui_menuFiles");
    if (menu && menu->window.name) {
        Q_strncpyz(lastName, menu->window.name, sizeof(lastName));
    }
    if (menuSet == NULL || menuSet[0] == '\0') {
        menuSet = "ui/menus.txt";
    }

    String_Init();

    UI_ParseGameInfo("gameinfo.txt");
    UI_LoadArenas();

    UI_LoadMenus(menuSet, qtrue);
    Menus_CloseAll();
    Menus_ActivateByName(lastName);
}

static const char* handicapValues[] = {"None", "95", "90", "85", "80", "75", "70", "65", "60", "55", "50", "45", "40", "35", "30", "25", "20", "15", "10", "5", NULL};

static void UI_DrawHandicap(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    int i, h;

    h = Com_Clamp(5, 100, trap_Cvar_VariableValue("handicap"));
    i = 20 - h / 5;

    Text_Paint(rect->x, rect->y, scale, color, handicapValues[i], 0, 0, textStyle);
}

static void UI_DrawClanName(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    Text_Paint(rect->x, rect->y, scale, color, UI_Cvar_VariableString("ui_teamName"), 0, 0, textStyle);
}

static void UI_SetCapFragLimits(qboolean uiVars) {
    int cap = 5;
    int frag = 10;
    if (uiInfo.gameTypes[ui_gameType.integer].gtEnum == GT_OBELISK) {
        cap = 4;
    } else if (uiInfo.gameTypes[ui_gameType.integer].gtEnum == GT_HARVESTER) {
        cap = 15;
    }
    if (uiVars) {
        trap_Cvar_Set("ui_captureLimit", va("%d", cap));
        trap_Cvar_Set("ui_fragLimit", va("%d", frag));
    } else {
        trap_Cvar_Set("capturelimit", va("%d", cap));
        trap_Cvar_Set("fraglimit", va("%d", frag));
    }
}
// ui_gameType assumes gametype 0 is -1 ALL and will not show
static void UI_DrawGameType(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    Text_Paint(rect->x, rect->y, scale, color, uiInfo.gameTypes[ui_gameType.integer].gameType, 0, 0, textStyle);
}

static void UI_DrawNetGameType(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    if (ui_netGameType.integer < 0 || ui_netGameType.integer > uiInfo.numGameTypes) {
        trap_Cvar_Set("ui_netGameType", "0");
        trap_Cvar_Set("ui_actualNetGameType", "0");
    }
    Text_Paint(rect->x, rect->y, scale, color, uiInfo.gameTypes[ui_netGameType.integer].gameType, 0, 0, textStyle);
}

static void UI_DrawJoinGameType(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    if (ui_joinGameType.integer < 0 || ui_joinGameType.integer > uiInfo.numJoinGameTypes) {
        trap_Cvar_Set("ui_joinGameType", "0");
    }
    Text_Paint(rect->x, rect->y, scale, color, uiInfo.joinGameTypes[ui_joinGameType.integer].gameType, 0, 0, textStyle);
}

static int UI_TeamIndexFromName(const char* name) {
    int i;

    if (name && *name) {
        for (i = 0; i < uiInfo.teamCount; i++) {
            if (Q_stricmp(name, uiInfo.teamList[i].teamName) == 0) {
                return i;
            }
        }
    }

    return 0;
}

static void UI_DrawClanLogo(rectDef_t* rect, float scale, vec4_t color) {
    int i;
    i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    if (i >= 0 && i < uiInfo.teamCount) {
        trap_R_SetColor(color);

        if (uiInfo.teamList[i].teamIcon == -1) {
            uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
            uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal", uiInfo.teamList[i].imageName));
            uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
        }

        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
        trap_R_SetColor(NULL);
    }
}

static void UI_DrawClanCinematic(rectDef_t* rect, float scale, vec4_t color) {
    int i;
    i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    if (i >= 0 && i < uiInfo.teamCount) {
        if (uiInfo.teamList[i].cinematic >= -2) {
            if (uiInfo.teamList[i].cinematic == -1) {
                uiInfo.teamList[i].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.teamList[i].imageName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
            }
            if (uiInfo.teamList[i].cinematic >= 0) {
                trap_CIN_RunCinematic(uiInfo.teamList[i].cinematic);
                UI_DrawCinematic(uiInfo.teamList[i].cinematic, rect->x, rect->y, rect->w, rect->h);
            } else {
                trap_R_SetColor(color);
                UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Metal);
                trap_R_SetColor(NULL);
                uiInfo.teamList[i].cinematic = -2;
            }
        } else {
            trap_R_SetColor(color);
            UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
            trap_R_SetColor(NULL);
        }
    }
}

static void UI_DrawPreviewCinematic(rectDef_t* rect, float scale, vec4_t color) {
    if (uiInfo.previewMovie > -2) {
        uiInfo.previewMovie = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.movieList[uiInfo.movieIndex]), 0, 0, 0, 0, (CIN_loop | CIN_silent));
        if (uiInfo.previewMovie >= 0) {
            trap_CIN_RunCinematic(uiInfo.previewMovie);
            UI_DrawCinematic(uiInfo.previewMovie, rect->x, rect->y, rect->w, rect->h);
        } else {
            uiInfo.previewMovie = -2;
        }
    }
}

static void UI_DrawTeamName(rectDef_t* rect, float scale, vec4_t color, qboolean blue, int textStyle) {
    int i;
    i = UI_TeamIndexFromName(UI_Cvar_VariableString((blue) ? "ui_blueTeam" : "ui_redTeam"));
    if (i >= 0 && i < uiInfo.teamCount) {
        Text_Paint(rect->x, rect->y, scale, color, va("%s: %s", (blue) ? "Blue" : "Red", uiInfo.teamList[i].teamName), 0, 0, textStyle);
    }
}

static void UI_DrawEffects(rectDef_t* rect, float scale, vec4_t color) {
    UI_DrawHandlePic(rect->x, rect->y - 14, 128, 8, uiInfo.uiDC.Assets.fxBasePic);
    UI_DrawHandlePic(rect->x + uiInfo.effectsColor * 16 + 8, rect->y - 16, 16, 12, uiInfo.uiDC.Assets.fxPic[uiInfo.effectsColor]);
}

static void UI_DrawMapPreview(rectDef_t* rect, float scale, vec4_t color, qboolean net) {
    int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
    if (map < 0 || map > uiInfo.mapCount) {
        if (net) {
            ui_currentNetMap.integer = 0;
            trap_Cvar_Set("ui_currentNetMap", "0");
        } else {
            ui_currentMap.integer = 0;
            trap_Cvar_Set("ui_currentMap", "0");
        }
        map = 0;
    }

    if (uiInfo.mapList[map].levelShot == -1) {
        uiInfo.mapList[map].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[map].imageName);
    }

    if (uiInfo.mapList[map].levelShot > 0) {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.mapList[map].levelShot);
    } else {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, trap_R_RegisterShaderNoMip("menu/art/unknownmap"));
    }
}

static void UI_DrawMapTimeToBeat(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    int minutes, seconds, time;
    if (ui_currentMap.integer < 0 || ui_currentMap.integer > uiInfo.mapCount) {
        ui_currentMap.integer = 0;
        trap_Cvar_Set("ui_currentMap", "0");
    }

    time = uiInfo.mapList[ui_currentMap.integer].timeToBeat[uiInfo.gameTypes[ui_gameType.integer].gtEnum];

    minutes = time / 60;
    seconds = time % 60;

    Text_Paint(rect->x, rect->y, scale, color, va("%02i:%02i", minutes, seconds), 0, 0, textStyle);
}

static void UI_DrawMapCinematic(rectDef_t* rect, float scale, vec4_t color, qboolean net) {
    int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
    if (map < 0 || map > uiInfo.mapCount) {
        if (net) {
            ui_currentNetMap.integer = 0;
            trap_Cvar_Set("ui_currentNetMap", "0");
        } else {
            ui_currentMap.integer = 0;
            trap_Cvar_Set("ui_currentMap", "0");
        }
        map = 0;
    }

    if (uiInfo.mapList[map].cinematic >= -1) {
        if (uiInfo.mapList[map].cinematic == -1) {
            uiInfo.mapList[map].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[map].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
        }
        if (uiInfo.mapList[map].cinematic >= 0) {
            trap_CIN_RunCinematic(uiInfo.mapList[map].cinematic);
            UI_DrawCinematic(uiInfo.mapList[map].cinematic, rect->x, rect->y, rect->w, rect->h);
        } else {
            uiInfo.mapList[map].cinematic = -2;
        }
    } else {
        UI_DrawMapPreview(rect, scale, color, net);
    }
}

static qboolean updateModel = qtrue;
static qboolean q3Model = qfalse;

static void UI_DrawPlayerModel(rectDef_t* rect) {
    static playerInfo_t info;
    char model[MAX_QPATH];
    char team[256];
    char head[256];
    vec3_t viewangles;
    vec3_t moveangles;

    if (trap_Cvar_VariableValue("ui_Q3Model")) {
        Q_strncpyz(model, UI_Cvar_VariableString("model"), sizeof(model));
        Q_strncpyz(head, UI_Cvar_VariableString("headmodel"), sizeof(head));
        if (!q3Model) {
            q3Model = qtrue;
            updateModel = qtrue;
        }
        team[0] = '\0';
    } else {
        Q_strncpyz(team, UI_Cvar_VariableString("ui_teamName"), sizeof(team));
        Q_strncpyz(model, UI_Cvar_VariableString("team_model"), sizeof(model));
        Q_strncpyz(head, UI_Cvar_VariableString("team_headmodel"), sizeof(head));
        if (q3Model) {
            q3Model = qfalse;
            updateModel = qtrue;
        }
    }
    if (updateModel) {
        memset(&info, 0, sizeof(playerInfo_t));
        viewangles[YAW] = 180 - 10;
        viewangles[PITCH] = 0;
        viewangles[ROLL] = 0;
        VectorClear(moveangles);
        UI_PlayerInfo_SetModel(&info, model, head, team);
        UI_PlayerInfo_SetInfo(&info, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse);
        //		UI_RegisterClientModelname( &info, model, head, team);
        updateModel = qfalse;
    }

    UI_DrawPlayer(rect->x, rect->y, rect->w, rect->h, &info, uiInfo.uiDC.realTime / 2);
}

static void UI_DrawNetMapPreview(rectDef_t* rect, float scale, vec4_t color) {
    if (uiInfo.serverStatus.currentServerPreview > 0) {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.serverStatus.currentServerPreview);
    } else {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, trap_R_RegisterShaderNoMip("menu/art/unknownmap"));
    }
}

static void UI_DrawNetMapCinematic(rectDef_t* rect, float scale, vec4_t color) {
    if (ui_currentNetMap.integer < 0 || ui_currentNetMap.integer > uiInfo.mapCount) {
        ui_currentNetMap.integer = 0;
        trap_Cvar_Set("ui_currentNetMap", "0");
    }

    if (uiInfo.serverStatus.currentServerCinematic >= 0) {
        trap_CIN_RunCinematic(uiInfo.serverStatus.currentServerCinematic);
        UI_DrawCinematic(uiInfo.serverStatus.currentServerCinematic, rect->x, rect->y, rect->w, rect->h);
    } else {
        UI_DrawNetMapPreview(rect, scale, color);
    }
}

static const char* UI_EnglishMapName(const char* map) {
    int i;
    for (i = 0; i < uiInfo.mapCount; i++) {
        if (Q_stricmp(map, uiInfo.mapList[i].mapLoadName) == 0) {
            return uiInfo.mapList[i].mapName;
        }
    }
    return "";
}

static const char* UI_AIFromName(const char* name) {
    int j;
    for (j = 0; j < uiInfo.aliasCount; j++) {
        if (Q_stricmp(uiInfo.aliasList[j].name, name) == 0) {
            return uiInfo.aliasList[j].ai;
        }
    }
    return "James";
}

static qboolean updateOpponentModel = qtrue;
static void UI_DrawOpponent(rectDef_t* rect) {
    static playerInfo_t info2;
    char model[MAX_QPATH];
    char headmodel[MAX_QPATH];
    char team[256];
    vec3_t viewangles;
    vec3_t moveangles;

    if (updateOpponentModel) {
        Q_strncpyz(model, UI_Cvar_VariableString("ui_opponentModel"), sizeof(model));
        Q_strncpyz(headmodel, UI_Cvar_VariableString("ui_opponentModel"), sizeof(headmodel));
        team[0] = '\0';

        memset(&info2, 0, sizeof(playerInfo_t));
        viewangles[YAW] = 180 - 10;
        viewangles[PITCH] = 0;
        viewangles[ROLL] = 0;
        VectorClear(moveangles);
        UI_PlayerInfo_SetModel(&info2, model, headmodel, "");
        UI_PlayerInfo_SetInfo(&info2, LEGS_IDLE, TORSO_STAND, viewangles, vec3_origin, WP_MACHINEGUN, qfalse);
        UI_RegisterClientModelname(&info2, model, headmodel, team);
        updateOpponentModel = qfalse;
    }

    UI_DrawPlayer(rect->x, rect->y, rect->w, rect->h, &info2, uiInfo.uiDC.realTime / 2);
}

static void UI_NextOpponent(void) {
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    int j = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    i++;
    if (i >= uiInfo.teamCount) {
        i = 0;
    }
    if (i == j) {
        i++;
        if (i >= uiInfo.teamCount) {
            i = 0;
        }
    }
    trap_Cvar_Set("ui_opponentName", uiInfo.teamList[i].teamName);
}

static void UI_PriorOpponent(void) {
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    int j = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    i--;
    if (i < 0) {
        i = uiInfo.teamCount - 1;
    }
    if (i == j) {
        i--;
        if (i < 0) {
            i = uiInfo.teamCount - 1;
        }
    }
    trap_Cvar_Set("ui_opponentName", uiInfo.teamList[i].teamName);
}

static void UI_DrawPlayerLogo(rectDef_t* rect, vec3_t color) {
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));

    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal", uiInfo.teamList[i].imageName));
        uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
    }

    trap_R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
    trap_R_SetColor(NULL);
}

static void UI_DrawPlayerLogoMetal(rectDef_t* rect, vec3_t color) {
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal", uiInfo.teamList[i].imageName));
        uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
    }

    trap_R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Metal);
    trap_R_SetColor(NULL);
}

static void UI_DrawPlayerLogoName(rectDef_t* rect, vec3_t color) {
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal", uiInfo.teamList[i].imageName));
        uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
    }

    trap_R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Name);
    trap_R_SetColor(NULL);
}

static void UI_DrawOpponentLogo(rectDef_t* rect, vec3_t color) {
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal", uiInfo.teamList[i].imageName));
        uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
    }

    trap_R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
    trap_R_SetColor(NULL);
}

static void UI_DrawOpponentLogoMetal(rectDef_t* rect, vec3_t color) {
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal", uiInfo.teamList[i].imageName));
        uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
    }

    trap_R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Metal);
    trap_R_SetColor(NULL);
}

static void UI_DrawOpponentLogoName(rectDef_t* rect, vec3_t color) {
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal", uiInfo.teamList[i].imageName));
        uiInfo.teamList[i].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[i].imageName));
    }

    trap_R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Name);
    trap_R_SetColor(NULL);
}

static void UI_DrawAllMapsSelection(rectDef_t* rect, float scale, vec4_t color, int textStyle, qboolean net) {
    int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
    if (map >= 0 && map < uiInfo.mapCount) {
        Text_Paint(rect->x, rect->y, scale, color, uiInfo.mapList[map].mapName, 0, 0, textStyle);
    }
}

static void UI_DrawOpponentName(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    Text_Paint(rect->x, rect->y, scale, color, UI_Cvar_VariableString("ui_opponentName"), 0, 0, textStyle);
}

static int UI_OwnerDrawWidth(int ownerDraw, float scale) {
    int i, h;
    const char* s = NULL;

    switch (ownerDraw) {
        case UI_HANDICAP:
            h = Com_Clamp(5, 100, trap_Cvar_VariableValue("handicap"));
            i = 20 - h / 5;
            s = handicapValues[i];
            break;
        case UI_GAMETYPE:
            s = uiInfo.gameTypes[ui_gameType.integer].gameType;
            break;
        case UI_ALLMAPS_SELECTION:
            break;
        case UI_OPPONENT_NAME:
            break;
        case UI_KEYBINDSTATUS:
            if (Display_KeyBindPending()) {
                s = "Waiting for new key... Press ESCAPE to cancel";
            } else {
                s = "Press ENTER or CLICK to change, Press BACKSPACE to clear";
            }
            break;
        default:
            break;
    }

    if (s) {
        return Text_Width(s, scale, 0);
    }
    return 0;
}

static void UI_DrawBotName(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    int value = uiInfo.botIndex;
    int game = trap_Cvar_VariableValue("g_gametype");
    const char* text;
    if (game >= GT_TEAM) {
        text = uiInfo.characterList[value].name;
    } else {
        text = UI_GetBotNameByNumber(value);
    }
    Text_Paint(rect->x, rect->y, scale, color, text, 0, 0, textStyle);
}

static void UI_DrawRedBlue(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    Text_Paint(rect->x, rect->y, scale, color, (uiInfo.redBlue == 0) ? "Red" : "Blue", 0, 0, textStyle);
}

static void UI_DrawCrosshair(rectDef_t* rect, float scale, vec4_t color) {
    if (!uiInfo.currentCrosshair) {
        return;
    }
    trap_R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y - rect->h, rect->w, rect->h, uiInfo.uiDC.Assets.crosshairShader[uiInfo.currentCrosshair]);
    trap_R_SetColor(NULL);
}

/*
===============
UI_BuildPlayerList
===============
*/
static void UI_BuildPlayerList(void) {
    uiClientState_t cs;
    int n, count, team, team2, playerTeamNumber;
    char info[MAX_INFO_STRING];

    trap_GetClientState(&cs);
    trap_GetConfigString(CS_PLAYERS + cs.clientNum, info, MAX_INFO_STRING);
    uiInfo.playerNumber = cs.clientNum;
    uiInfo.teamLeader = atoi(Info_ValueForKey(info, "tl"));
    team = atoi(Info_ValueForKey(info, "t"));
    trap_GetConfigString(CS_SERVERINFO, info, sizeof(info));
    count = atoi(Info_ValueForKey(info, "sv_maxclients"));
    uiInfo.playerCount = 0;
    uiInfo.myTeamCount = 0;
    playerTeamNumber = 0;
    for (n = 0; n < count; n++) {
        trap_GetConfigString(CS_PLAYERS + n, info, MAX_INFO_STRING);

        if (info[0]) {
            Q_strncpyz(uiInfo.playerNames[uiInfo.playerCount], Info_ValueForKey(info, "n"), MAX_NAME_LENGTH);
            Q_CleanStr(uiInfo.playerNames[uiInfo.playerCount]);
            uiInfo.playerCount++;
            team2 = atoi(Info_ValueForKey(info, "t"));
            if (team2 == team) {
                Q_strncpyz(uiInfo.teamNames[uiInfo.myTeamCount], Info_ValueForKey(info, "n"), MAX_NAME_LENGTH);
                Q_CleanStr(uiInfo.teamNames[uiInfo.myTeamCount]);
                uiInfo.teamClientNums[uiInfo.myTeamCount] = n;
                if (uiInfo.playerNumber == n) {
                    playerTeamNumber = uiInfo.myTeamCount;
                }
                uiInfo.myTeamCount++;
            }
        }
    }

    if (!uiInfo.teamLeader) {
        trap_Cvar_Set("cg_selectedPlayer", va("%d", playerTeamNumber));
    }

    n = trap_Cvar_VariableValue("cg_selectedPlayer");
    if (n < 0 || n > uiInfo.myTeamCount) {
        n = 0;
    }
    if (n < uiInfo.myTeamCount) {
        trap_Cvar_Set("cg_selectedPlayerName", uiInfo.teamNames[n]);
    }
}

static void UI_DrawSelectedPlayer(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) {
        uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
        UI_BuildPlayerList();
    }
    Text_Paint(rect->x, rect->y, scale, color, (uiInfo.teamLeader) ? UI_Cvar_VariableString("cg_selectedPlayerName") : UI_Cvar_VariableString("name"), 0, 0, textStyle);
}

static void UI_DrawServerMOTD(rectDef_t* rect, float scale, vec4_t color) {
    if (uiInfo.serverStatus.motdLen) {
        float maxX;

        if (uiInfo.serverStatus.motdWidth == -1) {
            uiInfo.serverStatus.motdWidth = 0;
            uiInfo.serverStatus.motdPaintX = rect->x + 1;
            uiInfo.serverStatus.motdPaintX2 = -1;
        }

        if (uiInfo.serverStatus.motdOffset > uiInfo.serverStatus.motdLen) {
            uiInfo.serverStatus.motdOffset = 0;
            uiInfo.serverStatus.motdPaintX = rect->x + 1;
            uiInfo.serverStatus.motdPaintX2 = -1;
        }

        if (uiInfo.uiDC.realTime > uiInfo.serverStatus.motdTime) {
            uiInfo.serverStatus.motdTime = uiInfo.uiDC.realTime + 10;
            if (uiInfo.serverStatus.motdPaintX <= rect->x + 2) {
                if (uiInfo.serverStatus.motdOffset < uiInfo.serverStatus.motdLen) {
                    uiInfo.serverStatus.motdPaintX += Text_Width(&uiInfo.serverStatus.motd[uiInfo.serverStatus.motdOffset], scale, 1) - 1;
                    uiInfo.serverStatus.motdOffset++;
                } else {
                    uiInfo.serverStatus.motdOffset = 0;
                    if (uiInfo.serverStatus.motdPaintX2 >= 0) {
                        uiInfo.serverStatus.motdPaintX = uiInfo.serverStatus.motdPaintX2;
                    } else {
                        uiInfo.serverStatus.motdPaintX = rect->x + rect->w - 2;
                    }
                    uiInfo.serverStatus.motdPaintX2 = -1;
                }
            } else {
                // serverStatus.motdPaintX--;
                uiInfo.serverStatus.motdPaintX -= 2;
                if (uiInfo.serverStatus.motdPaintX2 >= 0) {
                    // serverStatus.motdPaintX2--;
                    uiInfo.serverStatus.motdPaintX2 -= 2;
                }
            }
        }

        maxX = rect->x + rect->w - 2;
        Text_Paint_Limit(&maxX, uiInfo.serverStatus.motdPaintX, rect->y + rect->h - 3, scale, color, &uiInfo.serverStatus.motd[uiInfo.serverStatus.motdOffset], 0, 0);
        if (uiInfo.serverStatus.motdPaintX2 >= 0) {
            float maxX2 = rect->x + rect->w - 2;
            Text_Paint_Limit(&maxX2, uiInfo.serverStatus.motdPaintX2, rect->y + rect->h - 3, scale, color, uiInfo.serverStatus.motd, 0, uiInfo.serverStatus.motdOffset);
        }
        if (uiInfo.serverStatus.motdOffset && maxX > 0) {
            // if we have an offset ( we are skipping the first part of the string ) and we fit the string
            if (uiInfo.serverStatus.motdPaintX2 == -1) {
                uiInfo.serverStatus.motdPaintX2 = rect->x + rect->w - 2;
            }
        } else {
            uiInfo.serverStatus.motdPaintX2 = -1;
        }
    }
}

static void UI_DrawKeyBindStatus(rectDef_t* rect, float scale, vec4_t color, int textStyle) {
    //	int ofs = 0; TTimo: unused
    if (Display_KeyBindPending()) {
        Text_Paint(rect->x, rect->y, scale, color, "Waiting for new key... Press ESCAPE to cancel", 0, 0, textStyle);
    } else {
        Text_Paint(rect->x, rect->y, scale, color, "Press ENTER or CLICK to change, Press BACKSPACE to clear", 0, 0, textStyle);
    }
}

// FIXME: table drive
//
static void UI_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, float scale, vec4_t color, qhandle_t shader, int textStyle) {
    rectDef_t rect;

    rect.x = x + text_x;
    rect.y = y + text_y;
    rect.w = w;
    rect.h = h;

    switch (ownerDraw) {
        case UI_HANDICAP:
            UI_DrawHandicap(&rect, scale, color, textStyle);
            break;
        case UI_PLAYERMODEL:
            UI_DrawPlayerModel(&rect);
            break;
        case UI_CLANCINEMATIC:
            UI_DrawClanCinematic(&rect, scale, color);
            break;
        case UI_PREVIEWCINEMATIC:
            UI_DrawPreviewCinematic(&rect, scale, color);
            break;
        case UI_GAMETYPE:
            UI_DrawGameType(&rect, scale, color, textStyle);
            break;
        case UI_NETGAMETYPE:
            UI_DrawNetGameType(&rect, scale, color, textStyle);
            break;
        case UI_JOINGAMETYPE:
            UI_DrawJoinGameType(&rect, scale, color, textStyle);
            break;
        case UI_MAPPREVIEW:
            UI_DrawMapPreview(&rect, scale, color, qtrue);
            break;
        case UI_MAP_TIMETOBEAT:
            UI_DrawMapTimeToBeat(&rect, scale, color, textStyle);
            break;
        case UI_MAPCINEMATIC:
            UI_DrawMapCinematic(&rect, scale, color, qfalse);
            break;
        case UI_STARTMAPCINEMATIC:
            UI_DrawMapCinematic(&rect, scale, color, qtrue);
            break;
        case UI_NETMAPPREVIEW:
            UI_DrawNetMapPreview(&rect, scale, color);
            break;
        case UI_NETMAPCINEMATIC:
            UI_DrawNetMapCinematic(&rect, scale, color);
            break;
        case UI_OPPONENTMODEL:
            UI_DrawOpponent(&rect);
            break;
        case UI_ALLMAPS_SELECTION:
            UI_DrawAllMapsSelection(&rect, scale, color, textStyle, qtrue);
            break;
        case UI_MAPS_SELECTION:
            UI_DrawAllMapsSelection(&rect, scale, color, textStyle, qfalse);
            break;
        case UI_OPPONENT_NAME:
            UI_DrawOpponentName(&rect, scale, color, textStyle);
            break;
        case UI_BOTNAME:
            UI_DrawBotName(&rect, scale, color, textStyle);
            break;
        case UI_REDBLUE:
            UI_DrawRedBlue(&rect, scale, color, textStyle);
            break;
        case UI_CROSSHAIR:
            UI_DrawCrosshair(&rect, scale, color);
            break;
        case UI_SELECTEDPLAYER:
            UI_DrawSelectedPlayer(&rect, scale, color, textStyle);
            break;
        case UI_SERVERMOTD:
            UI_DrawServerMOTD(&rect, scale, color);
            break;
        case UI_KEYBINDSTATUS:
            UI_DrawKeyBindStatus(&rect, scale, color, textStyle);
            break;
        default:
            break;
    }
}

static qboolean UI_OwnerDrawVisible(int flags) {
    qboolean vis = qtrue;

    while (flags) {
        if (flags & UI_SHOW_FFA) {
            if (trap_Cvar_VariableValue("g_gametype") != GT_FFA) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_FFA;
        }

        if (flags & UI_SHOW_NOTFFA) {
            if (trap_Cvar_VariableValue("g_gametype") == GT_FFA) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_NOTFFA;
        }

        if (flags & UI_SHOW_LEADER) {
            // these need to show when this client can give orders to a player or a group
            if (!uiInfo.teamLeader) {
                vis = qfalse;
            } else {
                // if showing yourself
                if (ui_selectedPlayer.integer < uiInfo.myTeamCount && uiInfo.teamClientNums[ui_selectedPlayer.integer] == uiInfo.playerNumber) {
                    vis = qfalse;
                }
            }
            flags &= ~UI_SHOW_LEADER;
        }
        if (flags & UI_SHOW_NOTLEADER) {
            // these need to show when this client is assigning their own status or they are NOT the leader
            if (uiInfo.teamLeader) {
                // if not showing yourself
                if (!(ui_selectedPlayer.integer < uiInfo.myTeamCount && uiInfo.teamClientNums[ui_selectedPlayer.integer] == uiInfo.playerNumber)) {
                    vis = qfalse;
                }
                // these need to show when this client can give orders to a player or a group
            }
            flags &= ~UI_SHOW_NOTLEADER;
        }
        if (flags & UI_SHOW_ANYTEAMGAME) {
            if (uiInfo.gameTypes[ui_gameType.integer].gtEnum <= GT_TEAM) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_ANYTEAMGAME;
        }
        if (flags & UI_SHOW_ANYNONTEAMGAME) {
            if (uiInfo.gameTypes[ui_gameType.integer].gtEnum > GT_TEAM) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_ANYNONTEAMGAME;
        }
        if (flags & UI_SHOW_NETANYTEAMGAME) {
            if (uiInfo.gameTypes[ui_netGameType.integer].gtEnum <= GT_TEAM) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_NETANYTEAMGAME;
        }
        if (flags & UI_SHOW_NETANYNONTEAMGAME) {
            if (uiInfo.gameTypes[ui_netGameType.integer].gtEnum > GT_TEAM) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_NETANYNONTEAMGAME;
        }
        if (flags & UI_SHOW_NEWHIGHSCORE) {
            if (uiInfo.newHighScoreTime < uiInfo.uiDC.realTime) {
                vis = qfalse;
            } else {
                if (uiInfo.soundHighScore) {
                    if (trap_Cvar_VariableValue("sv_killserver") == 0) {
                        // wait on server to go down before playing sound
                        trap_S_StartLocalSound(uiInfo.newHighScoreSound, CHAN_ANNOUNCER);
                        uiInfo.soundHighScore = qfalse;
                    }
                }
            }
            flags &= ~UI_SHOW_NEWHIGHSCORE;
        }
        if (flags & UI_SHOW_NEWBESTTIME) {
            if (uiInfo.newBestTime < uiInfo.uiDC.realTime) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_NEWBESTTIME;
        }
        if (flags & UI_SHOW_DEMOAVAILABLE) {
            if (!uiInfo.demoAvailable) {
                vis = qfalse;
            }
            flags &= ~UI_SHOW_DEMOAVAILABLE;
        } else {
            flags = 0;
        }
    }
    return vis;
}

static qboolean UI_Handicap_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int h;

        h = Com_Clamp(5, 100, trap_Cvar_VariableValue("handicap"));
        h += 5 * select;

        if (h > 100) {
            h = 5;
        } else if (h < 5) {
            h = 100;
        }

        trap_Cvar_SetValue("handicap", h);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_Effects_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        uiInfo.effectsColor += select;

        if (uiInfo.effectsColor > 6) {
            uiInfo.effectsColor = 0;
        } else if (uiInfo.effectsColor < 0) {
            uiInfo.effectsColor = 6;
        }

        trap_Cvar_SetValue("color1", uitogamecode[uiInfo.effectsColor]);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_ClanName_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int i;

        i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));

        if (uiInfo.teamList[i].cinematic >= 0) {
            trap_CIN_StopCinematic(uiInfo.teamList[i].cinematic);
            uiInfo.teamList[i].cinematic = -1;
        }

        i += select;

        if (i >= uiInfo.teamCount) {
            i = 0;
        } else if (i < 0) {
            i = uiInfo.teamCount - 1;
        }

        trap_Cvar_Set("ui_teamName", uiInfo.teamList[i].teamName);
        UI_HeadCountByTeam();
        UI_FeederSelection(FEEDER_HEADS, 0);
        updateModel = qtrue;
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_GameType_HandleKey(int flags, float* special, int key, qboolean resetMap) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int oldCount = UI_MapCountByGameType(qtrue);

        // hard coded mess here
        if (select < 0) {
            ui_gameType.integer--;
            if (ui_gameType.integer == 2) {
                ui_gameType.integer = 1;
            } else if (ui_gameType.integer < 2) {
                ui_gameType.integer = uiInfo.numGameTypes - 1;
            }
        } else {
            ui_gameType.integer++;
            if (ui_gameType.integer >= uiInfo.numGameTypes) {
                ui_gameType.integer = 1;
            } else if (ui_gameType.integer == 2) {
                ui_gameType.integer = 3;
            }
        }

        if (uiInfo.gameTypes[ui_gameType.integer].gtEnum < GT_TEAM) {
            trap_Cvar_SetValue("ui_Q3Model", 1);
        } else {
            trap_Cvar_SetValue("ui_Q3Model", 0);
        }

        trap_Cvar_SetValue("ui_gameType", ui_gameType.integer);
        UI_SetCapFragLimits(qtrue);
        if (resetMap && oldCount != UI_MapCountByGameType(qtrue)) {
            trap_Cvar_SetValue("ui_currentMap", 0);
            Menu_SetFeederSelection(NULL, FEEDER_MAPS, 0, NULL);
        }
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_NetGameType_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        ui_netGameType.integer += select;

        if (ui_netGameType.integer < 0) {
            ui_netGameType.integer = uiInfo.numGameTypes - 1;
        } else if (ui_netGameType.integer >= uiInfo.numGameTypes) {
            ui_netGameType.integer = 0;
        }

        trap_Cvar_SetValue("ui_netGameType", ui_netGameType.integer);
        trap_Cvar_SetValue("ui_actualnetGameType", uiInfo.gameTypes[ui_netGameType.integer].gtEnum);
        trap_Cvar_SetValue("ui_currentNetMap", 0);
        UI_MapCountByGameType(qfalse);
        Menu_SetFeederSelection(NULL, FEEDER_ALLMAPS, 0, NULL);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_JoinGameType_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        ui_joinGameType.integer += select;

        if (ui_joinGameType.integer < 0) {
            ui_joinGameType.integer = uiInfo.numJoinGameTypes - 1;
        } else if (ui_joinGameType.integer >= uiInfo.numJoinGameTypes) {
            ui_joinGameType.integer = 0;
        }

        trap_Cvar_SetValue("ui_joinGameType", ui_joinGameType.integer);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_TeamName_HandleKey(int flags, float* special, int key, qboolean blue) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int i;

        i = UI_TeamIndexFromName(UI_Cvar_VariableString((blue) ? "ui_blueTeam" : "ui_redTeam"));
        i += select;

        if (i >= uiInfo.teamCount) {
            i = 0;
        } else if (i < 0) {
            i = uiInfo.teamCount - 1;
        }

        trap_Cvar_Set((blue) ? "ui_blueTeam" : "ui_redTeam", uiInfo.teamList[i].teamName);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_OpponentName_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        if (select < 0) {
            UI_PriorOpponent();
        } else {
            UI_NextOpponent();
        }
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_BotName_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int game = trap_Cvar_VariableValue("g_gametype");
        int value = uiInfo.botIndex;

        value += select;

        if (game >= GT_TEAM) {
            if (value >= uiInfo.characterCount) {
                value = 0;
            } else if (value < 0) {
                value = uiInfo.characterCount - 1;
            }
        } else {
            if (value >= UI_GetNumBots()) {
                value = 0;
            } else if (value < 0) {
                value = UI_GetNumBots() - 1;
            }
        }
        uiInfo.botIndex = value;
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_RedBlue_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        uiInfo.redBlue ^= 1;
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_Crosshair_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        uiInfo.currentCrosshair += select;

        if (uiInfo.currentCrosshair >= NUM_CROSSHAIRS) {
            uiInfo.currentCrosshair = 0;
        } else if (uiInfo.currentCrosshair < 0) {
            uiInfo.currentCrosshair = NUM_CROSSHAIRS - 1;
        }
        trap_Cvar_SetValue("cg_drawCrosshair", uiInfo.currentCrosshair);
        return qtrue;
    }
    return qfalse;
}

static qboolean UI_SelectedPlayer_HandleKey(int flags, float* special, int key) {
    int select = UI_SelectForKey(key);
    if (select != 0) {
        int selected;

        UI_BuildPlayerList();
        if (!uiInfo.teamLeader) {
            return qfalse;
        }
        selected = trap_Cvar_VariableValue("cg_selectedPlayer");

        selected += select;

        if (selected > uiInfo.myTeamCount) {
            selected = 0;
        } else if (selected < 0) {
            selected = uiInfo.myTeamCount;
        }

        if (selected == uiInfo.myTeamCount) {
            trap_Cvar_Set("cg_selectedPlayerName", "Everyone");
        } else {
            trap_Cvar_Set("cg_selectedPlayerName", uiInfo.teamNames[selected]);
        }
        trap_Cvar_SetValue("cg_selectedPlayer", selected);
    }
    return qfalse;
}

static qboolean UI_OwnerDrawHandleKey(int ownerDraw, int flags, float* special, int key) {
    switch (ownerDraw) {
        case UI_HANDICAP:
            return UI_Handicap_HandleKey(flags, special, key);
            break;
        case UI_GAMETYPE:
            return UI_GameType_HandleKey(flags, special, key, qtrue);
            break;
        case UI_NETGAMETYPE:
            return UI_NetGameType_HandleKey(flags, special, key);
            break;
        case UI_JOINGAMETYPE:
            return UI_JoinGameType_HandleKey(flags, special, key);
            break;
        case UI_OPPONENT_NAME:
            UI_OpponentName_HandleKey(flags, special, key);
            break;
        case UI_BOTNAME:
            return UI_BotName_HandleKey(flags, special, key);
            break;
        case UI_REDBLUE:
            UI_RedBlue_HandleKey(flags, special, key);
            break;
        case UI_CROSSHAIR:
            UI_Crosshair_HandleKey(flags, special, key);
            break;
        case UI_SELECTEDPLAYER:
            UI_SelectedPlayer_HandleKey(flags, special, key);
            break;
        default:
            break;
    }

    return qfalse;
}

static float UI_GetValue(int ownerDraw) {
    return 0;
}

/*
===============
UI_LoadMods
===============
*/
static void UI_LoadMods(void) {
    int numdirs;
    char dirlist[2048];
    char* dirptr;
    char* descptr;
    int i;
    int dirlen;

    uiInfo.modCount = 0;
    numdirs = trap_FS_GetFileList("$modlist", "", dirlist, sizeof(dirlist));
    dirptr = dirlist;
    for (i = 0; i < numdirs; i++) {
        dirlen = strlen(dirptr) + 1;
        descptr = dirptr + dirlen;
        uiInfo.modList[uiInfo.modCount].modName = String_Alloc(dirptr);
        uiInfo.modList[uiInfo.modCount].modDescr = String_Alloc(descptr);
        dirptr += dirlen + strlen(descptr) + 1;
        uiInfo.modCount++;
        if (uiInfo.modCount >= MAX_MODS) {
            break;
        }
    }
}

/*
===============
UI_LoadTeams
===============
*/
static void UI_LoadTeams(void) {
    char teamList[4096];
    char* teamName;
    int i, len, count;

    count = trap_FS_GetFileList("", "team", teamList, 4096);

    if (count) {
        teamName = teamList;
        for (i = 0; i < count; i++) {
            len = strlen(teamName);
            UI_ParseTeamInfo(teamName);
            teamName += len + 1;
        }
    }
}

/*
===============
UI_LoadMovies
===============
*/
static void UI_LoadMovies(void) {
    char movielist[4096];
    char* moviename;
    int i, len;

    uiInfo.movieCount = trap_FS_GetFileList("video", "roq", movielist, 4096);

    if (uiInfo.movieCount) {
        if (uiInfo.movieCount > MAX_MOVIES) {
            uiInfo.movieCount = MAX_MOVIES;
        }
        moviename = movielist;
        for (i = 0; i < uiInfo.movieCount; i++) {
            len = strlen(moviename);
            if (!Q_stricmp(moviename + len - 4, ".roq")) {
                moviename[len - 4] = '\0';
            }
            Q_strupr(moviename);
            uiInfo.movieList[i] = String_Alloc(moviename);
            moviename += len + 1;
        }
    }
}

#define NAMEBUFSIZE (MAX_DEMOS * 32)

/*
===============
UI_LoadDemos
===============
*/
static void UI_LoadDemos(void) {
    char demolist[NAMEBUFSIZE];
    char demoExt[32];
    char* demoname;
    int i, j, len;
    int protocol, protocolLegacy;

    protocolLegacy = trap_Cvar_VariableValue("com_legacyprotocol");
    protocol = trap_Cvar_VariableValue("com_protocol");

    if (!protocol)
        protocol = trap_Cvar_VariableValue("protocol");
    if (protocolLegacy == protocol)
        protocolLegacy = 0;

    Com_sprintf(demoExt, sizeof(demoExt), ".%s%d", DEMOEXT, protocol);
    uiInfo.demoCount = trap_FS_GetFileList("demos", demoExt, demolist, ARRAY_LEN(demolist));

    demoname = demolist;
    i = 0;

    for (j = 0; j < 2; j++) {
        if (uiInfo.demoCount > MAX_DEMOS)
            uiInfo.demoCount = MAX_DEMOS;

        for (; i < uiInfo.demoCount; i++) {
            len = strlen(demoname);
            uiInfo.demoList[i] = String_Alloc(demoname);
            demoname += len + 1;
        }

        if (!j) {
            if (protocolLegacy > 0 && uiInfo.demoCount < MAX_DEMOS) {
                Com_sprintf(demoExt, sizeof(demoExt), ".%s%d", DEMOEXT, protocolLegacy);
                uiInfo.demoCount += trap_FS_GetFileList("demos", demoExt, demolist, ARRAY_LEN(demolist));
                demoname = demolist;
            } else
                break;
        }
    }
}

static qboolean UI_SetNextMap(int actual, int index) {
    int i;
    for (i = actual + 1; i < uiInfo.mapCount; i++) {
        if (uiInfo.mapList[i].active) {
            Menu_SetFeederSelection(NULL, FEEDER_MAPS, index + 1, "skirmish");
            return qtrue;
        }
    }
    return qfalse;
}

static void UI_StartSkirmish(qboolean next) {
    int i, k, g, delay, temp;
    float skill;
    char buff[MAX_STRING_CHARS];

    if (next) {
        int actual;
        int index = trap_Cvar_VariableValue("ui_mapIndex");
        UI_MapCountByGameType(qtrue);
        UI_SelectedMap(index, &actual);
        if (UI_SetNextMap(actual, index)) {
        } else {
            UI_GameType_HandleKey(0, NULL, K_MOUSE1, qfalse);
            UI_MapCountByGameType(qtrue);
            Menu_SetFeederSelection(NULL, FEEDER_MAPS, 0, "skirmish");
        }
    }

    g = uiInfo.gameTypes[ui_gameType.integer].gtEnum;
    trap_Cvar_SetValue("g_gametype", g);
    trap_Cmd_ExecuteText(EXEC_APPEND, va("wait ; wait ; map %s\n", uiInfo.mapList[ui_currentMap.integer].mapLoadName));
    skill = trap_Cvar_VariableValue("g_spSkill");
    trap_Cvar_Set("ui_scoreMap", uiInfo.mapList[ui_currentMap.integer].mapName);

    k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));

    trap_Cvar_Set("ui_singlePlayerActive", "1");

    // set up sp overrides, will be replaced on postgame
    temp = trap_Cvar_VariableValue("capturelimit");
    trap_Cvar_Set("ui_saveCaptureLimit", va("%i", temp));
    temp = trap_Cvar_VariableValue("fraglimit");
    trap_Cvar_Set("ui_saveFragLimit", va("%i", temp));

    UI_SetCapFragLimits(qfalse);

    temp = trap_Cvar_VariableValue("cg_drawTimer");
    trap_Cvar_Set("ui_drawTimer", va("%i", temp));
    temp = trap_Cvar_VariableValue("g_doWarmup");
    trap_Cvar_Set("ui_doWarmup", va("%i", temp));
    temp = trap_Cvar_VariableValue("g_friendlyFire");
    trap_Cvar_Set("ui_friendlyFire", va("%i", temp));
    temp = trap_Cvar_VariableValue("sv_maxClients");
    trap_Cvar_Set("ui_maxClients", va("%i", temp));
    temp = trap_Cvar_VariableValue("g_warmup");
    trap_Cvar_Set("ui_Warmup", va("%i", temp));
    temp = trap_Cvar_VariableValue("sv_pure");
    trap_Cvar_Set("ui_pure", va("%i", temp));

    trap_Cvar_Set("cg_cameraOrbit", "0");
    trap_Cvar_Set("cg_thirdPerson", "0");
    trap_Cvar_Set("cg_drawTimer", "1");
    trap_Cvar_Set("g_doWarmup", "1");
    trap_Cvar_Set("g_warmup", "15");
    trap_Cvar_Set("sv_pure", "0");
    trap_Cvar_Set("g_friendlyFire", "0");
    trap_Cvar_Set("g_redTeam", UI_Cvar_VariableString("ui_teamName"));
    trap_Cvar_Set("g_blueTeam", UI_Cvar_VariableString("ui_opponentName"));

    if (trap_Cvar_VariableValue("ui_recordSPDemo")) {
        Com_sprintf(buff, MAX_STRING_CHARS, "%s_%i", uiInfo.mapList[ui_currentMap.integer].mapLoadName, g);
        trap_Cvar_Set("ui_recordSPDemoName", buff);
    }

    delay = 500;

    if (g == GT_TOURNAMENT) {
        trap_Cvar_Set("sv_maxClients", "2");
        Com_sprintf(buff, sizeof(buff),
                    "wait ; addbot %s %f "
                    ", %i \n",
                    uiInfo.mapList[ui_currentMap.integer].opponentName, skill, delay);
        trap_Cmd_ExecuteText(EXEC_APPEND, buff);
    } else {
        temp = uiInfo.mapList[ui_currentMap.integer].teamMembers * 2;
        trap_Cvar_Set("sv_maxClients", va("%d", temp));
        for (i = 0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers; i++) {
            Com_sprintf(buff, sizeof(buff), "addbot %s %f %s %i %s\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill, (g == GT_FFA) ? "" : "Blue", delay, uiInfo.teamList[k].teamMembers[i]);
            trap_Cmd_ExecuteText(EXEC_APPEND, buff);
            delay += 500;
        }
        k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
        for (i = 0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers - 1; i++) {
            Com_sprintf(buff, sizeof(buff), "addbot %s %f %s %i %s\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill, (g == GT_FFA) ? "" : "Red", delay, uiInfo.teamList[k].teamMembers[i]);
            trap_Cmd_ExecuteText(EXEC_APPEND, buff);
            delay += 500;
        }
    }
    if (g >= GT_TEAM) {
        // send team command for vanilla q3 game qvm
        trap_Cmd_ExecuteText(EXEC_APPEND, "wait 5; team Red\n");

        // set g_localTeamPref for ioq3 game qvm
        trap_Cvar_Set("g_localTeamPref", "Red");
    }
}

static void UI_Update(const char* name) {
    int val = trap_Cvar_VariableValue(name);

    if (Q_stricmp(name, "ui_SetName") == 0) {
        trap_Cvar_Set("name", UI_Cvar_VariableString("ui_Name"));
    } else if (Q_stricmp(name, "ui_setRate") == 0) {
        float rate = trap_Cvar_VariableValue("rate");
        if (rate >= 5000) {
            trap_Cvar_Set("cl_maxpackets", "30");
            trap_Cvar_Set("cl_packetdup", "1");
        } else if (rate >= 4000) {
            trap_Cvar_Set("cl_maxpackets", "15");
            trap_Cvar_Set("cl_packetdup", "2");  // favor less prediction errors when there's packet loss
        } else {
            trap_Cvar_Set("cl_maxpackets", "15");
            trap_Cvar_Set("cl_packetdup", "1");  // favor lower bandwidth
        }
    } else if (Q_stricmp(name, "ui_GetName") == 0) {
        trap_Cvar_Set("ui_Name", UI_Cvar_VariableString("name"));
    } else if (Q_stricmp(name, "r_colorbits") == 0) {
        switch (val) {
            case 0:
                trap_Cvar_SetValue("r_depthbits", 0);
                trap_Cvar_Reset("r_stencilbits");
                break;
            case 16:
                trap_Cvar_SetValue("r_depthbits", 16);
                trap_Cvar_SetValue("r_stencilbits", 0);
                break;
            case 32:
                trap_Cvar_SetValue("r_depthbits", 24);
                trap_Cvar_SetValue("r_stencilbits", 8);
                break;
        }
    } else if (Q_stricmp(name, "r_lodbias") == 0) {
        switch (val) {
            case 0:
                trap_Cvar_SetValue("r_subdivisions", 4);
                break;
            case 1:
                trap_Cvar_SetValue("r_subdivisions", 12);
                break;
            case 2:
                trap_Cvar_SetValue("r_subdivisions", 20);
                break;
        }
    } else if (Q_stricmp(name, "ui_glCustom") == 0) {
        switch (val) {
            case 0:  // high quality
                trap_Cvar_SetValue("r_fullScreen", 1);
                trap_Cvar_SetValue("r_subdivisions", 4);
                trap_Cvar_SetValue("r_vertexlight", 0);
                trap_Cvar_SetValue("r_lodbias", 0);
                trap_Cvar_SetValue("r_colorbits", 32);
                trap_Cvar_SetValue("r_depthbits", 24);
                trap_Cvar_SetValue("r_stencilbits", 8);
                trap_Cvar_SetValue("r_picmip", 0);
                trap_Cvar_SetValue("r_mode", 4);
                trap_Cvar_Set("ui_videomode", "800x600");
                trap_Cvar_SetValue("r_texturebits", 32);
                trap_Cvar_SetValue("r_fastSky", 0);
                trap_Cvar_SetValue("r_inGameVideo", 1);
                trap_Cvar_SetValue("cg_shadows", 1);
                trap_Cvar_SetValue("cg_brassTime", 2500);
                trap_Cvar_Set("r_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
                break;
            case 1:  // normal
                trap_Cvar_SetValue("r_fullScreen", 1);
                trap_Cvar_SetValue("r_subdivisions", 12);
                trap_Cvar_SetValue("r_vertexlight", 0);
                trap_Cvar_SetValue("r_lodbias", 0);
                trap_Cvar_SetValue("r_colorbits", 0);
                trap_Cvar_SetValue("r_depthbits", 0);
                trap_Cvar_Reset("r_stencilbits");
                trap_Cvar_SetValue("r_picmip", 1);
                trap_Cvar_SetValue("r_mode", 3);
                trap_Cvar_Set("ui_videomode", "640x480");
                trap_Cvar_SetValue("r_texturebits", 0);
                trap_Cvar_SetValue("r_fastSky", 0);
                trap_Cvar_SetValue("r_inGameVideo", 1);
                trap_Cvar_SetValue("cg_brassTime", 2500);
                trap_Cvar_Set("r_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
                trap_Cvar_SetValue("cg_shadows", 0);
                break;
            case 2:  // fast
                trap_Cvar_SetValue("r_fullScreen", 1);
                trap_Cvar_SetValue("r_subdivisions", 8);
                trap_Cvar_SetValue("r_vertexlight", 0);
                trap_Cvar_SetValue("r_lodbias", 1);
                trap_Cvar_SetValue("r_colorbits", 0);
                trap_Cvar_SetValue("r_depthbits", 0);
                trap_Cvar_Reset("r_stencilbits");
                trap_Cvar_SetValue("r_picmip", 1);
                trap_Cvar_SetValue("r_mode", 3);
                trap_Cvar_Set("ui_videomode", "640x480");
                trap_Cvar_SetValue("r_texturebits", 0);
                trap_Cvar_SetValue("cg_shadows", 0);
                trap_Cvar_SetValue("r_fastSky", 1);
                trap_Cvar_SetValue("r_inGameVideo", 0);
                trap_Cvar_SetValue("cg_brassTime", 0);
                trap_Cvar_Set("r_texturemode", "GL_LINEAR_MIPMAP_NEAREST");
                break;
            case 3:  // fastest
                trap_Cvar_SetValue("r_fullScreen", 1);
                trap_Cvar_SetValue("r_subdivisions", 20);
                trap_Cvar_SetValue("r_vertexlight", 1);
                trap_Cvar_SetValue("r_lodbias", 2);
                trap_Cvar_SetValue("r_colorbits", 16);
                trap_Cvar_SetValue("r_depthbits", 16);
                trap_Cvar_SetValue("r_stencilbits", 0);
                trap_Cvar_SetValue("r_mode", 3);
                trap_Cvar_Set("ui_videomode", "640x480");
                trap_Cvar_SetValue("r_picmip", 2);
                trap_Cvar_SetValue("r_texturebits", 16);
                trap_Cvar_SetValue("cg_shadows", 0);
                trap_Cvar_SetValue("cg_brassTime", 0);
                trap_Cvar_SetValue("r_fastSky", 1);
                trap_Cvar_SetValue("r_inGameVideo", 0);
                trap_Cvar_Set("r_texturemode", "GL_LINEAR_MIPMAP_NEAREST");
                break;
        }
    } else if (Q_stricmp(name, "ui_mousePitch") == 0) {
        if (val == 0) {
            trap_Cvar_SetValue("m_pitch", 0.022f);
        } else {
            trap_Cvar_SetValue("m_pitch", -0.022f);
        }
    }
}

static void UI_RunMenuScript(char** args) {
    const char *name, *name2;
    char buff[1024];

    if (String_Parse(args, &name)) {
        if (Q_stricmp(name, "StartServer") == 0) {
            int clients;
            float skill;
            trap_Cvar_Set("cg_thirdPerson", "0");
            trap_Cvar_Set("cg_cameraOrbit", "0");
            trap_Cvar_Set("ui_singlePlayerActive", "0");
            trap_Cvar_SetValue("dedicated", Com_Clamp(0, 2, ui_dedicated.integer));
            trap_Cvar_SetValue("g_gametype", Com_Clamp(0, GT_MAX_GAME_TYPE - 1, uiInfo.gameTypes[ui_netGameType.integer].gtEnum));
            trap_Cvar_Set("g_redTeam", UI_Cvar_VariableString("ui_teamName"));
            trap_Cvar_Set("g_blueTeam", UI_Cvar_VariableString("ui_opponentName"));
            trap_Cmd_ExecuteText(EXEC_APPEND, va("wait ; wait ; map %s\n", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName));
            skill = trap_Cvar_VariableValue("g_spSkill");
            // set max clients based on spots
            clients = trap_Cvar_VariableValue("sv_maxClients");
        } else if (Q_stricmp(name, "resetDefaults") == 0) {
            trap_Cmd_ExecuteText(EXEC_APPEND, "exec default.cfg\n");
            trap_Cmd_ExecuteText(EXEC_APPEND, "cvar_restart\n");
            Controls_SetDefaults();
            trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
        } else if (Q_stricmp(name, "loadArenas") == 0) {
            UI_LoadArenasIntoMapList();
            UI_MapCountByGameType(qfalse);
            Menu_SetFeederSelection(NULL, FEEDER_ALLMAPS, 0, "createserver");
        } else if (Q_stricmp(name, "saveControls") == 0) {
            Controls_SetConfig(qtrue);
        } else if (Q_stricmp(name, "loadControls") == 0) {
            Controls_GetConfig();
        } else if (Q_stricmp(name, "clearError") == 0) {
            trap_Cvar_Set("com_errorMessage", "");
        } else if (Q_stricmp(name, "loadGameInfo") == 0) {
            UI_ParseGameInfo("gameinfo.txt");
        } else if (Q_stricmp(name, "LoadDemos") == 0) {
            UI_LoadDemos();
        } else if (Q_stricmp(name, "LoadMovies") == 0) {
            UI_LoadMovies();
        } else if (Q_stricmp(name, "LoadMods") == 0) {
            UI_LoadMods();
        } else if (Q_stricmp(name, "RunMod") == 0) {
            trap_Cvar_Set("fs_game", uiInfo.modList[uiInfo.modIndex].modName);
            trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart;");
        } else if (Q_stricmp(name, "RunDemo") == 0) {
            trap_Cmd_ExecuteText(EXEC_APPEND, va("demo %s\n", uiInfo.demoList[uiInfo.demoIndex]));
        } else if (Q_stricmp(name, "Quake3") == 0) {
            trap_Cvar_Set("fs_game", "");
            trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart;");
        } else if (Q_stricmp(name, "closeJoin") == 0) {
            Menus_CloseByName("joinserver");
            Menus_OpenByName("main");
        } else if (Q_stricmp(name, "UpdateFilter") == 0) {
            UI_FeederSelection(FEEDER_SERVERS, 0);
        } else if (Q_stricmp(name, "JoinServer") == 0) {
            trap_Cvar_Set("cg_thirdPerson", "0");
            trap_Cvar_Set("cg_cameraOrbit", "0");
            trap_Cvar_Set("ui_singlePlayerActive", "0");
            if (uiInfo.serverStatus.currentServer >= 0 && uiInfo.serverStatus.currentServer < uiInfo.serverStatus.numDisplayServers) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("connect %s\n", buff));
            }
        } else if (Q_stricmp(name, "FoundPlayerJoinServer") == 0) {
            trap_Cvar_Set("ui_singlePlayerActive", "0");
            if (uiInfo.currentFoundPlayerServer >= 0 && uiInfo.currentFoundPlayerServer < uiInfo.numFoundPlayerServers) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("connect %s\n", uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer]));
            }
        } else if (Q_stricmp(name, "Quit") == 0) {
            trap_Cvar_Set("ui_singlePlayerActive", "0");
            trap_Cmd_ExecuteText(EXEC_NOW, "quit");
        } else if (Q_stricmp(name, "Controls") == 0) {
            trap_Cvar_Set("cl_paused", "1");
            trap_Key_SetCatcher(KEYCATCH_UI);
            Menus_CloseAll();
            Menus_ActivateByName("setup_menu2");
        } else if (Q_stricmp(name, "Leave") == 0) {
            trap_Cmd_ExecuteText(EXEC_APPEND, "disconnect\n");
            trap_Key_SetCatcher(KEYCATCH_UI);
            Menus_CloseAll();
            Menus_ActivateByName("main");
        } else if (Q_stricmp(name, "nextSkirmish") == 0) {
            UI_StartSkirmish(qtrue);
        } else if (Q_stricmp(name, "SkirmishStart") == 0) {
            UI_StartSkirmish(qfalse);
        } else if (Q_stricmp(name, "closeingame") == 0) {
            trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
            trap_Key_ClearStates();
            trap_Cvar_Set("cl_paused", "0");
            Menus_CloseAll();
        } else if (Q_stricmp(name, "voteMap") == 0) {
            if (ui_currentNetMap.integer >= 0 && ui_currentNetMap.integer < uiInfo.mapCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("callvote map %s\n", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName));
            }
        } else if (Q_stricmp(name, "voteKick") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("callvote kick %s\n", uiInfo.playerNames[uiInfo.playerIndex]));
            }
        } else if (Q_stricmp(name, "voteGame") == 0) {
            if (ui_netGameType.integer >= 0 && ui_netGameType.integer < uiInfo.numGameTypes) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("callvote g_gametype %i\n", uiInfo.gameTypes[ui_netGameType.integer].gtEnum));
            }
        } else if (Q_stricmp(name, "addBot") == 0) {
            if (trap_Cvar_VariableValue("g_gametype") >= GT_TEAM) {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("addbot %s %i %s\n", uiInfo.characterList[uiInfo.botIndex].name, uiInfo.skillIndex + 1, (uiInfo.redBlue == 0) ? "Red" : "Blue"));
            } else {
                trap_Cmd_ExecuteText(EXEC_APPEND, va("addbot %s %i %s\n", UI_GetBotNameByNumber(uiInfo.botIndex), uiInfo.skillIndex + 1, (uiInfo.redBlue == 0) ? "Red" : "Blue"));
            }
        } else if (Q_stricmp(name, "orders") == 0) {
            const char* orders;
            if (String_Parse(args, &orders)) {
                int selectedPlayer = trap_Cvar_VariableValue("cg_selectedPlayer");
                if (selectedPlayer < uiInfo.myTeamCount) {
                    Com_sprintf(buff, sizeof(buff), orders, uiInfo.teamClientNums[selectedPlayer]);
                    trap_Cmd_ExecuteText(EXEC_APPEND, buff);
                    trap_Cmd_ExecuteText(EXEC_APPEND, "\n");
                } else {
                    int i;
                    for (i = 0; i < uiInfo.myTeamCount; i++) {
                        if (uiInfo.playerNumber == uiInfo.teamClientNums[i]) {
                            continue;
                        }
                        Com_sprintf(buff, sizeof(buff), orders, uiInfo.teamClientNums[i]);
                        trap_Cmd_ExecuteText(EXEC_APPEND, buff);
                        trap_Cmd_ExecuteText(EXEC_APPEND, "\n");
                    }
                }
                trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
                trap_Key_ClearStates();
                trap_Cvar_Set("cl_paused", "0");
                Menus_CloseAll();
            }
        } else if (Q_stricmp(name, "glCustom") == 0) {
            trap_Cvar_Set("ui_glCustom", "4");
        } else if (Q_stricmp(name, "update") == 0) {
            if (String_Parse(args, &name2)) {
                UI_Update(name2);
            }
        } else if (Q_stricmp(name, "setPbClStatus") == 0) {
            int stat;
            if (Int_Parse(args, &stat))
                trap_SetPbClStatus(stat);
        } else if (Q_stricmp(name, "stopRefresh") == 0) {
            // do nothing - the Quake Live menus do this often so don't error out in console.
        } else {
            Com_Printf("unknown UI script %s\n", name);
        }
    }
}

static void UI_GetTeamColor(vec4_t* color) {
}

/*
==================
UI_MapCountByGameType
==================
*/
static int UI_MapCountByGameType(qboolean singlePlayer) {
    int i, c, game;
    c = 0;
    game = singlePlayer ? uiInfo.gameTypes[ui_gameType.integer].gtEnum : uiInfo.gameTypes[ui_netGameType.integer].gtEnum;
    if (game == GT_SINGLE_PLAYER) {
        game++;
    }
    if (game == GT_TEAM) {
        game = GT_FFA;
    }

    for (i = 0; i < uiInfo.mapCount; i++) {
        uiInfo.mapList[i].active = qfalse;
        if (uiInfo.mapList[i].typeBits & (1 << game)) {
            if (singlePlayer) {
                if (!(uiInfo.mapList[i].typeBits & (1 << GT_SINGLE_PLAYER))) {
                    continue;
                }
            }
            c++;
            uiInfo.mapList[i].active = qtrue;
        }
    }
    return c;
}

qboolean UI_hasSkinForBase(const char* base, const char* team) {
    char test[MAX_QPATH];

    Com_sprintf(test, sizeof(test), "models/players/%s/%s/lower_default.skin", base, team);

    if (trap_FS_FOpenFile(test, NULL, FS_READ)) {
        return qtrue;
    }
    Com_sprintf(test, sizeof(test), "models/players/characters/%s/%s/lower_default.skin", base, team);

    if (trap_FS_FOpenFile(test, NULL, FS_READ)) {
        return qtrue;
    }
    return qfalse;
}

/*
==================
UI_MapCountByTeam
==================
*/
static int UI_HeadCountByTeam(void) {
    static int init = 0;
    int i, j, k, c, tIndex;

    c = 0;
    if (!init) {
        for (i = 0; i < uiInfo.characterCount; i++) {
            uiInfo.characterList[i].reference = 0;
            for (j = 0; j < uiInfo.teamCount; j++) {
                if (UI_hasSkinForBase(uiInfo.characterList[i].base, uiInfo.teamList[j].teamName)) {
                    uiInfo.characterList[i].reference |= (1 << j);
                }
            }
        }
        init = 1;
    }

    tIndex = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));

    // do names
    for (i = 0; i < uiInfo.characterCount; i++) {
        uiInfo.characterList[i].active = qfalse;
        for (j = 0; j < TEAM_MEMBERS; j++) {
            if (uiInfo.teamList[tIndex].teamMembers[j] != NULL) {
                if (uiInfo.characterList[i].reference & (1 << tIndex)) {  // && Q_stricmp(uiInfo.teamList[tIndex].teamMembers[j], uiInfo.characterList[i].name)==0) {
                    uiInfo.characterList[i].active = qtrue;
                    c++;
                    break;
                }
            }
        }
    }

    // and then aliases
    for (j = 0; j < TEAM_MEMBERS; j++) {
        for (k = 0; k < uiInfo.aliasCount; k++) {
            if (uiInfo.aliasList[k].name != NULL) {
                if (Q_stricmp(uiInfo.teamList[tIndex].teamMembers[j], uiInfo.aliasList[k].name) == 0) {
                    for (i = 0; i < uiInfo.characterCount; i++) {
                        if (uiInfo.characterList[i].headImage != -1 && uiInfo.characterList[i].reference & (1 << tIndex) && Q_stricmp(uiInfo.aliasList[k].ai, uiInfo.characterList[i].name) == 0) {
                            if (uiInfo.characterList[i].active == qfalse) {
                                uiInfo.characterList[i].active = qtrue;
                                c++;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    return c;
}

/*
==================
stristr
==================
*/
static char* stristr(char* str, char* charset) {
    int i;

    while (*str) {
        for (i = 0; charset[i] && str[i]; i++) {
            if (toupper(charset[i]) != toupper(str[i]))
                break;
        }
        if (!charset[i])
            return str;
        str++;
    }
    return NULL;
}

/*
==================
UI_FeederCount
==================
*/
static int UI_FeederCount(float feederID) {
    if (feederID == FEEDER_HEADS) {
        return UI_HeadCountByTeam();
    } else if (feederID == FEEDER_Q3HEADS) {
        return uiInfo.q3HeadCount;
    } else if (feederID == FEEDER_CINEMATICS) {
        return uiInfo.movieCount;
    } else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS) {
        return UI_MapCountByGameType(feederID == FEEDER_MAPS ? qtrue : qfalse);
    } else if (feederID == FEEDER_FINDPLAYER) {
        return uiInfo.numFoundPlayerServers;
    } else if (feederID == FEEDER_PLAYER_LIST) {
        if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) {
            uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
            UI_BuildPlayerList();
        }
        return uiInfo.playerCount;
    } else if (feederID == FEEDER_TEAM_LIST) {
        if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) {
            uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
            UI_BuildPlayerList();
        }
        return uiInfo.myTeamCount;
    } else if (feederID == FEEDER_MODS) {
        return uiInfo.modCount;
    } else if (feederID == FEEDER_DEMOS) {
        return uiInfo.demoCount;
    }
    return 0;
}

static const char* UI_SelectedMap(int index, int* actual) {
    int i, c;
    c = 0;
    *actual = 0;
    for (i = 0; i < uiInfo.mapCount; i++) {
        if (uiInfo.mapList[i].active) {
            if (c == index) {
                *actual = i;
                return uiInfo.mapList[i].mapName;
            } else {
                c++;
            }
        }
    }
    return "";
}

static const char* UI_SelectedHead(int index, int* actual) {
    int i, c;
    c = 0;
    *actual = 0;
    for (i = 0; i < uiInfo.characterCount; i++) {
        if (uiInfo.characterList[i].active) {
            if (c == index) {
                *actual = i;
                return uiInfo.characterList[i].name;
            } else {
                c++;
            }
        }
    }
    return "";
}

static int UI_GetIndexFromSelection(int actual) {
    int i, c;
    c = 0;
    for (i = 0; i < uiInfo.mapCount; i++) {
        if (uiInfo.mapList[i].active) {
            if (i == actual) {
                return c;
            }
            c++;
        }
    }
    return 0;
}

static const char* UI_FeederItemText(float feederID, int index, int column, qhandle_t* handle) {
    static char info[MAX_STRING_CHARS];
    static char hostname[1024];
    static char clientBuff[32];
    static int lastColumn = -1;
    static int lastTime = 0;
    *handle = -1;
    if (feederID == FEEDER_HEADS) {
        int actual;
        return UI_SelectedHead(index, &actual);
    } else if (feederID == FEEDER_Q3HEADS) {
        if (index >= 0 && index < uiInfo.q3HeadCount) {
            return uiInfo.q3HeadNames[index];
        }
    } else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS) {
        int actual;
        return UI_SelectedMap(index, &actual);
    } else if (feederID == FEEDER_FINDPLAYER) {
        if (index >= 0 && index < uiInfo.numFoundPlayerServers) {
            // return uiInfo.foundPlayerServerAddresses[index];
            return uiInfo.foundPlayerServerNames[index];
        }
    } else if (feederID == FEEDER_PLAYER_LIST) {
        if (index >= 0 && index < uiInfo.playerCount) {
            return uiInfo.playerNames[index];
        }
    } else if (feederID == FEEDER_TEAM_LIST) {
        if (index >= 0 && index < uiInfo.myTeamCount) {
            return uiInfo.teamNames[index];
        }
    } else if (feederID == FEEDER_MODS) {
        if (index >= 0 && index < uiInfo.modCount) {
            if (uiInfo.modList[index].modDescr && *uiInfo.modList[index].modDescr) {
                return uiInfo.modList[index].modDescr;
            } else {
                return uiInfo.modList[index].modName;
            }
        }
    } else if (feederID == FEEDER_CINEMATICS) {
        if (index >= 0 && index < uiInfo.movieCount) {
            return uiInfo.movieList[index];
        }
    } else if (feederID == FEEDER_DEMOS) {
        if (index >= 0 && index < uiInfo.demoCount) {
            return uiInfo.demoList[index];
        }
    }
    return "";
}

static qhandle_t UI_FeederItemImage(float feederID, int index) {
    if (feederID == FEEDER_HEADS) {
        int actual;
        UI_SelectedHead(index, &actual);
        index = actual;
        if (index >= 0 && index < uiInfo.characterCount) {
            if (uiInfo.characterList[index].headImage == -1) {
                uiInfo.characterList[index].headImage = trap_R_RegisterShaderNoMip(uiInfo.characterList[index].imageName);
            }
            return uiInfo.characterList[index].headImage;
        }
    } else if (feederID == FEEDER_Q3HEADS) {
        if (index >= 0 && index < uiInfo.q3HeadCount) {
            return uiInfo.q3HeadIcons[index];
        }
    } else if (feederID == FEEDER_ALLMAPS || feederID == FEEDER_MAPS) {
        int actual;
        UI_SelectedMap(index, &actual);
        index = actual;
        if (index >= 0 && index < uiInfo.mapCount) {
            if (uiInfo.mapList[index].levelShot == -1) {
                uiInfo.mapList[index].levelShot = trap_R_RegisterShaderNoMip(uiInfo.mapList[index].imageName);
            }
            return uiInfo.mapList[index].levelShot;
        }
    }
    return 0;
}

static void UI_FeederSelection(float feederID, int index) {
    static char info[MAX_STRING_CHARS];
    if (feederID == FEEDER_HEADS) {
        int actual;
        UI_SelectedHead(index, &actual);
        index = actual;
        if (index >= 0 && index < uiInfo.characterCount) {
            trap_Cvar_Set("team_model", uiInfo.characterList[index].base);
            trap_Cvar_Set("team_headmodel", va("*%s", uiInfo.characterList[index].name));
            updateModel = qtrue;
        }
    } else if (feederID == FEEDER_Q3HEADS) {
        if (index >= 0 && index < uiInfo.q3HeadCount) {
            trap_Cvar_Set("model", uiInfo.q3HeadNames[index]);
            trap_Cvar_Set("headmodel", uiInfo.q3HeadNames[index]);
            updateModel = qtrue;
        }
    } else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS) {
        int actual, map;
        map = (feederID == FEEDER_ALLMAPS) ? ui_currentNetMap.integer : ui_currentMap.integer;
        if (uiInfo.mapList[map].cinematic >= 0) {
            trap_CIN_StopCinematic(uiInfo.mapList[map].cinematic);
            uiInfo.mapList[map].cinematic = -1;
        }
        UI_SelectedMap(index, &actual);
        trap_Cvar_Set("ui_mapIndex", va("%d", index));
        ui_mapIndex.integer = index;

        if (feederID == FEEDER_MAPS) {
            ui_currentMap.integer = actual;
            trap_Cvar_Set("ui_currentMap", va("%d", actual));
            uiInfo.mapList[ui_currentMap.integer].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[ui_currentMap.integer].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
            trap_Cvar_Set("ui_opponentModel", uiInfo.mapList[ui_currentMap.integer].opponentName);
            updateOpponentModel = qtrue;
        } else {
            ui_currentNetMap.integer = actual;
            trap_Cvar_Set("ui_currentNetMap", va("%d", actual));
            uiInfo.mapList[ui_currentNetMap.integer].cinematic = trap_CIN_PlayCinematic(va("%s.roq", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName), 0, 0, 0, 0, (CIN_loop | CIN_silent));
        }
    } else if (feederID == FEEDER_SERVERSTATUS) {
        //
    } else if (feederID == FEEDER_FINDPLAYER) {
        uiInfo.currentFoundPlayerServer = index;
        //
        if (index < uiInfo.numFoundPlayerServers - 1) {
            // build a new server status for this server
            Q_strncpyz(uiInfo.serverStatusAddress, uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer], sizeof(uiInfo.serverStatusAddress));
            Menu_SetFeederSelection(NULL, FEEDER_SERVERSTATUS, 0, NULL);
        }
    } else if (feederID == FEEDER_PLAYER_LIST) {
        uiInfo.playerIndex = index;
    } else if (feederID == FEEDER_TEAM_LIST) {
        uiInfo.teamIndex = index;
    } else if (feederID == FEEDER_MODS) {
        uiInfo.modIndex = index;
    } else if (feederID == FEEDER_CINEMATICS) {
        uiInfo.movieIndex = index;
        if (uiInfo.previewMovie >= 0) {
            trap_CIN_StopCinematic(uiInfo.previewMovie);
        }
        uiInfo.previewMovie = -1;
    } else if (feederID == FEEDER_DEMOS) {
        uiInfo.demoIndex = index;
    }
}

static qboolean Team_Parse(char** p) {
    char* token;
    const char* tempStr;
    int i;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        if (token[0] == '{') {
            if (uiInfo.teamCount == MAX_TEAMS) {
                uiInfo.teamCount--;
                Com_Printf("Too many teams, last team replaced!\n");
            }

            // seven tokens per line, team name and icon, and 5 team member names
            if (!String_Parse(p, &uiInfo.teamList[uiInfo.teamCount].teamName) || !String_Parse(p, &tempStr)) {
                return qfalse;
            }

            uiInfo.teamList[uiInfo.teamCount].imageName = tempStr;
            uiInfo.teamList[uiInfo.teamCount].teamIcon = trap_R_RegisterShaderNoMip(uiInfo.teamList[uiInfo.teamCount].imageName);
            uiInfo.teamList[uiInfo.teamCount].teamIcon_Metal = trap_R_RegisterShaderNoMip(va("%s_metal", uiInfo.teamList[uiInfo.teamCount].imageName));
            uiInfo.teamList[uiInfo.teamCount].teamIcon_Name = trap_R_RegisterShaderNoMip(va("%s_name", uiInfo.teamList[uiInfo.teamCount].imageName));

            uiInfo.teamList[uiInfo.teamCount].cinematic = -1;

            for (i = 0; i < TEAM_MEMBERS; i++) {
                uiInfo.teamList[uiInfo.teamCount].teamMembers[i] = NULL;
                if (!String_Parse(p, &uiInfo.teamList[uiInfo.teamCount].teamMembers[i])) {
                    return qfalse;
                }
            }

            Com_Printf("Loaded team %s with team icon %s.\n", uiInfo.teamList[uiInfo.teamCount].teamName, tempStr);
            uiInfo.teamCount++;

            token = COM_ParseExt(p, qtrue);
            if (token[0] != '}') {
                return qfalse;
            }
        }
    }

    return qfalse;
}

static qboolean Character_Parse(char** p) {
    char* token;
    const char* tempStr;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        if (token[0] == '{') {
            if (uiInfo.characterCount == MAX_HEADS) {
                uiInfo.characterCount--;
                Com_Printf("Too many characters, last character replaced!\n");
            }

            // two tokens per line, character name and sex
            if (!String_Parse(p, &uiInfo.characterList[uiInfo.characterCount].name) || !String_Parse(p, &tempStr)) {
                return qfalse;
            }

            uiInfo.characterList[uiInfo.characterCount].headImage = -1;
            uiInfo.characterList[uiInfo.characterCount].imageName = String_Alloc(va("models/players/heads/%s/icon_default.tga", uiInfo.characterList[uiInfo.characterCount].name));

            if (tempStr && (!Q_stricmp(tempStr, "female"))) {
                uiInfo.characterList[uiInfo.characterCount].base = String_Alloc("Janet");
            } else if (tempStr && (!Q_stricmp(tempStr, "male"))) {
                uiInfo.characterList[uiInfo.characterCount].base = String_Alloc("James");
            } else {
                uiInfo.characterList[uiInfo.characterCount].base = String_Alloc(tempStr);
            }

            Com_Printf("Loaded %s character %s.\n", uiInfo.characterList[uiInfo.characterCount].base, uiInfo.characterList[uiInfo.characterCount].name);
            uiInfo.characterCount++;

            token = COM_ParseExt(p, qtrue);
            if (token[0] != '}') {
                return qfalse;
            }
        }
    }

    return qfalse;
}

static qboolean Alias_Parse(char** p) {
    char* token;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        if (token[0] == '{') {
            if (uiInfo.aliasCount == MAX_ALIASES) {
                uiInfo.aliasCount--;
                Com_Printf("Too many aliases, last alias replaced!\n");
            }

            // three tokens per line, character name, bot alias, and preferred action a - all purpose, d - defense, o - offense
            if (!String_Parse(p, &uiInfo.aliasList[uiInfo.aliasCount].name) || !String_Parse(p, &uiInfo.aliasList[uiInfo.aliasCount].ai) || !String_Parse(p, &uiInfo.aliasList[uiInfo.aliasCount].action)) {
                return qfalse;
            }

            Com_Printf("Loaded character alias %s using character ai %s.\n", uiInfo.aliasList[uiInfo.aliasCount].name, uiInfo.aliasList[uiInfo.aliasCount].ai);
            uiInfo.aliasCount++;

            token = COM_ParseExt(p, qtrue);
            if (token[0] != '}') {
                return qfalse;
            }
        }
    }

    return qfalse;
}

// mode
// 0 - high level parsing
// 1 - team parsing
// 2 - character parsing
static void UI_ParseTeamInfo(const char* teamFile) {
    char* token;
    char* p;
    char* buff = NULL;
    // static int mode = 0; TTimo: unused

    buff = GetMenuBuffer(teamFile);
    if (!buff) {
        return;
    }

    p = buff;

    while (1) {
        token = COM_ParseExt(&p, qtrue);
        if (!token[0] || token[0] == '}') {
            break;
        }

        if (Q_stricmp(token, "}") == 0) {
            break;
        }

        if (Q_stricmp(token, "teams") == 0) {
            if (Team_Parse(&p)) {
                continue;
            } else {
                break;
            }
        }

        if (Q_stricmp(token, "characters") == 0) {
            Character_Parse(&p);
        }

        if (Q_stricmp(token, "aliases") == 0) {
            Alias_Parse(&p);
        }
    }
}

static qboolean GameType_Parse(char** p, qboolean join) {
    char* token;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    if (join) {
        uiInfo.numJoinGameTypes = 0;
    } else {
        uiInfo.numGameTypes = 0;
    }

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        if (token[0] == '{') {
            // two tokens per line, gametype name and number
            if (join) {
                if (!String_Parse(p, &uiInfo.joinGameTypes[uiInfo.numJoinGameTypes].gameType) || !Int_Parse(p, &uiInfo.joinGameTypes[uiInfo.numJoinGameTypes].gtEnum)) {
                    return qfalse;
                }
            } else {
                if (!String_Parse(p, &uiInfo.gameTypes[uiInfo.numGameTypes].gameType) || !Int_Parse(p, &uiInfo.gameTypes[uiInfo.numGameTypes].gtEnum)) {
                    return qfalse;
                }
            }

            if (join) {
                if (uiInfo.numJoinGameTypes < MAX_GAMETYPES) {
                    uiInfo.numJoinGameTypes++;
                } else {
                    Com_Printf("Too many net game types, last one replace!\n");
                }
            } else {
                if (uiInfo.numGameTypes < MAX_GAMETYPES) {
                    uiInfo.numGameTypes++;
                } else {
                    Com_Printf("Too many game types, last one replace!\n");
                }
            }

            token = COM_ParseExt(p, qtrue);
            if (token[0] != '}') {
                return qfalse;
            }
        }
    }
    return qfalse;
}

static qboolean MapList_Parse(char** p) {
    char* token;

    token = COM_ParseExt(p, qtrue);

    if (token[0] != '{') {
        return qfalse;
    }

    uiInfo.mapCount = 0;

    while (1) {
        token = COM_ParseExt(p, qtrue);

        if (Q_stricmp(token, "}") == 0) {
            return qtrue;
        }

        if (!token[0]) {
            return qfalse;
        }

        if (token[0] == '{') {
            if (!String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].mapName) || !String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].mapLoadName) || !Int_Parse(p, &uiInfo.mapList[uiInfo.mapCount].teamMembers)) {
                return qfalse;
            }

            if (!String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].opponentName)) {
                return qfalse;
            }

            uiInfo.mapList[uiInfo.mapCount].typeBits = 0;

            while (1) {
                token = COM_ParseExt(p, qtrue);
                if (token[0] >= '0' && token[0] <= '9') {
                    uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << (token[0] - 0x030));
                    if (!Int_Parse(p, &uiInfo.mapList[uiInfo.mapCount].timeToBeat[token[0] - 0x30])) {
                        return qfalse;
                    }
                } else {
                    break;
                }
            }

            uiInfo.mapList[uiInfo.mapCount].cinematic = -1;
            uiInfo.mapList[uiInfo.mapCount].levelShot = trap_R_RegisterShaderNoMip(va("levelshots/preview/%s", uiInfo.mapList[uiInfo.mapCount].mapLoadName));

            if (uiInfo.mapCount < MAX_MAPS) {
                uiInfo.mapCount++;
            } else {
                Com_Printf("Too many maps, last one replaced!\n");
            }
        }
    }
    return qfalse;
}

static void UI_ParseGameInfo(const char* teamFile) {
    char* token;
    char* p;
    char* buff = NULL;
    // int mode = 0; TTimo: unused

    buff = GetMenuBuffer(teamFile);
    if (!buff) {
        return;
    }

    p = buff;

    while (1) {
        token = COM_ParseExt(&p, qtrue);
        if (!token[0] || token[0] == '}') {
            break;
        }

        if (Q_stricmp(token, "}") == 0) {
            break;
        }

        if (Q_stricmp(token, "gametypes") == 0) {
            if (GameType_Parse(&p, qfalse)) {
                continue;
            } else {
                break;
            }
        }

        if (Q_stricmp(token, "joingametypes") == 0) {
            if (GameType_Parse(&p, qtrue)) {
                continue;
            } else {
                break;
            }
        }

        if (Q_stricmp(token, "maps") == 0) {
            // start a new menu
            MapList_Parse(&p);
        }
    }
}

static void UI_Pause(qboolean b) {
    if (b) {
        // pause the game and set the ui keycatcher
        trap_Cvar_Set("cl_paused", "1");
        trap_Key_SetCatcher(KEYCATCH_UI);
    } else {
        // unpause the game and clear the ui keycatcher
        trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
        trap_Key_ClearStates();
        trap_Cvar_Set("cl_paused", "0");
    }
}

static int UI_PlayCinematic(const char* name, float x, float y, float w, float h) {
    return trap_CIN_PlayCinematic(name, x, y, w, h, (CIN_loop | CIN_silent));
}

static void UI_StopCinematic(int handle) {
    if (handle >= 0) {
        trap_CIN_StopCinematic(handle);
    } else {
        handle = abs(handle);
        if (handle == UI_MAPCINEMATIC) {
            if (uiInfo.mapList[ui_currentMap.integer].cinematic >= 0) {
                trap_CIN_StopCinematic(uiInfo.mapList[ui_currentMap.integer].cinematic);
                uiInfo.mapList[ui_currentMap.integer].cinematic = -1;
            }
        } else if (handle == UI_NETMAPCINEMATIC) {
            if (uiInfo.serverStatus.currentServerCinematic >= 0) {
                trap_CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
                uiInfo.serverStatus.currentServerCinematic = -1;
            }
        } else if (handle == UI_CLANCINEMATIC) {
            int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
            if (i >= 0 && i < uiInfo.teamCount) {
                if (uiInfo.teamList[i].cinematic >= 0) {
                    trap_CIN_StopCinematic(uiInfo.teamList[i].cinematic);
                    uiInfo.teamList[i].cinematic = -1;
                }
            }
        }
    }
}

static void UI_DrawCinematic(int handle, float x, float y, float w, float h) {
    // adjust coords to get correct placement in wide screen
    UI_AdjustFrom640(&x, &y, &w, &h);

    // CIN_SetExtents takes stretched 640x480 virtualized coords
    x *= SCREEN_WIDTH / (float)uiInfo.uiDC.glconfig.vidWidth;
    w *= SCREEN_WIDTH / (float)uiInfo.uiDC.glconfig.vidWidth;
    y *= SCREEN_HEIGHT / (float)uiInfo.uiDC.glconfig.vidHeight;
    h *= SCREEN_HEIGHT / (float)uiInfo.uiDC.glconfig.vidHeight;

    trap_CIN_SetExtents(handle, x, y, w, h);
    trap_CIN_DrawCinematic(handle);
}

static void UI_RunCinematicFrame(int handle) {
    trap_CIN_RunCinematic(handle);
}

/*
=================
PlayerModel_BuildList
=================
*/
static void UI_BuildQ3Model_List(void) {
    int numdirs;
    int numfiles;
    char dirlist[2048];
    char filelist[2048];
    char skinname[MAX_QPATH];
    char scratch[256];
    char* dirptr;
    char* fileptr;
    int i;
    int j, k, dirty;
    int dirlen;
    int filelen;

    uiInfo.q3HeadCount = 0;

    // iterate directory of all player models
    numdirs = trap_FS_GetFileList("models/players", "/", dirlist, 2048);
    dirptr = dirlist;
    for (i = 0; i < numdirs && uiInfo.q3HeadCount < MAX_PLAYERMODELS; i++, dirptr += dirlen + 1) {
        dirlen = strlen(dirptr);

        if (dirlen && dirptr[dirlen - 1] == '/')
            dirptr[dirlen - 1] = '\0';

        if (!strcmp(dirptr, ".") || !strcmp(dirptr, ".."))
            continue;

        // iterate all skin files in directory
        numfiles = trap_FS_GetFileList(va("models/players/%s", dirptr), "tga", filelist, 2048);
        fileptr = filelist;
        for (j = 0; j < numfiles && uiInfo.q3HeadCount < MAX_PLAYERMODELS; j++, fileptr += filelen + 1) {
            filelen = strlen(fileptr);

            COM_StripExtension(fileptr, skinname, sizeof(skinname));

            // look for icon_????
            if (Q_stricmpn(skinname, "icon_", 5) == 0 && !(Q_stricmp(skinname, "icon_blue") == 0 || Q_stricmp(skinname, "icon_red") == 0)) {
                if (Q_stricmp(skinname, "icon_default") == 0) {
                    Com_sprintf(scratch, sizeof(scratch), "%s", dirptr);
                } else {
                    Com_sprintf(scratch, sizeof(scratch), "%s/%s", dirptr, skinname + 5);
                }
                dirty = 0;
                for (k = 0; k < uiInfo.q3HeadCount; k++) {
                    if (!Q_stricmp(scratch, uiInfo.q3HeadNames[uiInfo.q3HeadCount])) {
                        dirty = 1;
                        break;
                    }
                }
                if (!dirty) {
                    Com_sprintf(uiInfo.q3HeadNames[uiInfo.q3HeadCount], sizeof(uiInfo.q3HeadNames[uiInfo.q3HeadCount]), "%s", scratch);
                    uiInfo.q3HeadIcons[uiInfo.q3HeadCount++] = trap_R_RegisterShaderNoMip(va("models/players/%s/%s", dirptr, skinname));
                }
            }
        }
    }
}

/*
=================
UI_Init
=================
*/
void _UI_Init(qboolean inGameLoad) {
    const char* menuSet;

    UI_RegisterCvars();
    UI_InitMemory();

    // cache redundant calulations
    trap_GetGlconfig(&uiInfo.uiDC.glconfig);

    trap_Cvar_Set("ui_videomode", va("%dx%d", uiInfo.uiDC.glconfig.vidWidth, uiInfo.uiDC.glconfig.vidHeight));

    // for 640x480 virtualized screen
    uiInfo.uiDC.yscale = uiInfo.uiDC.glconfig.vidHeight * (1.0 / (float)SCREEN_HEIGHT);
    uiInfo.uiDC.xscale = uiInfo.uiDC.glconfig.vidWidth * (1.0 / (float)SCREEN_WIDTH);
    if (uiInfo.uiDC.glconfig.vidWidth * SCREEN_HEIGHT > uiInfo.uiDC.glconfig.vidHeight * SCREEN_WIDTH) {
        // wide screen
        uiInfo.uiDC.bias = 0.5 * (uiInfo.uiDC.glconfig.vidWidth - (uiInfo.uiDC.glconfig.vidHeight * ((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT)));
        uiInfo.uiDC.xscale = uiInfo.uiDC.yscale;
    } else {
        // no wide screen
        uiInfo.uiDC.bias = 0;
    }

    uiInfo.uiDC.registerShaderNoMip = &trap_R_RegisterShaderNoMip;
    uiInfo.uiDC.setColor = &UI_SetColor;
    uiInfo.uiDC.drawHandlePic = &UI_DrawHandlePic;
    uiInfo.uiDC.drawStretchPic = &trap_R_DrawStretchPic;
    uiInfo.uiDC.drawText = &Text_Paint;
    uiInfo.uiDC.textWidth = &Text_Width;
    uiInfo.uiDC.textHeight = &Text_Height;
    uiInfo.uiDC.registerModel = &trap_R_RegisterModel;
    uiInfo.uiDC.modelBounds = &trap_R_ModelBounds;
    uiInfo.uiDC.fillRect = &UI_FillRect;
    uiInfo.uiDC.drawRect = &_UI_DrawRect;
    uiInfo.uiDC.drawSides = &_UI_DrawSides;
    uiInfo.uiDC.drawTopBottom = &_UI_DrawTopBottom;
    uiInfo.uiDC.clearScene = &trap_R_ClearScene;
    uiInfo.uiDC.addRefEntityToScene = &trap_R_AddRefEntityToScene;
    uiInfo.uiDC.renderScene = &trap_R_RenderScene;
    uiInfo.uiDC.registerFont = &trap_R_RegisterFont;
    uiInfo.uiDC.ownerDrawItem = &UI_OwnerDraw;
    uiInfo.uiDC.getValue = &UI_GetValue;
    uiInfo.uiDC.ownerDrawVisible = &UI_OwnerDrawVisible;
    uiInfo.uiDC.runScript = &UI_RunMenuScript;
    uiInfo.uiDC.getTeamColor = &UI_GetTeamColor;
    uiInfo.uiDC.setCVar = trap_Cvar_Set;
    uiInfo.uiDC.getCVarString = trap_Cvar_VariableStringBuffer;
    uiInfo.uiDC.getCVarValue = trap_Cvar_VariableValue;
    uiInfo.uiDC.drawTextWithCursor = &Text_PaintWithCursor;
    uiInfo.uiDC.setOverstrikeMode = &trap_Key_SetOverstrikeMode;
    uiInfo.uiDC.getOverstrikeMode = &trap_Key_GetOverstrikeMode;
    uiInfo.uiDC.startLocalSound = &trap_S_StartLocalSound;
    uiInfo.uiDC.ownerDrawHandleKey = &UI_OwnerDrawHandleKey;
    uiInfo.uiDC.feederCount = &UI_FeederCount;
    uiInfo.uiDC.feederItemImage = &UI_FeederItemImage;
    uiInfo.uiDC.feederItemText = &UI_FeederItemText;
    uiInfo.uiDC.feederSelection = &UI_FeederSelection;
    uiInfo.uiDC.setBinding = &trap_Key_SetBinding;
    uiInfo.uiDC.getBindingBuf = &trap_Key_GetBindingBuf;
    uiInfo.uiDC.keynumToStringBuf = &trap_Key_KeynumToStringBuf;
    uiInfo.uiDC.executeText = &trap_Cmd_ExecuteText;
    uiInfo.uiDC.Error = &Com_Error;
    uiInfo.uiDC.Print = &Com_Printf;
    uiInfo.uiDC.Pause = &UI_Pause;
    uiInfo.uiDC.ownerDrawWidth = &UI_OwnerDrawWidth;
    uiInfo.uiDC.registerSound = &trap_S_RegisterSound;
    uiInfo.uiDC.startBackgroundTrack = &trap_S_StartBackgroundTrack;
    uiInfo.uiDC.stopBackgroundTrack = &trap_S_StopBackgroundTrack;
    uiInfo.uiDC.playCinematic = &UI_PlayCinematic;
    uiInfo.uiDC.stopCinematic = &UI_StopCinematic;
    uiInfo.uiDC.drawCinematic = &UI_DrawCinematic;
    uiInfo.uiDC.runCinematicFrame = &UI_RunCinematicFrame;

    Init_Display(&uiInfo.uiDC);

    String_Init();

    uiInfo.uiDC.cursor = trap_R_RegisterShaderNoMip("menu/art/3_cursor2");
    uiInfo.uiDC.whiteShader = trap_R_RegisterShaderNoMip("white");

    AssetCache();

    uiInfo.teamCount = 0;
    uiInfo.characterCount = 0;
    uiInfo.aliasCount = 0;

    UI_ParseTeamInfo("ui/teaminfo.txt");
    UI_LoadTeams();
    UI_LoadArenas();

    menuSet = UI_Cvar_VariableString("ui_menuFiles");
    if (menuSet == NULL || menuSet[0] == '\0') {
        menuSet = "ui/menus.txt";
    }

    UI_LoadMenus(menuSet, qtrue);
    UI_LoadMenus("ui/ingame.txt", qfalse);

    Menus_CloseAll();

    UI_BuildQ3Model_List();
    UI_LoadBots();

    // sets defaults for ui temp cvars
    uiInfo.effectsColor = gamecodetoui[(int)trap_Cvar_VariableValue("color1") - 1];
    uiInfo.currentCrosshair = (int)trap_Cvar_VariableValue("cg_drawCrosshair") % NUM_CROSSHAIRS;
    if (uiInfo.currentCrosshair < 0) {
        uiInfo.currentCrosshair = 0;
    }
    trap_Cvar_Set("ui_mousePitch", (trap_Cvar_VariableValue("m_pitch") >= 0) ? "0" : "1");

    uiInfo.serverStatus.currentServerCinematic = -1;
    uiInfo.previewMovie = -1;

    trap_Cvar_Register(NULL, "debug_protocol", "", 0);

    trap_Cvar_Set("ui_actualNetGameType", va("%d", ui_netGameType.integer));
}

/*
=================
UI_KeyEvent
=================
*/
void _UI_KeyEvent(int key, qboolean down) {
    if (Menu_Count() > 0) {
        menuDef_t* menu = Menu_GetFocused();
        if (menu) {
            if (key == K_ESCAPE && down && !Menus_AnyFullScreenVisible()) {
                Menus_CloseAll();
            } else {
                Menu_HandleKey(menu, key, down);
            }
        } else {
            trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
            trap_Key_ClearStates();
            trap_Cvar_Set("cl_paused", "0");
        }
    }
}

/*
=================
UI_MouseEvent
=================
*/
void _UI_MouseEvent(int dx, int dy) {
    int bias;

    // convert X bias to 640 coords
    bias = uiInfo.uiDC.bias / uiInfo.uiDC.xscale;

    // update mouse screen position
    uiInfo.uiDC.cursorx += dx;
    if (uiInfo.uiDC.cursorx < -bias)
        uiInfo.uiDC.cursorx = -bias;
    else if (uiInfo.uiDC.cursorx > SCREEN_WIDTH + bias)
        uiInfo.uiDC.cursorx = SCREEN_WIDTH + bias;

    uiInfo.uiDC.cursory += dy;
    if (uiInfo.uiDC.cursory < 0)
        uiInfo.uiDC.cursory = 0;
    else if (uiInfo.uiDC.cursory > SCREEN_HEIGHT)
        uiInfo.uiDC.cursory = SCREEN_HEIGHT;

    if (Menu_Count() > 0) {
        Display_MouseMove(NULL, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
    }
}

void UI_LoadNonIngame(void) {
    const char* menuSet = UI_Cvar_VariableString("ui_menuFiles");
    if (menuSet == NULL || menuSet[0] == '\0') {
        menuSet = "ui/menus.txt";
    }
    UI_LoadMenus(menuSet, qfalse);
    uiInfo.inGameLoad = qfalse;
}

void _UI_SetActiveMenu(uiMenuCommand_t menu) {
    char buf[256];

    // this should be the ONLY way the menu system is brought up
    // ensure minimum menu data is cached
    if (Menu_Count() > 0) {
        vec3_t v;
        v[0] = v[1] = v[2] = 0;
        switch (menu) {
            case UIMENU_NONE:
                trap_Key_SetCatcher(trap_Key_GetCatcher() & ~KEYCATCH_UI);
                trap_Key_ClearStates();
                trap_Cvar_Set("cl_paused", "0");
                Menus_CloseAll();

                return;
            case UIMENU_MAIN:
                trap_Cvar_Set("sv_killserver", "1");
                trap_Key_SetCatcher(KEYCATCH_UI);
                if (uiInfo.inGameLoad) {
                    UI_LoadNonIngame();
                }
                Menus_CloseAll();
                Menus_ActivateByName("main");
                trap_Cvar_VariableStringBuffer("com_errorMessage", buf, sizeof(buf));
                if (strlen(buf)) {
                    if (!ui_singlePlayerActive.integer) {
                        Menus_ActivateByName("error_popmenu");
                    } else {
                        trap_Cvar_Set("com_errorMessage", "");
                    }
                }
                return;
            case UIMENU_TEAM:
                trap_Key_SetCatcher(KEYCATCH_UI);
                Menus_ActivateByName("team");
                return;
            case UIMENU_POSTGAME:
                trap_Cvar_Set("sv_killserver", "1");
                trap_Key_SetCatcher(KEYCATCH_UI);
                if (uiInfo.inGameLoad) {
                    UI_LoadNonIngame();
                }
                Menus_CloseAll();
                Menus_ActivateByName("endofgame");
                return;
            case UIMENU_INGAME:
                trap_Cvar_Set("cl_paused", "1");
                trap_Key_SetCatcher(KEYCATCH_UI);
                UI_BuildPlayerList();
                Menus_CloseAll();
                Menus_ActivateByName("ingame");
                return;
        }
    }
}

qboolean _UI_IsFullscreen(void) {
    return Menus_AnyFullScreenVisible();
}

static connstate_t lastConnState;
static char lastLoadingText[MAX_INFO_VALUE];

static void UI_ReadableSize(char* buf, int bufsize, int value) {
    if (value > 1024 * 1024 * 1024) {  // gigs
        Com_sprintf(buf, bufsize, "%d", value / (1024 * 1024 * 1024));
        Com_sprintf(buf + strlen(buf), bufsize - strlen(buf), ".%02d GB",
                    (value % (1024 * 1024 * 1024)) * 100 / (1024 * 1024 * 1024));
    } else if (value > 1024 * 1024) {  // megs
        Com_sprintf(buf, bufsize, "%d", value / (1024 * 1024));
        Com_sprintf(buf + strlen(buf), bufsize - strlen(buf), ".%02d MB",
                    (value % (1024 * 1024)) * 100 / (1024 * 1024));
    } else if (value > 1024) {  // kilos
        Com_sprintf(buf, bufsize, "%d KB", value / 1024);
    } else {  // bytes
        Com_sprintf(buf, bufsize, "%d bytes", value);
    }
}

// Assumes time is in msec
static void UI_PrintTime(char* buf, int bufsize, int time) {
    time /= 1000;  // change to seconds

    if (time > 3600) {  // in the hours range
        Com_sprintf(buf, bufsize, "%d hr %d min", time / 3600, (time % 3600) / 60);
    } else if (time > 60) {  // mins
        Com_sprintf(buf, bufsize, "%d min %d sec", time / 60, time % 60);
    } else {  // secs
        Com_sprintf(buf, bufsize, "%d sec", time);
    }
}

void Text_PaintCenter(float x, float y, float scale, vec4_t color, const char* text, float adjust) {
    int len = Text_Width(text, scale, 0);
    Text_Paint(x - len / 2, y, scale, color, text, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
}

void Text_PaintCenter_AutoWrapped(float x, float y, float xmax, float ystep, float scale, vec4_t color, const char* str, float adjust) {
    int width;
    char *s1, *s2, *s3;
    char c_bcp;
    char buf[1024];

    if (!str || str[0] == '\0')
        return;

    Q_strncpyz(buf, str, sizeof(buf));
    s1 = s2 = s3 = buf;

    while (1) {
        do {
            s3++;
        } while (*s3 != ' ' && *s3 != '\0');
        c_bcp = *s3;
        *s3 = '\0';
        width = Text_Width(s1, scale, 0);
        *s3 = c_bcp;
        if (width > xmax) {
            if (s1 == s2) {
                // fuck, don't have a clean cut, we'll overflow
                s2 = s3;
            }
            *s2 = '\0';
            Text_PaintCenter(x, y, scale, color, s1, adjust);
            y += ystep;
            if (c_bcp == '\0') {
                // that was the last word
                // we could start a new loop, but that wouldn't be much use
                // even if the word is too long, we would overflow it (see above)
                // so just print it now if needed
                s2++;
                if (*s2 != '\0')  // if we are printing an overflowing line we have s2 == s3
                    Text_PaintCenter(x, y, scale, color, s2, adjust);
                break;
            }
            s2++;
            s1 = s2;
            s3 = s2;
        } else {
            s2 = s3;
            if (c_bcp == '\0')  // we reached the end
            {
                Text_PaintCenter(x, y, scale, color, s1, adjust);
                break;
            }
        }
    }
}

static void UI_DisplayDownloadInfo(const char* downloadName, float centerPoint, float yStart, float scale) {
    static char dlText[] = "Downloading:";
    static char etaText[] = "Estimated time left:";
    static char xferText[] = "Transfer rate:";

    int downloadSize, downloadCount, downloadTime;
    char dlSizeBuf[64], totalSizeBuf[64], xferRateBuf[64], dlTimeBuf[64];
    int xferRate;
    int leftWidth;
    const char* s;

    downloadSize = trap_Cvar_VariableValue("cl_downloadSize");
    downloadCount = trap_Cvar_VariableValue("cl_downloadCount");
    downloadTime = trap_Cvar_VariableValue("cl_downloadTime");

    leftWidth = 320;

    UI_SetColor(colorWhite);
    Text_PaintCenter(centerPoint, yStart + 112, scale, colorWhite, dlText, 0);
    Text_PaintCenter(centerPoint, yStart + 192, scale, colorWhite, etaText, 0);
    Text_PaintCenter(centerPoint, yStart + 248, scale, colorWhite, xferText, 0);

    if (downloadSize > 0) {
        s = va("%s (%d%%)", downloadName,
               (int)((float)downloadCount * 100.0f / downloadSize));
    } else {
        s = downloadName;
    }

    Text_PaintCenter(centerPoint, yStart + 136, scale, colorWhite, s, 0);

    UI_ReadableSize(dlSizeBuf, sizeof dlSizeBuf, downloadCount);
    UI_ReadableSize(totalSizeBuf, sizeof totalSizeBuf, downloadSize);

    if (downloadCount < 4096 || !downloadTime) {
        Text_PaintCenter(leftWidth, yStart + 216, scale, colorWhite, "estimating", 0);
        Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, va("(%s of %s copied)", dlSizeBuf, totalSizeBuf), 0);
    } else {
        if ((uiInfo.uiDC.realTime - downloadTime) / 1000) {
            xferRate = downloadCount / ((uiInfo.uiDC.realTime - downloadTime) / 1000);
        } else {
            xferRate = 0;
        }
        UI_ReadableSize(xferRateBuf, sizeof xferRateBuf, xferRate);

        // Extrapolate estimated completion time
        if (downloadSize && xferRate) {
            int n = downloadSize / xferRate;  // estimated time for entire d/l in secs

            // We do it in K (/1024) because we'd overflow around 4MB
            UI_PrintTime(dlTimeBuf, sizeof dlTimeBuf,
                         (n - (((downloadCount / 1024) * n) / (downloadSize / 1024))) * 1000);

            Text_PaintCenter(leftWidth, yStart + 216, scale, colorWhite, dlTimeBuf, 0);
            Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, va("(%s of %s copied)", dlSizeBuf, totalSizeBuf), 0);
        } else {
            Text_PaintCenter(leftWidth, yStart + 216, scale, colorWhite, "estimating", 0);
            if (downloadSize) {
                Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, va("(%s of %s copied)", dlSizeBuf, totalSizeBuf), 0);
            } else {
                Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, va("(%s copied)", dlSizeBuf), 0);
            }
        }

        if (xferRate) {
            Text_PaintCenter(leftWidth, yStart + 272, scale, colorWhite, va("%s/Sec", xferRateBuf), 0);
        }
    }
}

/*
========================
UI_DrawConnectScreen

This will also be overlaid on the cgame info screen during loading
to prevent it from blinking away too rapidly on local or lan games.
========================
*/
void UI_DrawConnectScreen(qboolean overlay) {
    char* s;
    uiClientState_t cstate;
    char info[MAX_INFO_VALUE];
    char text[256];
    float centerPoint, yStart, scale;

    menuDef_t* menu = Menus_FindByName("Connect");

    if (!overlay && menu) {
        Menu_Paint(menu, qtrue);
    }

    if (!overlay) {
        centerPoint = 320;
        yStart = 130;
        scale = 0.5f;
    } else {
        return;
    }

    // see what information we should display
    trap_GetClientState(&cstate);

    info[0] = '\0';
    if (trap_GetConfigString(CS_SERVERINFO, info, sizeof(info))) {
        Text_PaintCenter(centerPoint, yStart, scale, colorWhite, va("Loading %s", Info_ValueForKey(info, "mapname")), 0);
    }

    if (!Q_stricmp(cstate.servername, "localhost")) {
        Text_PaintCenter(centerPoint, yStart + 48, scale, colorWhite, "Starting up...", ITEM_TEXTSTYLE_SHADOWEDMORE);
    } else {
        Com_sprintf(text, sizeof(text), "Connecting to %s", cstate.servername);
        Text_PaintCenter(centerPoint, yStart + 48, scale, colorWhite, text, ITEM_TEXTSTYLE_SHADOWEDMORE);
    }

    // display global MOTD at bottom
    Text_PaintCenter(centerPoint, 600, scale, colorWhite, Info_ValueForKey(cstate.updateInfoString, "motd"), 0);
    // print any server info (server full, bad version, etc)
    if (cstate.connState < CA_CONNECTED) {
        Text_PaintCenter_AutoWrapped(centerPoint, yStart + 176, 630, 20, scale, colorWhite, cstate.messageString, 0);
    }

    if (lastConnState > cstate.connState) {
        lastLoadingText[0] = '\0';
    }
    lastConnState = cstate.connState;

    switch (cstate.connState) {
        case CA_CONNECTING:
            s = va("Awaiting connection...%i", cstate.connectPacketCount);
            break;
        case CA_CHALLENGING:
            s = va("Awaiting challenge...%i", cstate.connectPacketCount);
            break;
        case CA_CONNECTED: {
            char downloadName[MAX_INFO_VALUE];

            trap_Cvar_VariableStringBuffer("cl_downloadName", downloadName, sizeof(downloadName));
            if (*downloadName) {
                UI_DisplayDownloadInfo(downloadName, centerPoint, yStart, scale);
                return;
            }
        }
            s = "Awaiting gamestate...";
            break;
        case CA_LOADING:
            s = "Game initialising...";
            break;
        case CA_PRIMED:
            s = "Awaiting initial frame...";
            break;
        default:
            return;
    }

    if (Q_stricmp(cstate.servername, "localhost")) {
        Text_PaintCenter(centerPoint, yStart + 80, scale, colorWhite, s, 0);
    }

    // password required / connection rejected information goes here
}

/*
================
cvars
================
*/

typedef struct {
    vmCvar_t* vmCvar;
    char* cvarName;
    char* defaultString;
    int cvarFlags;
} cvarTable_t;

vmCvar_t ui_ffa_fraglimit;
vmCvar_t ui_ffa_timelimit;

vmCvar_t ui_tourney_fraglimit;
vmCvar_t ui_tourney_timelimit;

vmCvar_t ui_team_fraglimit;
vmCvar_t ui_team_timelimit;
vmCvar_t ui_team_friendly;

vmCvar_t ui_ctf_capturelimit;
vmCvar_t ui_ctf_timelimit;
vmCvar_t ui_ctf_friendly;

vmCvar_t ui_arenasFile;
vmCvar_t ui_botsFile;
vmCvar_t ui_spAwards;
vmCvar_t ui_spVideos;
vmCvar_t ui_spSkill;

vmCvar_t ui_spSelection;

vmCvar_t ui_brassTime;
vmCvar_t ui_drawCrosshair;
vmCvar_t ui_drawCrosshairNames;
vmCvar_t ui_marks;

vmCvar_t ui_redteam;
vmCvar_t ui_blueteam;
vmCvar_t ui_teamName;
vmCvar_t ui_dedicated;
vmCvar_t ui_gameType;
vmCvar_t ui_netGameType;
vmCvar_t ui_actualNetGameType;
vmCvar_t ui_joinGameType;
vmCvar_t ui_netSource;
vmCvar_t ui_serverFilterType;
vmCvar_t ui_opponentName;
vmCvar_t ui_menuFiles;
vmCvar_t ui_currentMap;
vmCvar_t ui_currentNetMap;
vmCvar_t ui_mapIndex;
vmCvar_t ui_currentOpponent;
vmCvar_t ui_selectedPlayer;
vmCvar_t ui_selectedPlayerName;
vmCvar_t ui_singlePlayerActive;
vmCvar_t ui_scoreAccuracy;
vmCvar_t ui_scoreImpressives;
vmCvar_t ui_scoreExcellents;
vmCvar_t ui_scoreCaptures;
vmCvar_t ui_scoreDefends;
vmCvar_t ui_scoreAssists;
vmCvar_t ui_scoreGauntlets;
vmCvar_t ui_scoreScore;
vmCvar_t ui_scorePerfect;
vmCvar_t ui_scoreTeam;
vmCvar_t ui_scoreBase;
vmCvar_t ui_scoreTimeBonus;
vmCvar_t ui_scoreSkillBonus;
vmCvar_t ui_scoreShutoutBonus;
vmCvar_t ui_scoreTime;
vmCvar_t ui_captureLimit;
vmCvar_t ui_fragLimit;
vmCvar_t ui_smallFont;
vmCvar_t ui_bigFont;
vmCvar_t ui_findPlayer;
vmCvar_t ui_Q3Model;
vmCvar_t ui_hudFiles;
vmCvar_t ui_recordSPDemo;
vmCvar_t ui_realCaptureLimit;
vmCvar_t ui_realWarmUp;
vmCvar_t ui_serverStatusTimeOut;

static cvarTable_t cvarTable[] = {
    {&ui_ffa_fraglimit, "ui_ffa_fraglimit", "20", CVAR_ARCHIVE},
    {&ui_ffa_timelimit, "ui_ffa_timelimit", "0", CVAR_ARCHIVE},

    {&ui_tourney_fraglimit, "ui_tourney_fraglimit", "0", CVAR_ARCHIVE},
    {&ui_tourney_timelimit, "ui_tourney_timelimit", "15", CVAR_ARCHIVE},

    {&ui_team_fraglimit, "ui_team_fraglimit", "0", CVAR_ARCHIVE},
    {&ui_team_timelimit, "ui_team_timelimit", "20", CVAR_ARCHIVE},
    {&ui_team_friendly, "ui_team_friendly", "1", CVAR_ARCHIVE},

    {&ui_ctf_capturelimit, "ui_ctf_capturelimit", "8", CVAR_ARCHIVE},
    {&ui_ctf_timelimit, "ui_ctf_timelimit", "30", CVAR_ARCHIVE},
    {&ui_ctf_friendly, "ui_ctf_friendly", "0", CVAR_ARCHIVE},

    {&ui_arenasFile, "g_arenasFile", "", CVAR_INIT | CVAR_ROM},
    {&ui_botsFile, "g_botsFile", "", CVAR_INIT | CVAR_ROM},

    {&ui_brassTime, "cg_brassTime", "2500", CVAR_ARCHIVE},
    {&ui_drawCrosshair, "cg_drawCrosshair", "4", CVAR_ARCHIVE},
    {&ui_drawCrosshairNames, "cg_drawCrosshairNames", "1", CVAR_ARCHIVE},
    {&ui_marks, "cg_marks", "1", CVAR_ARCHIVE},

    {&ui_debug, "ui_debug", "0", CVAR_TEMP},
    {&ui_initialized, "ui_initialized", "0", CVAR_TEMP},
    {&ui_dedicated, "ui_dedicated", "0", CVAR_ARCHIVE},
    {&ui_gameType, "ui_gametype", "3", CVAR_ARCHIVE},
    {&ui_joinGameType, "ui_joinGametype", "0", CVAR_ARCHIVE},
    {&ui_netGameType, "ui_netGametype", "3", CVAR_ARCHIVE},
    {&ui_actualNetGameType, "ui_actualNetGametype", "3", CVAR_ARCHIVE},
    {&ui_blueteam, "ui_blueTeam", "Blue", CVAR_ARCHIVE},
    {&ui_redteam, "ui_redTeam", "Red", CVAR_ARCHIVE},
    {&ui_netSource, "ui_netSource", "1", CVAR_ARCHIVE},
    {&ui_menuFiles, "ui_menuFiles", "ui/menus.txt", CVAR_ARCHIVE},
    {&ui_currentMap, "ui_currentMap", "0", CVAR_ARCHIVE},
    {&ui_currentNetMap, "ui_currentNetMap", "0", CVAR_ARCHIVE},
    {&ui_mapIndex, "ui_mapIndex", "0", CVAR_ARCHIVE},
    {&ui_currentOpponent, "ui_currentOpponent", "0", CVAR_ARCHIVE},
    {&ui_selectedPlayer, "cg_selectedPlayer", "0", CVAR_ARCHIVE},
    {&ui_selectedPlayerName, "cg_selectedPlayerName", "", CVAR_ARCHIVE},
    {&ui_singlePlayerActive, "ui_singlePlayerActive", "0", 0},
    {&ui_scoreAccuracy, "ui_scoreAccuracy", "0", CVAR_ARCHIVE},
    {&ui_scoreImpressives, "ui_scoreImpressives", "0", CVAR_ARCHIVE},
    {&ui_scoreExcellents, "ui_scoreExcellents", "0", CVAR_ARCHIVE},
    {&ui_scoreCaptures, "ui_scoreCaptures", "0", CVAR_ARCHIVE},
    {&ui_scoreDefends, "ui_scoreDefends", "0", CVAR_ARCHIVE},
    {&ui_scoreAssists, "ui_scoreAssists", "0", CVAR_ARCHIVE},
    {&ui_scoreGauntlets, "ui_scoreGauntlets", "0", CVAR_ARCHIVE},
    {&ui_scoreScore, "ui_scoreScore", "0", CVAR_ARCHIVE},
    {&ui_scorePerfect, "ui_scorePerfect", "0", CVAR_ARCHIVE},
    {&ui_scoreTeam, "ui_scoreTeam", "0 to 0", CVAR_ARCHIVE},
    {&ui_scoreBase, "ui_scoreBase", "0", CVAR_ARCHIVE},
    {&ui_scoreTime, "ui_scoreTime", "00:00", CVAR_ARCHIVE},
    {&ui_scoreTimeBonus, "ui_scoreTimeBonus", "0", CVAR_ARCHIVE},
    {&ui_scoreSkillBonus, "ui_scoreSkillBonus", "0", CVAR_ARCHIVE},
    {&ui_scoreShutoutBonus, "ui_scoreShutoutBonus", "0", CVAR_ARCHIVE},
    {&ui_fragLimit, "ui_fragLimit", "10", 0},
    {&ui_captureLimit, "ui_captureLimit", "5", 0},
    {&ui_smallFont, "ui_smallFont", "0.25", CVAR_ARCHIVE},
    {&ui_bigFont, "ui_bigFont", "0.4", CVAR_ARCHIVE},
    {&ui_findPlayer, "ui_findPlayer", "Sarge", CVAR_ARCHIVE},
    {&ui_Q3Model, "ui_q3model", "0", CVAR_ARCHIVE},
    {&ui_hudFiles, "cg_hudFiles", "ui/hud.txt", CVAR_ARCHIVE},
    {&ui_recordSPDemo, "ui_recordSPDemo", "0", CVAR_ARCHIVE},
    {&ui_realWarmUp, "g_warmup", "20", CVAR_ARCHIVE},
    {&ui_realCaptureLimit, "capturelimit", "8", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART},
    {&ui_serverStatusTimeOut, "ui_serverStatusTimeOut", "7000", CVAR_ARCHIVE},

    {NULL, "ui_videomode", "", CVAR_ROM},
    {NULL, "g_localTeamPref", "", 0},
};

static int cvarTableSize = ARRAY_LEN(cvarTable);

/*
=================
UI_RegisterCvars
=================
*/
void UI_RegisterCvars(void) {
    int i;
    cvarTable_t* cv;

    for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++) {
        trap_Cvar_Register(cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags);
    }
}

/*
=================
UI_UpdateCvars
=================
*/
void UI_UpdateCvars(void) {
    int i;
    cvarTable_t* cv;

    for (i = 0, cv = cvarTable; i < cvarTableSize; i++, cv++) {
        if (!cv->vmCvar) {
            continue;
        }

        trap_Cvar_Update(cv->vmCvar);
    }
}
