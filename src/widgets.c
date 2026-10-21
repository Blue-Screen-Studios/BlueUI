#include "blueui/widgets.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

#include "internal.h"

bui_theme bui_default_theme(void) {
  bui_theme t;
  t.pixel = 2.0f;
  t.text = bui_rgb(0.92f, 0.94f, 0.88f), t.dim = bui_rgb(0.6f, 0.72f, 0.6f), t.blurb = bui_rgb(0.55f, 0.64f, 0.55f);
  t.value = bui_rgb(0.92f, 0.85f, 0.4f), t.off = bui_rgb(0.4f, 0.46f, 0.4f);
  t.stripe = bui_rgba(1.0f, 1.0f, 1.0f, 0.04f), t.hover = bui_rgba(0.3f, 0.6f, 0.3f, 0.12f), t.rule = bui_rgb(0.16f, 0.4f, 0.14f);
  t.edge = bui_rgb(0.38f, 0.5f, 0.3f), t.panel = bui_rgba(0.025f, 0.06f, 0.035f, 0.97f), t.title_bar = bui_rgb(0.09f, 0.26f, 0.1f);
  t.title = bui_rgb(1.0f, 1.0f, 1.0f), t.title_right = bui_rgb(0.6f, 0.9f, 0.45f);
  t.tab_on = bui_rgb(0.16f, 0.4f, 0.14f), t.tab_off = bui_rgb(0.06f, 0.12f, 0.06f), t.tab_hot = bui_rgb(0.11f, 0.2f, 0.1f);
  t.tab_text = bui_rgb(1.0f, 1.0f, 1.0f), t.tab_text_off = bui_rgb(0.62f, 0.75f, 0.6f);
  t.toggle_on = bui_rgb(0.12f, 0.42f, 0.15f), t.toggle_on_hot = bui_rgb(0.18f, 0.56f, 0.2f), t.toggle_off = bui_rgb(0.2f, 0.22f, 0.2f);
  t.toggle_off_hot = bui_rgb(0.26f, 0.28f, 0.26f), t.toggle_text_on = bui_rgb(1.0f, 1.0f, 1.0f), t.toggle_text_off = bui_rgb(0.7f, 0.72f, 0.7f);
  t.well = bui_rgb(0.04f, 0.09f, 0.04f), t.arrow = bui_rgb(0.12f, 0.36f, 0.13f), t.arrow_hot = bui_rgb(0.18f, 0.5f, 0.18f);
  t.choice_text = bui_rgb(1.0f, 0.95f, 0.8f);
  t.track = bui_rgb(0.1f, 0.14f, 0.1f), t.fill = bui_rgb(0.3f, 0.7f, 0.22f), t.knob = bui_rgb(0.92f, 0.92f, 0.7f);
  t.knob_hot = bui_rgb(1.0f, 0.92f, 0.7f), t.knob_off = bui_rgb(0.35f, 0.38f, 0.35f), t.tick = bui_rgb(0.45f, 0.6f, 0.42f);
  t.button_edge = bui_rgb(0.55f, 0.48f, 0.12f), t.button = bui_rgba(0.06f, 0.14f, 0.06f, 0.95f);
  t.button_hot = bui_rgba(0.16f, 0.3f, 0.12f, 0.95f), t.button_text = bui_rgb(1.0f, 0.95f, 0.8f), t.button_text_off = bui_rgb(0.5f, 0.5f, 0.5f);
  t.button_edged = true;
  t.control_width = 120.0f, t.arrow_width = 11.0f, t.blurb_at = 130.0f;
  return t;
}

/* ------------------------------------------------------------ helpers */

static float px(const bui_theme* t, float pixel) { return pixel > 0.0f ? pixel : t->pixel; }
static float maxf(float a, float b) { return a > b ? a : b; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float width(const char* s, float p) { return bui_text_width(s, BUI_NUL, p); }
static bool has(const char* s) { return s && *s; }

/* A copy in the frame's arena, its letters changed. */
static const char* recased(bui_context* ui, const char* s, int (*to)(int)) {
  char* out = bui__arena_dup(bui__frame_arena(ui), s);
  for (char* c = out; *c; ++c) *c = (char)to((unsigned char)*c);
  return out;
}
static const char* upper(bui_context* ui, const char* s) { return recased(ui, s, toupper); }
static const char* lower(bui_context* ui, const char* s) { return recased(ui, s, tolower); }

void bui_label(bui_context* ui, const bui_theme* t, const char* text, float x, float y, bui_color c, float shadow, float pixel) {
  bui_text(ui, text, x, y, px(t, pixel), c, shadow);
}

void bui_centered(bui_context* ui, const bui_theme* t, bui_rect r, const char* text, bui_color c, float pixel) {
  const float p = px(t, pixel);
  bui_text(ui, text, roundf(0.5f * (r.x0 + r.x1 - width(text, p))), roundf(0.5f * (r.y0 + r.y1 - BUI_GLYPH_H * p)), p, c, 0.0f);
}

/* A row's name, value and description, its stripe, its light under the mouse. */
static void row_text(bui_context* ui, const bui_theme* t, const bui_row_parts* rp, const bui_field* f, const char* description,
                     const char* shown, bool hovered) {
  const float p = t->pixel;
  const bool even = bui_row_stripe(ui) % 2 == 0;
  if (!bui_visible(ui, rp->row)) return;
  if (even) bui_fill(ui, rp->row, t->stripe);
  if (hovered && !bui_navigating(ui)) bui_fill(ui, rp->row, t->hover);
  const float ty = roundf(0.5f * (rp->row.y0 + rp->row.y1 - BUI_GLYPH_H * p));
  bui_text(ui, upper(ui, f->name), rp->row.x0 + 3.0f * p, ty, p, f->disabled ? t->off : t->text, 0.0f);
  const float shown_x = rp->control.x0 - 8.0f * p - width(shown, p);
  if (has(shown)) bui_text(ui, shown, shown_x, ty, p, f->disabled ? t->off : t->value, 0.0f);
  /* (What it does, when it fits before its value and its control.) */
  const float blurb_x = rp->row.x0 + maxf(t->blurb_at * p, 0.3f * bui_rect_w(rp->row));
  if (has(description) && blurb_x + width(description, p) < (has(shown) ? shown_x : rp->control.x0) - 8.0f * p)
    bui_text(ui, description, blurb_x, ty, p, f->disabled ? t->off : t->blurb, 0.0f);
}

/* What's said of a row: its own words, or its name and value (what it does
 * too while it's off: why). */
static const char* row_spoken(bui_context* ui, const bui_field* f, const char* value, const char* description) {
  if (has(f->spoken)) return f->spoken;
  return bui_format(ui, "%s%s%s%s%s", bui__s(f->name), has(value) ? ", " : "", bui__s(value), f->disabled && has(description) ? ". " : "",
                    f->disabled && has(description) ? description : "");
}

/* ------------------------------------------------------------ widgets */

void bui_window(bui_context* ui, const bui_theme* t, bui_rect r, const char* title, const char* right) {
  const float p = t->pixel;
  bui_fill(ui, bui_rect_grow(r, p), t->edge);
  bui_fill(ui, r, t->panel);
  bui_fill(ui, bui_rect_make(r.x0, r.y0, r.x1, r.y0 + 14.0f * p), t->title_bar);
  bui_text(ui, title, r.x0 + 6.0f * p, r.y0 + 4.0f * p, p, t->title, 0.7f);
  if (has(right)) bui_text(ui, right, r.x1 - 6.0f * p - width(right, p), r.y0 + 4.0f * p, p, t->title_right, 0.7f);
}

bool bui_button(bui_context* ui, const bui_theme* t, const char* id, const char* text, bui_rect r, const bui_button_opts* o) {
  const bui_button_opts none = {0};
  if (!o) o = &none;
  bui_item it = {0};
  it.kind = BUI_BUTTON, it.rect = r, it.disabled = o->disabled, it.back = o->back;
  it.spoken = has(o->spoken) ? o->spoken : text;
  const bui_result res = bui_declare(ui, id, &it);
  if (!bui_visible(ui, r)) return res.activated;
  const float p = px(t, o->pixel);
  const bool hot = !o->disabled && res.hovered;
  if (t->button_edged) bui_fill(ui, bui_rect_grow(r, p), t->button_edge);
  bui_fill(ui, r, hot ? t->button_hot : t->button);
  bui_text(ui, text, roundf(0.5f * (r.x0 + r.x1 - width(text, p))), roundf(0.5f * (r.y0 + r.y1 - BUI_GLYPH_H * p)), p,
           o->disabled ? t->button_text_off : t->button_text, 0.7f);
  return res.activated;
}

bool bui_tab_bar(bui_context* ui, const bui_theme* t, const char* id, bui_rect area, const char* const* names, int n, int* current,
                 float min_width) {
  const float p = t->pixel;
  if (n <= 0) return false;
  if (min_width <= 0.0f) min_width = 64.0f;
  const int was = *current;
  const int step = bui_take_tab_step(ui);
  if (step) *current = ((*current + step) % n + n) % n;
  const int jump = bui_take_tab_jump(ui);
  if (jump >= 0 && jump < n) *current = jump;
  float x = area.x0;
  for (int k = 0; k < n; ++k) {
    const char* shown = bui_format(ui, "%d %s", k + 1, upper(ui, names[k]));
    const float w = maxf(min_width * p, width(shown, p) + 12.0f * p);
    const bui_rect b = bui_rect_make(x, area.y0, x + w, area.y1);
    x += w + 2.0f * p;
    bui_item it = {0};
    it.kind = BUI_TAB, it.rect = b, it.unfocusable = true;
    it.spoken = bui_format(ui, "%s %s", names[k], bui_get_strings(ui)->tab);
    const bui_result r = bui_declare(ui, bui_format(ui, "%s#%d", id, k), &it);
    if (r.activated) *current = k;
    const bool on = k == *current;
    bui_fill(ui, b, on ? t->tab_on : r.hovered ? t->tab_hot : t->tab_off);
    bui_centered(ui, t, b, shown, on ? t->tab_text : t->tab_text_off, 0.0f);
  }
  bui_fill(ui, bui_rect_make(area.x0, area.y1, area.x1, area.y1 + p), t->tab_on);
  bui_page(ui, names[clampi(*current, 0, n - 1)]);
  return *current != was;
}

bui_row_parts bui_row_layout(const bui_theme* t, bui_rect row) {
  const float p = t->pixel, a = t->arrow_width * p;
  bui_row_parts rp;
  rp.row = row;
  rp.control = bui_rect_make(row.x1 - 2.0f * p - t->control_width * p, row.y0 + 3.0f * p, row.x1 - 2.0f * p, row.y1 - 3.0f * p);
  rp.prev = bui_rect_make(rp.control.x0, rp.control.y0, rp.control.x0 + a, rp.control.y1);
  rp.next = bui_rect_make(rp.control.x1 - a, rp.control.y0, rp.control.x1, rp.control.y1);
  return rp;
}

bool bui_toggle_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, bool* on, const char* on_text,
                    const char* off_text) {
  const bui_option off = {has(off_text) ? off_text : "OFF", NULL, NULL}, yes = {has(on_text) ? on_text : "ON", NULL, NULL};
  return bui_toggle_row_ex(ui, t, id, row, f, on, &off, &yes);
}

/* A toggle's state: what it means, what's said of it. */
static const char* toggle_described(const bui_field* f, const bui_option* o) { return has(o->description) ? o->description : f->description; }
static const char* toggle_said(bui_context* ui, const bui_field* f, const bui_option* o) {
  return has(o->spoken) ? o->spoken : row_spoken(ui, f, lower(ui, o->shown), toggle_described(f, o));
}

bool bui_toggle_row_ex(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, bool* on,
                       const bui_option* off, const bui_option* on_option) {
  const bui_row_parts rp = bui_row_layout(t, row);
  const bui_option* state = *on ? on_option : off;
  bui_item it = {0};
  it.kind = BUI_TOGGLE, it.rect = row, it.press = rp.control, it.disabled = f->disabled, it.value = state->shown;
  it.spoken = toggle_said(ui, f, state);
  const bui_result r = bui_declare(ui, id, &it);
  const bool was = *on;
  if (r.activated || r.step) *on = !*on;
  state = *on ? on_option : off;
  if (*on != was) bui_revise(ui, state->shown, toggle_said(ui, f, state));
  row_text(ui, t, &rp, f, toggle_described(f, state), NULL, r.hovered);
  if (!bui_visible(ui, row)) return *on != was;
  const bool hot = !f->disabled && r.hovered;
  bui_fill(ui, rp.control, *on ? (hot ? t->toggle_on_hot : t->toggle_on) : (hot ? t->toggle_off_hot : t->toggle_off));
  bui_centered(ui, t, rp.control, state->shown, *on ? t->toggle_text_on : t->toggle_text_off, 0.0f);
  return *on != was;
}

/* A choice's option: what's said of it. */
static const char* choice_said(bui_context* ui, const bui_field* f, const bui_option* o) {
  const char* d = has(o->description) ? o->description : bui__s(f->description);
  if (has(o->spoken)) return o->spoken;
  const char* s = row_spoken(ui, f, o->shown, d);
  return !has(f->spoken) && !f->disabled && has(d) ? bui_format(ui, "%s. %s", s, d) : s;
}

bool bui_choice_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, int* index,
                    const bui_option* options, int n) {
  if (n <= 0) return false;
  *index = clampi(*index, 0, n - 1);
  const bui_row_parts rp = bui_row_layout(t, row);
  bui_item it = {0};
  it.kind = BUI_CHOICE, it.rect = row, it.prev = rp.prev, it.next = rp.next, it.press = rp.next, it.disabled = f->disabled;
  it.value = options[*index].shown, it.spoken = choice_said(ui, f, &options[*index]);
  const bui_result r = bui_declare(ui, id, &it);
  const int was = *index;
  if (r.step) *index = ((*index + r.step) % n + n) % n;
  else if (r.activated) *index = (*index + 1) % n;
  const bui_option* o = &options[*index];
  if (*index != was) bui_revise(ui, o->shown, choice_said(ui, f, o));
  row_text(ui, t, &rp, f, has(o->description) ? o->description : f->description, NULL, r.hovered);
  if (!bui_visible(ui, row)) return *index != was;
  const bui_vec2 mouse = bui_get_input(ui)->mouse;
  bui_fill(ui, rp.control, t->well);
  bui_fill(ui, rp.prev, bui_rect_contains(rp.prev, mouse) && !f->disabled ? t->arrow_hot : t->arrow);
  bui_fill(ui, rp.next, bui_rect_contains(rp.next, mouse) && !f->disabled ? t->arrow_hot : t->arrow);
  bui_centered(ui, t, rp.prev, "<", bui_rgb(1, 1, 1), 0.0f);
  bui_centered(ui, t, rp.next, ">", bui_rgb(1, 1, 1), 0.0f);
  bui_centered(ui, t, bui_rect_make(rp.prev.x1, rp.control.y0, rp.next.x0, rp.control.y1), o->shown, f->disabled ? t->off : t->choice_text,
               0.0f);
  return *index != was;
}

/* A slider's track and knob (and its stops marked). */
static void slider_look(bui_context* ui, const bui_theme* t, const bui_row_parts* rp, float fraction, bool enabled, bool hot, int stops) {
  const float p = t->pixel;
  const bui_rect c = rp->control;
  fraction = fraction < 0.0f ? 0.0f : fraction > 1.0f ? 1.0f : fraction;
  const float my = 0.5f * (c.y0 + c.y1), kx = c.x0 + fraction * bui_rect_w(c);
  bui_fill(ui, bui_rect_make(c.x0, my - 2.0f * p, c.x1, my + 2.0f * p), t->track);
  if (enabled) bui_fill(ui, bui_rect_make(c.x0, my - 2.0f * p, kx, my + 2.0f * p), t->fill);
  for (int k = 0; k < stops; ++k) {
    const float sx = roundf(c.x0 + (float)k / (float)(stops > 1 ? stops - 1 : 1) * bui_rect_w(c));
    bui_fill(ui, bui_rect_make(sx - 0.5f * p, my + 2.0f * p, sx + 0.5f * p, c.y1), t->tick);
  }
  bui_fill(ui, bui_rect_make(kx - 2.0f * p, c.y0, kx + 2.0f * p, c.y1), enabled ? (hot ? t->knob_hot : t->knob) : t->knob_off);
}

/* A slider's value: as shown, as said. */
static const char* slider_shown(bui_context* ui, bui_format_fn shown, float v, void* user) {
  char buf[128] = "";
  if (shown) shown(v, buf, sizeof(buf), user);
  return bui_format(ui, "%s", buf);
}
static const char* slider_said(bui_context* ui, const bui_field* f, bui_format_fn shown, bui_format_fn spoken, float v, void* user) {
  if (spoken) {
    char buf[512] = "";
    spoken(v, buf, sizeof(buf), user);
    return bui_format(ui, "%s", buf);
  }
  return row_spoken(ui, f, slider_shown(ui, shown, v, user), f->description);
}

bool bui_slider_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, float* value, float lo, float hi,
                    float step, bui_format_fn shown, bui_format_fn spoken, void* user) {
  const bui_row_parts rp = bui_row_layout(t, row);
  bui_item it = {0};
  it.kind = BUI_SLIDER, it.rect = row, it.track = rp.control, it.disabled = f->disabled;
  it.value = slider_shown(ui, shown, *value, user), it.spoken = slider_said(ui, f, shown, spoken, *value, user);
  const bui_result r = bui_declare(ui, id, &it);
  const float was = *value;
  if (r.step && step > 0.0f) {
    const float v = roundf((*value + (float)r.step * step - lo) / step) * step + lo;
    *value = v < lo ? lo : v > hi ? hi : v;
  }
  if (r.dragging) *value = lo + r.drag * (hi - lo);
  const char* now = slider_shown(ui, shown, *value, user);
  if (*value != was) bui_revise(ui, now, slider_said(ui, f, shown, spoken, *value, user));
  row_text(ui, t, &rp, f, f->description, now, r.hovered);
  if (bui_visible(ui, row)) slider_look(ui, t, &rp, hi > lo ? (*value - lo) / (hi - lo) : 0.0f, !f->disabled, r.hovered || r.dragging, 0);
  return *value != was;
}

/* A stop: what it means, what's said of it. */
static const char* stop_described(const bui_field* f, const bui_option* o) { return has(o->description) ? o->description : f->description; }
static const char* stop_said(bui_context* ui, const bui_field* f, const bui_option* o) {
  return has(o->spoken) ? o->spoken : row_spoken(ui, f, o->shown, stop_described(f, o));
}

bool bui_stops_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, int* stop, const bui_option* stops,
                   int n) {
  if (n <= 0) return false;
  *stop = clampi(*stop, 0, n - 1);
  const bui_row_parts rp = bui_row_layout(t, row);
  bui_item it = {0};
  it.kind = BUI_SLIDER, it.rect = row, it.track = rp.control, it.disabled = f->disabled;
  it.value = stops[*stop].shown, it.spoken = stop_said(ui, f, &stops[*stop]);
  const bui_result r = bui_declare(ui, id, &it);
  const int was = *stop;
  if (r.step) *stop = clampi(*stop + r.step, 0, n - 1);
  if (r.dragging) *stop = (int)lroundf(r.drag * (float)(n - 1));  /* (to its stops) */
  const bui_option* now = &stops[*stop];
  if (*stop != was) bui_revise(ui, now->shown, stop_said(ui, f, now));
  row_text(ui, t, &rp, f, stop_described(f, now), now->shown, r.hovered);
  if (bui_visible(ui, row)) slider_look(ui, t, &rp, n > 1 ? (float)*stop / (float)(n - 1) : 0.0f, !f->disabled, r.hovered || r.dragging, n);
  return *stop != was;
}

bool bui_button_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, const char* text) {
  const bui_row_parts rp = bui_row_layout(t, row);
  bui_item it = {0};
  it.kind = BUI_BUTTON, it.rect = row, it.press = rp.control, it.disabled = f->disabled;
  it.spoken = has(f->spoken) ? f->spoken : has(f->description) ? bui_format(ui, "%s: %s", bui__s(f->name), f->description) : f->name;
  const bui_result r = bui_declare(ui, id, &it);
  row_text(ui, t, &rp, f, f->description, NULL, r.hovered);
  if (!bui_visible(ui, row)) return r.activated && !f->disabled;
  bui_fill(ui, rp.control, r.hovered && !f->disabled ? bui_rgb(0.3f, 0.2f, 0.1f) : bui_rgb(0.22f, 0.15f, 0.1f));
  bui_centered(ui, t, rp.control, text, bui_rgb(1, 1, 1), 0.0f);
  return r.activated && !f->disabled;
}

static const char* foldout_said(bui_context* ui, const bui_field* f, bool open) {
  return has(f->spoken) ? f->spoken : bui_format(ui, "%s%s", bui__s(f->name), open ? ", expanded" : ", collapsed");
}

bool bui_foldout_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, bool* open) {
  const float p = t->pixel;
  bui_item it = {0};
  it.kind = BUI_BUTTON, it.rect = row, it.value = *open ? "open" : "shut", it.spoken = foldout_said(ui, f, *open);
  const bui_result r = bui_declare(ui, id, &it);
  if (r.activated) {
    *open = !*open;
    bui_revise(ui, *open ? "open" : "shut", foldout_said(ui, f, *open));
  }
  if (!bui_visible(ui, row)) return r.activated;
  const float ty = roundf(0.5f * (row.y0 + row.y1 - BUI_GLYPH_H * p));
  if (r.hovered) bui_fill(ui, row, t->hover);
  bui_text(ui, *open ? "-" : "+", row.x0 + 3.0f * p, ty, p, t->value, 0.0f);
  bui_text(ui, upper(ui, f->name), row.x0 + 11.0f * p, ty, p, t->text, 0.0f);
  if (has(f->description)) bui_text(ui, f->description, row.x0 + maxf(t->blurb_at * p, 0.3f * bui_rect_w(row)), ty, p, t->blurb, 0.0f);
  bui_fill(ui, bui_rect_make(row.x0, row.y1, row.x1, row.y1 + p), t->rule);
  return r.activated;
}

bui_captured bui_capture(bui_context* ui, const char* id, const bui_item* item, const char* prompt, double seconds) {
  bui_captured c = {0};
  const bui_result r = bui_declare(ui, id, item);
  if (r.activated && !bui_capturing(ui, NULL)) {
    bui_start_capture(ui, id, seconds > 0.0 ? seconds : 8.0);
    bui_announce(ui, prompt, BUI_SAY_NORMAL);
    c.started = true;
  }
  c.has_code = bui_take_captured(ui, id, &c.code, &c.cancelled) && !c.cancelled;
  c.waiting = bui_capturing(ui, id);
  return c;
}
