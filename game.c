
#include <stdio.h>
#include <string.h>

#include "libgame.h"
#include <libemu.h>
#include "gfx_types.h"
#include "font.h"
#include "shared.h"

//#define	NULL	(void*)0

#ifndef HAVE_NEWLIB
extern int sprintf(char *__s, const char *format, ...);
#endif

extern void **ftab;
uint16_t *(*int_get_framebuffer)(void);
uint16_t *(*int_get_shadowbuffer)(void);
void (*set_lcd_fb_format)(int);
int (*getLCDBuffFormat)(void);
int (*getLCDWidth)(void);
int (*getLCDHeight)(void);
uint16_t *(*getLCDShadowBuffer)(void);
uint16_t *(*getLCDFrameBuffer)(void);
void (*setLCDFrameBuffer)(uint16_t *fb); // educated guess
void (*lcd_flip)(void);
void (*lcd_clear)(void); // actually returns BitBlt_hw retval, but that is always 0
int (*NativeGE_getKeyInput)(key_data_t *) = (void *)0x326cf4;

#define fs_fprintf(fd, x...) { \
  char buf[256]; int _res; \
  sprintf(buf, x); \
  fs_write(fd, buf, strlen(buf), &_res); \
}

#define FRAMESKIP 2

sound_params_t sp;
void update_sound(int off)
{
        int i;
        for (i = 0; i < snd.buffer_size; i++) {
            sp.buf[i + off * snd.buffer_size] = snd.buffer[1][i] >> 8;
        }
}

int main()
{
	int i;
	gfx_rect_t rect;
	int fd;
	key_data_t keys, okeys;

	// initialize the game api
	libgame_init();
	libemu_init();

	set_lcd_fb_format = gDisplayDev[0];
	getLCDBuffFormat = gDisplayDev[1];
	getLCDWidth = gDisplayDev[2];
	getLCDHeight = gDisplayDev[3];
	getLCDShadowBuffer = gDisplayDev[4];
	setLCDFrameBuffer = gDisplayDev[5];
	lcd_flip = gDisplayDev[6];
	lcd_clear = gDisplayDev[7];
	//setLCDShadowBuffer
	getLCDFrameBuffer = gDisplayDev[9];

	graph_params_t gp;
	/* XXX: This should be 512, not 400. */
	gp.pixels = malloc(400 * 256 * 2);
	gp.width = 400;
	gp.height = 256;
	gp.unknown_flag = 0;
	gp.src_clip_x = 0x20;
	gp.src_clip_y = 0;
	gp.src_clip_w = 256;
	gp.src_clip_h = 240;

	rect.x = 0;
	rect.y = 0;
	rect.width = 480;
	rect.height = 272;

	int res;
	
	fs_open("fb.txt", FS_O_CREAT|FS_O_WRONLY|FS_O_TRUNC, &fd);
	res = emuIfGraphInit(&gp);
	fs_fprintf(fd, "emuIfGraphInit (%08x) returns %08x\n", (uint32_t)emuIfGraphInit, res);
	fs_fprintf(fd, "Framebuffer %08x, shadow buffer %08x\n", getLCDFrameBuffer(), getLCDShadowBuffer());
	fs_fprintf(fd, "Width %d, Height %d\n", getLCDWidth(), getLCDHeight());
	fs_fprintf(fd, "LCD format %08x\n", getLCDBuffFormat());
	uint16_t sound_buf[44100 * 2]; //735 * 2];//367 * 2];
	sp.buf = sound_buf;
	sp.buf_size = 735 * (FRAMESKIP + 1);// * 2;//367 * 2;
	sp.rate = 22050;
	sp.channels = 1; //2;
	sp.depth = 0;
	sp.callback = 0;
	fs_fprintf(fd, "emuIfSoundInit returns %d, sp.rate %d\n", emuIfSoundInit(&sp), sp.rate);
	
	okeys.key1 = 0;
	okeys.key2 = 1;
	i=0;
	
	fs_fprintf(fd, "loading ROM\n");
	res = load_rom("pce.pce", 0, 0);
	if (res != 1) {
		fs_fprintf(fd, "failed to load ROM: %d\n", res);
		fs_close(fd);
		return 0;
	}

#ifdef RENDER_8BPP
#define RGB(r, g, b) ( (((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3) )
#define RGBR(r, g, b) ( (((r)) << 11) | (((g)) << 5) | ((b)) )
	uint16_t palette[256];
	for (i = 0; i < 256; i++) {
		int r = ((i >> 2) & 7);
		r = ((r << 3) | r);
		int g = ((i >> 5) & 7);
		g = (g << 3) | g;
		int b = ((i >> 0) & 3);
		b = (b << 4) | b;
		palette[i] = RGBR(r >> 1, g, b >> 1);
		fs_fprintf(fd, "palette raw %02x r %d g %d b %d rgb %04x\n", i, r, g, b, palette[i]);
	}
#endif
    memset(getLCDFrameBuffer(), 0, getLCDHeight() * getLCDWidth() * 2);
    memset(getLCDShadowBuffer(), 0, getLCDHeight() * getLCDWidth() * 2);

    bitmap.width = 400;
    bitmap.height = 256;
#ifdef RENDER_8BPP
    bitmap.depth = 8;
    bitmap.granularity = (bitmap.depth >> 3);
    uint8_t bitmap_buf[bitmap.width * bitmap.height * bitmap.granularity];
    memset(bitmap_buf, 0, bitmap.width * bitmap.height * bitmap.granularity);
    bitmap.data = bitmap_buf; //getLCDShadowBuffer(); //bitmap_buf;
#else
    bitmap.depth = 16;
    bitmap.granularity = (bitmap.depth >> 3);
    bitmap.data = (uint8 *)gp.pixels;
#endif
    
    bitmap.pitch = (bitmap.width * bitmap.granularity);
    bitmap.viewport.w = 256;
    bitmap.viewport.h = 240;
    bitmap.viewport.x = 0x20;
    bitmap.viewport.y = 0x00;
	
	fs_fprintf(fd, "system_init\n");
	system_init(44100);
	fs_fprintf(fd, "system_reset\n");
	system_reset();
	fs_fprintf(fd, "snd.enabled %d, buffer size %d\n", snd.enabled, snd.buffer_size);
	fs_close(fd);
	//return 0;
	
	while (1) {
		get_keys(&keys);

#if 0
		if ((keys.key2 & KEY_DOWN) && !(okeys.key2 & KEY_DOWN)) {
			format++;
			set_lcd_fb_format(format % 9);
		}
#endif
#if 0
		if ((keys.key2 & KEY_UP) && !(okeys.key2 & KEY_UP)) {
		        sp.buf_size = 44100 * 2;
			for (i = 0; i < sp.buf_size; i++) {
				if (i % 256 < 128) {
					sp.buf[i] = 0;
				}
				else {
					sp.buf[i] = 77;
				}
			}
			emuIfSoundPlay(&sp);
			//emuIfSoundPlay(&sp);
		}
#endif
			input.pad[0] = 0;
			    if(keys.key2 & KEY_UP)     input.pad[0] |= INPUT_UP;
			    if(keys.key2 & KEY_DOWN)   input.pad[0] |= INPUT_DOWN;
			    if(keys.key2 & KEY_LEFT)   input.pad[0] |= INPUT_LEFT;
			    if(keys.key2 & KEY_RIGHT)  input.pad[0] |= INPUT_RIGHT;
			    if(keys.key2 & KEY_X)      input.pad[0] |= INPUT_B2;
			    if(keys.key2 & KEY_O)      input.pad[0] |= INPUT_B1;
			    //if(keys.key2 & KEY_D)      input.pad[0] |= INPUT_SELECT;
			    if(keys.key2 & KEY_START)      input.pad[0] |= INPUT_RUN;
				
			//bitmap.data = (uint8 *)getLCDShadowBuffer();
			//uint16_t *fb = getLCDShadowBuffer();
                        for (i = 0; i < FRAMESKIP; i++) {
        			system_frame(1);
        			update_sound(i);
                        }
#ifndef RENDER_8BPP
                bitmap.data = (uint8 *)gp.pixels;
#endif
			system_frame(0);
			update_sound(i);
#ifdef RENDER_8BPP
			uint8_t *o = bitmap.data;
			for (i = 0; i < bitmap.height; i++) {
				int j;
				for (j = 0; j < bitmap.width; j++) {
					fb[j] = palette[o[j]];
				}
				fb += bitmap.width;
				o += bitmap.width;
			}
#endif
			uint16_t *fb = getLCDShadowBuffer();
			key_data_t nkeys;
			NativeGE_getKeyInput(&nkeys);
			for (i = 0; i < 32; i++) {
				memset(fb + bitmap.width * 260 + i * 10, (keys.key2 & (1 << i)) ? 0xff : 0, 20);
				memset(fb + bitmap.width * 265 + i * 10, (keys.key1 & (1 << i)) ? 0xf0 : 0, 20);
				memset(fb + bitmap.width * 270 + i * 10, (nkeys.key2 & (1 << i)) ? 0x0f : 0, 20);
			}
			
			emuIfSoundPlay(&sp);
#if 0
		}
#endif
                if (emuIfGraphShow() < 0)
                        return 0;
		okeys = keys;
	}

	return 0;
}
