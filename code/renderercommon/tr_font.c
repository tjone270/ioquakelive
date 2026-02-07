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
// tr_font.c
//
// Font rendering using FreeType 2.x for TrueType fonts.

#include "tr_common.h"
#include "../qcommon/qcommon.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H
#include FT_SYSTEM_H
#include FT_IMAGE_H
#include FT_OUTLINE_H

#define _FLOOR(x) ((x) & -64)
#define _CEIL(x) (((x) + 63) & -64)
#define _TRUNC(x) ((x) >> 6)

FT_Library ftLibrary = NULL;

#define MAX_FONTS 16  // [QL] raised from 6 - QL menus register more fonts
static int registeredFontCount = 0;
static fontInfo_t registeredFont[MAX_FONTS];

void R_GetGlyphInfo(FT_GlyphSlot glyph, int* left, int* right, int* width, int* top, int* bottom, int* height, int* pitch) {
    *left = _FLOOR(glyph->metrics.horiBearingX);
    *right = _CEIL(glyph->metrics.horiBearingX + glyph->metrics.width);
    *width = _TRUNC(*right - *left);

    *top = _CEIL(glyph->metrics.horiBearingY);
    *bottom = _FLOOR(glyph->metrics.horiBearingY - glyph->metrics.height);
    *height = _TRUNC(*top - *bottom);
    *pitch = (qtrue ? (*width + 3) & -4 : (*width + 7) >> 3);
}

FT_Bitmap* R_RenderGlyph(FT_GlyphSlot glyph, glyphInfo_t* glyphOut) {
    FT_Bitmap* bit2;
    int left, right, width, top, bottom, height, pitch, size;

    R_GetGlyphInfo(glyph, &left, &right, &width, &top, &bottom, &height, &pitch);

    if (glyph->format == ft_glyph_format_outline) {
        size = pitch * height;

        bit2 = ri.Malloc(sizeof(FT_Bitmap));

        bit2->width = width;
        bit2->rows = height;
        bit2->pitch = pitch;
        bit2->pixel_mode = ft_pixel_mode_grays;
        bit2->buffer = ri.Malloc(pitch * height);
        bit2->num_grays = 256;

        Com_Memset(bit2->buffer, 0, size);

        FT_Outline_Translate(&glyph->outline, -left, -bottom);

        FT_Outline_Get_Bitmap(ftLibrary, &glyph->outline, bit2);

        glyphOut->height = height;
        glyphOut->pitch = pitch;
        glyphOut->top = (glyph->metrics.horiBearingY >> 6) + 1;
        glyphOut->bottom = bottom;

        return bit2;
    } else {
        ri.Printf(PRINT_ALL, "Non-outline fonts are not supported\n");
    }
    return NULL;
}

static void WriteTGA(char* filename, byte* data, int width, int height) {
    byte* buffer;
    int i, c;
    int row;
    unsigned char* flip;
    unsigned char *src, *dst;

    buffer = ri.Malloc(width * height * 4 + 18);
    Com_Memset(buffer, 0, 18);
    buffer[2] = 2;  // uncompressed type
    buffer[12] = width & 255;
    buffer[13] = width >> 8;
    buffer[14] = height & 255;
    buffer[15] = height >> 8;
    buffer[16] = 32;  // pixel size

    // swap rgb to bgr
    c = 18 + width * height * 4;
    for (i = 18; i < c; i += 4) {
        buffer[i] = data[i - 18 + 2];      // blue
        buffer[i + 1] = data[i - 18 + 1];  // green
        buffer[i + 2] = data[i - 18 + 0];  // red
        buffer[i + 3] = data[i - 18 + 3];  // alpha
    }

    // flip upside down
    flip = (unsigned char*)ri.Malloc(width * 4);
    for (row = 0; row < height / 2; row++) {
        src = buffer + 18 + row * 4 * width;
        dst = buffer + 18 + (height - row - 1) * 4 * width;

        Com_Memcpy(flip, src, width * 4);
        Com_Memcpy(src, dst, width * 4);
        Com_Memcpy(dst, flip, width * 4);
    }
    ri.Free(flip);

    ri.FS_WriteFile(filename, buffer, c);

    ri.Free(buffer);
}

static glyphInfo_t* RE_ConstructGlyphInfo(unsigned char* imageOut, int* xOut, int* yOut, int* maxHeight, FT_Face face, const unsigned char c, qboolean calcHeight) {
    int i;
    static glyphInfo_t glyph;
    unsigned char *src, *dst;
    float scaled_width, scaled_height;
    FT_Bitmap* bitmap = NULL;

    Com_Memset(&glyph, 0, sizeof(glyphInfo_t));
    // make sure everything is here
    if (face != NULL) {
        FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FT_LOAD_DEFAULT);
        bitmap = R_RenderGlyph(face->glyph, &glyph);
        if (bitmap) {
            glyph.xSkip = (face->glyph->metrics.horiAdvance >> 6) + 1;
            glyph.left = face->glyph->metrics.horiBearingX >> 6;
        } else {
            return &glyph;
        }

        if (glyph.height > *maxHeight) {
            *maxHeight = glyph.height;
        }

        if (calcHeight) {
            ri.Free(bitmap->buffer);
            ri.Free(bitmap);
            return &glyph;
        }

        scaled_width = glyph.pitch;
        scaled_height = glyph.height;

        // we need to make sure we fit
        if (*xOut + scaled_width + 1 >= 255) {
            *xOut = 0;
            *yOut += *maxHeight + 1;
        }

        if (*yOut + *maxHeight + 1 >= 255) {
            *yOut = -1;
            *xOut = -1;
            ri.Free(bitmap->buffer);
            ri.Free(bitmap);
            return &glyph;
        }

        src = bitmap->buffer;
        dst = imageOut + (*yOut * 256) + *xOut;

        if (bitmap->pixel_mode == ft_pixel_mode_mono) {
            for (i = 0; i < glyph.height; i++) {
                int j;
                unsigned char* _src = src;
                unsigned char* _dst = dst;
                unsigned char mask = 0x80;
                unsigned char val = *_src;
                for (j = 0; j < glyph.pitch; j++) {
                    if (mask == 0x80) {
                        val = *_src++;
                    }
                    if (val & mask) {
                        *_dst = 0xff;
                    }
                    mask >>= 1;

                    if (mask == 0) {
                        mask = 0x80;
                    }
                    _dst++;
                }

                src += glyph.pitch;
                dst += 256;
            }
        } else {
            for (i = 0; i < glyph.height; i++) {
                Com_Memcpy(dst, src, glyph.pitch);
                src += glyph.pitch;
                dst += 256;
            }
        }

        // we now have an 8 bit per pixel grey scale bitmap
        // that is width wide and pf->ftSize->metrics.y_ppem tall

        glyph.imageHeight = scaled_height;
        glyph.imageWidth = scaled_width;
        glyph.s = (float)*xOut / 256;
        glyph.t = (float)*yOut / 256;
        glyph.s2 = glyph.s + (float)scaled_width / 256;
        glyph.t2 = glyph.t + (float)scaled_height / 256;

        *xOut += scaled_width + 1;

        ri.Free(bitmap->buffer);
        ri.Free(bitmap);
    }

    return &glyph;
}

void RE_RegisterFont(const char* fontName, int pointSize, fontInfo_t* font) {
    FT_Face face;
    int j, k, xOut, yOut, lastStart, imageNumber;
    int scaledSize, newSize, maxHeight, left;
    unsigned char *out, *imageBuff;
    glyphInfo_t* glyph;
    image_t* image;
    qhandle_t h;
    float max;
    float dpi = 72;
    float glyphScale;
    void* faceData;
    int i, len;
    char name[1024];

    if (!fontName) {
        ri.Printf(PRINT_ALL, "RE_RegisterFont: called with empty name\n");
        return;
    }

    if (pointSize <= 0) {
        pointSize = 12;
    }

    R_IssuePendingRenderCommands();

    if (registeredFontCount >= MAX_FONTS) {
        ri.Printf(PRINT_WARNING, "RE_RegisterFont: Too many fonts registered already.\n");
        return;
    }

    Com_sprintf(name, sizeof(name), "fonts/%s_%i", fontName, pointSize);
    for (i = 0; i < registeredFontCount; i++) {
        if (Q_stricmp(name, registeredFont[i].name) == 0) {
            Com_Memcpy(font, &registeredFont[i], sizeof(fontInfo_t));
            return;
        }
    }

    if (ftLibrary == NULL) {
        ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType not initialized.\n");
        return;
    }

    len = ri.FS_ReadFile(fontName, &faceData);
    if (len <= 0) {
        ri.Printf(PRINT_WARNING, "RE_RegisterFont: Unable to read font file '%s'\n", fontName);
        return;
    }

    // allocate on the stack first in case we fail
    if (FT_New_Memory_Face(ftLibrary, faceData, len, 0, &face)) {
        ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType, unable to allocate new face.\n");
        return;
    }

    if (FT_Set_Char_Size(face, pointSize << 6, pointSize << 6, dpi, dpi)) {
        ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType, unable to set face char size.\n");
        return;
    }

    // make a 256x256 image buffer, once it is full, register it, clean it and keep going
    // until all glyphs are rendered

    out = ri.Malloc(256 * 256);
    if (out == NULL) {
        ri.Printf(PRINT_WARNING, "RE_RegisterFont: ri.Malloc failure during output image creation.\n");
        return;
    }
    Com_Memset(out, 0, 256 * 256);

    maxHeight = 0;

    for (i = GLYPH_START; i <= GLYPH_END; i++) {
        RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qtrue);
    }

    xOut = 0;
    yOut = 0;
    i = GLYPH_START;
    lastStart = i;
    imageNumber = 0;

    while (i <= GLYPH_END + 1) {
        if (i == GLYPH_END + 1) {
            // upload/save current image buffer
            xOut = yOut = -1;
        } else {
            glyph = RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qfalse);
        }

        if (xOut == -1 || yOut == -1) {
            // ran out of room
            // we need to create an image from the bitmap, set all the handles in the glyphs to this point

            scaledSize = 256 * 256;
            newSize = scaledSize * 4;
            imageBuff = ri.Malloc(newSize);
            left = 0;
            max = 0;
            for (k = 0; k < (scaledSize); k++) {
                if (max < out[k]) {
                    max = out[k];
                }
            }

            if (max > 0) {
                max = 255 / max;
            }

            for (k = 0; k < (scaledSize); k++) {
                imageBuff[left++] = 255;
                imageBuff[left++] = 255;
                imageBuff[left++] = 255;

                imageBuff[left++] = ((float)out[k] * max);
            }

            Com_sprintf(name, sizeof(name), "fonts/fontImage_%s_%i_%i.tga", fontName, imageNumber++, pointSize);
            if (r_saveFontData->integer) {
                WriteTGA(name, imageBuff, 256, 256);
            }

            image = R_CreateImage(name, imageBuff, 256, 256, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE, 0);
            h = RE_RegisterShaderFromImage(name, LIGHTMAP_2D, image, qfalse);
            for (j = lastStart; j < i; j++) {
                font->glyphs[j].glyph = h;
                Q_strncpyz(font->glyphs[j].shaderName, name, sizeof(font->glyphs[j].shaderName));
            }
            lastStart = i;
            Com_Memset(out, 0, 256 * 256);
            xOut = 0;
            yOut = 0;
            ri.Free(imageBuff);
            if (i == GLYPH_END + 1)
                i++;
        } else {
            Com_Memcpy(&font->glyphs[i], glyph, sizeof(glyphInfo_t));
            i++;
        }
    }

    // change the scale to be relative to 1 based on 72 dpi ( so dpi of 144 means a scale of .5 )
    glyphScale = 72.0f / dpi;

    // we also need to adjust the scale based on point size relative to 48 points as the ui scaling is based on a 48 point font
    glyphScale *= 48.0f / pointSize;

    registeredFont[registeredFontCount].glyphScale = glyphScale;
    font->glyphScale = glyphScale;
    Com_sprintf(font->name, sizeof(font->name), "fonts/%s_%i", fontName, pointSize);
    Com_Memcpy(&registeredFont[registeredFontCount++], font, sizeof(fontInfo_t));

    ri.Free(out);

    ri.FS_FreeFile(faceData);
}

void R_InitFreeType(void) {
    if (FT_Init_FreeType(&ftLibrary)) {
        ri.Printf(PRINT_WARNING, "R_InitFreeType: Unable to initialize FreeType.\n");
    }
    registeredFontCount = 0;
}

void R_DoneFreeType(void) {
    if (ftLibrary) {
        FT_Done_FreeType(ftLibrary);
        ftLibrary = NULL;
    }
    registeredFontCount = 0;
}
