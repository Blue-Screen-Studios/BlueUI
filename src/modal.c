/* Modals: a window over the game's screen, with a message and buttons. The
 * built-in ones offer narration (when a context starts) and report a
 * controller being unplugged or plugged in. */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "blueui/widgets.h"
#include "internal.h"

static char* dup(const char* s) {
  s = bui__s(s);
  const size_t n = strlen(s) + 1;
  char* p = malloc(n);
  if (p) memcpy(p, s, n);
  return p;
}

static void free_modal(bui__modal* m) {
  free(m->title), free(m->message);
  for (int k = 0; k < m->count; ++k) free(m->buttons[k]);
  memset(m, 0, sizeof(*m));
}

static int find(const bui_context* ui, const char* id) {
  for (int k = 0; k < ui->modal_count; ++k)
    if (strcmp(ui->modals[k].id, bui__s(id)) == 0) return k;
  return -1;
}

static void remove_at(bui_context* ui, int k) {
  free_modal(&ui->modals[k]);
  memmove(&ui->modals[k], &ui->modals[k + 1], (size_t)(ui->modal_count - k - 1) * sizeof(bui__modal));
  --ui->modal_count;
  memset(&ui->modals[ui->modal_count], 0, sizeof(bui__modal));
  /* (None left: the focus back where it was, from the next frame.) */
  if (ui->modal_count == 0 && ui->focus_saved) ui->restore_pending = true;
}

void bui_open_modal(bui_context* ui, const char* id, const char* title, const char* message, const char* const* buttons, int count) {
  int k = find(ui, id);
  if (k < 0) {
    if (ui->modal_count >= BUI__MODAL_MAX) return;
    k = ui->modal_count++;
  } else {
    free_modal(&ui->modals[k]);
  }
  bui__modal* m = &ui->modals[k];
  const size_t n = strlen(bui__s(id)) < BUI_ID_MAX - 1 ? strlen(bui__s(id)) : BUI_ID_MAX - 1;
  memcpy(m->id, bui__s(id), n), m->id[n] = 0;
  m->title = dup(title), m->message = dup(message);
  if (!buttons || count <= 0) {
    m->buttons[0] = dup(ui->strings.ok), m->count = 1;
  } else {
    m->count = count < BUI__MODAL_BUTTONS ? count : BUI__MODAL_BUTTONS;
    for (int b = 0; b < m->count; ++b) m->buttons[b] = dup(buttons[b]);
  }
}

void bui_close_modal(bui_context* ui, const char* id) {
  const int k = find(ui, id);
  if (k >= 0) remove_at(ui, k);
}

bool bui_modal_open(const bui_context* ui, const char* id) { return id ? find(ui, id) >= 0 : ui->modal_count > 0; }

bool bui_take_modal_result(bui_context* ui, const char* id, int* button) {
  if (!ui->has_result || strcmp(ui->result_id, bui__s(id)) != 0) return false;
  ui->has_result = false;
  if (button) *button = ui->result_button;
  return true;
}

void bui_set_controller_modals(bui_context* ui, bui_controller_modals which) { ui->controller_modals = which; }

void bui__free_modals(bui_context* ui) {
  for (int k = 0; k < ui->modal_count; ++k) free_modal(&ui->modals[k]);
  ui->modal_count = 0;
}

/* ---------------------------------------------------------- controllers */

void bui__controller_events(bui_context* ui) {
  const bui_input* in = &ui->in;
  bool lost = in->gamepad_event == BUI_GAMEPAD_DISCONNECTED, found = in->gamepad_event == BUI_GAMEPAD_CONNECTED;
  if (ui->pad_known) lost = lost || (ui->pad_was && !in->gamepad_connected), found = found || (!ui->pad_was && in->gamepad_connected);
  ui->pad_known = true, ui->pad_was = in->gamepad_connected;
  if (lost == found) return;
  const bui_strings* s = &ui->strings;
  if (lost && ui->controller_modals != BUI_CONTROLLER_MODALS_NONE)
    bui_open_modal(ui, BUI_MODAL_CONTROLLER, s->pad_lost_title, s->pad_lost_text, NULL, 0);
  if (found) {
    bui_close_modal(ui, BUI_MODAL_CONTROLLER);  /* (back: no need to say it's gone) */
    if (ui->controller_modals == BUI_CONTROLLER_MODALS_ALL)
      bui_open_modal(ui, BUI_MODAL_CONTROLLER, s->pad_found_title, s->pad_found_text, NULL, 0);
  }
}

/* ---------------------------------------------------------------- frame */

/* `text`'s lines, wrapped at word breaks to `width`: each one drawn, from
 * (x, y); returns how many there were. */
static int wrapped(bui_context* ui, const char* text, float x, float y, float width, float p, bui_color c, bool draw) {
  int lines = 0;
  const char* line = text;
  while (*line) {
    const char* fit = line;
    for (const char* end = line; *end;) {
      const char* next = end;
      while (*next == ' ') ++next;
      while (*next && *next != ' ') ++next;
      if (bui_text_width(line, (size_t)(next - line), p) > width && fit != line) break;
      fit = end = next;
    }
    if (fit == line) fit = line + strlen(line);
    if (draw) bui_text(ui, bui_format(ui, "%.*s", (int)(fit - line), line), x, y + (float)lines * BUI_LINE_ADVANCE * p, p, c, 0.0f);
    ++lines;
    line = fit;
    while (*line == ' ') ++line;
  }
  return lines;
}

void bui__modal_frame(bui_context* ui) {
  bui__modal* m = &ui->modals[0];
  const bui_theme* t = &ui->theme;
  const float p = t->pixel;
  /* The narrator's offer: heard, whatever speech was. */
  if (strcmp(m->id, BUI_MODAL_NARRATOR) == 0) ui->speech_on = true;

  /* Where: the screen's size, or the extent of what's drawn. */
  float sw = ui->in.screen_size.x, sh = ui->in.screen_size.y;
  if (sw <= 0.0f || sh <= 0.0f) {
    sw = sh = 0.0f;
    for (size_t k = 0; k < ui->draw_count; ++k) sw = fmaxf(sw, ui->draw[k].x1), sh = fmaxf(sh, ui->draw[k].y1);
    if (sw <= 0.0f || sh <= 0.0f) sw = 640.0f, sh = 360.0f;
  }
  bui_screen(ui, bui_format(ui, "bui.modal.%s", m->id), bui_format(ui, "%s. %s", bui__s(m->title), bui__s(m->message)), NULL);
  bui_fill(ui, bui_rect_make(0, 0, sw, sh), t->backdrop);

  /* The window: its title, its message, its buttons along the bottom. As
   * wide as they need (as the screen allows); too many for that, one above
   * another, each the window's width. */
  float widths[BUI__MODAL_BUTTONS];
  float row = 0.0f;
  for (int k = 0; k < m->count; ++k) {
    widths[k] = fmaxf(60.0f * p, bui_text_width(m->buttons[k], BUI_NUL, p) + 16.0f * p);
    row += widths[k] + (k > 0 ? 6.0f * p : 0.0f);
  }
  const float w = fminf(sw - 16.0f * p, fmaxf(240.0f * p, row + 12.0f * p)), text_w = w - 12.0f * p;
  const bool stacked = row > text_w;
  const int lines = wrapped(ui, bui__s(m->message), 0, 0, text_w, p, t->text, false);
  const float bh = 16.0f * p, bgap = 4.0f * p;
  const float buttons_h = stacked ? (float)m->count * bh + (float)(m->count - 1) * bgap : bh;
  const float h = 14.0f * p + 10.0f * p + (float)lines * BUI_LINE_ADVANCE * p + 10.0f * p + buttons_h + 8.0f * p;
  const float x0 = roundf(0.5f * (sw - w)), y0 = roundf(0.5f * (sh - h));
  const bui_rect win = bui_rect_make(x0, y0, x0 + w, y0 + h);
  bui_window(ui, win, bui__s(m->title), NULL);
  wrapped(ui, bui__s(m->message), x0 + 6.0f * p, y0 + 24.0f * p, text_w, p, t->text, true);

  /* (Side by side: right-aligned. Stacked: the first on top.) */
  float bx = win.x1 - 6.0f * p - row;
  const float top = win.y1 - 8.0f * p - buttons_h;
  int chosen = -1;
  for (int k = 0; k < m->count; ++k) {
    bui_rect r;
    if (stacked) {
      const float by = top + (float)k * (bh + bgap);
      r = bui_rect_make(win.x0 + 6.0f * p, by, win.x1 - 6.0f * p, by + bh);
    } else {
      r = bui_rect_make(bx, top, bx + widths[k], top + bh);
      bx += widths[k] + 6.0f * p;
    }
    bui_button_opts o = {0};
    o.back = k == m->count - 1;  /* (Back: the last button) */
    if (bui_button(ui, bui_format(ui, "bui.modal.%d", k), bui_format(ui, "%s", m->buttons[k]), r, &o) && chosen < 0) chosen = k;
  }
  if (chosen < 0) return;

  /* Answered: its result kept for the game; the narrator's choice applied
   * from the next frame (said there, with the screen it goes back to). */
  memcpy(ui->result_id, m->id, BUI_ID_MAX);
  ui->result_button = chosen, ui->has_result = true;
  if (strcmp(m->id, BUI_MODAL_NARRATOR) == 0) ui->speech_choice_pending = true, ui->speech_choice = chosen == 0;
  remove_at(ui, 0);
}
