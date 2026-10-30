#include "blueui/widgets.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

/* A tech tree: its nodes in the tree's units, drawn at the view's zoom
 * about its centre; everything drawn clipped to the canvas (no scissor:
 * the rectangles cut, after). The canvas a scroll view that never scrolls,
 * so a node out of it is neither hovered nor clicked. */


static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static bool has(const char* s) { return s && *s; }

static bui_tech_tree_opts sizes(const bui_tech_tree_opts* o) {
  bui_tech_tree_opts s = {0};
  if (o) s = *o;
  if (s.node_w <= 0.0f) s.node_w = 120.0f;
  if (s.node_h <= 0.0f) s.node_h = 40.0f;
  if (s.min_zoom <= 0.0f) s.min_zoom = 0.35f;
  if (s.max_zoom <= 0.0f) s.max_zoom = 2.5f;
  if (s.button_h <= 0.0f) s.button_h = 16.0f;
  return s;
}

/* The strip of zoom buttons at the area's foot; the canvas above it. */
static float strip_h(const bui_theme* t) { return 14.0f * t->pixel; }
static bui_rect canvas_of(const bui_theme* t, bui_rect area) {
  return bui_rect_make(area.x0, area.y0, area.x1, area.y1 - strip_h(t) - 3.0f * t->pixel);
}

static bui_rect clip(bui_rect r, bui_rect c) {
  return bui_rect_make(maxf(r.x0, c.x0), maxf(r.y0, c.y0), minf(r.x1, c.x1), minf(r.y1, c.y1));
}

/* What was drawn since `from`, cut to `c` (a texture's box cut with it). */
static void clip_since(bui_context* ui, size_t from, bui_rect c) {
  size_t k = from;
  for (size_t i = from; i < ui->draw_count; ++i) {
    bui_draw_cmd d = ui->draw[i];
    const bui_rect r = clip(bui_rect_make(d.x0, d.y0, d.x1, d.y1), c);
    if (bui_rect_empty(r)) continue;
    if (d.u1 > d.u0) {
      const float du = (d.u1 - d.u0) / (d.x1 - d.x0), dv = (d.v1 - d.v0) / (d.y1 - d.y0);
      d.u0 += (r.x0 - d.x0) * du, d.u1 -= (d.x1 - r.x1) * du;
      d.v0 += (r.y0 - d.y0) * dv, d.v1 -= (d.y1 - r.y1) * dv;
    }
    d.x0 = r.x0, d.y0 = r.y0, d.x1 = r.x1, d.y1 = r.y1;
    ui->draw[k++] = d;
  }
  ui->draw_count = k;
}

static bui_vec2 to_screen(const bui_tech_view* v, bui_rect c, float x, float y) {
  const bui_vec2 m = bui_rect_center(c);
  bui_vec2 s;
  s.x = m.x + (x - v->x) * v->zoom, s.y = m.y + (y - v->y) * v->zoom;
  return s;
}

static bui_rect snapped(float x0, float y0, float x1, float y1) { return bui_rect_make(roundf(x0), roundf(y0), roundf(x1), roundf(y1)); }

static bui_rect box_of(const bui_tech_view* v, bui_rect c, const bui_tech_node* n, const bui_tech_tree_opts* s) {
  const bui_vec2 m = to_screen(v, c, n->x, n->y);
  const float hw = 0.5f * s->node_w * v->zoom, hh = 0.5f * s->node_h * v->zoom;
  return snapped(m.x - hw, m.y - hh, m.x + hw, m.y + hh);
}

static bui_rect button_of(const bui_tech_view* v, bui_rect c, const bui_tech_node* n, const bui_tech_tree_opts* s) {
  const bui_vec2 m = to_screen(v, c, n->x, n->y);
  const float z = v->zoom, hw = 0.4f * s->node_w * z, y0 = m.y + (0.5f * s->node_h + 0.25f * s->button_h) * z;
  return snapped(m.x - hw, y0, m.x + hw, y0 + s->button_h * z);
}

/* How far a node reaches below its centre (its button too). */
static float foot(const bui_tech_node* n, const bui_tech_tree_opts* s) { return 0.5f * s->node_h + (has(n->button) ? 1.25f * s->button_h : 0.0f); }

/* The tree's extent, in units (false: no nodes). */
static bool bounds(const bui_tech_node* nodes, int count, const bui_tech_tree_opts* s, bui_rect* out) {
  if (count <= 0) return false;
  bui_rect b = bui_rect_make(1e30f, 1e30f, -1e30f, -1e30f);
  for (int i = 0; i < count; ++i) {
    const bui_tech_node* n = &nodes[i];
    b.x0 = minf(b.x0, n->x - 0.5f * s->node_w), b.x1 = maxf(b.x1, n->x + 0.5f * s->node_w);
    b.y0 = minf(b.y0, n->y - 0.5f * s->node_h), b.y1 = maxf(b.y1, n->y + foot(n, s));
  }
  *out = b;
  return true;
}

/* Zoomed by `by`, the point under `at` kept there. */
static void zoom_about(bui_tech_view* v, bui_rect c, bui_vec2 at, float by, const bui_tech_tree_opts* s) {
  const bui_vec2 m = bui_rect_center(c);
  const float wx = v->x + (at.x - m.x) / v->zoom, wy = v->y + (at.y - m.y) / v->zoom;
  v->zoom = clampf(v->zoom * by, s->min_zoom, s->max_zoom);
  v->x = wx - (at.x - m.x) / v->zoom, v->y = wy - (at.y - m.y) / v->zoom;
}

void bui_tech_fit(bui_context* ui, bui_tech_view* v, bui_rect area, const bui_tech_node* nodes, int count, const bui_tech_tree_opts* o) {
  const bui_tech_tree_opts s = sizes(o);
  const bui_rect c = canvas_of(bui_get_theme(ui), area);
  bui_rect b;
  if (!bounds(nodes, count, &s, &b)) {
    v->x = v->y = 0.0f, v->zoom = 1.0f;
    return;
  }
  const float pad = 16.0f;
  const float zx = bui_rect_w(c) / (bui_rect_w(b) + 2.0f * pad), zy = bui_rect_h(c) / (bui_rect_h(b) + 2.0f * pad);
  v->zoom = clampf(minf(minf(zx, zy), 1.5f), s.min_zoom, s.max_zoom);  /* (a small tree: not blown up past 1.5) */
  v->x = 0.5f * (b.x0 + b.x1), v->y = 0.5f * (b.y0 + b.y1);
}

/* A line of text centred between x0 and x1, cut short to fit. */
static void line_in(bui_context* ui, const char* s, float x0, float x1, float y, float tp, bui_color col) {
  size_t len = strlen(s);
  const size_t most = (size_t)maxf(0.0f, (x1 - x0 + tp) / (BUI_GLYPH_ADVANCE * tp));
  if (len > most) len = most;
  if (!len) return;
  const float x = roundf(0.5f * (x0 + x1 - bui_text_width(s, len, tp)));
  bui_draw_cmd buf[512];
  const size_t n = bui_text_rects(s, len, x, roundf(y), tp, col, 0.0f, buf, 512);
  if (n <= 512) {
    bui_draw(ui, buf, n);
    return;
  }
  bui_draw_cmd* big = malloc(n * sizeof(bui_draw_cmd));
  if (!big) return;
  bui_text_rects(s, len, x, roundf(y), tp, col, 0.0f, big, n);
  bui_draw(ui, big, n);
  free(big);
}

/* A line of thickness `w` from (x0, y0) to (x1, y1), across or down. */
static void segment(bui_context* ui, float x0, float y0, float x1, float y1, float w, bui_color col) {
  const float h = 0.5f * w;
  bui_fill(ui, snapped(minf(x0, x1) - h, minf(y0, y1) - h, maxf(x0, x1) + h, maxf(y0, y1) + h), col);
}

static void frame_rect(bui_context* ui, bui_rect r, float w, bui_color col) {
  bui_fill(ui, bui_rect_make(r.x0 - w, r.y0 - w, r.x1 + w, r.y0), col);
  bui_fill(ui, bui_rect_make(r.x0 - w, r.y1, r.x1 + w, r.y1 + w), col);
  bui_fill(ui, bui_rect_make(r.x0 - w, r.y0, r.x0, r.y1), col);
  bui_fill(ui, bui_rect_make(r.x1, r.y0, r.x1 + w, r.y1), col);
}

static bui_color state_color(const bui_theme* t, bui_tech_state s) {
  return s == BUI_TECH_UNLOCKED ? t->tech_unlocked : s == BUI_TECH_RESEARCHING ? t->tech_researching : s == BUI_TECH_AVAILABLE ? t->tech_available : t->tech_locked;
}

static const char* const kStates[] = {"locked", "available", "researching", "unlocked"};

int bui_tech_tree(bui_context* ui, const char* id, bui_rect area, const bui_tech_node* nodes, int count, const bui_tech_link* links,
                  int link_count, bui_tech_view* v, const bui_tech_tree_opts* o) {
  const bui_theme* t = bui_get_theme(ui);
  const bui_input* in = bui_get_input(ui);
  const float p = t->pixel;
  const bui_tech_tree_opts s = sizes(o);
  const bui_rect c = canvas_of(t, area);
  bui_rect b;
  const bool any = bounds(nodes, count, &s, &b);
  const bool across = s.horizontal;
  if (v->zoom <= 0.0f) bui_tech_fit(ui, v, area, nodes, count, o);
  /* (Across: Left and Right pan it a third of the canvas.) */
  if (across)
    for (int k = 0; k < in->key_count; ++k)
      v->x += (in->keys[k] == BUI_KEY_RIGHT ? 0.3f : in->keys[k] == BUI_KEY_LEFT ? -0.3f : 0.0f) * bui_rect_w(c) / v->zoom;
  if (v->selected >= count) v->selected = count - 1;

  /* The wheel: zoomed about the mouse. */
  if (in->wheel != 0.0f && bui_rect_contains(c, in->mouse)) zoom_about(v, c, in->mouse, powf(1.15f, in->wheel), &s);
  /* A press on the canvas: dragged past 4 pixels, panned; let go where it
   * was pressed, on nothing, the selection cleared. */
  /* (Let go without dragging: the node pressed on, if it's let go on it too.) */
  int released = -1;
  if (in->clicked && bui_rect_contains(c, in->click_at))
    v->pressing = true, v->panning = false, v->press_empty = true, v->press_at = v->last = in->click_at, v->press_node = 0;
  else if (v->pressing && !in->mouse_down) {
    if (!v->panning && v->press_empty) v->selected = -1;
    if (!v->panning) released = v->press_node - 1;
    v->pressing = v->panning = false, v->press_node = 0;
  } else if (v->pressing) {
    if (!v->panning && hypotf(in->mouse.x - v->press_at.x, in->mouse.y - v->press_at.y) > 4.0f) v->panning = true;
    if (v->panning) v->x -= (in->mouse.x - v->last.x) / v->zoom, v->y -= (in->mouse.y - v->last.y) / v->zoom;
    v->last = in->mouse;
  }
  if (any) v->x = clampf(v->x, b.x0, b.x1), v->y = clampf(v->y, b.y0, b.y1);

  const float z = v->zoom;
  const float tp = z >= 0.75f ? roundf(p * z) : floorf(p * z);  /* (the text's font pixel: whole, crisp; under 1, none) */
  bui_fill(ui, c, t->tech_canvas);
  const size_t from = ui->draw_count;

  /* The lines: down from a node's foot, across, down to the one needing it
   * (across: from its right side, down or up, on to the next's left). */
  const float lw = maxf(1.0f, roundf(1.5f * z));
  for (int k = 0; k < link_count; ++k) {
    if (links[k].from < 0 || links[k].from >= count || links[k].to < 0 || links[k].to >= count) continue;
    const bui_tech_node *a = &nodes[links[k].from], *n = &nodes[links[k].to];
    const bui_color col = n->state == BUI_TECH_UNLOCKED ? t->tech_link_lit : t->tech_link;
    if (across) {
      const bui_vec2 s0 = to_screen(v, c, a->x + 0.5f * s.node_w, a->y), s1 = to_screen(v, c, n->x - 0.5f * s.node_w, n->y);
      const float mid = 0.5f * (s0.x + s1.x);
      segment(ui, s0.x, s0.y, mid, s0.y, lw, col);
      segment(ui, mid, s0.y, mid, s1.y, lw, col);
      segment(ui, mid, s1.y, s1.x, s1.y, lw, col);
      continue;
    }
    const bui_vec2 s0 = to_screen(v, c, a->x, a->y + foot(a, &s)), s1 = to_screen(v, c, n->x, n->y - 0.5f * s.node_h);
    const float mid = 0.5f * (s0.y + s1.y);
    segment(ui, s0.x, s0.y, s0.x, mid, lw, col);
    segment(ui, s0.x, mid, s1.x, mid, lw, col);
    segment(ui, s1.x, mid, s1.x, s1.y, lw, col);
  }

  /* The nodes, each its button under it. */
  int pressed = -1, focus = -1;
  bui_begin_scroll(ui, id, c, 0.0f);
  for (int i = 0; i < count; ++i) {
    const bui_tech_node* n = &nodes[i];
    const bui_rect box = box_of(v, c, n, &s), cut = clip(box, c);
    const bool seen = !bui_rect_empty(cut);
    const bool live = n->pressable && !n->button_disabled;  /* (its box the button, and it may be pressed now) */
    bui_item it = {0};
    it.kind = n->pressable ? BUI_BUTTON : BUI_CUSTOM, it.role = "tech", it.rect = seen ? cut : box, it.value = kStates[n->state & 3];
    it.disabled = n->pressable && n->button_disabled;
    it.spoken = has(n->spoken) ? n->spoken : bui_format(ui, "%s%s%s", bui__s(n->title), has(n->detail) ? ", " : "", bui__s(n->detail));
    const bui_result r = bui_declare(ui, bui_format(ui, "%s.n%d", id, i), &it);
    if (r.activated) v->selected = i;
    if (r.focused) focus = i;
    if (in->clicked && seen && bui_rect_contains(cut, in->click_at)) v->press_empty = false, v->press_node = i + 1;
    /* (Pressed: let go on it, not dragged; or Enter on it.) */
    if (live && ((released == i && seen && bui_rect_contains(cut, in->mouse)) || (r.activated && !in->clicked))) pressed = i, v->selected = i;

    const bool picked = v->selected == i;
    const float ew = maxf(1.0f, roundf(z)) * (picked ? 2.0f : 1.0f);
    bui_fill(ui, box, t->tech_node);
    if (r.hovered && !bui_navigating(ui) && (live || !n->pressable)) bui_fill(ui, box, t->hover);
    frame_rect(ui, box, ew, picked ? t->tech_selected : state_color(t, n->state));
    if (n->state == BUI_TECH_RESEARCHING) {
      const float bh = maxf(1.0f, roundf(3.0f * z));
      const bui_rect bar = bui_rect_make(box.x0, box.y1 - bh, box.x1, box.y1);
      bui_fill(ui, bar, t->tech_locked);
      bui_fill(ui, bui_rect_make(bar.x0, bar.y0, bar.x0 + roundf(bui_rect_w(bar) * clampf(n->progress, 0.0f, 1.0f)), bar.y1), t->tech_researching);
    }
    if (tp >= 1.0f) {
      const bool two = has(n->detail) && bui_rect_h(box) >= (2 * BUI_GLYPH_H + 4) * tp;
      const float pad = 2.0f * tp, tall = two ? (2 * BUI_GLYPH_H + 3) * tp : BUI_GLYPH_H * tp;
      const float y = box.y0 + 0.5f * (bui_rect_h(box) - tall);
      const bool off = n->state == BUI_TECH_LOCKED || (n->pressable && n->button_disabled && n->state == BUI_TECH_AVAILABLE);
      line_in(ui, bui__s(n->title), box.x0 + pad, box.x1 - pad, y, tp, off ? t->off : t->text);
      if (two) {
        const bui_color dc = n->state == BUI_TECH_LOCKED || n->state == BUI_TECH_AVAILABLE ? t->dim : state_color(t, n->state);
        line_in(ui, n->detail, box.x0 + pad, box.x1 - pad, y + (BUI_GLYPH_H + 3) * tp, tp, dc);
      }
    }

    if (!has(n->button)) continue;
    const bui_rect br = button_of(v, c, n, &s), bcut = clip(br, c);
    const bool bseen = !bui_rect_empty(bcut);
    bui_item bt = {0};
    bt.kind = BUI_BUTTON, bt.rect = bseen ? bcut : br, bt.disabled = n->button_disabled;
    bt.spoken = bui_format(ui, "%s, %s", n->button, bui__s(n->title));
    const bui_result rb = bui_declare(ui, bui_format(ui, "%s.b%d", id, i), &bt);
    if (rb.activated) pressed = i, v->selected = i;
    if (rb.focused) focus = i;
    if (in->clicked && bseen && bui_rect_contains(bcut, in->click_at)) v->press_empty = false;
    const float bw = maxf(1.0f, roundf(0.5f * z));
    if (t->button_edged) bui_fill(ui, bui_rect_grow(br, bw), t->button_edge);
    bui_fill(ui, br, !n->button_disabled && rb.hovered ? t->button_hot : t->button);
    if (tp >= 1.0f) line_in(ui, n->button, br.x0, br.x1, br.y0 + 0.5f * (bui_rect_h(br) - BUI_GLYPH_H * tp), tp,
                            n->button_disabled ? t->button_text_off : t->button_text);
  }
  bui_end_scroll(ui);
  clip_since(ui, from, c);

  /* The focus come to a node (or its button): it picked, panned to if it's
   * out of sight. (Once each time it moves: the mouse may pan away.) */
  if (focus >= 0 && focus + 1 != v->revealed) {
    v->revealed = focus + 1, v->selected = focus;
    if (!bui_rect_inside(box_of(v, c, &nodes[focus], &s), c)) v->x = nodes[focus].x, v->y = nodes[focus].y;
  } else if (focus < 0) v->revealed = 0;

  /* The strip: zoom out, in, the whole tree. */
  const float sy = area.y1 - strip_h(t), bwid = 18.0f * p;
  const bui_rect out = bui_rect_make(area.x0 + p, sy, area.x0 + p + bwid, area.y1);
  const bui_rect into = bui_rect_make(out.x1 + 3.0f * p, sy, out.x1 + 3.0f * p + bwid, area.y1);
  const bui_rect fit = bui_rect_make(into.x1 + 3.0f * p, sy, into.x1 + 3.0f * p + 28.0f * p, area.y1);
  const bui_button_opts zo = {"Zoom out", z <= s.min_zoom, false, 0.0f}, zi = {"Zoom in", z >= s.max_zoom, false, 0.0f};
  const bui_button_opts zf = {"Fit the whole tree", false, false, 0.0f};
  if (bui_button(ui, bui_format(ui, "%s.out", id), "-", out, &zo)) zoom_about(v, c, bui_rect_center(c), 1.0f / 1.25f, &s);
  if (bui_button(ui, bui_format(ui, "%s.in", id), "+", into, &zi)) zoom_about(v, c, bui_rect_center(c), 1.25f, &s);
  if (bui_button(ui, bui_format(ui, "%s.fit", id), "FIT", fit, &zf)) bui_tech_fit(ui, v, area, nodes, count, o);
  return pressed;
}
