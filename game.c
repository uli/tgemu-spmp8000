/* game.c - part of the TGEmu SPMP port
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

#include <stdio.h>
#include <string.h>

#include "libgame.h"
#include "shared.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <text.h>
#include "ui.h"

#include "pc_engine.h"
#include "tgemu_logo.h"

#define fs_fprintf(fd, x...) { \
  char buf[256]; int _res; \
  sprintf(buf, x); \
  NativeGE_fsWrite(fd, buf, strlen(buf), &_res); \
}

#define MAX_FRAMESKIP 8
#define SHOW_KEYS

emu_sound_params_t sp;
#define SOUNDBUF_FRAMES 16
#define SOUND_CHANNELS 2
#define SOUND_RATE 44010
#define SOUND_BUFFER_SIZE (SOUND_RATE / 60)
uint16_t sound_ring[SOUND_BUFFER_SIZE * SOUND_CHANNELS * SOUNDBUF_FRAMES];
int sound_ring_read = 0;
int sound_ring_write = 0;
void sync_sound(void)
{
    if (sound_ring_write >= sound_ring_read + 2 ||
        (sound_ring_write == 0 && sound_ring_read == SOUNDBUF_FRAMES - 2)) {
        sp.buf = (uint8_t *)&sound_ring[sound_ring_read * SOUND_BUFFER_SIZE * SOUND_CHANNELS];
        /* two frames, two channels, two bytes per sample */
        sp.buf_size = SOUND_BUFFER_SIZE * 2 * SOUND_CHANNELS * 2;
        /* emuIfSoundPlay() can block, so we must exclude it from the
           calculated frame time used to tune frameskip. */
        emuIfSoundPlay(&sp);
        sound_ring_read = (sound_ring_read + 2) % SOUNDBUF_FRAMES;
    }
}

void update_sound(void)
{
    if (!snd.enabled)
        return;
    int i;
    for (i = 0; i < SOUND_BUFFER_SIZE; i++) {
        int pos = i * SOUND_CHANNELS + sound_ring_write * snd.buffer_size * SOUND_CHANNELS;
        sound_ring[pos] = snd.buffer[0][i];
        sound_ring[pos + 1] = snd.buffer[1][i];
    }
    sound_ring_write = (sound_ring_write + 1) % SOUNDBUF_FRAMES;
    sync_sound();
}

emu_graph_params_t gp;
#define VISIBLEWIDTH 512	/* visible horizontal size of source bitmap */
#define BMWIDTH (32 + VISIBLEWIDTH + 32)	/* total horizontal size of source bitmap */

int show_timing = 0;
int widescreen = 1;

uint32_t nkeys;
emu_keymap_t keymap;

void update_input(void)
{
    static int show_timing_triggered = 0;
    static int widescreen_triggered = 0;
    static int sound_triggered = 0;

    input.pad[0] = 0;

    ge_key_data_t keys;
    NativeGE_getKeyInput4Ntv(&keys);
    
    nkeys = emuIfKeyGetInput(&keymap);
    if (nkeys & keymap.scancode[EMU_KEY_UP])
        input.pad[0] |= INPUT_UP;
    if (nkeys & keymap.scancode[EMU_KEY_DOWN])
        input.pad[0] |= INPUT_DOWN;
    if (nkeys & keymap.scancode[EMU_KEY_LEFT])
        input.pad[0] |= INPUT_LEFT;
    if (nkeys & keymap.scancode[EMU_KEY_RIGHT])
        input.pad[0] |= INPUT_RIGHT;
    if (nkeys & keymap.scancode[EMU_KEY_X])
        input.pad[0] |= INPUT_B2;
    if (nkeys & keymap.scancode[EMU_KEY_O])
        input.pad[0] |= INPUT_B1;
    if (nkeys & keymap.scancode[EMU_KEY_START])
        input.pad[0] |= INPUT_RUN;
    if (nkeys & keymap.scancode[EMU_KEY_SELECT])
        input.pad[0] |= INPUT_SELECT;
    if (nkeys & keymap.scancode[EMU_KEY_START])
        input.pad[0] |= INPUT_RUN;
    if (nkeys & keymap.scancode[EMU_KEY_R]) {
        if (!show_timing_triggered) {
            show_timing_triggered = 1;
            show_timing = !show_timing;
        }
    }
    else {
        show_timing_triggered = 0;
    }
    if (nkeys & keymap.scancode[EMU_KEY_L]) {
        if (!widescreen_triggered) {
            widescreen_triggered = 1;
            widescreen = !widescreen;
            /* Widescreen mode is the "normal" mode of rendering the
               screen in which the visible graphics are blitted as
               they are to the full size of the screen. To achieve a
               pillarboxed rendering in the original aspect ratio we
               make the source bitmap wider, have the emulator render
               into the middle of this bitmap, and then blit the
               entire bitmap to the screen. */
            if (widescreen) {
                bitmap.pitch = (bitmap.width * bitmap.granularity);
                bitmap.data = (uint8 *)gp.pixels;
                gp.width = BMWIDTH;
                gp.src_clip_w = bitmap.viewport.w;
            }
            else {
                /* use 1/4 wider bitmap for emulator rendering */
                bitmap.pitch = ((bitmap.width + VISIBLEWIDTH / 4) * bitmap.granularity);
                /* render with an offset of 1/8 screen size to the right */
                bitmap.data = (uint8 *)(gp.pixels + bitmap.viewport.w / 8);
                /* blit 1/4 wider bitmap */
                gp.width = BMWIDTH + VISIBLEWIDTH / 4;
                gp.src_clip_w = bitmap.viewport.w + bitmap.viewport.w / 4;
            }
            emuIfGraphChgView(&gp);
        }
    }
    else {
        widescreen_triggered = 0;
    }
    if (nkeys & keymap.scancode[EMU_KEY_TRIANGLE]) {
        if (!sound_triggered) {
            snd.enabled = !snd.enabled;
            sound_triggered = 1;
        }
    }
    else
        sound_triggered = 0;
}

#ifdef PROFILE
void init_profile(void);
void dump_profile(void);
#endif

uint16_t *original_shadow = 0;
int my_exit(uint32_t _unknown)
{
    (void)_unknown;
    system_shutdown();
    /* re-enable double buffering */
    if (original_shadow)
        gDisplayDev->setShadowBuffer(original_shadow);
#ifdef PROFILE
    dump_profile();
#endif
    text_free();
    return NativeGE_gameExit();
}

static void bitblit(int x, int y, uint16_t *data, int w, int h)
{
    int fb_w = gDisplayDev->getWidth();
    uint16_t *fb = gDisplayDev->getShadowBuffer() + x + y * fb_w;
    int i;
    if (x <= -w || x > fb_w)	/* off screen */
        return;
    int bytes = w * 2;
    if (x < 0) {
        fb -= x;
        data -= x;
        bytes += x * 2;
    }
    else if (x + w >= fb_w) {
        bytes -= (x + w - fb_w) * 2;
    }
    for (i = 0; i < h; i++) {
        memcpy(fb, data, bytes);
        fb += fb_w;
        data += w;
    }
}
static void bitblit_alpha(int x, int y, uint16_t *data, int w, int h, uint16_t key)
{
    int fb_w = gDisplayDev->getWidth();
    uint16_t *fb = gDisplayDev->getShadowBuffer() + x + y * fb_w;
    int i, j;
    if (x <= -w || x > fb_w)	/* off screen */
        return;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++)
            if (x + j >= 0 && x + j < fb_w && data[j] != key)
                fb[j] = data[j];
        fb += fb_w;
        data += w;
    }
}

int main()
{
    int fd, res, i;

#ifdef PROFILE
    init_profile();
#endif

    // initialize the game api
    libgame_init();

    g_stEmuAPIs->exit = my_exit;

    /* Turn off OS debug output. It is enabled by default on some systems
       and causes horrible slowdowns .*/
    libgame_set_debug(0);

    /* Allocate a quarter more for non-widescreen mode (see below). */
    gp.pixels = malloc((BMWIDTH + VISIBLEWIDTH / 4) * 256 * 2);
    gp.width = BMWIDTH;
    gp.height = 256;
    gp.unknown_flag = 0;
    gp.src_clip_x = 0x20;
    gp.src_clip_y = 0;
    gp.src_clip_w = 256;
    gp.src_clip_h = 240;

    NativeGE_fsOpen("fb.txt", FS_O_CREAT | FS_O_WRONLY | FS_O_TRUNC, &fd);
    res = emuIfGraphInit(&gp);
    fs_fprintf(fd, "emuIfGraphInit (%08x) returns %08x\n", (uint32_t)emuIfGraphInit, res);
    fs_fprintf(fd, "Framebuffer %08x, shadow buffer %08x\n",
               gDisplayDev->getFrameBuffer(), gDisplayDev->getShadowBuffer());
    fs_fprintf(fd, "Width %d, Height %d\n", gDisplayDev->getWidth(), gDisplayDev->getHeight());
    fs_fprintf(fd, "LCD format %08x\n", gDisplayDev->getBuffFormat());
    fs_fprintf(fd, "ARM frequency %d\n", GetArmCoreFreq());
    fs_fprintf(fd, "system ID %d\n", libgame_system_id);

    sp.rate = SOUND_RATE;
    sp.channels = SOUND_CHANNELS;
    sp.depth = 0;	/* 0 seems to mean 16 bits */
    sp.callback = 0;	/* not used for native games, AFAIK */
    res = emuIfSoundInit(&sp);
    fs_fprintf(fd, "emuIfSoundInit returns %d, sp.rate %d\n", res, sp.rate);

    keymap.controller = 0;
    if (emuIfKeyInit(&keymap) < 0) {
        fs_fprintf(fd, "emuIfKeyInit() failed\n");
        NativeGE_fsClose(fd);
        return 0;
    }
    for (i = 0; i < 20; i++)
        fs_fprintf(fd, "key %d scancode %08x\n", i, keymap.scancode[i]);
    load_buttons(&keymap);

    res = text_init();
    if (res < 0) {
        fs_fprintf(fd, "text_init() failed (%d)\n", res);
    }

    gDisplayDev->clear();
    int scr_w = gDisplayDev->getWidth();
    int scr_h = gDisplayDev->getHeight();
    memset(gDisplayDev->getShadowBuffer(), 0xff, scr_w * scr_h * 2);
    bitblit(scr_w / 2 - 200 / 2, scr_h / 3 - 150 / 2, (uint16_t *)pc_engine_data, 200, 150);
    bitblit_alpha(scr_w / 2 - 160 / 2, 8, (uint16_t *)tgemu_logo_data, 160, 90, 0xffff);
    text_set_font_face(FONT_FACE_SONGTI);
    text_set_font_size(FONT_SIZE_12);
    text_set_fg_color(0);
    text_set_bg_color(0xffff);
    text_render_centered("Original code: Charles MacDonald", scr_h - 80);
    text_render_centered("SPMP8000 port: Ulrich Hecht", scr_h - 66);
    text_render_centered("ulrich.hecht@gmail.com", scr_h - 52);
    text_set_fg_color(MAKE_RGB565(255, 0, 0));
    text_render_centered("Press DOWN to map buttons", scr_h - 32);
    text_render_centered("Press any other key to continue", scr_h - 16);
    cache_sync();
    gDisplayDev->flip();

    ge_key_data_t keys = wait_for_key();
    if (keys.key2 & GE_KEY_DOWN)
        map_buttons(&keymap);

    char *romname = 0;
    if ((res = select_file(NULL, "pce|gz|zip", &romname, FONT_SIZE_16)) < 0) {
        fs_fprintf(fd, "select_file() %d\n", res);
        NativeGE_fsClose(fd);
        return 0;
    }
    
    text_set_fg_color(0xffff);
    text_set_bg_color(0);
    text_set_font_size(FONT_SIZE_12);

    fs_fprintf(fd, "loading ROM %s\n", romname);
    res = load_rom(romname, 0, 0);
    if (res != 1) {
        fs_fprintf(fd, "failed to load ROM %s: %d\n", romname, res);
        NativeGE_fsClose(fd);
        return 0;
    }
    free(romname);

    bitmap.width = BMWIDTH;
    bitmap.height = 256;
    bitmap.depth = 16;
    bitmap.granularity = (bitmap.depth >> 3);
    bitmap.data = (uint8 *)gp.pixels;

    bitmap.pitch = bitmap.width * bitmap.granularity;
    bitmap.viewport.w = 256;
    bitmap.viewport.h = 240;
    bitmap.viewport.x = 0x20;
    bitmap.viewport.y = 0x00;
    bitmap.viewport.changed = 0;

    fs_fprintf(fd, "system_init\n");
    system_init(SOUND_RATE);
    fs_fprintf(fd, "system_reset\n");
    system_reset();
    fs_fprintf(fd, "snd.enabled %d, buffer size %d\n", snd.enabled, snd.buffer_size);
    NativeGE_fsClose(fd);
    // return 0;

    int frameskip = MAX_FRAMESKIP;
#ifdef SHOW_KEYS
    char fps[32] = "";
#else
    char fps[16] = "";
#endif

    int avg_full = 16666;
    int avg_skip = 16666;
    
    /* Disable v-sync, we don't have that much time to waste. */
    original_shadow = gDisplayDev->getShadowBuffer();
    gDisplayDev->setShadowBuffer(gDisplayDev->getFrameBuffer());

    while (1) {
        uint32_t start_time = libgame_utime();
        update_input();

        system_frame(0);

        if (bitmap.viewport.changed) {
            gp.src_clip_x = bitmap.viewport.x;
            gp.src_clip_y = bitmap.viewport.y;
            gp.src_clip_w = bitmap.viewport.w;
            /* see above for explanation of non-widescreen rendering */
            if (!widescreen)
                gp.src_clip_w += bitmap.viewport.w / 4;
            gp.src_clip_h = bitmap.viewport.h;
            emuIfGraphChgView(&gp);
            if (!widescreen)
                bitmap.data = (uint8 *)(gp.pixels + bitmap.viewport.w / 8);
            else
                bitmap.data = (uint8 *)gp.pixels;
            bitmap.viewport.changed = 0;
        }

        if (show_timing) {
            text_render_ex((uint16_t *)bitmap.data, bitmap.pitch / 2, fps, bitmap.viewport.x, bitmap.viewport.y);
        }
        if (!widescreen) {
            /* In non-widescreen mode, the black bars to the left and right
               are filled with non-visible graphic artifacts left there by
               the emulator, so we have to blank them ourselves. */
            uint16_t *bar_left = gp.pixels + bitmap.viewport.x + (bitmap.viewport.y) * bitmap.pitch / 2;
            uint16_t *bar_right = bar_left + bitmap.viewport.w + bitmap.viewport.w / 8;
            uint32_t pitch = bitmap.pitch / 2;
            uint32_t width = bitmap.viewport.w / 4;
            for (i = 0; i < bitmap.viewport.h; i++) {
                memset(bar_left, 0, width);
                memset(bar_right, 0, width);
                bar_left += pitch;
                bar_right += pitch;
            }
        }

        if (emuIfGraphShow() < 0)
            return 0;

        avg_full = (avg_full * 7 + libgame_utime() - start_time) / 8;
        update_sound();

        for (i = 0; i < frameskip; i++) {
            start_time = libgame_utime();
            system_frame(1);
            /* Skipping input is not a good idea, makes the game less
               responsive. */
            update_input();
            avg_skip = (avg_skip * 15 + libgame_utime() - start_time) / 16;
            update_sound();
        }

        if (avg_full + frameskip * avg_skip > 16666 * (frameskip + 1) && frameskip < MAX_FRAMESKIP)
            frameskip++;
        else if (avg_full + frameskip * avg_skip < 16666 * (frameskip + 1) - avg_skip && frameskip > 0)
            frameskip--;

        if (show_timing)
#ifdef SHOW_KEYS
            sprintf(fps, "%d/%dms %d k%d", avg_full, avg_skip, frameskip, nkeys);
#else
            sprintf(fps, "%dms %d", (int)avg, frameskip);
#endif
    }

    /* never reached */
    return 0;
}

