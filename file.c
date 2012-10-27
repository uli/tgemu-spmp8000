#include "text.h"
#include <unistd.h>
#include "libgame.h"
#include <string.h>
#include <sys/stat.h>

extern key_data_t wait_for_key(void);

static void invert(uint16_t *start, int size)
{
  while (size) {
    *start = ~(*start);
    start++;
    size--;
  }
}

int select_file(const char *start, const char *extension, char **file)
{
  struct _ecos_dirent *dents;
  struct _ecos_stat *stats;
  dents = malloc(sizeof(struct _ecos_dirent) * 10);
  stats = malloc(sizeof(struct _ecos_stat) * 10);
  int num_dents = 10;
  
  char wd[256];
  _ecos_getcwd(wd, 256);
  if (start) {
    if (_ecos_chdir(start) < 0)
      return -1;
  }

  _ecos_DIR *dir;
reload_dir:
  dir = _ecos_opendir(".");
  if (!dir)
    return -2;
  int dent_count = 0;
  struct _ecos_dirent *de;
  while ((de = _ecos_readdir(dir))) {
    _ecos_stat(de->d_name, &stats[dent_count]);
    if (extension) {
      if (!_ECOS_S_ISDIR(stats[dent_count].st_mode)) {
        int len = strlen(de->d_name);
        if (len < 4 || strcasecmp(de->d_name + len - 3, extension) || de->d_name[len - 4] != '.')
          continue;
      }
    }
    memcpy(&dents[dent_count], de, sizeof(struct _ecos_dirent));
    dent_count++;
    if (dent_count >= num_dents) {
      num_dents *= 2;
      dents = realloc(dents, sizeof(struct _ecos_dirent) * num_dents);
      stats = realloc(stats, sizeof(struct _ecos_stat) * num_dents);
    }
  }
  _ecos_closedir(dir);
  
  int screen_width = gDisplayDev->getWidth();
  int screen_height = gDisplayDev->getHeight();
  int max_entries_displayed = screen_height / 12;
  int current_top = 0;
  int current_entry = 0;

  for (;;) {
    int i;
    uint16_t *fb = gDisplayDev->getShadowBuffer();
    memset(fb, 0, screen_width * screen_height * 2);
    for (i = current_top; i < current_top + max_entries_displayed && i < current_top + dent_count; i++) {
      if (_ECOS_S_ISDIR(stats[i].st_mode)) {
        int cw = draw_character('[', 0, (i-current_top) * 12);
        cw += render_text(dents[i].d_name, cw, (i-current_top) * 12);
        draw_character(']', cw, (i-current_top) * 12);
      }
      else {
        render_text(dents[i].d_name, 0, (i - current_top) * 12);
      }
    }
    invert(fb + (current_entry - current_top) * screen_width * 12, 12 * screen_width);
    cache_sync();
    gDisplayDev->lcdFlip();
    key_data_t keys = wait_for_key();
    if ((keys.key2 & KEY_UP) && current_entry > 0) {
      current_entry--;
      if (current_entry < current_top)
        current_top--;
    }
    else if ((keys.key2 & KEY_DOWN) && current_entry < dent_count - 1) {
      current_entry++;
      if ((current_entry - current_top + 1) * 12 > screen_height)
        current_top++;
    }
    else if (keys.key2 & KEY_O) {
      if (_ECOS_S_ISDIR(stats[current_entry].st_mode)) {
        _ecos_chdir(dents[current_entry].d_name);
        goto reload_dir;
      }
      else
        break;
    }
  }
  char *filename = strdup(dents[current_entry].d_name);
  *file = filename;
  free(dents);
  free(stats);
  _ecos_chdir(wd);
  return 0;
}
