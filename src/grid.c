#include "blueui/widgets.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

/* Grids: square cells, or hexagons in a honeycomb (blueui/widgets.h). Each
 * cell its own item ("<id>#<n>"); the arrow keys moved between them here,
 * before bui_end_frame's linear navigation sees them. */

#define SQRT3 1.7320508f

static float minf(float a, float b) { return a < b ? a : b; }
static int count_of(const bui_grid_opts* o) {
  if (o->count > 0) return o->count;
  return o->columns * (o->rows > 0 ? o->rows : 1);
}
static int rows_of(const bui_grid_opts* o) {
  if (o->rows > 0) return o->rows;
  return o->columns > 0 ? (count_of(o) + o->columns - 1) / o->columns : 0;
}
static bool hex(const bui_grid_opts* o) { return o->shape != BUI_GRID_SQUARE; }
static bool pointy(const bui_grid_opts* o) { return o->shape == BUI_GRID_HEX_POINTY; }
/* (Row r of pointy cells, or column c of flat ones: shifted half a cell.) */
static bool shifted(const bui_grid_opts* o, int k) { return ((k & 1) == 0) == o->stagger_first; }

/* A cell's box size, and the pitch from one cell to the next across and down. */
static void metrics(const bui_grid_opts* o, float* w, float* h, float* px, float* py) {
  const float c = o->cell, g = o->gap;
  switch (o->shape) {
    case BUI_GRID_HEX_POINTY:
      *w = c, *h = c * 2.0f / SQRT3;
      *px = c + g, *py = 0.75f * *h + g * SQRT3 * 0.5f;
      break;
    case BUI_GRID_HEX_FLAT:
      *h = c, *w = c * 2.0f / SQRT3;
      *px = 0.75f * *w + g * SQRT3 * 0.5f, *py = c + g;
      break;
    default: *w = *h = c, *px = *py = c + g; break;
  }
}

bui_vec2 bui_grid_size(const bui_grid_opts* o) {
  float w, h, px, py;
  metrics(o, &w, &h, &px, &py);
  const int cols = o->columns, rows = rows_of(o);
  bui_vec2 s = {0.0f, 0.0f};
  if (cols <= 0 || rows <= 0) return s;
  s.x = (float)(cols - 1) * px + w;
  s.y = (float)(rows - 1) * py + h;
  if (pointy(o) && (rows > 1 || shifted(o, 0))) s.x += 0.5f * px;
  if (o->shape == BUI_GRID_HEX_FLAT && (cols > 1 || shifted(o, 0))) s.y += 0.5f * py;
  return s;
}

bui_rect bui_grid_cell_rect(const bui_grid_opts* o, bui_rect area, int index) {
  float w, h, px, py;
  metrics(o, &w, &h, &px, &py);
  const int cols = o->columns > 0 ? o->columns : 1;
  const int c = index % cols, r = index / cols;
  float x = area.x0 + (float)c * px, y = area.y0 + (float)r * py;
  if (pointy(o) && shifted(o, r)) x += 0.5f * px;
  if (o->shape == BUI_GRID_HEX_FLAT && shifted(o, c)) y += 0.5f * py;
  return bui_rect_make(x, y, x + w, y + h);
}

/* Whether p is in the hexagon filling `box`. */
static bool in_hex(bui_rect box, bool point_up, bui_vec2 p) {
  const bui_vec2 m = bui_rect_center(box);
  float dx = fabsf(p.x - m.x), dy = fabsf(p.y - m.y);
  float a = 0.5f * bui_rect_w(box), b = 0.5f * bui_rect_h(box);
  if (!point_up) {  /* (flat: the pointy one turned a quarter) */
    float t = dx;
    dx = dy, dy = t;
    t = a, a = b, b = t;
  }
  if (a <= 0.0f || dx > a || dy > b) return false;
  return dy <= b - 0.5f * b * dx / a;
}

int bui_grid_hit(const bui_grid_opts* o, bui_rect area, bui_vec2 p) {
  const int n = count_of(o);
  for (int i = 0; i < n; ++i) {
    const bui_rect r = bui_grid_cell_rect(o, area, i);
    if (!bui_rect_contains(r, p)) continue;
    if (!hex(o) || in_hex(r, pointy(o), p)) return i;
  }
  return -1;
}

int bui_grid_neighbor(const bui_grid_opts* o, int index, int dx, int dy) {
  const int n = count_of(o), cols = o->columns > 0 ? o->columns : 1, rows = rows_of(o);
  if (index < 0 || index >= n) return -1;
  int c = index % cols, r = index / cols;
  c += dx, r += dy;
  if (c < 0 || c >= cols || r < 0 || r >= rows) return -1;
  const int to = r * cols + c;
  if (to < n) return to;
  /* (The last row short of it: down to its last cell; across, nothing.) */
  return dy > 0 ? n - 1 : -1;
}

size_t bui_hex_rects(bui_rect box, bool point_up, bui_color c, bui_draw_cmd* out, size_t cap) {
  /* The middle (a rectangle), then each tip in strips narrowing to it. */
  const float w = bui_rect_w(box), h = bui_rect_h(box);
  const float along = point_up ? h : w;  /* tip to tip */
  int k = (int)roundf(along * 0.125f);
  k = k < 3 ? 3 : k > 12 ? 12 : k;
  size_t n = 0;
#define PUT(X0, Y0, X1, Y1)                                                           \
  do {                                                                                \
    if (n < cap) {                                                                    \
      const bui_draw_cmd d = {(X0), (Y0), (X1), (Y1), c.r, c.g, c.b, c.a, 0, 0, 0, 0}; \
      out[n] = d;                                                                     \
    }                                                                                 \
    ++n;                                                                              \
  } while (0)
  const bui_vec2 m = bui_rect_center(box);
  if (point_up) {
    PUT(box.x0, box.y0 + 0.25f * h, box.x1, box.y1 - 0.25f * h);
    const float strip = 0.25f * h / (float)k;
    for (int i = 0; i < k; ++i) {
      /* (Strip i from the middle out: as wide as the hexagon at its middle.) */
      const float d = ((float)i + 0.5f) / (float)k;  /* 0..1 toward the tip */
      const float half = 0.5f * w * (1.0f - d);
      const float y0 = box.y0 + 0.25f * h - (float)(i + 1) * strip, y1 = y0 + strip;
      PUT(m.x - half, y0, m.x + half, y1);
      PUT(m.x - half, box.y1 - 0.25f * h + (float)i * strip, m.x + half, box.y1 - 0.25f * h + (float)(i + 1) * strip);
    }
  } else {
    PUT(box.x0 + 0.25f * w, box.y0, box.x1 - 0.25f * w, box.y1);
    const float strip = 0.25f * w / (float)k;
    for (int i = 0; i < k; ++i) {
      const float d = ((float)i + 0.5f) / (float)k;
      const float half = 0.5f * h * (1.0f - d);
      const float x0 = box.x0 + 0.25f * w - (float)(i + 1) * strip, x1 = x0 + strip;
      PUT(x0, m.y - half, x1, m.y + half);
      PUT(box.x1 - 0.25f * w + (float)i * strip, m.y - half, box.x1 - 0.25f * w + (float)(i + 1) * strip, m.y + half);
    }
  }
#undef PUT
  return n;
}

/* The hexagon (or square) at `box` in `c`: from the texture's mask, else rectangles. */
static void shape(bui_context* ui, const bui_grid_opts* o, bui_rect box, bui_rect uv, bui_color c) {
  if (!hex(o)) {
    bui_fill(ui, box, c);
    return;
  }
  if (!bui_rect_empty(uv)) {
    const bui_draw_cmd d = {box.x0, box.y0, box.x1, box.y1, c.r, c.g, c.b, c.a, uv.x0, uv.y0, uv.x1, uv.y1};
    bui_draw(ui, &d, 1);
    return;
  }
  bui_draw_cmd buf[64];
  const size_t n = bui_hex_rects(box, pointy(o), c, buf, 64);
  bui_draw(ui, buf, n < 64 ? n : 64);
}

/* A square's ring: four sides. */
static void square_ring(bui_context* ui, bui_rect b, float t, bui_color c) {
  bui_fill(ui, bui_rect_make(b.x0, b.y0, b.x1, b.y0 + t), c);
  bui_fill(ui, bui_rect_make(b.x0, b.y1 - t, b.x1, b.y1), c);
  bui_fill(ui, bui_rect_make(b.x0, b.y0 + t, b.x0 + t, b.y1 - t), c);
  bui_fill(ui, bui_rect_make(b.x1 - t, b.y0 + t, b.x1, b.y1 - t), c);
}

/* A cell's face and ring. */
static void face(bui_context* ui, const bui_grid_opts* o, bui_rect box, unsigned state) {
  const bui_theme* t = bui_get_theme(ui);
  const bool sel = state & BUI_CELL_SELECTED, hot = state & BUI_CELL_HOVERED;
  const bui_color ring = sel ? t->grid_selected : t->grid_ring;
  const bui_color fill = hot ? t->grid_hover : sel ? t->grid_selected_face : t->grid_cell;
  const float th = o->ring > 0.0f ? o->ring : t->pixel;
  if (!hex(o)) {
    bui_fill(ui, box, fill);
    square_ring(ui, box, sel ? 2.0f * th : th, ring);
    return;
  }
  if (!bui_rect_empty(o->ring_uv)) {
    shape(ui, o, box, o->hex_uv, fill);
    shape(ui, o, box, o->ring_uv, ring);
    return;
  }
  /* (No ring mask: the ring's hexagon, the face's smaller one over it.) */
  const float d = sel ? 2.0f * th : th;
  const float s = 1.0f - 2.0f * d / minf(bui_rect_w(box), bui_rect_h(box));
  const bui_vec2 m = bui_rect_center(box);
  const float hw = 0.5f * bui_rect_w(box) * s, hh = 0.5f * bui_rect_h(box) * s;
  shape(ui, o, box, o->hex_uv, ring);
  shape(ui, o, bui_rect_make(m.x - hw, m.y - hh, m.x + hw, m.y + hh), o->hex_uv, bui_rgba(fill.r, fill.g, fill.b, 1.0f));
}

/* The square in a cell its contents go in (a hexagon's inscribed one). */
static bui_rect inner(const bui_grid_opts* o, bui_rect box) {
  if (!hex(o)) return box;
  const float s = 0.5f * 0.732f * (pointy(o) ? bui_rect_w(box) : bui_rect_h(box));
  const bui_vec2 m = bui_rect_center(box);
  return bui_rect_make(m.x - s, m.y - s, m.x + s, m.y + s);
}

/* The cell of this grid the focus is on (-1: none). */
static int focused_cell(const bui_context* ui, const char* id) {
  const size_t len = strlen(id);
  if (strncmp(ui->focus_id, id, len) != 0 || ui->focus_id[len] != '#') return -1;
  return atoi(ui->focus_id + len + 1);
}

static void cell_id(char* out, const char* id, int i) { snprintf(out, BUI_ID_MAX, "%.50s#%d", id, i); }

bui_grid_result bui_grid(bui_context* ui, const char* id, bui_rect area, const bui_grid_opts* o) {
  bui_grid_result res = {-1, -1, -1, -1};
  const int n = count_of(o);
  if (n <= 0 || o->columns <= 0) return res;
  const bui_input* in = bui_get_input(ui);
  char cid[BUI_ID_MAX];

  /* The arrow keys, on one of its cells: across it, and up and down it
   * (off its top or bottom: on to the items before or after it). */
  int focus = o->inert ? -1 : focused_cell(ui, id);
  if (focus >= n) focus = n - 1;
  if (focus >= 0 && ui->navigating) {
    int to = focus;
    if (ui->pending_step) {
      const int s = bui_grid_neighbor(o, focus, ui->pending_step, 0);
      if (s >= 0) to = s;
      ui->pending_step = 0;
    }
    if (ui->pending_vmove) {
      const int dy = ui->pending_vmove > 0 ? 1 : -1;
      const int s = bui_grid_neighbor(o, to, 0, dy);
      if (s >= 0) {
        to = s;
        ui->pending_move -= ui->pending_vmove;
      } else {
        to = dy > 0 ? n - 1 : 0;  /* (the linear move goes on from its end) */
      }
      ui->pending_vmove = 0;
    }
    if (to != focus) {
      focus = to;
      cell_id(cid, id, focus);
      bui_focus(ui, cid);
      bui_reveal(ui, cid);
    }
  }

  const int hit = o->inert ? -1 : bui_grid_hit(o, area, in->mouse);
  const int click = !o->inert && in->clicked ? bui_grid_hit(o, area, in->click_at) : -1;
  const int alt = !o->inert && in->alt_clicked ? bui_grid_hit(o, area, in->alt_click_at) : -1;
  if (!ui->navigating) res.hovered = hit;
  char hovered_before[BUI_ID_MAX]; /* (an item before it under the mouse, or none) */
  memcpy(hovered_before, ui->hovered_id, BUI_ID_MAX);

  for (int i = 0; i < n; ++i) {
    const bui_rect box = bui_grid_cell_rect(o, area, i);
    unsigned state = 0;
    if (!o->inert) {
      const char* said = o->spoken ? o->spoken(o->user, i) : NULL;
      const char* spoken = said && *said ? said : bui_format(ui, "Slot %d", i + 1);
      bui_item it = {0};
      it.kind = BUI_CUSTOM, it.rect = box, it.spoken = spoken, it.value = spoken;
      it.role = o->role ? o->role : "slot";
      cell_id(cid, id, i);
      const bui_result r = bui_declare(ui, cid, &it);
      /* (A click: the cell exactly under it, not the box declare went by.) */
      const bool activated = in->clicked ? i == click : r.activated;
      if (activated) res.activated = i;
      if (r.focused) res.focused = i, state |= BUI_CELL_FOCUSED | BUI_CELL_HOVERED;
      if (!ui->navigating && i == hit) state |= BUI_CELL_HOVERED;
    }
    if (i == o->selected) state |= BUI_CELL_SELECTED;
    if (!bui_visible(ui, box)) continue;
    if (!o->no_faces) face(ui, o, box, state);
    if (o->draw_cell) o->draw_cell(o->user, i, inner(o, box), state);
  }
  if (!o->inert) {
    /* The mouse over it: the cell exactly under it, the one hovered (and,
     * clicked, focused); between cells, none of them. */
    if (!ui->navigating) {
      if (hit >= 0) cell_id(ui->hovered_id, id, hit);
      else memcpy(ui->hovered_id, hovered_before, BUI_ID_MAX);
    }
    if (click >= 0) {
      cell_id(cid, id, click);
      bui_focus(ui, cid);
    }
    if (alt >= 0) res.alt_activated = alt;
    if (ui->navigating && res.focused >= 0) res.hovered = res.focused;
  }
  return res;
}
