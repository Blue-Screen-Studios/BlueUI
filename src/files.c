#include "blueui/widgets.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "internal.h"

/* The file picker (blueui/widgets.h): a folder read from the disk, its
 * subfolders then its files listed as buttons in a scroll view. */

static void copy_path(char* dst, const char* src) { snprintf(dst, BUI_PATH_MAX, "%s", src ? src : ""); }

static void home_dir(char* out) {
  const char* h = getenv("HOME");
#if defined(_WIN32)
  if (!h || !*h) h = getenv("USERPROFILE");
#endif
  copy_path(out, h && *h ? h : "/");
}

void bui_path_parent(const char* path, char* out, size_t size) {
  char buf[BUI_PATH_MAX];
  snprintf(buf, sizeof(buf), "%s", path ? path : "");
  size_t n = strlen(buf);
  while (n > 1 && (buf[n - 1] == '/' || buf[n - 1] == '\\')) buf[--n] = 0;  /* (its trailing slashes) */
  char* slash = NULL;
  for (char* c = buf; *c; ++c)
    if (*c == '/' || *c == '\\') slash = c;
  if (!slash) {
    snprintf(out, size, "%s", buf);  /* (no parent: it stays) */
    return;
  }
  if (slash == buf) slash[1] = 0;  /* "/a" -> "/" */
#if defined(_WIN32)
  else if (slash == buf + 2 && buf[1] == ':') slash[1] = 0;  /* "C:\a" -> "C:\" */
#endif
  else *slash = 0;
  snprintf(out, size, "%s", buf);
}

/* dir/name into `out` (too long for it: "", nothing chosen or opened). */
static void join(char* out, const char* dir, const char* name) {
  const size_t n = strlen(dir), m = strlen(name);
  const bool sep = n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\');
  if (n + (sep ? 0 : 1) + m + 1 > BUI_PATH_MAX) {
    out[0] = 0;
    return;
  }
  memcpy(out, dir, n);
  if (!sep) out[n] = '/';
  memcpy(out + n + (sep ? 0 : 1), name, m + 1);
}

/* Whether `name` ends in one of `exts` (";"-separated, any case). */
static bool wanted(const char* name, const char* exts) {
  if (!exts || !*exts) return true;
  const size_t n = strlen(name);
  const char* e = exts;
  while (*e) {
    const char* end = strchr(e, ';');
    const size_t len = end ? (size_t)(end - e) : strlen(e);
    if (len > 0 && len <= n) {
      bool same = true;
      for (size_t k = 0; k < len && same; ++k) same = tolower((unsigned char)name[n - len + k]) == tolower((unsigned char)e[k]);
      if (same) return true;
    }
    e += len + (end ? 1 : 0);
  }
  return false;
}

static int by_kind_then_name(const void* a, const void* b) {
  const bui_file_entry* x = a;
  const bui_file_entry* y = b;
  if (x->folder != y->folder) return x->folder ? -1 : 1;
  for (size_t k = 0;; ++k) {
    const int cx = tolower((unsigned char)x->name[k]), cy = tolower((unsigned char)y->name[k]);
    if (cx != cy || !cx) return cx - cy;
  }
}

static void add(bui_file_picker* p, int* cap, const char* name, bool folder) {
  if (p->entry_count == *cap) {
    const int grown = *cap ? *cap * 2 : 64;
    bui_file_entry* e = realloc(p->entries, (size_t)grown * sizeof(bui_file_entry));
    if (!e) return;
    p->entries = e, *cap = grown;
  }
  bui_file_entry* e = &p->entries[p->entry_count++];
  snprintf(e->name, sizeof(e->name), "%s", name);
  e->folder = folder;
}

/* The folder read again: its subfolders and its wanted files, sorted. */
static void read_folder(bui_file_picker* p) {
  free(p->entries);
  p->entries = NULL, p->entry_count = 0, p->unreadable = false;
  int cap = 0;
#if defined(_WIN32)
  char pattern[BUI_PATH_MAX];
  join(pattern, p->dir, "*");
  WIN32_FIND_DATAA f;
  HANDLE h = FindFirstFileA(pattern, &f);
  if (h == INVALID_HANDLE_VALUE) {
    p->unreadable = true;
  } else {
    do {
      if (!strcmp(f.cFileName, ".") || !strcmp(f.cFileName, "..")) continue;
      if (!p->show_hidden && (f.cFileName[0] == '.' || (f.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))) continue;
      const bool folder = (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      if (folder || wanted(f.cFileName, p->extensions)) add(p, &cap, f.cFileName, folder);
    } while (FindNextFileA(h, &f));
    FindClose(h);
  }
#else
  DIR* d = opendir(p->dir);
  if (!d) {
    p->unreadable = true;
  } else {
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
      if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
      if (!p->show_hidden && de->d_name[0] == '.') continue;
      char full[BUI_PATH_MAX];
      join(full, p->dir, de->d_name);
      struct stat st;
      if (stat(full, &st) != 0) continue;  /* (a dangling link) */
      const bool folder = S_ISDIR(st.st_mode);
      if (folder || (S_ISREG(st.st_mode) && wanted(de->d_name, p->extensions))) add(p, &cap, de->d_name, folder);
    }
    closedir(d);
  }
#endif
  if (p->entry_count > 1) qsort(p->entries, (size_t)p->entry_count, sizeof(bui_file_entry), by_kind_then_name);
  copy_path(p->read_dir, p->dir);
}

void bui_file_picker_free(bui_file_picker* p) {
  free(p->entries);
  p->entries = NULL;
  p->entry_count = 0;
  p->read_dir[0] = 0;
}

/* `text`, its start cut off ("...") till it fits `w` at `px`. */
static const char* fit_left(bui_context* ui, const char* text, float w, float px) {
  if (bui_text_width(text, BUI_NUL, px) <= w) return text;
  const size_t n = strlen(text);
  for (size_t cut = 1; cut < n; ++cut) {
    const char* s = bui_format(ui, "...%s", text + cut);
    if (bui_text_width(s, BUI_NUL, px) <= w) return s;
  }
  return "...";
}

bui_pick bui_file_picker_show(bui_context* ui, const char* id, bui_rect area, bui_file_picker* p, const char* title) {
  const bui_theme* t = bui_get_theme(ui);
  const float px = t->pixel;
  if (!p->dir[0]) home_dir(p->dir);
  if (strcmp(p->dir, p->read_dir) != 0) read_folder(p);
  bui_pick result = BUI_PICK_OPEN;
  char to[BUI_PATH_MAX];
  to[0] = 0;

  bui_window(ui, area, title, NULL);
  const bui_rect c = bui_rect_make(area.x0 + 6.0f * px, area.y0 + 18.0f * px, area.x1 - 6.0f * px, area.y1 - 6.0f * px);
  const float row = 16.0f * px, gap = 2.0f * px;
  /* Where it is; up, home. */
  bui_label(ui, fit_left(ui, p->dir, bui_rect_w(c), px), c.x0, c.y0, t->value, 0.0f, 0.0f);
  float y = c.y0 + 12.0f * px;
  const float bw = 60.0f * px;
  bui_button_opts up = {0};
  up.spoken = bui_format(ui, "Up a folder, to %s", p->dir);
  if (bui_button(ui, bui_format(ui, "%s.up", id), "UP", bui_rect_make(c.x0, y, c.x0 + bw, y + row), &up))
    bui_path_parent(p->dir, to, sizeof(to));
  if (bui_button(ui, bui_format(ui, "%s.home", id), "HOME", bui_rect_make(c.x0 + bw + gap, y, c.x0 + 2.0f * bw + gap, y + row), NULL))
    home_dir(to);
  y += row + 4.0f * px;
  /* Its entries, scrolled; under them, Cancel. */
  const bui_rect view = bui_rect_make(c.x0, y, c.x1, c.y1 - row - 4.0f * px);
  const float content = (float)p->entry_count * (row + gap);
  const float off = bui_begin_scroll(ui, bui_format(ui, "%s.list", id), view, content);
  if (p->unreadable || p->entry_count == 0)
    bui_label(ui, p->unreadable ? "This folder can't be read." : "Nothing here.", view.x0 + 3.0f * px, view.y0 + 3.0f * px, t->dim,
              0.0f, 0.0f);
  for (int i = 0; i < p->entry_count; ++i) {
    const bui_file_entry* e = &p->entries[i];
    const float ey = view.y0 - off + (float)i * (row + gap);
    const bui_rect r = bui_rect_make(view.x0, ey, view.x1 - 6.0f * px, ey + row);
    bui_item it = {0};
    it.kind = BUI_BUTTON;
    it.rect = r;
    it.spoken = bui_format(ui, "%s %s", e->folder ? "Folder" : "File", e->name);
    const bui_result res = bui_declare(ui, bui_format(ui, "%s.e.%d", id, i), &it);
    if (res.activated) {
      if (e->folder) join(to, p->dir, e->name);
      else {
        join(p->chosen, p->dir, e->name);
        if (p->chosen[0]) result = BUI_PICK_CHOSEN;
      }
    }
    if (!bui_visible(ui, r)) continue;
    bui_fill(ui, r, res.hovered ? t->button_hot : (i % 2 ? t->stripe : bui_rgba(0, 0, 0, 0)));
    const float ty = roundf(0.5f * (r.y0 + r.y1 - BUI_GLYPH_H * px));
    bui_text(ui, e->folder ? ">" : "-", r.x0 + 3.0f * px, ty, px, e->folder ? t->value : t->dim, 0.0f);
    bui_text(ui, e->name, r.x0 + 12.0f * px, ty, px, e->folder ? t->text : t->choice_text, 0.0f);
  }
  bui_end_scroll(ui);
  bui_button_opts cancel = {0};
  cancel.back = true;
  if (bui_button(ui, bui_format(ui, "%s.cancel", id), "CANCEL", bui_rect_make(c.x1 - bw, c.y1 - row, c.x1, c.y1), &cancel))
    result = BUI_PICK_CANCELLED;
  /* Into another folder: read next frame, its list from the top. */
  if (to[0] && result == BUI_PICK_OPEN) {
    copy_path(p->dir, to);
    bui_set_scroll(ui, bui_format(ui, "%s.list", id), 0.0f);
    bui_announce(ui, bui_format(ui, "In %s", p->dir), BUI_SAY_NORMAL);
  }
  return result;
}
