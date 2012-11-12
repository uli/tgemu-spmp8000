#include <stdint.h>

#define FONT_SIZE_12 12
#define FONT_SIZE_16 16

#define FONT_FACE_HZX 0
#define FONT_FACE_SONGTI 1

int load_fonts(void);
void free_fonts(void);
int draw_character(uint32_t codepoint, int x, int y);
int draw_character_ex(uint16_t *buf, int width, uint32_t codepoint, int x, int y);
int render_text(const char *t, int x, int y);
int render_text_centered(const char *t, int y);
int render_text_ex(uint16_t *buf, int width, const char *t, int x, int y);
void text_set_font_size(int fs);
int text_get_font_size(void);
void text_set_font_face(int face);
