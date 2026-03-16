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
// cg_draw.c -- draw all of the graphical elements during
// active (after loading) gameplay

#include "cg_local.h"

#include "../ui/ui_shared.h"

// used for scoreboard
extern displayContextDef_t cgDC;
menuDef_t* menuScoreboard = NULL;
menuDef_t* menuEndScoreboard = NULL;

int sortedTeamPlayers[TEAM_MAXOVERLAY];
int numSortedTeamPlayers;

char systemChat[256];
char teamChat1[256];
char teamChat2[256];

/*
=================
CG_KeyNameForCommand

Resolves a command binding (e.g. "+attack") to its display key name (e.g. "MOUSE1").
If no key is bound, writes the fallback string "???" into buf.
=================
*/
void CG_KeyNameForCommand(const char* command, char* buf, int buflen) {
	int key;

	key = trap_Key_GetKey(command);
	if (key >= 0) {
		trap_Key_KeynumToStringBuf(key, buf, buflen);
	} else {
		Q_strncpyz(buf, "???", buflen);
	}
}

// [QL] Resolve fontIndex to a fontInfo_t pointer.
// fontIndex 0 = textFont (NotoSans), 1 = bigFont (handelgothic), 2 = smallFont (DroidSansMono)
// Falls back to scale-based selection for fontIndex 0.
static fontInfo_t* CG_FontForIndex(int fontIndex) {
	if (fontIndex > 0 && fontIndex < 3) {
		return &cgDC.Assets.extraFonts[fontIndex];
	}
	return &cgDC.Assets.extraFonts[0];
}

// [QL] Font-pointer variants - used by DC wrappers to bypass scale-based selection
float CG_Text_Width_Font(const char* text, float scale, int limit, fontInfo_t* font) {
	int count, len;
	float out;
	glyphInfo_t* glyph;
	float useScale;
	const char* s = text;
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

float CG_Text_Height_Font(const char* text, float scale, int limit, fontInfo_t* font) {
	int len, count;
	float max;
	glyphInfo_t* glyph;
	float useScale;
	const char* s = text;
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

void CG_Text_Paint_Font(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style, fontInfo_t* font) {
	int len, count;
	vec4_t newColor;
	glyphInfo_t* glyph;
	float useScale;
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
				float xadj = useScale * glyph->left;
				if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE) {
					int ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
					colorBlack[3] = newColor[3];
					trap_R_SetColor(colorBlack);
					CG_Text_PaintChar(x + xadj + ofs, y - yadj + ofs,
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
				CG_Text_PaintChar(x + xadj, y - yadj,
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

// [QL] DC wrapper functions - resolve fontIndex then delegate to _Font variants
void CG_DrawText_DC(float x, float y, float scale, vec4_t color, const char* text,
					float adjust, int limit, int style, int fontIndex) {
	fontInfo_t* font = CG_FontForIndex(fontIndex);
	CG_Text_Paint_Font(x, y, scale, color, text, adjust, limit, style, font);
}

float CG_TextWidth_DC(const char* text, float scale, int limit, int fontIndex) {
	fontInfo_t* font = CG_FontForIndex(fontIndex);
	return CG_Text_Width_Font(text, scale, limit, font);
}

float CG_TextHeight_DC(const char* text, float scale, int limit, int fontIndex) {
	fontInfo_t* font = CG_FontForIndex(fontIndex);
	return CG_Text_Height_Font(text, scale, limit, font);
}

void CG_DrawTextWithCursor_DC(float x, float y, float scale, vec4_t color, const char* text,
							  int cursorPos, char cursor, int limit, int style, int fontIndex) {
	fontInfo_t* font = CG_FontForIndex(fontIndex);
	CG_Text_Paint_Font(x, y, scale, color, text, 0, limit, style, font);
}

// [QL] CG_DrawText - matching binary's 0x10008440 in cgamex86.dll.
// Does bulk widescreen adjustment on x,y coords, then renders all glyphs at
// screen-space coordinates. Shadow pass is done as a separate full-string pass
// (not per-character like CG_Text_Paint_Font).
static void CG_DrawText_Glyphs(float x, float y, float useScale,
                                float xscale, float yscale,
                                const char *text, float adjust, int maxChars,
                                fontInfo_t *font) {
	int count = 0, len = strlen(text);
	vec4_t newColor;
	const char *s = text;
	if (maxChars > 0 && len > maxChars) len = maxChars;

	while (s && *s && count < len) {
		if (Q_IsColorString(s)) {
			memcpy(newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
			trap_R_SetColor(newColor);
			s += 2;
			continue;
		}
		glyphInfo_t *glyph = &font->glyphs[*s & 255];
		float w = glyph->imageWidth * useScale * xscale;
		float h = glyph->imageHeight * useScale * yscale;
		float gx = x + (useScale * glyph->left) * xscale;
		float gy = y - (useScale * glyph->top) * yscale;
		trap_R_DrawStretchPic(gx, gy, w, h,
		                      glyph->s, glyph->t, glyph->s2, glyph->t2,
		                      glyph->glyph);
		x += (glyph->xSkip * useScale + adjust) * xscale;
		s++;
		count++;
	}
}

void CG_DrawText(float x, float y, int fontIndex, float scale, vec4_t color,
                 const char *text, float adjust, int maxChars, int textStyle) {
	float xscale, yscale, xbias;
	int ws;
	fontInfo_t *font;
	float useScale;

	if (!text || !*text) return;
	if (maxChars == 0) maxChars = -1;

	font = CG_FontForIndex(fontIndex);
	useScale = scale * font->glyphScale;

	// Compute widescreen-adjusted screen coordinates
	xbias = 0;
	ws = cg_currentWidescreen;

	if (cgs.widescreenBias > 0.0f &&
	    (ws != 0 || (trap_Key_GetCatcher() & KEYCATCH_CGAME))) {
		xscale = (cgs.glconfig.vidHeight * 4.0f / 3.0f) / 640.0f;
		yscale = cgs.screenYScale;
		if (ws == 2 || (trap_Key_GetCatcher() & KEYCATCH_CGAME))
			xbias = cgs.widescreenBias;
		else if (ws == 3)
			xbias = cgs.widescreenBias * 2.0f;
	} else {
		xscale = cgs.screenXScale;
		yscale = cgs.screenYScale;
	}

	// Shadow pass
	if (textStyle == ITEM_TEXTSTYLE_SHADOWED || textStyle == ITEM_TEXTSTYLE_SHADOWEDMORE) {
		float ofs = (textStyle == ITEM_TEXTSTYLE_SHADOWED) ? 1.0f : 2.0f;
		vec4_t shadowColor;
		shadowColor[0] = shadowColor[1] = shadowColor[2] = 0;
		shadowColor[3] = color[3];
		trap_R_SetColor(shadowColor);
		CG_DrawText_Glyphs((x + ofs) * xscale + xbias, (y + ofs) * yscale,
		                    useScale, xscale, yscale, text, adjust, maxChars, font);
	}

	// Main pass
	trap_R_SetColor(color);
	CG_DrawText_Glyphs(x * xscale + xbias, y * yscale,
	                    useScale, xscale, yscale, text, adjust, maxChars, font);
	trap_R_SetColor(NULL);
}

int CG_DrawTextWidth(const char *text, float scale, int limit, int fontIndex) {
	fontInfo_t *font = CG_FontForIndex(fontIndex);
	return CG_Text_Width_Font(text, scale, limit, font);
}

// Scale-based versions (unchanged) - used by ~90 direct call sites in cg_newdraw.c, cg_info.c, etc.
int CG_Text_Width(const char* text, float scale, int limit) {
	int count, len;
	float out;
	glyphInfo_t* glyph;
	float useScale;
	const char* s = text;
	fontInfo_t* font = &cgDC.Assets.textFont;

	if (scale > cg_bigFont.value) {
		font = &cgDC.Assets.bigFont;
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

int CG_Text_Height(const char* text, float scale, int limit) {
	int len, count;
	float max;
	glyphInfo_t* glyph;
	float useScale;
	const char* s = text;
	fontInfo_t* font = &cgDC.Assets.textFont;
	if (scale > cg_bigFont.value) {
		font = &cgDC.Assets.bigFont;
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

void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader) {
	float w, h;
	w = width * scale;
	h = height * scale;
	CG_AdjustFrom640(&x, &y, &w, &h);
	trap_R_DrawStretchPic(x, y, w, h, s, t, s2, t2, hShader);
}

void CG_Text_Paint(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style) {
	int len, count;
	vec4_t newColor;
	glyphInfo_t* glyph;
	float useScale;
	fontInfo_t* font = &cgDC.Assets.textFont;
	if (scale > cg_bigFont.value) {
		font = &cgDC.Assets.bigFont;
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
			// int yadj = Assets.textFont.glyphs[text[i]].bottom + Assets.textFont.glyphs[text[i]].top;
			// float yadj = scale * (Assets.textFont.glyphs[text[i]].imageHeight - Assets.textFont.glyphs[text[i]].height);
			if (Q_IsColorString(s)) {
				memcpy(newColor, g_color_table[ColorIndex(*(s + 1))], sizeof(newColor));
				newColor[3] = color[3];
				trap_R_SetColor(newColor);
				s += 2;
				continue;
			} else {
				float yadj = useScale * glyph->top;
				float xadj = useScale * glyph->left;
				if (style == ITEM_TEXTSTYLE_SHADOWED || style == ITEM_TEXTSTYLE_SHADOWEDMORE) {
					int ofs = style == ITEM_TEXTSTYLE_SHADOWED ? 1 : 2;
					colorBlack[3] = newColor[3];
					trap_R_SetColor(colorBlack);
					CG_Text_PaintChar(x + xadj + ofs, y - yadj + ofs,
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
				CG_Text_PaintChar(x + xadj, y - yadj,
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

/*
================
CG_Draw3DModel

================
*/
void CG_Draw3DModel(float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles) {
	refdef_t refdef;
	refEntity_t ent;

	if (!cg_draw3dIcons.integer || !cg_drawIcons.integer) {
		return;
	}

	CG_AdjustFrom640(&x, &y, &w, &h);

	memset(&refdef, 0, sizeof(refdef));

	memset(&ent, 0, sizeof(ent));
	AnglesToAxis(angles, ent.axis);
	VectorCopy(origin, ent.origin);
	ent.hModel = model;
	ent.customSkin = skin;
	ent.renderfx = RF_NOSHADOW;  // no stencil shadows

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear(refdef.viewaxis);

	refdef.fov_x = 30;
	refdef.fov_y = 30;

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	refdef.time = cg.time;

	trap_R_ClearScene();
	trap_R_AddRefEntityToScene(&ent);
	trap_R_RenderScene(&refdef);
}

/*
================
CG_DrawHead

Used for both the status bar and the scoreboard
================
*/
void CG_DrawHead(float x, float y, float w, float h, int clientNum, vec3_t headAngles) {
	clipHandle_t cm;
	clientInfo_t* ci;
	float len;
	vec3_t origin;
	vec3_t mins, maxs;

	ci = &cgs.clientinfo[clientNum];

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

		CG_Draw3DModel(x, y, w, h, ci->headModel, ci->headSkin, origin, headAngles);
	} else if (cg_drawIcons.integer) {
		CG_DrawPic(x, y, w, h, ci->modelIcon);
	}

	// if they are deferred, draw a cross out
	if (ci->deferred) {
		CG_DrawPic(x, y, w, h, cgs.media.deferShader);
	}
}

/*
================
CG_DrawFlagModel

Used for both the status bar and the scoreboard
================
*/
void CG_DrawFlagModel(float x, float y, float w, float h, int team, qboolean force2D) {
	qhandle_t cm;
	float len;
	vec3_t origin, angles;
	vec3_t mins, maxs;
	qhandle_t handle;

	if (!force2D && cg_draw3dIcons.integer) {
		VectorClear(angles);

		cm = cgs.media.redFlagModel;

		// offset the origin y and z to center the flag
		trap_R_ModelBounds(cm, mins, maxs);

		origin[2] = -0.5 * (mins[2] + maxs[2]);
		origin[1] = 0.5 * (mins[1] + maxs[1]);

		// calculate distance so the flag nearly fills the box
		// assume heads are taller than wide
		len = 0.5 * (maxs[2] - mins[2]);
		origin[0] = len / 0.268;  // len / tan( fov/2 )

		angles[YAW] = 60 * sin(cg.time / 2000.0);
		;

		if (team == TEAM_RED) {
			handle = cgs.media.redFlagModel;
		} else if (team == TEAM_BLUE) {
			handle = cgs.media.blueFlagModel;
		} else if (team == TEAM_FREE) {
			handle = cgs.media.neutralFlagModel;
		} else {
			return;
		}
		CG_Draw3DModel(x, y, w, h, handle, 0, origin, angles);
	} else if (cg_drawIcons.integer) {
		gitem_t* item;

		if (team == TEAM_RED) {
			item = BG_FindItemForPowerup(PW_REDFLAG);
		} else if (team == TEAM_BLUE) {
			item = BG_FindItemForPowerup(PW_BLUEFLAG);
		} else if (team == TEAM_FREE) {
			item = BG_FindItemForPowerup(PW_NEUTRALFLAG);
		} else {
			return;
		}
		if (item) {
			CG_DrawPic(x, y, w, h, cg_items[ITEM_INDEX(item)].icon);
		}
	}
}

/*
================
CG_DrawTeamBackground

================
*/
void CG_DrawTeamBackground(int x, int y, int w, int h, float alpha, int team) {
	vec4_t hcolor;

	hcolor[3] = alpha;
	if (team == TEAM_RED) {
		hcolor[0] = 1;
		hcolor[1] = 0;
		hcolor[2] = 0;
	} else if (team == TEAM_BLUE) {
		hcolor[0] = 0;
		hcolor[1] = 0;
		hcolor[2] = 1;
	} else {
		return;
	}
	trap_R_SetColor(hcolor);
	CG_DrawPic(x, y, w, h, cgs.media.teamStatusBar);
	trap_R_SetColor(NULL);
}

/*
===========================================================================================

  UPPER RIGHT CORNER

===========================================================================================
*/

/*
================
CG_DrawAttacker

================
*/
static float CG_DrawAttacker(float y) {
	int t;
	float size;
	vec3_t angles;
	const char* info;
	const char* name;
	int clientNum;

	if (cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) {
		return y;
	}

	if (!cg.attackerTime) {
		return y;
	}

	clientNum = cg.predictedPlayerState.persistant[PERS_ATTACKER];
	if (clientNum < 0 || clientNum >= MAX_CLIENTS || clientNum == cg.snap->ps.clientNum) {
		return y;
	}

	if (!cgs.clientinfo[clientNum].infoValid) {
		cg.attackerTime = 0;
		return y;
	}

	t = cg.time - cg.attackerTime;
	if (t > ATTACKER_HEAD_TIME) {
		cg.attackerTime = 0;
		return y;
	}

	size = ICON_SIZE * 1.25;

	angles[PITCH] = 0;
	angles[YAW] = 180;
	angles[ROLL] = 0;
	CG_DrawHead(SCREEN_WIDTH - size, y, size, size, clientNum, angles);

	info = CG_ConfigString(CS_PLAYERS + clientNum);
	name = Info_ValueForKey(info, "n");
	y += size;
	{
		vec4_t halfAlpha = { 1, 1, 1, 0.5f };
		float tw = CG_Text_Width(name, 0.3f, 0);
		CG_Text_Paint(SCREEN_WIDTH - tw, y + 14, 0.3f, halfAlpha, name, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}

	return y + BIGCHAR_HEIGHT + 2;
}

/*
==================
CG_DrawSnapshot
==================
*/
static float CG_DrawSnapshot(float y) {
	char* s;
	int w;

	s = va("time:%i snap:%i cmd:%i", cg.snap->serverTime,
		   cg.latestSnapshotNum, cgs.serverCommandSequence);
	w = CG_Text_Width(s, 0.3f, 0);

	CG_Text_Paint(635 - w, y + 14, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	return y + BIGCHAR_HEIGHT + 4;
}

/*
==================
CG_DrawFPS
==================
*/
#define FPS_FRAMES 4
static float CG_DrawFPS(float y) {
	char* s;
	int w;
	static int previousTimes[FPS_FRAMES];
	static int index;
	int i, total;
	int fps;
	static int previous;
	int t, frameTime;

	// don't use serverTime, because that will be drifting to
	// correct for internet lag changes, timescales, timedemos, etc
	t = trap_Milliseconds();
	frameTime = t - previous;
	previous = t;

	previousTimes[index % FPS_FRAMES] = frameTime;
	index++;
	if (index > FPS_FRAMES) {
		// average multiple frames together to smooth changes out a bit
		total = 0;
		for (i = 0; i < FPS_FRAMES; i++) {
			total += previousTimes[i];
		}
		if (!total) {
			total = 1;
		}
		fps = 1000 * FPS_FRAMES / total;

		s = va("%ifps", fps);
		w = CG_Text_Width(s, 0.3f, 0);

		CG_Text_Paint(635 - w, y + 14, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}

	return y + BIGCHAR_HEIGHT + 4;
}

/*
=================
CG_DrawTimer
=================
*/
static float CG_DrawTimer(float y) {
	char* s;
	int w;
	int mins, seconds, tens;
	int msec;

	msec = cg.time - cgs.levelStartTime;

	seconds = msec / 1000;
	mins = seconds / 60;
	seconds -= mins * 60;
	tens = seconds / 10;
	seconds -= tens * 10;

	s = va("%i:%i%i", mins, tens, seconds);
	w = CG_Text_Width(s, 0.3f, 0);

	CG_Text_Paint(635 - w, y + 14, 0.3f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	return y + BIGCHAR_HEIGHT + 4;
}

/*
=================
CG_DrawTeamOverlay
=================
*/

static float CG_DrawTeamOverlay(float y, qboolean right, qboolean upper) {
	int x, w, h, xx;
	int i, j, len;
	const char* p;
	vec4_t hcolor;
	int pwidth, lwidth;
	int plyrs;
	char st[16];
	clientInfo_t* ci;
	gitem_t* item;
	int ret_y, count;

	if (!cg_drawTeamOverlay.integer) {
		return y;
	}

	if (cg.snap->ps.persistant[PERS_TEAM] != TEAM_RED && cg.snap->ps.persistant[PERS_TEAM] != TEAM_BLUE) {
		return y;  // Not on any team
	}

	plyrs = 0;

	// max player name width
	pwidth = 0;
	count = (numSortedTeamPlayers > 8) ? 8 : numSortedTeamPlayers;
	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if (ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
			plyrs++;
			len = CG_DrawStrlen(ci->name);
			if (len > pwidth)
				pwidth = len;
		}
	}

	if (!plyrs)
		return y;

	if (pwidth > TEAM_OVERLAY_MAXNAME_WIDTH)
		pwidth = TEAM_OVERLAY_MAXNAME_WIDTH;

	// max location name width
	lwidth = 0;
	for (i = 1; i < MAX_LOCATIONS; i++) {
		p = CG_ConfigString(CS_LOCATIONS + i);
		if (p && *p) {
			len = CG_DrawStrlen(p);
			if (len > lwidth)
				lwidth = len;
		}
	}

	if (lwidth > TEAM_OVERLAY_MAXLOCATION_WIDTH)
		lwidth = TEAM_OVERLAY_MAXLOCATION_WIDTH;

	w = (pwidth + lwidth + 4 + 7) * TINYCHAR_WIDTH;

	if (right)
		x = 640 - w;
	else
		x = 0;

	h = plyrs * TINYCHAR_HEIGHT;

	if (upper) {
		ret_y = y + h;
	} else {
		y -= h;
		ret_y = y;
	}

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED) {
		hcolor[0] = 1.0f;
		hcolor[1] = 0.0f;
		hcolor[2] = 0.0f;
		hcolor[3] = 0.33f;
	} else {  // if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE )
		hcolor[0] = 0.0f;
		hcolor[1] = 0.0f;
		hcolor[2] = 1.0f;
		hcolor[3] = 0.33f;
	}
	trap_R_SetColor(hcolor);
	CG_DrawPic(x, y, w, h, cgs.media.teamStatusBar);
	trap_R_SetColor(NULL);

	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if (ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
			hcolor[0] = hcolor[1] = hcolor[2] = hcolor[3] = 1.0;

			xx = x + TINYCHAR_WIDTH;

			CG_DrawStringExt(xx, y,
							 ci->name, hcolor, qfalse, qfalse,
							 TINYCHAR_WIDTH, TINYCHAR_HEIGHT, TEAM_OVERLAY_MAXNAME_WIDTH);

			if (lwidth) {
				p = CG_ConfigString(CS_LOCATIONS + ci->location);
				if (!p || !*p)
					p = "unknown";
				//				len = CG_DrawStrlen(p);
				//				if (len > lwidth)
				//					len = lwidth;

				//				xx = x + TINYCHAR_WIDTH * 2 + TINYCHAR_WIDTH * pwidth +
				//					((lwidth/2 - len/2) * TINYCHAR_WIDTH);
				xx = x + TINYCHAR_WIDTH * 2 + TINYCHAR_WIDTH * pwidth;
				CG_DrawStringExt(xx, y,
								 p, hcolor, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
								 TEAM_OVERLAY_MAXLOCATION_WIDTH);
			}

			CG_GetColorForHealth(ci->health, ci->armor, hcolor);

			Com_sprintf(st, sizeof(st), "%3i %3i", ci->health, ci->armor);

			xx = x + TINYCHAR_WIDTH * 3 +
				TINYCHAR_WIDTH * pwidth + TINYCHAR_WIDTH * lwidth;

			CG_DrawStringExt(xx, y,
							 st, hcolor, qfalse, qfalse,
							 TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0);

			// draw weapon icon
			xx += TINYCHAR_WIDTH * 3;

			if (cg_weapons[ci->curWeapon].weaponIcon) {
				CG_DrawPic(xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
						   cg_weapons[ci->curWeapon].weaponIcon);
			} else {
				CG_DrawPic(xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
						   cgs.media.deferShader);
			}

			// Draw powerup icons
			if (right) {
				xx = x;
			} else {
				xx = x + w - TINYCHAR_WIDTH;
			}
			for (j = 0; j <= PW_NUM_POWERUPS; j++) {
				if (ci->powerups & (1 << j)) {
					item = BG_FindItemForPowerup(j);

					if (item) {
						CG_DrawPic(xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
								   trap_R_RegisterShader(item->icon));
						if (right) {
							xx -= TINYCHAR_WIDTH;
						} else {
							xx += TINYCHAR_WIDTH;
						}
					}
				}
			}

			y += TINYCHAR_HEIGHT;
		}
	}

	return ret_y;
	// #endif
}

/*
=====================
CG_DrawSpeedometer

[QL] Draws the speedometer bar graph and text.
Modes: 1 = icon+bars+text (right), 2 = bars+text (center), 3 = text only (center)
Mode 4 is handled by the menu ownerdraw (cg_newdraw.c), not here.
=====================
*/
static void CG_DrawSpeedometer(void) {
	int mode = cg_speedometer.integer;
	int i, idx;
	float rectX, rectY, rectW, rectH;
	float barScale, barWidth, halfHeight;
	float speed, barHeight, barX, barY, fullBarH, overflowH, totalH;
	int graphX, graphY;
	int baseSpeed;
	const char* text;

	if (!cg.snap) return;

	baseSpeed = cg.snap->ps.speed;
	if (baseSpeed <= 0) baseSpeed = 320;

	// set positions based on mode
	if (mode < 2) {
		// mode 1: right-aligned, icon + bars + text
		rectX = 592.0f;
		rectY = 384.0f;
		rectW = 48.0f;
		rectH = 48.0f;
		graphX = 592;
		graphY = 384;
		cg.speedBarColor1[3] = 1.0f;
		cg.speedBarColor2[3] = 1.0f;
		CG_SetWidescreen(WIDESCREEN_RIGHT);
	} else {
		// modes 2-3: centered
		rectX = 256.0f;
		graphY = cg_crosshairY.integer / 2 + 241;
		rectY = (float)graphY;
		rectW = 128.0f;
		rectH = 32.0f;
		graphX = 256;
		cg.speedBarColor1[3] = 0.75f;
		cg.speedBarColor2[3] = 0.75f;
		CG_SetWidescreen(WIDESCREEN_CENTER);
	}

	// adjust coordinates to screen space
	CG_AdjustFrom640(&rectX, &rectY, &rectW, &rectH);

	halfHeight = rectH * 0.5f;
	barScale = (rectH - 5.0f) / ((float)baseSpeed * 3.0f);
	barWidth = rectW / (float)SPEED_HISTORY_SIZE;

	// mode 1: draw background (lagometer shader, same as binary)
	if (mode < 2) {
		trap_R_SetColor(NULL);
		CG_DrawPic((float)graphX, (float)graphY, 48.0f, 48.0f, cgs.media.lagometerShader);
	}

	// modes 1-2: draw bar graph
	if (mode < 3 && cg.speedHistoryCount > 0) {
		idx = cg.speedHistoryIndex + 1;
		if (idx > SPEED_HISTORY_SIZE - 1) idx = 0;

		for (i = 0; i < cg.speedHistoryCount; i++) {
			speed = cg.speedHistory[idx];
			if (speed > 0.0f) {
				barHeight = barScale * speed;
				if (barHeight > halfHeight) barHeight = halfHeight;

				// lower half bar (green)
				trap_R_SetColor(cg.speedBarColor1);
				barX = rectX + barWidth * (float)i;
				barY = (rectY + rectH) - barHeight;
				trap_R_DrawStretchPic(barX, barY, barWidth, barHeight,
									  0, 0, 0, 0, cgs.media.whiteShader);

				// overflow bar (yellow) - when speed exceeds half
				fullBarH = barScale * speed;
				if (fullBarH > halfHeight) {
					overflowH = fullBarH - barHeight;
					if (overflowH > halfHeight) overflowH = halfHeight;
					totalH = fullBarH;
					if (totalH > rectH) totalH = rectH;
					trap_R_SetColor(cg.speedBarColor2);
					barY = (rectY + rectH) - totalH;
					trap_R_DrawStretchPic(barX, barY, barWidth, overflowH,
										  0, 0, 0, 0, cgs.media.whiteShader);
				}
			}

			idx++;
			if (idx > SPEED_HISTORY_SIZE - 1) idx = 0;
		}
	}

	// text: draw current speed value below the bar area
	{
		int speedInt = (int)cg.speedHistory[cg.speedHistoryIndex];
		int textX, textY;
		float tw;

		text = va("%d", speedInt);
		tw = CG_Text_Width(text, 0.175f, 0);

		if (mode < 2) {
			// mode 1: right-aligned, text below 48px bar area
			CG_SetWidescreen(WIDESCREEN_RIGHT);
			textX = graphX + 2;
			textY = graphY + 8;
		} else {
			// modes 2-3: centered text below 32px bar area
			textX = 320 - (int)(tw / 2);
			textY = graphY + 32 + 1;
		}

		CG_Text_Paint((float)textX, (float)textY,
					  0.15f, colorWhite, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}

	trap_R_SetColor(NULL);
	CG_SetWidescreen(WIDESCREEN_STRETCH);
}

/*
=====================
CG_DrawUpperRight

=====================
*/
static void CG_DrawUpperRight(stereoFrame_t stereoFrame) {
	float y;

	y = 0;

	if (cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 1) {
		y = CG_DrawTeamOverlay(y, qtrue, qtrue);
	}
	if (cg_drawSnapshot.integer) {
		y = CG_DrawSnapshot(y);
	}
	if (cg_drawFPS.integer && (stereoFrame == STEREO_CENTER || stereoFrame == STEREO_RIGHT)) {
		y = CG_DrawFPS(y);
	}
	if (cg_drawTimer.integer) {
		y = CG_DrawTimer(y);
	}
	if (cg_drawAttacker.integer) {
		CG_DrawAttacker(y);
	}
	// [QL] speedometer - bar graph + text drawn for modes 1-3
	// (mode 4 is text-only via menu ownerdraw, not drawn here)
	if (cg_speedometer.integer > 0 && cg_speedometer.integer < 4) {
		CG_DrawSpeedometer();
	}
}

/*
===========================================================================================

  LOWER RIGHT CORNER

===========================================================================================
*/

/*
===================
CG_DrawReward
===================
*/
static void CG_DrawReward(void) {
	float* color;
	int i, count;
	float x, y;
	char buf[32];

	if (!cg_drawRewards.integer) {
		return;
	}

	color = CG_FadeColor(cg.rewardTime, REWARD_TIME);
	if (!color) {
		if (cg.rewardStack > 0) {
			for (i = 0; i < cg.rewardStack; i++) {
				cg.rewardSound[i] = cg.rewardSound[i + 1];
				cg.rewardShader[i] = cg.rewardShader[i + 1];
				cg.rewardCount[i] = cg.rewardCount[i + 1];
			}
			cg.rewardTime = cg.time;
			cg.rewardStack--;
			color = CG_FadeColor(cg.rewardTime, REWARD_TIME);
			trap_S_StartLocalSound(cg.rewardSound[0], CHAN_ANNOUNCER);
		} else {
			return;
		}
	}

	trap_R_SetColor(color);

	if (cg.rewardCount[0] >= 10) {
		y = 56;
		x = 320 - ICON_SIZE / 2;
		CG_DrawPic(x, y, ICON_SIZE - 4, ICON_SIZE - 4, cg.rewardShader[0]);
		Com_sprintf(buf, sizeof(buf), "%d", cg.rewardCount[0]);
		{
			float tw = CG_Text_Width(buf, 0.22f, 0);
			CG_Text_Paint((SCREEN_WIDTH - tw) / 2, y + ICON_SIZE + 12, 0.22f, color, buf, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
		}
	} else {
		count = cg.rewardCount[0];

		y = 56;
		x = 320 - count * ICON_SIZE / 2;
		for (i = 0; i < count; i++) {
			CG_DrawPic(x, y, ICON_SIZE - 4, ICON_SIZE - 4, cg.rewardShader[0]);
			x += ICON_SIZE;
		}
	}
	trap_R_SetColor(NULL);
}

/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define LAG_SAMPLES 128

typedef struct {
	int frameSamples[LAG_SAMPLES];
	int frameCount;
	int snapshotFlags[LAG_SAMPLES];
	int snapshotSamples[LAG_SAMPLES];
	int snapshotCount;
} lagometer_t;

lagometer_t lagometer;

/*
==============
CG_AddLagometerFrameInfo

Adds the current interpolate / extrapolate bar for this frame
==============
*/
void CG_AddLagometerFrameInfo(void) {
	int offset;

	offset = cg.time - cg.latestSnapshotTime;
	lagometer.frameSamples[lagometer.frameCount & (LAG_SAMPLES - 1)] = offset;
	lagometer.frameCount++;
}

/*
==============
CG_AddLagometerSnapshotInfo

Each time a snapshot is received, log its ping time and
the number of snapshots that were dropped before it.

Pass NULL for a dropped packet.
==============
*/
void CG_AddLagometerSnapshotInfo(snapshot_t* snap) {
	// dropped packet
	if (!snap) {
		lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = -1;
		lagometer.snapshotCount++;
		return;
	}

	// add this snapshot's info
	lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = snap->ping;
	lagometer.snapshotFlags[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = snap->snapFlags;
	lagometer.snapshotCount++;
}

/*
==============
CG_DrawDisconnect

Should we draw something differnet for long lag vs no packets?
==============
*/
static void CG_DrawDisconnect(void) {
	float x, y;
	int cmdNum;
	usercmd_t cmd;
	const char* s;
	int w;

	// draw the phone jack if we are completely past our buffers
	cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
	trap_GetUserCmd(cmdNum, &cmd);
	if (cmd.serverTime <= cg.snap->ps.commandTime || cmd.serverTime > cg.time) {  // special check for map_restart
		return;
	}

	// also add text in center of screen
	s = "Connection Interrupted";
	w = CG_Text_Width(s, 0.5f, 0);
	CG_Text_Paint(320 - w / 2, 116, 0.5f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	// blink the icon
	if ((cg.time >> 9) & 1) {
		return;
	}

	x = SCREEN_WIDTH - 48;
	y = SCREEN_HEIGHT - 144;

	CG_DrawPic(x, y, 48, 48, trap_R_RegisterShader("gfx/2d/net.jpg"));
}

#define MAX_LAGOMETER_PING 900
#define MAX_LAGOMETER_RANGE 300

/*
==============
CG_DrawLagometer
==============
*/
static void CG_DrawLagometer(void) {
	int a, x, y, i;
	float v;
	float ax, ay, aw, ah, mid, range;
	int color;
	float vscale;

	if (!cg_lagometer.integer || cg.demoPlayback) {
		CG_DrawDisconnect();
		return;
	}

	//
	// draw the graph
	//
	x = SCREEN_WIDTH - 48;
	y = SCREEN_HEIGHT - 144;

	trap_R_SetColor(NULL);
	CG_DrawPic(x, y, 48, 48, cgs.media.lagometerShader);

	ax = x;
	ay = y;
	aw = 48;
	ah = 48;
	CG_AdjustFrom640(&ax, &ay, &aw, &ah);

	color = -1;
	range = ah / 3;
	mid = ay + range;

	vscale = range / MAX_LAGOMETER_RANGE;

	// draw the frame interpoalte / extrapolate graph
	for (a = 0; a < aw; a++) {
		i = (lagometer.frameCount - 1 - a) & (LAG_SAMPLES - 1);
		v = lagometer.frameSamples[i];
		v *= vscale;
		if (v > 0) {
			if (color != 1) {
				color = 1;
				trap_R_SetColor(g_color_table[ColorIndex(COLOR_YELLOW)]);
			}
			if (v > range) {
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, mid - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		} else if (v < 0) {
			if (color != 2) {
				color = 2;
				trap_R_SetColor(g_color_table[ColorIndex(COLOR_BLUE)]);
			}
			v = -v;
			if (v > range) {
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, mid, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		}
	}

	// draw the snapshot latency / drop graph
	range = ah / 2;
	vscale = range / MAX_LAGOMETER_PING;

	for (a = 0; a < aw; a++) {
		i = (lagometer.snapshotCount - 1 - a) & (LAG_SAMPLES - 1);
		v = lagometer.snapshotSamples[i];
		if (v > 0) {
			if (lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED) {
				if (color != 5) {
					color = 5;  // YELLOW for rate delay
					trap_R_SetColor(g_color_table[ColorIndex(COLOR_YELLOW)]);
				}
			} else {
				if (color != 3) {
					color = 3;
					trap_R_SetColor(g_color_table[ColorIndex(COLOR_GREEN)]);
				}
			}
			v = v * vscale;
			if (v > range) {
				v = range;
			}
			trap_R_DrawStretchPic(ax + aw - a, ay + ah - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader);
		} else if (v < 0) {
			if (color != 4) {
				color = 4;  // RED for dropped snapshots
				trap_R_SetColor(g_color_table[ColorIndex(COLOR_RED)]);
			}
			trap_R_DrawStretchPic(ax + aw - a, ay + ah - range, 1, range, 0, 0, 0, 0, cgs.media.whiteShader);
		}
	}

	trap_R_SetColor(NULL);

	if (cg_nopredict.integer || cg_synchronousClients.integer) {
		CG_Text_Paint(x, y + 14, 0.3f, colorWhite, "snc", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}

	// [QL] draw ping number when cg_lagometer == 2
	if (cg_lagometer.integer == 2 && cg.snap) {
		static int lastPing;
		const char* text;
		int textX, textY;
		float tw;

		if (cg.snap->ping) {
			lastPing = cg.snap->ping;
		}
		text = va("%d", lastPing);
		tw = CG_Text_Width(text, 0.175f, 0);
		textX = x + 2;
		textY = y + 8;
		CG_Text_Paint((float)textX, (float)textY,
					  0.15f, colorWhite, text, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	}

	CG_DrawDisconnect();
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

/*
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_CenterPrint(const char* str, int y, int charWidth) {
	char* s;

	Q_strncpyz(cg.centerPrint, str, sizeof(cg.centerPrint));

	cg.centerPrintTime = cg.time;
	cg.centerPrintY = y;
	cg.centerPrintCharWidth = charWidth;

	// count the number of lines for centering
	cg.centerPrintLines = 1;
	s = cg.centerPrint;
	while (*s) {
		if (*s == '\n')
			cg.centerPrintLines++;
		s++;
	}
}

/*
===================
CG_DrawCenterString
===================
*/
static void CG_DrawCenterString(void) {
	char* start;
	int l;
	int x, y, w;

	int h;

	float* color;

	if (!cg.centerPrintTime) {
		return;
	}

	color = CG_FadeColor(cg.centerPrintTime, 1000 * cg_centertime.value);
	if (!color) {
		return;
	}

	trap_R_SetColor(color);

	start = cg.centerPrint;

	y = cg.centerPrintY - cg.centerPrintLines * BIGCHAR_HEIGHT / 2;

	while (1) {
		char linebuffer[1024];

		for (l = 0; l < 50; l++) {
			if (!start[l] || start[l] == '\n') {
				break;
			}
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = CG_Text_Width(linebuffer, 0.5, 0);
		h = CG_Text_Height(linebuffer, 0.5, 0);
		x = (SCREEN_WIDTH - w) / 2;
		CG_Text_Paint(x, y + h, 0.5, color, linebuffer, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
		y += h + 6;

		while (*start && (*start != '\n')) {
			start++;
		}
		if (!*start) {
			break;
		}
		start++;
	}

	trap_R_SetColor(NULL);
}

/*
================================================================================

CROSSHAIR

================================================================================
*/

/*
=================
CG_DrawCrosshair
=================
*/
static void CG_DrawCrosshair(void) {
	float w, h;
	qhandle_t hShader;
	float f;
	float x, y;
	int ca;

	if (!cg_drawCrosshair.integer) {
		return;
	}

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
		return;
	}

	if (cg.renderingThirdPerson) {
		return;
	}

	// set color based on health
	if (cg_crosshairHealth.integer) {
		vec4_t hcolor;

		CG_ColorForHealth(hcolor);
		trap_R_SetColor(hcolor);
	} else {
		trap_R_SetColor(NULL);
	}

	w = h = cg_crosshairSize.value;

	// pulse the size of the crosshair when picking up items
	f = cg.time - cg.itemPickupBlendTime;
	if (f > 0 && f < ITEM_BLOB_TIME) {
		f /= ITEM_BLOB_TIME;
		w *= (1 + f);
		h *= (1 + f);
	}

	x = cg_crosshairX.integer;
	y = cg_crosshairY.integer;
	CG_AdjustFrom640(&x, &y, &w, &h);

	ca = cg_drawCrosshair.integer;
	if (ca < 0) {
		ca = 0;
	}
	hShader = cgs.media.crosshairShader[ca % NUM_CROSSHAIRS];

	trap_R_DrawStretchPic(x + cg.refdef.x + 0.5 * (cg.refdef.width - w),
						  y + cg.refdef.y + 0.5 * (cg.refdef.height - h),
						  w, h, 0, 0, 1, 1, hShader);

	trap_R_SetColor(NULL);
}

/*
=================
CG_DrawCrosshair3D
=================
*/
static void CG_DrawCrosshair3D(void) {
	float w;
	qhandle_t hShader;
	float f;
	int ca;

	trace_t trace;
	vec3_t endpos;
	float stereoSep, zProj, maxdist, xmax;
	char rendererinfos[128];
	refEntity_t ent;

	if (!cg_drawCrosshair.integer) {
		return;
	}

	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
		return;
	}

	if (cg.renderingThirdPerson) {
		return;
	}

	w = cg_crosshairSize.value;

	// pulse the size of the crosshair when picking up items
	f = cg.time - cg.itemPickupBlendTime;
	if (f > 0 && f < ITEM_BLOB_TIME) {
		f /= ITEM_BLOB_TIME;
		w *= (1 + f);
	}

	ca = cg_drawCrosshair.integer;
	if (ca < 0) {
		ca = 0;
	}
	hShader = cgs.media.crosshairShader[ca % NUM_CROSSHAIRS];

	// Use a different method rendering the crosshair so players don't see two of them when
	// focusing their eyes at distant objects with high stereo separation
	// We are going to trace to the next shootable object and place the crosshair in front of it.

	// first get all the important renderer information
	trap_Cvar_VariableStringBuffer("r_zProj", rendererinfos, sizeof(rendererinfos));
	zProj = atof(rendererinfos);
	trap_Cvar_VariableStringBuffer("r_stereoSeparation", rendererinfos, sizeof(rendererinfos));
	stereoSep = zProj / atof(rendererinfos);

	xmax = zProj * tan(cg.refdef.fov_x * M_PI / 360.0f);

	// let the trace run through until a change in stereo separation of the crosshair becomes less than one pixel.
	maxdist = cgs.glconfig.vidWidth * stereoSep * zProj / (2 * xmax);
	VectorMA(cg.refdef.vieworg, maxdist, cg.refdef.viewaxis[0], endpos);
	CG_Trace(&trace, cg.refdef.vieworg, NULL, NULL, endpos, 0, MASK_SHOT);

	memset(&ent, 0, sizeof(ent));
	ent.reType = RT_SPRITE;
	ent.renderfx = RF_DEPTHHACK | RF_CROSSHAIR;

	VectorCopy(trace.endpos, ent.origin);

	// scale the crosshair so it appears the same size for all distances
	ent.radius = w / 640 * xmax * trace.fraction * maxdist / zProj;
	ent.customShader = hShader;

	trap_R_AddRefEntityToScene(&ent);
}

/*
=================
CG_ScanForCrosshairEntity
=================
*/
static void CG_ScanForCrosshairEntity(void) {
	trace_t trace;
	vec3_t start, end;
	int content;

	VectorCopy(cg.refdef.vieworg, start);
	VectorMA(start, 131072, cg.refdef.viewaxis[0], end);

	CG_Trace(&trace, start, vec3_origin, vec3_origin, end,
			 cg.snap->ps.clientNum, CONTENTS_SOLID | CONTENTS_BODY);
	if (trace.entityNum >= MAX_CLIENTS) {
		return;
	}

	// if the player is in fog, don't show it
	content = CG_PointContents(trace.endpos, 0);
	if (content & CONTENTS_FOG) {
		return;
	}

	// if the player is invisible, don't show it
	if (cg_entities[trace.entityNum].currentState.powerups & (1 << PW_INVIS)) {
		return;
	}

	// update the fade timer
	cg.crosshairClientNum = trace.entityNum;
	cg.crosshairClientTime = cg.time;
}

/*
=====================
CG_DrawCrosshairNames
=====================
*/
static void CG_DrawCrosshairNames(void) {
	float* color;
	char* name;
	float w;

	if (!cg_drawCrosshair.integer) {
		return;
	}
	if (!cg_drawCrosshairNames.integer) {
		return;
	}
	if (cg.renderingThirdPerson) {
		return;
	}

	// scan the known entities to see if the crosshair is sighted on one
	CG_ScanForCrosshairEntity();

	// draw the name of the player being looked at
	color = CG_FadeColor(cg.crosshairClientTime, 1000);
	if (!color) {
		trap_R_SetColor(NULL);
		return;
	}

	name = cgs.clientinfo[cg.crosshairClientNum].name;
	color[3] *= 0.5f;
	w = CG_Text_Width(name, 0.3f, 0);
	CG_Text_Paint(320 - w / 2, 190, 0.3f, color, name, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	trap_R_SetColor(NULL);
}

//==============================================================================

/*
=================
CG_DrawSpectator

[QL] Rewritten to match binary (cgamex86.dll 0x10034d70).
- CA/AD eliminated players: "Round In Progress" centered at y=60
- Pure spectators: "SPECTATOR MODE" + "Press mouse button 1..." header,
  then left-aligned "waiting to play" (duel) or "press ESC..." (team)
=================
*/
static void CG_DrawSpectator(void) {
	float scale, w, y;
	const char* s;
	vec4_t grayColor = {0.73f, 0.73f, 0.73f, 0.7f};
	char attackKey[32], menuKey[32];

	// [QL] CA/AD: show "Round In Progress" for eliminated players (on a team but spectating)
	if ((cgs.gametype == GT_CA || cgs.gametype == GT_AD) &&
		cg.snap->ps.pm_type == PM_SPECTATOR &&
		cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR) {
		scale = 0.35f;
		s = "Round In Progress";
		CG_SetWidescreen(WIDESCREEN_CENTER);
		w = CG_Text_Width(s, scale, 0);
		CG_Text_Paint(320 - w / 2, 60, scale, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
		return;
	}

	CG_KeyNameForCommand("+attack", attackKey, sizeof(attackKey));
	CG_KeyNameForCommand("togglemenu", menuKey, sizeof(menuKey));

	// "SPECTATOR MODE" header
	scale = 0.22f;
	s = "SPECTATOR MODE";
	w = CG_Text_Width(s, scale, 0);
	CG_Text_Paint(320 - w / 2, 440, scale, colorWhite, s, 0, 0, 0);

	// "Press <key> to cycle through players"
	y = 452;
	s = va("Press %s to cycle through players", attackKey);
	w = CG_Text_Width(s, 0.18f, 0);
	CG_Text_Paint(320 - w / 2, y, 0.18f, grayColor, s, 0, 0, 0);

	// Bottom hints - left-aligned, skip when following a player
	if (!(cg.snap->ps.pm_flags & PMF_FOLLOW)) {
		CG_SetWidescreen(WIDESCREEN_LEFT);
		if (cgs.gametype == GT_DUEL) {
			CG_Text_Paint(20, 461, 0.28f, colorWhite, "waiting to play", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
		} else if (cgs.gametype >= GT_TEAM) {
			s = va("press %s and use the JOIN buttons", menuKey);
			CG_Text_Paint(20, 453, 0.28f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
			CG_Text_Paint(20, 470, 0.28f, colorWhite, "to enter the game", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
		}
	}
}

/*
=================
CG_DrawVote
=================
*/
static void CG_DrawVote(void) {
	char* s;
	int sec;

	if (!cgs.voteTime) {
		return;
	}

	// play a talk beep whenever it is modified
	if (cgs.voteModified) {
		cgs.voteModified = qfalse;
		trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
	}

	// [QL] Vote countdown: binary uses (voteTime - cg.time + 30000) / 1000
	sec = (cgs.voteTime - cg.time + VOTE_TIME) / 1000;
	if (sec < 0) {
		sec = 0;
	}
	// [QL] Show key bindings in vote text, Y=300/312 (binary-verified)
	s = va("VOTE(%is):%s yes:%i no:%i", sec, cgs.voteString, cgs.voteYes, cgs.voteNo);
	CG_Text_Paint(4, 300, 0.22f, colorYellow, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
	{
		char menuKey[32];
		CG_KeyNameForCommand("togglemenu", menuKey, sizeof(menuKey));
		s = va("or press %s then click Vote", menuKey);
	}
	CG_Text_Paint(8, 312, 0.22f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
}

/*
=================
CG_DrawTeamVote
=================
*/
static void CG_DrawTeamVote(void) {
	char* s;
	int sec, cs_offset;

	if (cgs.clientinfo[cg.clientNum].team == TEAM_RED)
		cs_offset = 0;
	else if (cgs.clientinfo[cg.clientNum].team == TEAM_BLUE)
		cs_offset = 1;
	else
		return;

	if (!cgs.teamVoteTime[cs_offset]) {
		return;
	}

	// play a talk beep whenever it is modified
	if (cgs.teamVoteModified[cs_offset]) {
		cgs.teamVoteModified[cs_offset] = qfalse;
		trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
	}

	sec = (VOTE_TIME - (cg.time - cgs.teamVoteTime[cs_offset])) / 1000;
	if (sec < 0) {
		sec = 0;
	}
	s = va("TEAMVOTE(%is):%s yes:%i no:%i", sec, cgs.teamVoteString[cs_offset],
		   cgs.teamVoteYes[cs_offset], cgs.teamVoteNo[cs_offset]);
	CG_Text_Paint(4, 324, 0.22f, colorYellow, s, 0, 0, ITEM_TEXTSTYLE_SHADOWED);
}

// [QL] Set both in-game and end-of-game scoreboard menus by gametype
void CG_SetEndScoreboardMenu(void) {
	switch (cgs.gametype) {
	case GT_FFA:        menuScoreboard = Menus_FindByName("score_menu_ffa");         menuEndScoreboard = Menus_FindByName("endscore_menu_ffa"); break;
	case GT_DUEL:       menuScoreboard = Menus_FindByName("score_menu_duel");        menuEndScoreboard = Menus_FindByName("endscore_menu_duel"); break;
	case GT_RACE:       menuScoreboard = Menus_FindByName("score_menu_race");        menuEndScoreboard = Menus_FindByName("endscore_menu_race"); break;
	case GT_RR:         menuScoreboard = Menus_FindByName("score_menu_rr");          menuEndScoreboard = Menus_FindByName("endscore_menu_rr"); break;
	case GT_TEAM:       menuScoreboard = Menus_FindByName("teamscore_menu_tdm");     menuEndScoreboard = Menus_FindByName("endteamscore_menu_tdm"); break;
	case GT_CA:         menuScoreboard = Menus_FindByName("teamscore_menu_ca");      menuEndScoreboard = Menus_FindByName("endteamscore_menu_ca"); break;
	case GT_CTF:        menuScoreboard = Menus_FindByName("teamscore_menu_ctf");     menuEndScoreboard = Menus_FindByName("endteamscore_menu_ctf"); break;
	case GT_1FCTF:      menuScoreboard = Menus_FindByName("teamscore_menu_1fctf");   menuEndScoreboard = Menus_FindByName("endteamscore_menu_1fctf"); break;
	case GT_HARVESTER:  menuScoreboard = Menus_FindByName("teamscore_menu_har");     menuEndScoreboard = Menus_FindByName("endteamscore_menu_har"); break;
	case GT_FREEZE:     menuScoreboard = Menus_FindByName("teamscore_menu_ft");      menuEndScoreboard = Menus_FindByName("endteamscore_menu_ft"); break;
	case GT_DOMINATION: menuScoreboard = Menus_FindByName("teamscore_menu_dom");     menuEndScoreboard = Menus_FindByName("endteamscore_menu_dom"); break;
	case GT_AD:         menuScoreboard = Menus_FindByName("teamscore_menu_ad");      menuEndScoreboard = Menus_FindByName("endteamscore_menu_ad"); break;
	default:            menuScoreboard = Menus_FindByName("teamscore_menu");         menuEndScoreboard = Menus_FindByName("endteamscore_menu"); break;
	}
}

static qboolean CG_DrawScoreboard(void) {
	static qboolean firstTime = qtrue;

	if (menuScoreboard) {
		menuScoreboard->window.flags &= ~WINDOW_FORCED;
	}
	if (cg_paused.integer) {
		cg.deferredPlayerLoading = 0;
		firstTime = qtrue;
		return qfalse;
	}

	// don't draw scoreboard during death while warmup up
	if (cg.warmup && !cg.showScores) {
		return qfalse;
	}

	if (cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD || cg.predictedPlayerState.pm_type == PM_INTERMISSION) {
	} else {
		if (!CG_FadeColor(cg.scoreFadeTime, FADE_TIME)) {
			// next time scoreboard comes up, don't print killer
			cg.deferredPlayerLoading = 0;
			cg.killerName[0] = 0;
			firstTime = qtrue;
			return qfalse;
		}
	}

	if (menuScoreboard == NULL) {
		CG_SetEndScoreboardMenu();
	}

	// [QL] Use end scoreboard during intermission, in-game scoreboard otherwise
	{
		menuDef_t *activeMenu;
		if (cg.predictedPlayerState.pm_type == PM_INTERMISSION && menuEndScoreboard) {
			activeMenu = menuEndScoreboard;
		} else {
			activeMenu = menuScoreboard;
		}

		if (activeMenu) {
			if (firstTime) {
				CG_SetScoreSelection(activeMenu);
				firstTime = qfalse;
			}
			Menu_Paint(activeMenu, qtrue);
		}
	}

	// load any models that have been deferred
	if (++cg.deferredPlayerLoading > 10) {
		CG_LoadDeferredPlayers();
	}

	return qtrue;
}

/*
=================
CG_DrawIntermission
=================
*/
static void CG_DrawIntermission(void) {
	cg.scoreFadeTime = cg.time;
	cg.scoreBoardShowing = CG_DrawScoreboard();
}

/*
=================
CG_DrawFollow
=================
*/
static qboolean CG_DrawFollow(void) {
	// [QL] Team-specific colors for followed player name (binary-verified)
	static vec4_t teamColorFree = { 1.0f, 1.0f, 1.0f, 1.0f };
	static vec4_t teamColorRed  = { 1.0f, 0.5f, 0.5f, 1.0f };
	static vec4_t teamColorBlue = { 0.5f, 0.75f, 1.0f, 1.0f };
	static vec4_t teamColorSpec = { 0.85f, 0.85f, 0.85f, 1.0f };
	const char* name;
	float *nameColor;

	if (!(cg.snap->ps.pm_flags & PMF_FOLLOW)) {
		return qfalse;
	}

	{
		float w = CG_Text_Width("following", 0.4f, 0);
		CG_Text_Paint(320 - w / 2, 38, 0.4f, colorWhite, "following", 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}

	name = cgs.clientinfo[cg.snap->ps.clientNum].name;

	switch (cgs.clientinfo[cg.snap->ps.clientNum].team) {
	case TEAM_RED:       nameColor = teamColorRed; break;
	case TEAM_BLUE:      nameColor = teamColorBlue; break;
	case TEAM_SPECTATOR: nameColor = teamColorSpec; break;
	default:             nameColor = teamColorFree; break;
	}

	{
		float w = CG_Text_Width(name, 0.6f, 0);
		CG_Text_Paint(320 - w / 2, 70, 0.6f, nameColor, name, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}

	return qtrue;
}

/*
=================
CG_DrawAmmoWarning
=================
*/
static void CG_DrawAmmoWarning(void) {
	const char* s;

	if (cg_drawAmmoWarning.integer == 0) {
		return;
	}

	if (!cg.lowAmmoWarning) {
		return;
	}

	if (cg.lowAmmoWarning == 2) {
		s = "OUT OF AMMO";
	} else {
		s = "LOW AMMO WARNING";
	}
	{
		float tw = CG_Text_Width(s, 0.4f, 0);
		CG_Text_Paint(320 - tw / 2, 78, 0.4f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}
}

/*
=================
CG_DrawProxWarning
=================
*/
static void CG_DrawProxWarning(void) {
	char s[32];
	static int proxTime;
	int proxTick;

	if (!(cg.snap->ps.eFlags & EF_TICKING)) {
		proxTime = 0;
		return;
	}

	if (proxTime == 0) {
		proxTime = cg.time;
	}

	proxTick = 10 - ((cg.time - proxTime) / 1000);

	if (proxTick > 0 && proxTick <= 5) {
		Com_sprintf(s, sizeof(s), "INTERNAL COMBUSTION IN: %i", proxTick);
	} else {
		Com_sprintf(s, sizeof(s), "YOU HAVE BEEN MINED");
	}

	{
		float tw = CG_Text_Width(s, 0.4f, 0);
		CG_Text_Paint(320 - tw / 2, 96, 0.4f, g_color_table[ColorIndex(COLOR_RED)], s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}
}

/*
=================
CG_DrawWarmupCountdown

[QL] Draw the countdown display during COUNT_DOWN phase.
Binary: FUN_1000ec00 in cgamex86.dll
=================
*/
static void CG_DrawWarmupCountdown(int gt) {
	int w;
	const char* s;
	const char* header = "";
	clientInfo_t* ci1;
	clientInfo_t* ci2;
	int i;

	// Gametype header line
	if (gt == GT_DUEL) {
		// Duel: show "X vs Y"
		ci1 = NULL;
		ci2 = NULL;
		for (i = 0; i < cgs.maxclients; i++) {
			if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_FREE) {
				if (!ci1) {
					ci1 = &cgs.clientinfo[i];
				} else {
					ci2 = &cgs.clientinfo[i];
				}
			}
		}
		if (ci1 && ci2) {
			header = va("%s vs %s", ci1->name, ci2->name);
		}
	} else if (gt >= 0 && gt <= GT_RR) {
		// Round-based gametypes with active rounds: "Round Begins in"
		switch (gt) {
		case GT_CA:
		case GT_FREEZE:
		case GT_AD:
		case GT_RR:
			header = "Round Begins in";
			break;
		default:
			header = gametypeDisplayNames[gt];
			break;
		}
	} else {
		header = "Unknown Gametype";
	}

	w = CG_Text_Width(header, 0.6f, 0);
	CG_Text_Paint(320 - w / 2, 90, 0.6f, colorWhite, header, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);

	// Countdown number
	switch (gt) {
	case GT_CA:
	case GT_FREEZE:
	case GT_AD:
	case GT_RR:
		// Round-based: bare number
		s = va("%i", cg.warmupCount);
		break;
	default:
		s = va("Starts in: %i", cg.warmupCount);
		break;
	}

	w = CG_Text_Width(s, 0.45f, 0);
	CG_Text_Paint(320 - w / 2, 125, 0.45f, colorWhite, s, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
}

/*
=================
CG_DrawWarmupMessages

[QL] Full warmup display: countdown tick sounds, PRE_GAME waiting messages,
and countdown display. Replaces the old CG_DrawWarmup.
Binary: CG_DrawWarmupMessages in cgamex86.dll
=================
*/
static void CG_DrawWarmup(void) {
	int sec;
	int i;
	int w;
	int gt;
	int teamCounts[4];
	const char* line1;
	const char* line2;

	// Handle countdown tick and sounds
	if (cg.warmup == 0) {
		if (cg.warmupCount == -1) {
			return;  // no warmup active
		}
		// warmup just ended, fall through to clear
	} else if (cg.warmup == -1) {
		cg.warmupCount = -1;
	} else if (cg.warmup > 0) {
		// Countdown active - calculate seconds remaining
		sec = (cg.warmup - cg.time + 1000) / 1000;
		if (sec < 0) {
			sec = 0;
		}

		if (sec != cg.warmupCount) {
			cg.warmupCount = sec;
			// Play countdown sounds
			switch (sec) {
			case 1:
				trap_S_StartLocalSound(cgs.media.count1Sound, CHAN_ANNOUNCER);
				break;
			case 2:
				trap_S_StartLocalSound(cgs.media.count2Sound, CHAN_ANNOUNCER);
				break;
			case 3:
				trap_S_StartLocalSound(cgs.media.count3Sound, CHAN_ANNOUNCER);
				break;
			}
		}
	}

	if (cg.showScores) {
		return;
	}

	// Determine effective gametype for display
	gt = (cg.warmupGametype >= 0) ? cg.warmupGametype : cgs.gametype;

	// Countdown display
	if (cg.warmupCount > 0) {
		CG_DrawWarmupCountdown(gt);
		return;
	}

	// PRE_GAME display (warmup == -1)
	if (cg.warmup >= 0) {
		return;
	}

	// Count players per team
	memset(teamCounts, 0, sizeof(teamCounts));
	for (i = 0; i < cgs.maxclients; i++) {
		if (cgs.clientinfo[i].infoValid) {
			teamCounts[cgs.clientinfo[i].team]++;
		}
	}

	line1 = NULL;
	line2 = NULL;

	if (gt == GT_RR) {
		// Red Rover
		if (cgs.teamSizeMin > 0 &&
			(teamCounts[TEAM_RED] < cgs.teamSizeMin || teamCounts[TEAM_BLUE] < cgs.teamSizeMin)) {
			line1 = "The match will begin";
			line2 = "when more players join.";
		} else {
			line1 = "The match will begin";
			line2 = "when more players are ready.";
		}
	} else if (gt < GT_TEAM) {
		// FFA / Duel / Race
		if (gt == GT_DUEL && teamCounts[TEAM_FREE] > 2) {
			line1 = "The match will begin when";
			line2 = "fewer players are in the match.";
		} else if (teamCounts[TEAM_FREE] < 2) {
			line1 = "The match will begin";
			line2 = "when more players join.";
		} else {
			line1 = "The match will begin";
			line2 = "when more players are ready.";
		}
	} else {
		// Team games (TDM, CA, CTF, 1FCTF, Overload, Harvester, FT, Dom, AD)
		if (teamCounts[TEAM_RED] < cgs.teamSizeMin) {
			if (teamCounts[TEAM_BLUE] < cgs.teamSizeMin) {
				// Both teams need players
				line1 = "Waiting for more players.";
				line2 = va("The match requires %i player%s per team.",
					cgs.teamSizeMin, cgs.teamSizeMin != 1 ? "s" : "");
			} else {
				// Only red needs players
				int need = cgs.teamSizeMin - teamCounts[TEAM_RED];
				line1 = va("Waiting for %i more player%s", need, need != 1 ? "s" : "");
				line2 = va("to join the %s.", "Red Team");
			}
		} else if (teamCounts[TEAM_BLUE] < cgs.teamSizeMin) {
			// Only blue needs players
			int need = cgs.teamSizeMin - teamCounts[TEAM_BLUE];
			line1 = va("Waiting for %i more player%s", need, need != 1 ? "s" : "");
			line2 = va("to join the %s.", "Blue Team");
		} else if (cgs.teamForceBalance &&
				   (teamCounts[TEAM_RED] > teamCounts[TEAM_BLUE] + 1 ||
				    teamCounts[TEAM_BLUE] > teamCounts[TEAM_RED] + 1)) {
			line1 = "The teams must be balanced";
			line2 = "before the match can begin.";
		} else {
			line1 = "The match will begin";
			line2 = "when more players are ready.";
		}
	}

	if (line1) {
		w = CG_Text_Width(line1, 0.35f, 0);
		CG_Text_Paint(320 - w / 2, 88, 0.35f, colorWhite, line1, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}
	if (line2) {
		w = CG_Text_Width(line2, 0.35f, 0);
		CG_Text_Paint(320 - w / 2, 108, 0.35f, colorWhite, line2, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE);
	}
}

//==================================================================================
/*
=================
CG_DrawTimedMenus
=================
*/
void CG_DrawTimedMenus(void) {
	if (cg.voiceTime) {
		int t = cg.time - cg.voiceTime;
		if (t > 2500) {
			Menus_CloseByName("voiceMenu");
			trap_Cvar_Set("cl_conXOffset", "0");
			cg.voiceTime = 0;
		}
	}
}

/*
=================
CG_Draw2D
=================
*/
static void CG_Draw2D(stereoFrame_t stereoFrame) {
	if (cgs.orderPending && cg.time > cgs.orderTime) {
		CG_CheckOrderPending();
	}

	// if we are taking a levelshot for the menu, don't draw anything
	if (cg.levelShot) {
		return;
	}

	if (cg_draw2D.integer == 0) {
		return;
	}

	if (cg.snap->ps.pm_type == PM_INTERMISSION) {
		CG_DrawIntermission();
		return;
	}

	/*
		if (cg.cameraMode) {
			return;
		}
	*/
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ||
		((cgs.gametype == GT_CA || cgs.gametype == GT_AD) &&
		 cg.snap->ps.pm_type == PM_SPECTATOR)) {
		CG_SetWidescreen(WIDESCREEN_CENTER);
		CG_DrawSpectator();
		CG_SetWidescreen(WIDESCREEN_STRETCH);

		if (stereoFrame == STEREO_CENTER)
			CG_DrawCrosshair();

		CG_SetWidescreen(WIDESCREEN_CENTER);
		CG_DrawCrosshairNames();
		CG_SetWidescreen(WIDESCREEN_STRETCH);
	} else {
		// don't draw any status if dead or the scoreboard is being explicitly shown
		if (!cg.showScores && cg.snap->ps.stats[STAT_HEALTH] > 0) {
			if (cg_drawStatus.integer) {
				Menu_PaintAll();
				CG_DrawTimedMenus();
			}

			CG_SetWidescreen(WIDESCREEN_CENTER);
			CG_DrawAmmoWarning();
			CG_DrawProxWarning();
			CG_SetWidescreen(WIDESCREEN_STRETCH);

			if (stereoFrame == STEREO_CENTER)
				CG_DrawCrosshair();

			CG_SetWidescreen(WIDESCREEN_CENTER);
			CG_DrawCrosshairNames();
			CG_DrawWeaponBar();  // [QL] handles all modes (Left/Right/Centered/Classic/No)
			CG_DrawReward();
			CG_SetWidescreen(WIDESCREEN_STRETCH);
		}
	}

	CG_SetWidescreen(WIDESCREEN_LEFT);
	CG_DrawVote();
	CG_SetWidescreen(WIDESCREEN_RIGHT);
	CG_DrawTeamVote();
	CG_SetWidescreen(WIDESCREEN_RIGHT);
	CG_DrawLagometer();
	CG_SetWidescreen(WIDESCREEN_STRETCH);

	if (!cg_paused.integer) {
		CG_SetWidescreen(WIDESCREEN_RIGHT);
		CG_DrawUpperRight(stereoFrame);
		CG_SetWidescreen(WIDESCREEN_STRETCH);
	}

	CG_SetWidescreen(WIDESCREEN_CENTER);
	if (!CG_DrawFollow()) {
		CG_DrawWarmup();
	}
	CG_SetWidescreen(WIDESCREEN_STRETCH);

	// [QL] race timer is drawn by menu system via CG_DrawRaceTimes ownerdraw
	// (removed hardcoded duplicate here)

	// don't draw center string if scoreboard is up
	cg.scoreBoardShowing = CG_DrawScoreboard();
	if (!cg.scoreBoardShowing) {
		CG_SetWidescreen(WIDESCREEN_CENTER);
		CG_DrawCenterString();
		CG_SetWidescreen(WIDESCREEN_STRETCH);
	}
}

/*
=====================
CG_DrawActive

CG_DrawFullScreenColor

QL binary: draws a fullscreen vignette overlay when cg_vignette is enabled (vmCvar 0x10A63960)
=====================
*/
static void CG_DrawFullScreenColor(void) {
	if (cg_vignette.integer) {
		CG_DrawPic(0, 0, 640, 480, cgs.media.vignetteShader);
	}
}

/*
=====================
Perform all drawing needed to completely fill the screen
=====================
*/
void CG_DrawActive(stereoFrame_t stereoView) {
	// optionally draw the info screen instead
	if (!cg.snap) {
		CG_DrawInformation();
		return;
	}

	// optionally draw the tournement scoreboard instead
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR &&
		(cg.snap->ps.pm_flags & PMF_SCOREBOARD)) {
		CG_DrawTourneyScoreboard();
		return;
	}

	// QL binary: cg_zoomOutOnDeath.integer resets zoom on death/freeze/intermission (vmCvar 0x10A61200)
	if (((cg.predictedPlayerState.pm_type == PM_DEAD ||
	      cg.predictedPlayerState.pm_type == PM_FREEZE) && cg_zoomOutOnDeath.integer) ||
	    cg.predictedPlayerState.pm_type == PM_INTERMISSION) {
		cg.zoomed = qfalse;
	}

	// clear around the rendered view if sized down
	CG_TileClear();

	if (stereoView != STEREO_CENTER)
		CG_DrawCrosshair3D();

	// draw 3D view
	trap_R_RenderScene(&cg.refdef);

	// draw status bar and other floating elements
	CG_Draw2D(stereoView);

	// QL binary: vignette overlay drawn after HUD
	if (!cg.renderingThirdPerson) {
		CG_DrawFullScreenColor();
	}
}
