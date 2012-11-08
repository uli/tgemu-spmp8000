#include <stdint.h>

int load_fonts(void);
void free_fonts(void);
int draw_character(uint32_t codepoint, int x, int y);
int draw_character_ex(uint16_t *buf, int width, uint32_t codepoint, int x, int y);
int render_text(const char *t, int x, int y);
int render_text_centered(const char *t, int y);
int render_text_ex(uint16_t *buf, int width, const char *t, int x, int y);
