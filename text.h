#include <stdint.h>

int load_fonts(void);
void free_fonts(void);
int draw_character(uint32_t codepoint, int x, int y);
void render_text(const char *t, int x, int y);
