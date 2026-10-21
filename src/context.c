#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

/* ---------------------------------------------------------- the helpers */

static void copy_id(char* dst, const char* src) {
  const size_t n = strlen(bui__s(src));
  const size_t k = n < BUI_ID_MAX - 1 ? n : BUI_ID_MAX - 1;
  memcpy(dst, bui__s(src), k);
  dst[k] = 0;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static void* grow(void* p, size_t* cap, size_t need, size_t size) {
  if (need <= *cap) return p;
  size_t c = *cap ? *cap : 16;
  while (c < need) c *= 2;
  void* q = realloc(p, c * size);
  if (!q) abort();
  *cap = c;
  return q;
}

void bui__arena_reset(bui__arena* a) {
  for (bui__block* b = a->head; b; b = b->next) b->used = 0;
  a->at = a->head;
}

void bui__arena_free(bui__arena* a) {
  for (bui__block* b = a->head; b;) {
    bui__block* next = b->next;
    free(b);
    b = next;
  }
  a->head = a->at = NULL;
}

static char* arena_alloc(bui__arena* a, size_t n) {
  for (;;) {
    if (a->at && a->at->cap - a->at->used >= n) {
      char* p = a->at->data + a->at->used;
      a->at->used += n;
      return p;
    }
    if (a->at && a->at->next) {
      a->at = a->at->next;
      continue;
    }
    const size_t cap = n > 16384 ? n : 16384;
    bui__block* b = malloc(sizeof(bui__block) + cap);
    if (!b) abort();
    b->next = NULL, b->used = 0, b->cap = cap;
    if (a->at) a->at->next = b;
    else a->head = b;
    a->at = b;
  }
}

char* bui__arena_dup(bui__arena* a, const char* s) {
  s = bui__s(s);
  const size_t n = strlen(s) + 1;
  char* p = arena_alloc(a, n);
  memcpy(p, s, n);
  return p;
}

char* bui__arena_vprintf(bui__arena* a, const char* fmt, va_list args) {
  va_list again;
  va_copy(again, args);
  const int n = vsnprintf(NULL, 0, fmt, args);
  char* p = arena_alloc(a, n > 0 ? (size_t)n + 1 : 1);
  if (n > 0) vsnprintf(p, (size_t)n + 1, fmt, again);
  else p[0] = 0;
  va_end(again);
  return p;
}

void bui__str_set(bui__str* s, const char* text) {
  s->len = 0;
  if (s->data) s->data[0] = 0;
  bui__str_add(s, text);
}

void bui__str_add(bui__str* s, const char* text) {
  text = bui__s(text);
  const size_t n = strlen(text);
  s->data = grow(s->data, &s->cap, s->len + n + 1, 1);
  memcpy(s->data + s->len, text, n + 1);
  s->len += n;
}

void bui__str_free(bui__str* s) {
  free(s->data);
  s->data = NULL, s->len = s->cap = 0;
}

/* ----------------------------------------------------------- the input */

void bui_input_clear(bui_input* in) {
  memset(in, 0, sizeof(*in));
  in->mouse.x = in->mouse.y = -1e9f;
  in->click_at = in->mouse;
}

bool bui_input_key(bui_input* in, bui_key key) {
  if (in->key_count >= BUI_MAX_KEYS) return false;
  in->keys[in->key_count++] = key;
  return true;
}

bool bui_input_pressed(bui_input* in, int64_t code) {
  if (in->pressed_count >= BUI_MAX_PRESSED) return false;
  in->pressed[in->pressed_count++] = code;
  return true;
}

/* --------------------------------------------------------- the context */

static const bui_strings kStrings = {
    "Item {i} of {n}", "Narrator on", "Narrator off. Press N to switch it back on", "tab", "Cancelled",
    {"button", "tab", "toggle", "slider", "selector", "text", ""}};

bui_context* bui_create(const bui_config* config) {
  bui_context* ui = calloc(1, sizeof(bui_context));
  if (!ui) return NULL;
  bui_config c = {0};
  if (config) c = *config;
  ui->voice = bui__voice_create(&c);
  bui__make_packs(ui);
  ui->strings = kStrings;
  ui->theme = c.theme ? *c.theme : bui_default_theme();
  ui->speech_on = true;
  bui_input_clear(&ui->in);
  bui_reset(ui);
  return ui;
}

void bui_destroy(bui_context* ui) {
  if (!ui) return;
  bui__voice_destroy(ui->voice);
  bui__free_packs(ui);
  bui__arena_free(&ui->arenas[0]), bui__arena_free(&ui->arenas[1]);
  free(ui->draw), free(ui->entries), free(ui->found), free(ui->hinted), free(ui->scrolls);
  bui__str* strs[] = {&ui->screen_name,  &ui->page_name,   &ui->last_page_name,  &ui->hint_keyboard, &ui->hint_gamepad,
                      &ui->events,       &ui->spoken_where, &ui->spoken_focus_id, &ui->last_line};
  for (size_t k = 0; k < sizeof(strs) / sizeof(strs[0]); ++k) bui__str_free(strs[k]);
  free(ui);
}

static void update_state(bui_context* ui);

void bui_reset(bui_context* ui) {
  bui__arena_reset(&ui->arenas[0]), bui__arena_reset(&ui->arenas[1]);
  ui->draw_count = ui->entry_count = ui->found_count = ui->hinted_count = ui->scroll_count = 0;
  ui->screen_id[0] = ui->last_screen_id[0] = ui->focus_id[0] = ui->hovered_id[0] = 0;
  ui->reveal_id[0] = ui->capture_id[0] = ui->captured_id[0] = 0;
  bui__str_set(&ui->screen_name, ""), bui__str_set(&ui->page_name, ""), bui__str_set(&ui->last_page_name, "");
  bui__str_set(&ui->events, ""), bui__str_set(&ui->spoken_where, ""), bui__str_set(&ui->spoken_focus_id, "");
  bui__str_set(&ui->last_line, "");
  ui->focus_index = ui->rows = 0;
  ui->navigating = ui->skip_arrival = ui->had_tabs = ui->has_tabs = false;
  ui->nav_anchored = true;
  ui->pending_activate = ui->pending_back = false;
  ui->pending_step = ui->pending_move = ui->tab_step = 0;
  ui->tab_jump = -1;
  ui->scroll_at = -1;
  ui->has_captured = ui->capture_cancelled = false;
  ui->holding = ui->hold_heard = false;
  ui->speech_toggled = false;
  ui->pending_sound = -1, ui->sound_requested = false, ui->last_change = -1e9, ui->sound_focus_id[0] = 0;
  update_state(ui);
}

bool bui_has_voice(const bui_context* ui) { return bui__voice_real(ui->voice); }
const bui_input* bui_get_input(const bui_context* ui) { return &ui->in; }
const bui_state* bui_get_state(const bui_context* ui) { return &ui->state; }

const bui_draw_cmd* bui_get_geometry(const bui_context* ui, size_t* count) {
  if (count) *count = ui->draw_count;
  return ui->draw;
}

bool bui_next_audio(bui_context* ui, bui_audio* out) { return bui__voice_next(ui->voice, out); }

void bui_audio_free(bui_audio* a) {
  if (!a) return;
  free(a->text), free(a->wav);
  memset(a, 0, sizeof(*a));
}

bool bui_synthesize(bui_context* ui, const char* text, bui_audio* out) { return bui__voice_synthesize(ui->voice, text, out); }

void bui_set_strings(bui_context* ui, const bui_strings* s) { ui->strings = s ? *s : kStrings; }
const bui_strings* bui_get_strings(const bui_context* ui) { return &ui->strings; }

void bui_set_theme(bui_context* ui, const bui_theme* theme) { ui->theme = theme ? *theme : bui_default_theme(); }
const bui_theme* bui_get_theme(const bui_context* ui) { return &ui->theme; }

void bui_draw(bui_context* ui, const bui_draw_cmd* cmds, size_t count) {
  ui->draw = grow(ui->draw, &ui->draw_cap, ui->draw_count + count, sizeof(bui_draw_cmd));
  memcpy(ui->draw + ui->draw_count, cmds, count * sizeof(bui_draw_cmd));
  ui->draw_count += count;
}

void bui_fill(bui_context* ui, bui_rect r, bui_color c) {
  const bui_draw_cmd cmd = {r.x0, r.y0, r.x1, r.y1, c.r, c.g, c.b, c.a, 0, 0, 0, 0};
  bui_draw(ui, &cmd, 1);
}

const char* bui_format(bui_context* ui, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const char* s = bui__arena_vprintf(bui__frame_arena(ui), fmt, args);
  va_end(args);
  return s;
}

/* -------------------------------------------------------------- speech */

static void speak(bui_context* ui, const char* line, bool queued) {
  bui__str_set(&ui->last_line, line);
  if (line && *line) bui__voice_say(ui->voice, line, queued);
}

void bui_say(bui_context* ui, const char* text, bool queued) {
  if (text && *text) bui__voice_say(ui->voice, text, queued);
}

void bui_stop_speech(bui_context* ui) { bui__voice_stop(ui->voice); }

void bui_set_speech_enabled(bui_context* ui, bool on, bool announce) {
  if (ui->speech_on && !on) bui__voice_stop(ui->voice);
  if (announce && on != ui->speech_on) ui->speech_toggled = true;
  ui->speech_on = on;
  ui->state.speech_enabled = on;
}

bool bui_speech_enabled(const bui_context* ui) { return ui->speech_on; }

void bui_announce(bui_context* ui, const char* text, bui_priority p) {
  if (!text || !*text) return;
  switch (p) {
    case BUI_SAY_NORMAL:
      if (ui->events.len) bui__str_add(&ui->events, "\x1f");  /* (a part's end) */
      bui__str_add(&ui->events, text);
      break;
    case BUI_SAY_INTERRUPT:
    case BUI_SAY_QUEUE:
      if (ui->speech_on) speak(ui, text, p == BUI_SAY_QUEUE);
      break;
    case BUI_SAY_HOLD:
      if (!ui->speech_on) break;
      speak(ui, text, false);
      ui->holding = true, ui->hold_heard = false, ui->hold_since = -1.0;  /* (timed from the next frame's end) */
      break;
  }
}

/* -------------------------------------------------------------- frames */

void bui_begin_frame(bui_context* ui, const bui_input* input) {
  if (input) ui->in = *input;
  else bui_input_clear(&ui->in);
  bui_input* in = &ui->in;
  ui->frame ^= 1;
  bui__arena_reset(bui__frame_arena(ui));
  ui->draw_count = ui->entry_count = 0;
  ui->rows = 0;
  ui->speech_toggled = false;
  ui->has_tabs = false;
  ui->hovered_id[0] = 0;
  ui->pending_activate = ui->pending_back = false;
  ui->pending_step = ui->pending_move = ui->tab_step = 0;
  ui->tab_jump = -1;
  ui->screen_id[0] = 0;
  bui__str_set(&ui->screen_name, ""), bui__str_set(&ui->page_name, "");
  bui__str_set(&ui->hint_keyboard, ""), bui__str_set(&ui->hint_gamepad, "");
  ui->scroll_at = -1;
  /* The mouse moved well away (or clicked): its turn again. */
  if (ui->navigating && !ui->nav_anchored) ui->nav_mouse = in->mouse, ui->nav_anchored = true;
  if (ui->navigating && (hypotf(in->mouse.x - ui->nav_mouse.x, in->mouse.y - ui->nav_mouse.y) > 8.0f || in->clicked))
    ui->navigating = false;

  /* A rebinding's wait: this frame's keys its (none of them navigating). */
  if (ui->capture_id[0]) {
    bool cancel = false;
    for (int k = 0; k < in->key_count; ++k) cancel = cancel || in->keys[k] == BUI_KEY_CANCEL_CAPTURE;
    if (cancel || in->time > ui->capture_until) {
      copy_id(ui->captured_id, ui->capture_id);
      ui->has_captured = false, ui->capture_cancelled = true;
      ui->capture_id[0] = 0;
      bui_announce(ui, ui->strings.cancelled, BUI_SAY_NORMAL);
    } else if (in->pressed_count > 0) {
      copy_id(ui->captured_id, ui->capture_id);
      ui->has_captured = true, ui->captured = in->pressed[0], ui->capture_cancelled = false;
      ui->capture_id[0] = 0;
    }
    return;
  }

  /* The keys: the first only shows where the focus is (the mouse's till then). */
  for (int k = 0; k < in->key_count; ++k) {
    const bui_key key = in->keys[k];
    bool go = true;  /* (navigating already: the key does its work) */
    if (key <= BUI_KEY_ACTIVATE && !(key == BUI_KEY_NEXT || key == BUI_KEY_PREV) && !ui->navigating) {
      ui->navigating = true, ui->nav_mouse = in->mouse, ui->nav_anchored = true;
      go = false;
    }
    switch (key) {
      case BUI_KEY_UP:
      case BUI_KEY_DOWN:
        if (go) ui->pending_move += key == BUI_KEY_DOWN ? 1 : -1;
        break;
      case BUI_KEY_NEXT:
      case BUI_KEY_PREV:
        if (ui->had_tabs) ui->tab_step += key == BUI_KEY_NEXT ? 1 : -1;
        else if (!ui->navigating) ui->navigating = true, ui->nav_mouse = in->mouse, ui->nav_anchored = true;
        else ui->pending_move += key == BUI_KEY_NEXT ? 1 : -1;
        break;
      case BUI_KEY_LEFT:
      case BUI_KEY_RIGHT:
        if (go) ui->pending_step = key == BUI_KEY_RIGHT ? 1 : -1;
        break;
      case BUI_KEY_ACTIVATE:
        if (go) ui->pending_activate = true;
        break;
      case BUI_KEY_BACK: ui->pending_back = true; break;
      case BUI_KEY_TAB_NEXT: ui->tab_step += 1; break;
      case BUI_KEY_TAB_PREV: ui->tab_step -= 1; break;
      case BUI_KEY_TOGGLE_SPEECH:
        ui->speech_on = !ui->speech_on, ui->speech_toggled = true;
        if (!ui->speech_on) bui__voice_stop(ui->voice);
        break;
      case BUI_KEY_REPEAT:
        if (ui->speech_on && ui->last_line.len) {
          bui__str line = {0};
          bui__str_set(&line, bui__str_c(&ui->last_line));
          speak(ui, line.data, false);
          bui__str_free(&line);
        }
        break;
      case BUI_KEY_CANCEL_CAPTURE: break;
    }
  }
  if (in->tab_hotkey > 0) ui->tab_jump = in->tab_hotkey - 1;
}

void bui_screen(bui_context* ui, const char* id, const char* name, const bui_hints* hints) {
  copy_id(ui->screen_id, id);
  bui__str_set(&ui->screen_name, name);
  bui__str_set(&ui->hint_keyboard, hints ? hints->keyboard : NULL);
  bui__str_set(&ui->hint_gamepad, hints ? hints->gamepad : NULL);
}

void bui_page(bui_context* ui, const char* name) { bui__str_set(&ui->page_name, name); }

int bui_take_tab_step(bui_context* ui) {
  ui->has_tabs = true;
  const int s = ui->tab_step;
  ui->tab_step = 0;
  return s;
}

int bui_take_tab_jump(bui_context* ui) {
  ui->has_tabs = true;
  const int j = ui->tab_jump;
  ui->tab_jump = -1;
  return j;
}

bool bui_back_pressed(const bui_context* ui) { return ui->pending_back; }
bool bui_navigating(const bui_context* ui) { return ui->navigating; }
void bui_begin_navigating(bui_context* ui) { ui->navigating = true, ui->nav_anchored = false; }
void bui_mark_arrived(bui_context* ui) { ui->skip_arrival = true; }
int bui_row_stripe(bui_context* ui) { return ui->rows++; }

/* -------------------------------------------------------------- items */

bui_result bui_declare(bui_context* ui, const char* id, const bui_item* it) {
  bui__arena* a = bui__frame_arena(ui);
  ui->entries = grow(ui->entries, &ui->entry_cap, ui->entry_count + 1, sizeof(bui__entry));
  bui__entry* e = &ui->entries[ui->entry_count++];
  e->id = bui__arena_dup(a, id);
  e->item = *it;
  e->item.spoken = bui__arena_dup(a, it->spoken);
  e->item.value = bui__arena_dup(a, it->value);
  e->item.changed = it->changed ? bui__arena_dup(a, it->changed) : NULL;
  e->item.role = bui__arena_dup(a, it->role);
  e->scroll = ui->scroll_at;
  e->shown = ui->scroll_at < 0 || bui_rect_inside(it->rect, ui->scrolls[ui->scroll_at].view);

  const bui_input* in = &ui->in;
  bui_result r = {0};
  const bool over = e->shown && bui_rect_contains(it->rect, in->mouse);
  if (over && !ui->navigating) copy_id(ui->hovered_id, id);
  r.focused = ui->navigating && !it->unfocusable && strcmp(e->id, ui->focus_id) == 0;
  r.hovered = ui->navigating ? r.focused : over;
  if (!it->disabled) {
    if (in->clicked && e->shown && bui_rect_contains(it->rect, in->click_at)) {
      if (!it->unfocusable) copy_id(ui->focus_id, id);
      const bui_vec2 c = in->click_at;
      if (!bui_rect_empty(it->prev) && bui_rect_contains(it->prev, c)) r.step = -1;
      else if (!bui_rect_empty(it->next) && bui_rect_contains(it->next, c)) r.step = 1;
      else if (!bui_rect_empty(it->press)) r.activated = bui_rect_contains(it->press, c);
      else if (bui_rect_empty(it->track)) r.activated = true;
    }
    if (r.focused && ui->pending_activate) r.activated = true, ui->pending_activate = false;
    if (r.focused && ui->pending_step) r.step = ui->pending_step, ui->pending_step = 0;
    if (it->back && ui->pending_back) r.activated = true, ui->pending_back = false;
    const bui_rect grab = bui_rect_make(it->track.x0 - 8, it->track.y0 - 4, it->track.x1 + 8, it->track.y1 + 4);
    if (!bui_rect_empty(it->track) && in->mouse_down && e->shown && !ui->navigating && bui_rect_contains(grab, in->mouse)) {
      const float w = bui_rect_w(it->track);
      r.dragging = true;
      r.drag = clampf((in->mouse.x - it->track.x0) / (w > 1.0f ? w : 1.0f), 0.0f, 1.0f);
    }
    if (r.activated) bui__auto_sound(ui, it->back ? BUI_SOUND_BACK : BUI_SOUND_PRESS);
  } else {
    /* (Pressed or stepped while disabled: heard, not changed.) */
    const bool clicked = in->clicked && e->shown && bui_rect_contains(it->rect, in->click_at);
    if (clicked || (r.focused && (ui->pending_activate || ui->pending_step))) bui__auto_sound(ui, BUI_SOUND_DENIED);
    if (r.focused) ui->pending_activate = false, ui->pending_step = 0;
  }
  return r;
}

void bui_revise(bui_context* ui, const char* value, const char* spoken) {
  if (!ui->entry_count) return;
  bui__entry* e = &ui->entries[ui->entry_count - 1];
  e->item.value = bui__arena_dup(bui__frame_arena(ui), value);
  e->item.spoken = bui__arena_dup(bui__frame_arena(ui), spoken);
}

/* ----------------------------------------------------------- scrolling */

static int scroll_index(const bui_context* ui, const char* id) {
  for (size_t k = 0; k < ui->scroll_count; ++k)
    if (strncmp(ui->scrolls[k].id, bui__s(id), BUI_ID_MAX - 1) == 0) return (int)k;
  return -1;
}

static bui__scroll* scroll_of(bui_context* ui, const char* id) {
  int k = scroll_index(ui, id);
  if (k < 0) {
    ui->scrolls = grow(ui->scrolls, &ui->scroll_cap, ui->scroll_count + 1, sizeof(bui__scroll));
    k = (int)ui->scroll_count++;
    memset(&ui->scrolls[k], 0, sizeof(bui__scroll));
    copy_id(ui->scrolls[k].id, id);
  }
  return &ui->scrolls[k];
}

static void clamp_scroll(bui__scroll* s) {
  const float most = s->content - bui_rect_h(s->view);
  s->offset = clampf(s->offset, 0.0f, most > 0.0f ? most : 0.0f);
}

float bui_begin_scroll(bui_context* ui, const char* id, bui_rect view, float content) {
  bui__scroll* s = scroll_of(ui, id);
  ui->scroll_at = (int)(s - ui->scrolls);
  s->view = view, s->content = content;
  if (ui->in.wheel != 0.0f && bui_rect_contains(view, ui->in.mouse)) s->offset -= ui->in.wheel * 0.15f * bui_rect_h(view);
  clamp_scroll(s);
  return s->offset;
}

void bui_end_scroll(bui_context* ui) { ui->scroll_at = -1; }

bool bui_visible(const bui_context* ui, bui_rect r) {
  return ui->scroll_at < 0 || bui_rect_inside(r, ui->scrolls[ui->scroll_at].view);
}

float bui_scroll_of(const bui_context* ui, const char* id) {
  const int k = scroll_index(ui, id);
  return k < 0 ? 0.0f : ui->scrolls[k].offset;
}

void bui_set_scroll(bui_context* ui, const char* id, float offset) { scroll_of(ui, id)->offset = offset; }
void bui_reveal(bui_context* ui, const char* id) { copy_id(ui->reveal_id, id); }

/* ----------------------------------------------------------- capturing */

void bui_start_capture(bui_context* ui, const char* id, double seconds) {
  copy_id(ui->capture_id, id);
  ui->capture_until = ui->in.time + seconds;
  ui->captured_id[0] = 0;
}

bool bui_capturing(const bui_context* ui, const char* id) {
  return id ? ui->capture_id[0] && strcmp(ui->capture_id, id) == 0 : ui->capture_id[0] != 0;
}

bool bui_take_captured(bui_context* ui, const char* id, int64_t* code, bool* cancelled) {
  if (cancelled) *cancelled = false;
  if (!ui->captured_id[0] || strcmp(ui->captured_id, bui__s(id)) != 0) return false;
  ui->captured_id[0] = 0;
  if (cancelled) *cancelled = ui->capture_cancelled;
  if (code && ui->has_captured) *code = ui->captured;
  return true;
}

/* --------------------------------------------------------------- focus */

const char* bui_focused_id(const bui_context* ui) { return ui->focus_id; }
void bui_focus(bui_context* ui, const char* id) { copy_id(ui->focus_id, id); }

const bui_item_state* bui_find(const bui_context* ui, const char* key) {
  key = bui__s(key);
  for (size_t k = 0; k < ui->found_count; ++k)
    if (strcmp(ui->found[k].id, key) == 0) return &ui->found[k];
  const size_t n = strlen(key);
  for (size_t k = 0; k < ui->found_count; ++k)
    if (strncmp(ui->found[k].spoken, key, n) == 0) return &ui->found[k];
  return NULL;
}

/* ------------------------------------------------------- the frame's end */

static void where(const bui_context* ui, bui__str* out) {
  bui__str_set(out, "");
  if (!ui->screen_name.len) return;
  bui__str_add(out, ui->screen_name.data);
  if (!ui->page_name.len) return;
  bui__str_add(out, ", "), bui__str_add(out, ui->page_name.data);
  bui__str_add(out, " "), bui__str_add(out, ui->strings.tab);
}

/* "{i}" and "{n}" in `pattern` replaced. */
static void item_of(bui__str* out, const char* pattern, int i, int n) {
  for (const char* p = bui__s(pattern); *p; ++p) {
    if (p[0] == '{' && (p[1] == 'i' || p[1] == 'n') && p[2] == '}') {
      char num[16];
      snprintf(num, sizeof(num), "%d", p[1] == 'i' ? i : n);
      bui__str_add(out, num);
      p += 2;
    } else {
      const char c[2] = {*p, 0};
      bui__str_add(out, c);
    }
  }
}

/* What's said of an entry when it's come to: "Its words. Item i of n, its kind". */
static void focus_line(const bui_context* ui, size_t i, bui__str* out) {
  const bui__entry* e = &ui->entries[i];
  bui__str_add(out, e->item.spoken);
  if (e->item.unfocusable || !*e->item.spoken) return;
  int index = 0, n = 0;
  for (size_t k = 0; k < ui->entry_count; ++k)
    if (!ui->entries[k].item.unfocusable) index += k < i, ++n;
  bui__str_add(out, ". ");
  item_of(out, ui->strings.item_of, index + 1, n);
  const char* kind = e->item.kind == BUI_CUSTOM ? e->item.role : ui->strings.kinds[e->item.kind];
  if (kind && *kind) bui__str_add(out, ", "), bui__str_add(out, kind);
}

static void outline(bui_context* ui, bui_rect b) {
  const float w = ui->theme.focus_width * ui->theme.pixel, g = ui->theme.focus_gap * ui->theme.pixel;
  const bui_rect o = bui_rect_make(b.x0 - g - w, b.y0 - g - w, b.x1 + g + w, b.y1 + g + w);
  const bui_rect sides[4] = {bui_rect_make(o.x0, o.y0, o.x1, o.y0 + w), bui_rect_make(o.x0, o.y1 - w, o.x1, o.y1),
                             bui_rect_make(o.x0, o.y0, o.x0 + w, o.y1), bui_rect_make(o.x1 - w, o.y0, o.x1, o.y1)};
  for (int k = 0; k < 4; ++k) bui_fill(ui, sides[k], ui->theme.focus);
}

static void scroll_to(bui_context* ui, const bui__entry* e) {
  if (e->scroll < 0) return;
  bui__scroll* s = &ui->scrolls[e->scroll];
  if (e->item.rect.y0 < s->view.y0) s->offset -= s->view.y0 - e->item.rect.y0;
  else if (e->item.rect.y1 > s->view.y1) s->offset += e->item.rect.y1 - s->view.y1;
  clamp_scroll(s);
}

static bool hinted(const bui_context* ui, const char* key) {
  for (size_t k = 0; k < ui->hinted_count; ++k)
    if (strcmp(ui->hinted[k], key) == 0) return true;
  return false;
}

static void add_hinted(bui_context* ui, const char* key) {
  if (hinted(ui, key)) return;
  ui->hinted = grow(ui->hinted, &ui->hinted_cap, ui->hinted_count + 1, sizeof(*ui->hinted));
  snprintf(ui->hinted[ui->hinted_count++], sizeof(*ui->hinted), "%s", key);
}

static void update_state(bui_context* ui) {
  bui_state* s = &ui->state;
  s->items = ui->found, s->item_count = ui->found_count;
  s->screen = ui->last_screen_id, s->page = bui__str_c(&ui->last_page_name), s->focused = ui->focus_id;
  s->capturing = ui->capture_id[0] ? ui->capture_id : NULL;
  s->last_line = bui__str_c(&ui->last_line);
  s->navigating = ui->navigating, s->speech_enabled = ui->speech_on, s->speech_toggled = ui->speech_toggled;
  s->back_pressed = ui->pending_back, s->device = ui->in.device;
}

/* A part of the frame's line (". " between them). */
static void add_part(bui__str* line, const char* part) {
  if (!part || !*part) return;
  if (line->len) bui__str_add(line, ". ");
  bui__str_add(line, part);
}

void bui_end_frame(bui_context* ui) {
  const bui_input* in = &ui->in;
  ui->had_tabs = ui->has_tabs;
  /* The focus: on the screen's items (a new screen or page: its first). */
  size_t* nav = malloc((ui->entry_count + 1) * sizeof(size_t));
  if (!nav) abort();
  int n = 0;
  for (size_t i = 0; i < ui->entry_count; ++i)
    if (!ui->entries[i].item.unfocusable) nav[n++] = i;
  const bool arrived = strcmp(ui->screen_id, ui->last_screen_id) != 0 || strcmp(bui__str_c(&ui->page_name), bui__str_c(&ui->last_page_name)) != 0;
  int index = -1, hovered = -1;
  for (int k = 0; k < n; ++k) {
    if (strcmp(ui->entries[nav[k]].id, ui->focus_id) == 0) index = k;
    if (ui->hovered_id[0] && strcmp(ui->entries[nav[k]].id, ui->hovered_id) == 0) hovered = k;
  }
  /* (A new screen: its first. Its focused item gone (folded away): the one in its place.) */
  if (arrived || (index < 0 && n > 0)) index = arrived ? 0 : (ui->focus_index < n - 1 ? ui->focus_index : n - 1);
  if (!ui->navigating && hovered >= 0) index = hovered;
  if (n == 0) index = -1;
  bool moved = arrived;
  if (index >= 0 && ui->pending_move != 0) {
    index = ((index + ui->pending_move) % n + n) % n;
    moved = true;
  }
  copy_id(ui->focus_id, index >= 0 ? ui->entries[nav[index]].id : "");
  ui->focus_index = index > 0 ? index : 0;
  /* (Moved to one scrolled out of its view, or asked for: scrolled to it, next frame.) */
  if (ui->navigating && moved && index >= 0) scroll_to(ui, &ui->entries[nav[index]]);
  if (ui->reveal_id[0]) {
    for (size_t i = 0; i < ui->entry_count; ++i)
      if (strcmp(ui->entries[i].id, ui->reveal_id) == 0) scroll_to(ui, &ui->entries[i]);
    ui->reveal_id[0] = 0;
  }
  if (ui->navigating && index >= 0 && ui->entries[nav[index]].shown) outline(ui, ui->entries[nav[index]].item.rect);

  /* The entry spoken of: the focus's, or the mouse's. */
  int said = -1;
  if (ui->navigating && index >= 0) said = (int)nav[index];
  else if (!ui->navigating)
    for (size_t i = 0; i < ui->entry_count; ++i)
      if (ui->hovered_id[0] && strcmp(ui->entries[i].id, ui->hovered_id) == 0) said = (int)i;
  const char* said_id = said >= 0 ? ui->entries[said].id : "";
  /* Its value changed since the last frame (the last frame's state: still there). */
  bool value_changed = false;
  if (said >= 0 && strcmp(said_id, bui__str_c(&ui->spoken_focus_id)) == 0) {
    for (size_t k = 0; k < ui->found_count; ++k)
      if (strcmp(ui->found[k].id, said_id) == 0) value_changed = strcmp(ui->found[k].value, ui->entries[said].item.value) != 0;
  }

  /* The frame's GUI sound: a new screen or page, the focus moved, a value changed. */
  {
    const bool new_screen = strcmp(ui->screen_id, ui->last_screen_id) != 0;
    const bool same_item = said >= 0 && strcmp(said_id, ui->sound_focus_id) == 0;
    if (new_screen) bui__auto_sound(ui, BUI_SOUND_SCREEN);
    else if (arrived) bui__auto_sound(ui, BUI_SOUND_TAB);
    else if (said >= 0 && !same_item) bui__auto_sound(ui, BUI_SOUND_MOVE);
    /* (A value dragged changes every frame: at most one change sound every 70 ms.) */
    if (same_item && in->time - ui->last_change > 0.07) {
      for (size_t k = 0; k < ui->found_count; ++k)
        if (strcmp(ui->found[k].id, said_id) == 0 && strcmp(ui->found[k].value, ui->entries[said].item.value) != 0)
          bui__auto_sound(ui, BUI_SOUND_CHANGE);
    }
    copy_id(ui->sound_focus_id, said_id);
    if (ui->pending_sound == BUI_SOUND_CHANGE) ui->last_change = in->time;
    bui__emit_sound(ui);
  }

  /* The state: this frame's items. */
  ui->found = grow(ui->found, &ui->found_cap, ui->entry_count + 1, sizeof(bui_item_state));
  ui->found_count = ui->entry_count;
  for (size_t i = 0, k = 0; i < ui->entry_count; ++i) {
    const bui__entry* e = &ui->entries[i];
    bui_item_state* f = &ui->found[i];
    f->id = e->id, f->kind = e->item.kind, f->rect = e->item.rect, f->spoken = e->item.spoken, f->value = e->item.value;
    f->enabled = !e->item.disabled, f->focusable = !e->item.unfocusable, f->shown = e->shown;
    f->index = f->focusable ? (int)k++ : -1;
    f->focused = ui->navigating && index >= 0 && nav[index] == i;
    f->hovered = !ui->navigating && ui->hovered_id[0] && strcmp(e->id, ui->hovered_id) == 0;
  }
  free(nav);
  copy_id(ui->last_screen_id, ui->screen_id);
  bui__str_set(&ui->last_page_name, bui__str_c(&ui->page_name));

  /* What's said: the screen come to (its keys, the first time), what has
   * the focus (or is under the mouse), its value changed, what happened. */
  bui__str w = {0}, line = {0};
  where(ui, &w);
  if (ui->skip_arrival) {
    char key[BUI_ID_MAX + 8];
    ui->skip_arrival = false;
    bui__str_set(&ui->spoken_where, bui__str_c(&w));
    add_hinted(ui, ui->screen_id);
    snprintf(key, sizeof(key), "%s#pad", ui->screen_id);
    add_hinted(ui, key);
    if (ui->navigating && index >= 0) bui__str_set(&ui->spoken_focus_id, "");  /* (its item still said, after) */
  }
  if (ui->speech_toggled && !ui->speech_on) {
    speak(ui, ui->strings.speech_off, false);
    bui__str_set(&ui->events, "");
    bui__str_set(&ui->spoken_where, bui__str_c(&w)), bui__str_set(&ui->spoken_focus_id, said_id);
    goto done;
  }
  /* (The opening line held: the rest waits for it, a key or a click.) */
  if (ui->holding) {
    const bool busy = bui__voice_busy(ui->voice) || in->speech_playing;
    if (ui->hold_since < 0.0) ui->hold_since = in->time;
    if (busy) ui->hold_heard = true;
    const bool over = (ui->hold_heard && !busy) || in->time - ui->hold_since > 15.0 || in->key_count > 0 || in->clicked || !ui->speech_on;
    if (!over) goto done;
    ui->holding = false;
  }
  if (ui->speech_toggled) {
    add_part(&line, ui->strings.speech_on);
    bui__str_set(&ui->spoken_where, ""), bui__str_set(&ui->spoken_focus_id, "\x01");
  }
  if (strcmp(bui__str_c(&w), bui__str_c(&ui->spoken_where)) != 0) {
    if (w.len) {
      const bool pad = in->device == BUI_DEVICE_GAMEPAD;
      const char* hint = pad && ui->hint_gamepad.len ? ui->hint_gamepad.data : bui__str_c(&ui->hint_keyboard);
      char key[BUI_ID_MAX + 8];
      snprintf(key, sizeof(key), "%s%s", ui->screen_id, pad ? "#pad" : "");
      add_part(&line, w.data);
      if (*hint && ui->speech_on && !hinted(ui, key)) add_hinted(ui, key), add_part(&line, hint);
    }
    bui__str_set(&ui->spoken_where, bui__str_c(&w));
    /* (Its item too, but not the tab just clicked: its name's just been said.) */
    if (said >= 0 && ui->entries[said].item.kind != BUI_TAB) {
      bui__str part = {0};
      focus_line(ui, (size_t)said, &part);
      add_part(&line, part.data);
      bui__str_free(&part);
    }
    bui__str_set(&ui->spoken_focus_id, said_id);
  } else if (strcmp(said_id, bui__str_c(&ui->spoken_focus_id)) != 0) {
    if (said >= 0) {
      bui__str part = {0};
      focus_line(ui, (size_t)said, &part);
      add_part(&line, part.data);
      bui__str_free(&part);
    }
    bui__str_set(&ui->spoken_focus_id, said_id);
  } else if (value_changed) {
    const bui_item* it = &ui->entries[said].item;
    add_part(&line, it->changed ? it->changed : it->spoken);
  }
  /* What was announced (its parts apart). */
  for (char* p = ui->events.data; p && *p;) {
    char* end = strchr(p, '\x1f');
    if (end) *end = 0;
    add_part(&line, p);
    p = end ? end + 1 : p + strlen(p);
  }
  bui__str_set(&ui->events, "");
  if (line.len && ui->speech_on) speak(ui, line.data, false);
done:
  bui__str_free(&w), bui__str_free(&line);
  update_state(ui);
}
