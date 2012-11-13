/* text.c - bitmap font rendering routines; part of the TGEmu SPMP port
 *
 * Copyright (C) 2012, Ulrich Hecht <ulrich.hecht@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */


#include "text.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgame.h>
#include <string.h>
#include <stdio.h>

#define CHINESE

static uint8_t *asc12_font = 0;
static uint8_t *asc16_font = 0;
#ifdef CHINESE
static uint16_t *hzx12_font = 0;
static uint16_t *hzx16_font = 0;
#include "hzktable.c"
#endif

struct sunplus_font {
    uint8_t _pad1[0x10];
    uint32_t glyph_start;
    uint32_t _pad2;
    uint32_t glyph_offset;
    uint32_t _pad3[2];
    uint32_t glyph_bytes;
};
FILE *songti_font = 0;
int8_t *songti_width = 0;
struct sunplus_font sp;

int text_load_fonts(void)
{
    int fd = open("/Rom/mw/fonts/CHINESE/ASC12", O_RDONLY);
    if (!fd) {
        return -1;
    }
    struct stat st;
    fstat(fd, &st);
    asc12_font = malloc(st.st_size);
    if (!asc12_font || read(fd, asc12_font, st.st_size) != st.st_size) {
        return -2;
    }
    close(fd);

    fd = open("/Rom/mw/fonts/CHINESE/ASC16", O_RDONLY);
    if (!fd) {
        return -1;
    }
    fstat(fd, &st);
    asc16_font = malloc(st.st_size);
    if (!asc16_font || read(fd, asc16_font, st.st_size) != st.st_size) {
        return -2;
    }
    close(fd);

#ifdef CHINESE
    /* XXX: Don't load these fonts until HZX font face is requested. */
    fd = open("/Rom/mw/fonts/CHINESE/HZX12", O_RDONLY);
    if (!fd) {
        return -3;
    }
    fstat(fd, &st);
    hzx12_font = malloc(st.st_size);
    if (!hzx12_font || read(fd, hzx12_font, st.st_size) != st.st_size) {
        return -4;
    }
    close(fd);

    fd = open("/Rom/mw/fonts/CHINESE/HZX16", O_RDONLY);
    if (!fd) {
        return -3;
    }
    fstat(fd, &st);
    hzx16_font = malloc(st.st_size);
    if (!hzx16_font || read(fd, hzx16_font, st.st_size) != st.st_size) {
        return -4;
    }
    close(fd);
#endif
    songti_font = fopen("/Rom/mw/fonts/SUNPLUS/SONGTI.FONT", "r");
    if (!songti_font)
        return -5;
    if (fread(&sp, sizeof(sp), 1, songti_font) != 1) {
        fclose(songti_font);
        songti_font = 0;
        return -6;
    }
    fd = open("/Rom/mw/fonts/SUNPLUS/SONGTI.WID", O_RDONLY);
    if (!fd)
        return -7;
    fstat(fd, &st);
    songti_width = malloc(st.st_size);
    if (!songti_width || read(fd, songti_width, st.st_size) != st.st_size)
        return -8;
    close(fd);

    return 0;
}

void text_free_fonts(void)
{
    if (asc12_font)
        free(asc12_font);
    asc12_font = 0;
    if (asc16_font)
        free(asc16_font);
    asc16_font = 0;
#ifdef CHINESE
    if (hzx12_font)
        free(hzx12_font);
    hzx12_font = 0;
    if (hzx16_font)
        free(hzx16_font);
    hzx16_font = 0;
#endif
    if (songti_font)
        fclose(songti_font);
    songti_font = 0;
    if (songti_width)
        free(songti_width);
    songti_width = 0;
}

int font_size = FONT_SIZE_12;
int font_face = FONT_FACE_SONGTI;

void text_set_font_size(int fs)
{
    font_size = fs;
}
int text_get_font_size(void)
{
    return font_size;
}

void text_set_font_face(int face)
{
    font_face = face;
}

int text_draw_character(uint32_t codepoint, int x, int y)
{
    return text_draw_character_ex(gDisplayDev->getShadowBuffer(),
                             gDisplayDev->getWidth(), codepoint, x, y);
}

int text_draw_character_ex(uint16_t *buf, int width, uint32_t codepoint, int x, int y)
{
    uint16_t *fb = buf + width * y + x;
    int i, j;
    uint8_t *asc_font = 0;
    uint16_t *hzx_font;
    uint16_t sunplus_char[sp.glyph_bytes / 2];
    int hzx_double_width = 0;
    int char_width;
    
    if (codepoint >= 0x4e00 && codepoint < 0x10000) {
        hzx_double_width = 1;
        if (font_size == FONT_SIZE_12 || font_face == FONT_FACE_HZX) {
            /* convert unicode to Big5 codepoint */
            for (i = 0; i < (int)sizeof(hzk2uni) / 2; i++) {
                if (hzk2uni[i] == codepoint) {
                    codepoint = i;
                    hzx_double_width = 1;
                    break;
                }
            }
            if (i == sizeof(hzk2uni) / 2) {
                codepoint = 1;
                hzx_double_width = 0;
            }
        }
    }

    switch (font_size) {
        case FONT_SIZE_12:
            asc_font = &asc12_font[codepoint * font_size];
            hzx_font = &hzx12_font[codepoint * font_size];
            if (hzx_double_width)
                char_width = 16;
            else
                char_width = 8;
            break;
        case FONT_SIZE_16:
            if (font_face == FONT_FACE_HZX) {
                asc_font = &asc16_font[codepoint * font_size];
                hzx_font = &hzx16_font[codepoint * font_size];
                if (hzx_double_width)
                    char_width = 16;
                else
                    char_width = 8;
            }
            else {
                fseek(songti_font, (codepoint - sp.glyph_offset) * sp.glyph_bytes + sp.glyph_start, SEEK_SET);
                fread(sunplus_char, sp.glyph_bytes, 1, songti_font);
                hzx_font = (uint16_t *)sunplus_char;
                char_width = songti_width[codepoint];
            }
            break;
        default:
            return -1;
    }

    if ((font_face == FONT_FACE_HZX || font_size == FONT_SIZE_12) && !hzx_double_width) {
        for (i = 0; i < font_size; i++) {
            uint8_t line = asc_font[i];
            for (j = 0; j < char_width; j++) {
                fb[j] = (line & 0x80) ? 0xffff : 0;
                line <<= 1;
            }
            fb += width;
        }
    }
#ifdef CHINESE
    else {
        for (i = 0; i < font_size; i++) {
            uint16_t line = hzx_font[i];
            line = (line >> 8) | (line << 8);
            for (j = 0; j < char_width; j++) {
                fb[j] = (line & 0x8000) ? 0xffff : 0;
                line <<= 1;
            }
            fb += width;
        }
    }
#endif
    return char_width;
}

int text_render(const char *t, int x, int y)
{
    return text_render_ex(gDisplayDev->getShadowBuffer(), gDisplayDev->getWidth(), t, x, y);
}

int text_render_centered(const char *t, int y)
{
    /* XXX: doesn't work correctly with double-width characters */
    return text_render(t, (gDisplayDev->getWidth() - strlen(t) * 8) / 2, y);
}

int text_render_ex(uint16_t *buf, int width, const char *t, int x, int y)
{
    int old_x = x;
    const uint8_t *text = (uint8_t *)t;
    while (*text) {
        uint32_t codepoint;
        if (*text >= 0x80) {
            if ((*text & 0x40) == 0) {
                /* invalid UTF-8 */
                text++;
                continue;
            }
            uint8_t first = text[0];
            uint8_t bytes = 0;
            while (first & 0x80) {
                bytes++;
                first <<= 1;
            }
            if (bytes > 6) {
                /* invalid UTF-8 */
                text++;
                continue;
            }
            first >>= bytes;
            int i;
            codepoint = first;
            for (i = 1; i < bytes; i++) {
                codepoint = (codepoint << 6) | (text[i] & 0x3f);
            }
            text += bytes;
        }
        else {
            codepoint = *text;
            text++;
        }
        x += text_draw_character_ex(buf, width, codepoint, x, y);
        if (x >= width - 8)
            break;
    }
    return x - old_x;
}
