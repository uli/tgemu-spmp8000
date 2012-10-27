#include "text.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgame.h>

//#define CHINESE

static uint8_t *asc12_font = 0;
#ifdef CHINESE
static uint16_t *hzx12_font = 0;
#endif

int load_fonts(void)
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

#ifdef CHINESE
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
#endif
        return 0;
}

void free_fonts(void)
{
    free(asc12_font);
    asc12_font = 0;
#ifdef CHINESE
    free(hzx12_font);
    hzx12_font = 0;
#endif
}

int draw_character(uint32_t codepoint, int x, int y)
{
    int width = gDisplayDev->getWidth();
    uint16_t *fb = gDisplayDev->getShadowBuffer() + width * y + x;
    int i, j;
    if (codepoint < 256) {
render_asc:
        for (i = 0; i < 12; i++) {
            uint8_t line = asc12_font[codepoint * 12 + i];
            for (j = 0; j < 8; j++) {
                fb[j] = (line & 0x80) ? 0xffff : 0;
                line <<= 1;
            }
            fb += width;
        }
        return 8;
    }
#ifdef CHINESE
    /* XXX: need to convert from Unicode to Big5 first! */
    else if (codepoint >= 0x4e00 && codepoint < 0x10000) {
        codepoint -= 0x4e00;
        for (i = 0; i < 12; i++) {
            uint16_t line = hzx12_font[codepoint * 12 + i];
            line = (line >> 8) | (line << 8);
            for (j = 0; j < 16; j++) {
                fb[j] = (line & 0x8000) ? 0xffff : 0;
                line <<= 1;
            }
            fb += width;
        }
        return 16;
    }
#endif
    else {
        codepoint = 1;
        goto render_asc;
    }
}

int render_text(const char *t, int x, int y)
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
        x += draw_character(codepoint, x, y);
    }
    return x - old_x;
}