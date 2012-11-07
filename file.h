#include "libgame.h"

int select_file(const char *start, const char *extension, char **file);
key_data_t wait_for_key(void);
void map_buttons(keymap_t *keymap);
void load_buttons(keymap_t *keymap);
