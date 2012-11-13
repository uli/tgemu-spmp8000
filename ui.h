#include "libgame.h"

int select_file(const char *start, const char *extension, char **file, int font_size);
ge_key_data_t wait_for_key(void);
void map_buttons(emu_keymap_t *keymap);
void load_buttons(emu_keymap_t *keymap);
