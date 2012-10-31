
#include <stdio.h>
#include <string.h>

#include "libgame.h"
#include "gfx_types.h"
#include "shared.h"
#include <sys/stat.h>
#include <fcntl.h>
#include "text.h"
#include "file.h"

extern void **ftab;

extern display_dev_t *gDisplayDev;

#define fs_fprintf(fd, x...) { \
  char buf[256]; int _res; \
  sprintf(buf, x); \
  NativeGE_fsWrite(fd, buf, strlen(buf), &_res); \
}

#define MAX_FRAMESKIP 6

uint32_t last_frame;

sound_params_t sp;
#define SOUNDBUF_FRAMES 8
uint8_t sound_ring[735 * SOUNDBUF_FRAMES];
int sound_ring_read = 0;
int sound_ring_write = 0;
void update_sound(void)
{
    int i;
    for (i = 0; i < snd.buffer_size; i++) {
        sound_ring[i + sound_ring_write * snd.buffer_size] = snd.buffer[1][i] >> 8;
    }
    sound_ring_write = (sound_ring_write + 1) % SOUNDBUF_FRAMES;
    if (sound_ring_write >= sound_ring_read + 2 ||
        (sound_ring_write == 0 && sound_ring_read == SOUNDBUF_FRAMES - 2)) {
        sp.buf = &sound_ring[sound_ring_read * snd.buffer_size];
        sp.buf_size = 735 * 2;
        /* emuIfSoundPlay() can block, so we must exclude it from the
           calculated frame time used to tune frameskip. */
        uint32_t time = NativeGE_getTime();
        emuIfSoundPlay(&sp);
        last_frame += NativeGE_getTime() - time;
        sound_ring_read = (sound_ring_read + 2) % SOUNDBUF_FRAMES;
    }
}

key_data_t wait_for_key(void)
{
    static int auto_repeat = 0;
    static key_data_t okeys;
    key_data_t keys;
    NativeGE_getKeyInput4Ntv(&keys);
    uint32_t last_press_tick = NativeGE_getTime();
    while (auto_repeat && keys.key2 == okeys.key2 && NativeGE_getTime() < last_press_tick + 100) {
        NativeGE_getKeyInput4Ntv(&keys);
    }
    if (auto_repeat && keys.key2 == okeys.key2)
        return keys;
    auto_repeat = 0;
    last_press_tick = NativeGE_getTime();
    while (keys.key2 && keys.key2 == okeys.key2) {
        NativeGE_getKeyInput4Ntv(&keys);
        if (NativeGE_getTime() > last_press_tick + 500) {
            auto_repeat = 1;
            okeys = keys;
            return keys;
        }
    }
    okeys = keys;
    while (okeys.key2 == keys.key2) {
        NativeGE_getKeyInput4Ntv(&keys);
    }
    okeys = keys;
    return keys;
}

int main()
{
    int i;
    int fd, res;
    key_data_t keys;

    // initialize the game api
    libgame_init();

    graph_params_t gp;
    /* XXX: This should be 512, not 400. */
#define BMWIDTH (32 + 512 + 32)
    gp.pixels = malloc(BMWIDTH * 256 * 2);
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

    sp.rate = 22050;
    sp.channels = 1;            // 2;
    sp.depth = 0;
    sp.callback = 0;
    fs_fprintf(fd, "emuIfSoundInit returns %d, sp.rate %d\n", emuIfSoundInit(&sp), sp.rate);

    res = load_fonts();
    if (res < 0) {
        fs_fprintf(fd, "load_fonts failed (%d)\n", res);
    }
    // memset(gDisplayDev->getShadowBuffer(), 0, gDisplayDev->getWidth() *
    // gDisplayDev->getHeight() * 2);
    gDisplayDev->lcdClear();
    render_text("TGEmu SPMP", (gDisplayDev->getWidth() - 10 * 8) / 2, 32);
    render_text("Press any key", (gDisplayDev->getWidth() - 13 * 8) / 2, 200);
    cache_sync();
    gDisplayDev->lcdFlip();
    wait_for_key();

    char *romname = 0;
    if ((res = select_file(NULL, "pce", &romname)) < 0) {
        fs_fprintf(fd, "select_file() %d\n", res);
        NativeGE_fsClose(fd);
        return 0;
    }

    fs_fprintf(fd, "loading ROM\n");
    res = load_rom(romname, 0, 0);
    if (res != 1) {
        fs_fprintf(fd, "failed to load ROM: %d\n", res);
        NativeGE_fsClose(fd);
        return 0;
    }
    free(romname);

    bitmap.width = BMWIDTH;
    bitmap.height = 256;
    bitmap.depth = 16;
    bitmap.granularity = (bitmap.depth >> 3);
    bitmap.data = (uint8 *) gp.pixels;

    bitmap.pitch = (bitmap.width * bitmap.granularity);
    bitmap.viewport.w = 256;
    bitmap.viewport.h = 240;
    bitmap.viewport.x = 0x20;
    bitmap.viewport.y = 0x00;
    bitmap.viewport.changed = 0;

    fs_fprintf(fd, "system_init\n");
    system_init(44100);
    fs_fprintf(fd, "system_reset\n");
    system_reset();
    fs_fprintf(fd, "snd.enabled %d, buffer size %d\n", snd.enabled, snd.buffer_size);
    NativeGE_fsClose(fd);
    // return 0;

    int frameskip = MAX_FRAMESKIP;
    last_frame = NativeGE_getTime();
    char fps[16] = "";
    float avg = 16.2;
    int countdown = 0;

    int show_timing = 0;
    int show_timing_triggered = 0;

    while (1) {

        input.pad[0] = 0;
        /* Do this even when using native input because it won't be possible
           to quit otherwise. */
        NativeGE_getKeyInput4Ntv(&keys);
        if (NativeGE_getKeyInput) {
            key_data_t nkeys;
            NativeGE_getKeyInput(&nkeys);
            if (nkeys.key2 & (1 << 0))
                input.pad[0] |= INPUT_UP;
            if (nkeys.key2 & (1 << 1))
                input.pad[0] |= INPUT_DOWN;
            if (nkeys.key2 & (1 << 2))
                input.pad[0] |= INPUT_LEFT;
            if (nkeys.key2 & (1 << 3))
                input.pad[0] |= INPUT_RIGHT;
            if (nkeys.key2 & (1 << 4))
                input.pad[0] |= INPUT_B1;
            if (nkeys.key2 & (1 << 5))
                input.pad[0] |= INPUT_B2;
            // if (nkeys.key2 & (1 << 7)) input.pad[0] |= INPUT_UP;
            if (nkeys.key2 & (1 << 8)) {
                if (!show_timing_triggered) {
                    show_timing_triggered = 1;
                    show_timing = !show_timing;
                }
            }
            else {
                show_timing_triggered = 0;
            }
            // if (nkeys.key2 & (1 << 9)) input.pad[0] |= INPUT_UP;
            if (nkeys.key2 & (1 << 10))
                input.pad[0] |= INPUT_SELECT;
            if (nkeys.key2 & (1 << 11))
                input.pad[0] |= INPUT_RUN;
        }
        else {
            if (keys.key2 & KEY_UP)
                input.pad[0] |= INPUT_UP;
            if (keys.key2 & KEY_DOWN)
                input.pad[0] |= INPUT_DOWN;
            if (keys.key2 & KEY_LEFT)
                input.pad[0] |= INPUT_LEFT;
            if (keys.key2 & KEY_RIGHT)
                input.pad[0] |= INPUT_RIGHT;
            if (keys.key2 & KEY_X)
                input.pad[0] |= INPUT_B2;
            if (keys.key2 & KEY_O)
                input.pad[0] |= INPUT_B1;
            // if(keys.key2 & KEY_D) input.pad[0] |= INPUT_SELECT;
            if (keys.key2 & KEY_START)
                input.pad[0] |= INPUT_RUN;
        }
        // bitmap.data = (uint8 *)getLCDShadowBuffer();
        // uint16_t *fb = getLCDShadowBuffer();
        for (i = 0; i < frameskip; i++) {
            system_frame(1);
            update_sound();
        }
        bitmap.data = (uint8 *) gp.pixels;
        system_frame(0);
        update_sound();
        sp.buf_size = 735 * (frameskip + 1);

        if (bitmap.viewport.changed) {
            gp.src_clip_x = bitmap.viewport.x;
            gp.src_clip_y = bitmap.viewport.y;
            gp.src_clip_w = bitmap.viewport.w;
            gp.src_clip_h = bitmap.viewport.h;
            emuIfGraphChgView(&gp);
            bitmap.viewport.changed = 0;
        }

#if 0
        uint16_t *fb = gp.pixels + bitmap.viewport.x + bitmap.viewport.y * BMWIDTH;
        for (i = 0; i < 32; i++) {
            memset(fb + bitmap.width * 24 + i * 6, (keys.key2 & (1 << i)) ? 0xff : 0, 12);
            memset(fb + bitmap.width * 25 + i * 6, (nkeys.key2 & (1 << i)) ? 0x0f : 0, 12);
        }
#endif

        if (show_timing)
            render_text_ex(gp.pixels, BMWIDTH, fps, bitmap.viewport.x, bitmap.viewport.y);
        if (emuIfGraphShow() < 0)
            return 0;

        uint32_t now = NativeGE_getTime();
        avg = ((avg * (32 - frameskip - 1)) + (now - last_frame)) / 32.0;

        if (show_timing)
            sprintf(fps, "%dms %d", (int)avg, frameskip);

        if (countdown)
            countdown--;
        if (!countdown && frameskip < MAX_FRAMESKIP && avg > 16.2) {
            frameskip++;
            countdown = 10;
        }
        else if (!countdown && frameskip > 0 && avg < 15.5) {
            frameskip--;
            countdown = 10;
        }

        last_frame = NativeGE_getTime();
    }

    free_fonts();
    return 0;
}
