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
// console.c

#include "client.h"

int g_console_field_width = 119;

#define NUM_CON_TIMES 4

#define CON_TEXTSIZE 524288
typedef struct {
    qboolean initialized;

    short text[CON_TEXTSIZE];
    int current;  // line where next message will be printed
    int x;        // offset in current line for next print
    int display;  // bottom of console displays this line

    int linewidth;   // characters across screen
    int totallines;  // total lines in console scrollback

    float xadjust;  // for wide aspect screens

    float finalFrac;  // 0.0 to 1.0 lines of console to display

    int vislines;       // in scanlines
    float displayFrac;  // aproaches finalFrac at con_speed

    int times[NUM_CON_TIMES];  // cls.realtime time the line was generated
    // for transparent notify lines
    vec4_t color;
} console_t;

console_t con;

cvar_t* con_background;
cvar_t* con_height;
cvar_t* con_matchlimit;
cvar_t* con_noprint;
cvar_t* con_opacity;
cvar_t* con_scale;
cvar_t* con_speed;
cvar_t* con_timestamps;

// [QL] Console base character sizes (binary uses 12x24, not Q3's 8x16)
#define CON_CHARW 12
#define CON_CHARH 24

/*
================
Con_CharW / Con_CharH

Returns scaled console character dimensions based on con_scale.
Base size is CON_CHARW x CON_CHARH (12x24), matching the QL binary.
At con_scale 0.5 (default): 6x12.  At con_scale 1.0: 12x24.
================
*/
static int Con_CharW(void) {
    if (con_scale) {
        int w = (int)(CON_CHARW * con_scale->value);
        return w > 0 ? w : 1;
    }
    return CON_CHARW;
}

static int Con_CharH(void) {
    if (con_scale) {
        int h = (int)(CON_CHARH * con_scale->value);
        return h > 0 ? h : 1;
    }
    return CON_CHARH;
}

/*
================
Con_DrawCharScaled

Draws a character at native screen resolution with specified dimensions.
Like SCR_DrawSmallChar but with variable width/height.
================
*/
static void Con_DrawCharScaled(int x, int y, int w, int h, int ch) {
    int row, col;
    float frow, fcol;
    float size;

    ch &= 255;

    if (ch == ' ') {
        return;
    }

    if (y < -h) {
        return;
    }

    row = ch >> 4;
    col = ch & 15;

    frow = row * 0.0625;
    fcol = col * 0.0625;
    size = 0.0625;

    re.DrawStretchPic(x, y, w, h,
                      fcol, frow,
                      fcol + size, frow + size,
                      cls.charSetShader);
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f(void) {
    // Can't toggle the console when it's the only thing available
    if (clc.state == CA_DISCONNECTED && Key_GetCatcher() == KEYCATCH_CONSOLE) {
        return;
    }

    Field_Clear(&g_consoleField);

    g_consoleField.widthInChars = g_console_field_width;

    Con_ClearNotify();
    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_CONSOLE);
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f(void) {
    chat_playerNum = -1;
    chat_team = qfalse;
    Field_Clear(&chatField);
    chatField.widthInChars = 30;

    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_MESSAGE);
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f(void) {
    chat_playerNum = -1;
    chat_team = qtrue;
    Field_Clear(&chatField);
    chatField.widthInChars = 25;
    Key_SetCatcher(Key_GetCatcher() ^ KEYCATCH_MESSAGE);
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f(void) {
    int i;

    for (i = 0; i < CON_TEXTSIZE; i++) {
        con.text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
    }

    Con_Bottom();  // go to end
}

/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f(void) {
    int l, x, i;
    short* line;
    fileHandle_t f;
    int bufferlen;
    char* buffer;
    char filename[MAX_QPATH];

    if (Cmd_Argc() != 2) {
        Com_Printf("usage: condump <filename>\n");
        return;
    }

    Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
    COM_DefaultExtension(filename, sizeof(filename), ".txt");

    if (!COM_CompareExtension(filename, ".txt")) {
        Com_Printf("Con_Dump_f: Only the \".txt\" extension is supported by this command!\n");
        return;
    }

    f = FS_FOpenFileWrite(filename);
    if (!f) {
        Com_Printf("ERROR: couldn't open %s.\n", filename);
        return;
    }

    Com_Printf("Dumped console text to %s.\n", filename);

    // skip empty lines
    for (l = con.current - con.totallines + 1; l <= con.current; l++) {
        line = con.text + (l % con.totallines) * con.linewidth;
        for (x = 0; x < con.linewidth; x++)
            if ((line[x] & 0xff) != ' ')
                break;
        if (x != con.linewidth)
            break;
    }

#ifdef _WIN32
    bufferlen = con.linewidth + 3 * sizeof(char);
#else
    bufferlen = con.linewidth + 2 * sizeof(char);
#endif

    buffer = Hunk_AllocateTempMemory(bufferlen);

    // write the remaining lines
    buffer[bufferlen - 1] = 0;
    for (; l <= con.current; l++) {
        line = con.text + (l % con.totallines) * con.linewidth;
        for (i = 0; i < con.linewidth; i++)
            buffer[i] = line[i] & 0xff;
        for (x = con.linewidth - 1; x >= 0; x--) {
            if (buffer[x] == ' ')
                buffer[x] = 0;
            else
                break;
        }
#ifdef _WIN32
        Q_strcat(buffer, bufferlen, "\r\n");
#else
        Q_strcat(buffer, bufferlen, "\n");
#endif
        FS_Write(buffer, strlen(buffer), f);
    }

    Hunk_FreeTempMemory(buffer);
    FS_FCloseFile(f);
}

/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify(void) {
    int i;

    for (i = 0; i < NUM_CON_TIMES; i++) {
        con.times[i] = 0;
    }
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize(void) {
    int i, j, width, oldwidth, oldtotallines, numlines, numchars;
    short tbuf[CON_TEXTSIZE];

    // [QL] calculate linewidth from vidWidth and con_scale
    con.xadjust = 0;
    if (con_scale && cls.glconfig.vidWidth > 0) {
        width = (int)(cls.glconfig.vidWidth / (con_scale->value * CON_CHARW)) - 2;
    } else {
        width = cls.glconfig.vidWidth / CON_CHARW - 2;
    }
    if (width < 1) {
        width = 78;  // fallback before video init
    }

    if (width == con.linewidth)
        return;

    if (con.linewidth <= 0) {
        // first-time init: zero-fill the buffer
        con.linewidth = width;
        con.totallines = CON_TEXTSIZE / con.linewidth;
        for (i = 0; i < CON_TEXTSIZE; i++)
            con.text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
    } else {
        oldwidth = con.linewidth;
        con.linewidth = width;
        oldtotallines = con.totallines;
        con.totallines = CON_TEXTSIZE / con.linewidth;
        numlines = oldtotallines;

        if (con.totallines < numlines)
            numlines = con.totallines;

        numchars = oldwidth;

        if (con.linewidth < numchars)
            numchars = con.linewidth;

        Com_Memcpy(tbuf, con.text, CON_TEXTSIZE * sizeof(short));
        for (i = 0; i < CON_TEXTSIZE; i++)

            con.text[i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';

        for (i = 0; i < numlines; i++) {
            for (j = 0; j < numchars; j++) {
                con.text[(con.totallines - 1 - i) * con.linewidth + j] =
                    tbuf[((con.current - i + oldtotallines) %
                          oldtotallines) *
                             oldwidth +
                         j];
            }
        }

        Con_ClearNotify();
    }

    con.current = con.totallines - 1;
    con.display = con.current;
}

/*
==================
Cmd_CompleteTxtName
==================
*/
void Cmd_CompleteTxtName(char* args, int argNum) {
    if (argNum == 2) {
        Field_CompleteFilename("", "txt", qfalse, qtrue);
    }
}

/*
================
[QL] Con_Find_f - search console buffer for a substring
================
*/
static void Con_Find_f(void) {
    char* search;
    int l, x, i;
    short* line;
    char text[1024];
    int matchCount = 0;
    int matchLimit;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: find <string>\n");
        return;
    }

    search = Cmd_Argv(1);

    if (!con.initialized || !search[0]) {
        return;
    }

    matchLimit = (con_matchlimit && con_matchlimit->integer > 0) ? con_matchlimit->integer : 16;

    // skip empty lines at the start
    for (l = con.current - con.totallines + 1; l <= con.current; l++) {
        line = con.text + (l % con.totallines) * con.linewidth;
        for (x = 0; x < con.linewidth; x++) {
            if ((line[x] & 0xff) != ' ')
                break;
        }
        if (x != con.linewidth)
            break;
    }

    for (; l <= con.current; l++) {
        line = con.text + (l % con.totallines) * con.linewidth;

        // Extract text from this line
        x = 0;
        for (i = 0; i < con.linewidth && x < (int)sizeof(text) - 1; i++) {
            text[x++] = line[i] & 0xff;
        }
        text[x] = '\0';

        // Strip trailing spaces
        while (x > 0 && text[x - 1] == ' ') {
            text[--x] = '\0';
        }

        // [QL] Search with match limiting and output filtering
        if (text[0] && Q_stristr(text, search) &&
            !strstr(text, "\\find") && !strstr(text, "\\##")) {
            matchCount++;
            if (matchCount <= matchLimit) {
                if (matchCount == 1) {
                    Com_Printf("\\## MATCH LIST:\n");
                }
                Com_Printf("\\## %s\n", text);
            }
        }
    }

    if (matchCount == 0) {
        Com_Printf("%d matches found.\n", 0);
    } else if (matchCount >= matchLimit) {
        Com_Printf("%d matches found. (Displaying the first %d matches.)\n", matchCount, matchLimit);
    } else {
        Com_Printf("%d %s found.\n", matchCount, matchCount < 2 ? "match" : "matches");
    }
}

/*
================
Con_Init
================
*/
void Con_Init(void) {
    int i;

    con_background = Cvar_Get("con_background", "0", CVAR_ARCHIVE);
    Cvar_CheckRange(con_background, 0, 1, qtrue);

    con_height = Cvar_Get("con_height", "0.5", CVAR_ARCHIVE);
    Cvar_CheckRange(con_height, 0.1f, 1.0f, qfalse);

    con_matchlimit = Cvar_Get("con_matchlimit", "16", 0);

    con_noprint = Cvar_Get("con_noprint", "0", 0);

    con_opacity = Cvar_Get("con_opacity", "0.9", CVAR_ARCHIVE);
    Cvar_CheckRange(con_opacity, 0.1f, 1.0f, qfalse);

    con_scale = Cvar_Get("con_scale", "0.5", CVAR_ARCHIVE);
    Cvar_CheckRange(con_scale, 0.5f, 1.0f, qfalse);

    con_speed = Cvar_Get("con_speed", "3", CVAR_ARCHIVE);
    Cvar_CheckRange(con_speed, 0.1f, 1000.0f, qfalse);

    con_timestamps = Cvar_Get("con_timestamps", "0", CVAR_ARCHIVE);

    Field_Clear(&g_consoleField);
    g_consoleField.widthInChars = g_console_field_width;
    for (i = 0; i < COMMAND_HISTORY; i++) {
        Field_Clear(&historyEditLines[i]);
        historyEditLines[i].widthInChars = g_console_field_width;
    }
    CL_LoadConsoleHistory();

    Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);
    Cmd_AddCommand("messagemode", Con_MessageMode_f);
    Cmd_AddCommand("messagemode2", Con_MessageMode2_f);
    Cmd_AddCommand("clear", Con_Clear_f);
    Cmd_AddCommand("condump", Con_Dump_f);
    Cmd_SetCommandCompletionFunc("condump", Cmd_CompleteTxtName);

    // [QL] console search
    Cmd_AddCommand("find", Con_Find_f);
}

/*
================
Con_Shutdown
================
*/
void Con_Shutdown(void) {
    Cmd_RemoveCommand("toggleconsole");
    Cmd_RemoveCommand("messagemode");
    Cmd_RemoveCommand("messagemode2");
    Cmd_RemoveCommand("clear");
    Cmd_RemoveCommand("condump");
    Cmd_RemoveCommand("find");
}

/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed(qboolean skipnotify) {
    int i;

    // mark time for transparent overlay
    if (con.current >= 0) {
        if (skipnotify)
            con.times[con.current % NUM_CON_TIMES] = 0;
        else
            con.times[con.current % NUM_CON_TIMES] = cls.realtime;
    }

    con.x = 0;
    if (con.display == con.current)
        con.display++;
    con.current++;
    for (i = 0; i < con.linewidth; i++)
        con.text[(con.current % con.totallines) * con.linewidth + i] = (ColorIndex(COLOR_WHITE) << 8) | ' ';
}

/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void CL_ConsolePrint(char* txt) {
    int y, l;
    unsigned char c;
    unsigned short color;
    qboolean skipnotify = qfalse;  // NERVE - SMF
    int prev;                      // NERVE - SMF

    // TTimo - prefix for text that shows up in console but not in notify
    // backported from RTCW
    if (!Q_strncmp(txt, "[skipnotify]", 12)) {
        skipnotify = qtrue;
        txt += 12;
    }

    // [QL] con_noprint suppresses all console output
    if (con_noprint && con_noprint->integer) {
        return;
    }

    if (!con.initialized) {
        con.color[0] =
            con.color[1] =
                con.color[2] =
                    con.color[3] = 1.0f;
        con.linewidth = -1;
        Con_CheckResize();
        con.initialized = qtrue;
    }

    // [QL] prepend timestamp at start of line
    if (con_timestamps && con_timestamps->integer && con.x == 0) {
        char tsbuf[64];
        int time_ms;
        float time_sec;
        int minutes, seconds, millis;
        char *p;

        time_ms = cls.realtime;
        time_sec = time_ms / 1000.0f;
        minutes = (int)time_sec / 60;
        seconds = (int)(time_sec - minutes * 60.0f);
        millis = (int)((time_sec - minutes * 60.0f - seconds) * 1000.0f);
        Com_sprintf(tsbuf, sizeof(tsbuf), "[%d:%02d.%03d] ", minutes, seconds, millis);

        p = tsbuf;
        while (*p) {
            if (con.x >= con.linewidth) {
                Con_Linefeed(qfalse);
            }
            y = con.current % con.totallines;
            con.text[y * con.linewidth + con.x] = (ColorIndex(COLOR_WHITE) << 8) | *p;
            con.x++;
            p++;
        }
    }

    color = ColorIndex(COLOR_WHITE);

    while ((c = *((unsigned char*)txt)) != 0) {
        if (Q_IsColorString(txt)) {
            color = ColorIndex(*(txt + 1));
            txt += 2;
            continue;
        }

        // count word length
        for (l = 0; l < con.linewidth; l++) {
            if (txt[l] <= ' ') {
                break;
            }
        }

        // word wrap
        if (l != con.linewidth && (con.x + l >= con.linewidth)) {
            Con_Linefeed(skipnotify);
        }

        txt++;

        switch (c) {
            case '\n':
                Con_Linefeed(skipnotify);
                break;
            case '\r':
                con.x = 0;
                break;
            default:  // display character and advance
                y = con.current % con.totallines;
                con.text[y * con.linewidth + con.x] = (color << 8) | c;
                con.x++;
                if (con.x >= con.linewidth)
                    Con_Linefeed(skipnotify);
                break;
        }
    }

    // mark time for transparent overlay
    if (con.current >= 0) {
        // NERVE - SMF
        if (skipnotify) {
            prev = con.current % NUM_CON_TIMES - 1;
            if (prev < 0)
                prev = NUM_CON_TIMES - 1;
            con.times[prev] = 0;
        } else
            // -NERVE - SMF
            con.times[con.current % NUM_CON_TIMES] = cls.realtime;
    }
}

/*
==============================================================================

DRAWING

==============================================================================
*/

/*
================
Con_DrawInput

Draw the editline after a ] prompt
================
*/
void Con_DrawInput(void) {
    int y, xx;
    int charW = Con_CharW();
    int charH = Con_CharH();
    int len, drawLen, prestep, i;
    int cursorChar;
    char str[MAX_STRING_CHARS];
    const char *s;
    int currentColor;

    if (clc.state != CA_DISCONNECTED && !(Key_GetCatcher() & KEYCATCH_CONSOLE)) {
        return;
    }

    y = con.vislines - (charH * 2);

    re.SetColor(con.color);

    // draw ']' prompt
    Con_DrawCharScaled((int)(con.xadjust + charW), y, charW, charH, ']');

    // draw the input field text (inline, scaled)
    drawLen = g_consoleField.widthInChars - 1;
    len = strlen(g_consoleField.buffer);

    if (len <= drawLen) {
        prestep = 0;
    } else {
        if (g_consoleField.scroll + drawLen > len) {
            g_consoleField.scroll = len - drawLen;
            if (g_consoleField.scroll < 0) {
                g_consoleField.scroll = 0;
            }
        }
        prestep = g_consoleField.scroll;
    }

    if (prestep + drawLen > len) {
        drawLen = len - prestep;
    }

    if (drawLen >= MAX_STRING_CHARS) {
        drawLen = MAX_STRING_CHARS - 1;
    }

    Com_Memcpy(str, g_consoleField.buffer + prestep, drawLen);
    str[drawLen] = 0;

    // draw text characters
    s = str;
    xx = (int)(con.xadjust + 2 * charW);
    currentColor = ColorIndex(COLOR_WHITE);
    re.SetColor(g_color_table[currentColor]);

    while (*s) {
        if (Q_IsColorString(s)) {
            currentColor = ColorIndex(*(s + 1));
            re.SetColor(g_color_table[currentColor]);
            s += 2;
            continue;
        }
        Con_DrawCharScaled(xx, y, charW, charH, *s);
        xx += charW;
        s++;
    }

    // draw the cursor
    if ((int)(cls.realtime >> 8) & 1) {
        re.SetColor(NULL);
        return;  // off blink
    }

    cursorChar = key_overstrikeMode ? 11 : 10;

    i = drawLen - strlen(str);
    re.SetColor(con.color);
    Con_DrawCharScaled((int)(con.xadjust + (2 + g_consoleField.cursor - prestep - i) * charW),
                       y, charW, charH, cursorChar);

    re.SetColor(NULL);
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify(void) {
    int x, v;
    short* text;
    int i;
    int time;
    int skip;
    int currentColor;

    currentColor = 7;
    re.SetColor(g_color_table[currentColor]);

    v = 0;
    for (i = con.current - NUM_CON_TIMES + 1; i <= con.current; i++) {
        if (i < 0)
            continue;
        time = con.times[i % NUM_CON_TIMES];
        if (time == 0)
            continue;
        time = cls.realtime - time;
        if (time > 3000)
            continue;
        text = con.text + (i % con.totallines) * con.linewidth;

        if (cl.snap.ps.pm_type != PM_INTERMISSION && Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME)) {
            continue;
        }

        // [QL] notify text is not scaled by con_scale - always at base SMALLCHAR size
        for (x = 0; x < con.linewidth; x++) {
            if ((text[x] & 0xff) == ' ') {
                continue;
            }
            if (ColorIndexForNumber(text[x] >> 8) != currentColor) {
                currentColor = ColorIndexForNumber(text[x] >> 8);
                re.SetColor(g_color_table[currentColor]);
            }
            SCR_DrawSmallChar(cl_conXOffset->integer + con.xadjust + (x + 1) * SMALLCHAR_WIDTH, v, text[x] & 0xff);
        }

        v += SMALLCHAR_HEIGHT;
    }

    re.SetColor(NULL);

    if (Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME)) {
        return;
    }

    // draw the chat line
    if (Key_GetCatcher() & KEYCATCH_MESSAGE) {
        if (chat_team) {
            SCR_DrawBigString(8, v, "say_team:", 1.0f, qfalse);
            skip = 10;
        } else {
            SCR_DrawBigString(8, v, "say:", 1.0f, qfalse);
            skip = 5;
        }

        Field_BigDraw(&chatField, skip * BIGCHAR_WIDTH, v,
                      SCREEN_WIDTH - (skip + 1) * BIGCHAR_WIDTH, qtrue, qtrue);
    }
}

/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole(float frac) {
    int i, x, y;
    int rows;
    short* text;
    int row;
    int lines;
    int currentColor;
    vec4_t backgroundColor;
    vec4_t color;
    int charW = Con_CharW();
    int charH = Con_CharH();

    lines = cls.glconfig.vidHeight * frac;
    if (lines <= 0)
        return;

    if (lines > cls.glconfig.vidHeight)
        lines = cls.glconfig.vidHeight;

    // on wide screens, we will center the text
    con.xadjust = 0;
    SCR_AdjustFrom640(&con.xadjust, NULL, NULL, NULL);

    // console background
    y = frac * SCREEN_HEIGHT;
    if (y < 1) {
        y = 0;
    } else {
        if (con_background->integer) {
            // draw the background shader
            SCR_DrawPic(0, 0, SCREEN_WIDTH, y, cls.consoleShader);
        } else {
            // draw the background color
            backgroundColor[0] = 0;                   // red
            backgroundColor[1] = 0;                   // green
            backgroundColor[2] = 0;                   // blue
            backgroundColor[3] = con_opacity->value;   // alpha
            SCR_FillRect(0, 0, SCREEN_WIDTH, y, backgroundColor);
        }
    }

    // draw the border line
    color[0] = 1;
#ifdef _DEBUG
    color[1] = 1;
#else
    color[1] = 0;
#endif
    color[2] = 0;
    color[3] = 1;

    SCR_FillRect(0, y, SCREEN_WIDTH, 2, color);

    // draw the version number
#ifdef _DEBUG
    re.SetColor(g_color_table[ColorIndex(COLOR_YELLOW)]);
#else
    re.SetColor(g_color_table[ColorIndex(COLOR_RED)]);
#endif

    i = strlen(FULL_PRODUCT_VERSION);

    for (x = 0; x < i; x++) {
        Con_DrawCharScaled(cls.glconfig.vidWidth - (i - x + 1) * charW,
                           lines - charH, charW, charH, FULL_PRODUCT_VERSION[x]);
    }

    // draw the text
    con.vislines = lines;
    rows = (lines - charH) / charH;  // rows of text to draw

    y = lines - (charH * 3);

    // draw from the bottom up
    if (con.display != con.current) {
        // draw arrows to show the buffer is backscrolled
        re.SetColor(g_color_table[ColorIndex(COLOR_RED)]);
        for (x = 0; x < con.linewidth; x += 4)
            Con_DrawCharScaled(con.xadjust + (x + 1) * charW, y, charW, charH, '^');
        y -= charH;
        rows--;
    }

    row = con.display;

    if (con.x == 0) {
        row--;
    }

    currentColor = 7;
    re.SetColor(g_color_table[currentColor]);

    for (i = 0; i < rows; i++, y -= charH, row--) {
        if (row < 0)
            break;
        if (con.current - row >= con.totallines) {
            // past scrollback wrap point
            continue;
        }

        text = con.text + (row % con.totallines) * con.linewidth;

        for (x = 0; x < con.linewidth; x++) {
            if ((text[x] & 0xff) == ' ') {
                continue;
            }

            if (ColorIndexForNumber(text[x] >> 8) != currentColor) {
                currentColor = ColorIndexForNumber(text[x] >> 8);
                re.SetColor(g_color_table[currentColor]);
            }
            Con_DrawCharScaled(con.xadjust + (x + 1) * charW, y, charW, charH, text[x] & 0xff);
        }
    }

    // draw the input prompt, user text, and cursor if desired
    Con_DrawInput();

    re.SetColor(NULL);
}

/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole(void) {
    // check for console width changes from a vid mode change
    Con_CheckResize();

    // if disconnected, render console full screen
    if (clc.state == CA_DISCONNECTED) {
        if (!(Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME))) {
            Con_DrawSolidConsole(1.0);
            return;
        }
    }

    if (con.displayFrac) {
        Con_DrawSolidConsole(con.displayFrac);
    } else {
        // draw notify lines
        if (clc.state == CA_ACTIVE) {
            Con_DrawNotify();
        }
    }
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole(void) {
    // decide on the destination height of the console
    if (Key_GetCatcher() & KEYCATCH_CONSOLE) {
        con.finalFrac = con_height->value;
    } else {
        con.finalFrac = 0;
    }

    // scroll towards the destination height
    if (con.finalFrac < con.displayFrac) {
        con.displayFrac -= con_speed->value * cls.realFrametime * 0.001;
        if (con.finalFrac > con.displayFrac)
            con.displayFrac = con.finalFrac;

    } else if (con.finalFrac > con.displayFrac) {
        con.displayFrac += con_speed->value * cls.realFrametime * 0.001;
        if (con.finalFrac < con.displayFrac)
            con.displayFrac = con.finalFrac;
    }
}

void Con_PageUp(void) {
    con.display -= 2;
    if (con.current - con.display >= con.totallines) {
        con.display = con.current - con.totallines + 1;
    }
}

void Con_PageDown(void) {
    con.display += 2;
    if (con.display > con.current) {
        con.display = con.current;
    }
}

void Con_Top(void) {
    con.display = con.totallines;
    if (con.current - con.display >= con.totallines) {
        con.display = con.current - con.totallines + 1;
    }
}

void Con_Bottom(void) {
    con.display = con.current;
}

void Con_Close(void) {
    if (!com_cl_running->integer) {
        return;
    }
    Field_Clear(&g_consoleField);
    Con_ClearNotify();
    Key_SetCatcher(Key_GetCatcher() & ~KEYCATCH_CONSOLE);
    con.finalFrac = 0;  // none visible
    con.displayFrac = 0;
}
