#include <stdint.h>

#define FONT_SIZE_12 12
#define FONT_SIZE_16 16

#define FONT_FACE_HZX 0
#define FONT_FACE_SONGTI 1

int text_load_fonts(void);
void text_free_fonts(void);
int text_draw_character(uint32_t codepoint, int x, int y);
int text_draw_character_ex(uint16_t *buf, int width, uint32_t codepoint, int x, int y);
int text_render(const char *t, int x, int y);
int text_render_centered(const char *t, int y);
int text_render_ex(uint16_t *buf, int width, const char *t, int x, int y);
void text_set_font_size(int fs);
int text_get_font_size(void);
void text_set_font_face(int face);
