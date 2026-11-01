/* The profiler's view (bui_profiler; docs/profiler.md): the frames the
 * profiler kept (profiler.c) drawn over an area — the frame graph (each
 * frame's CPU and GPU time), the shown frame's timeline (a lane a thread,
 * its tasks as bars, nested ones under their parents; the GPU's the last
 * lane) and its top tasks — as rectangles and text in the frame's geometry. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blueui/profiler.h"
#include "internal.h"

/* A task's color: by its name (the same every frame), bright on the dark. */
static bui_color color_of(const char* name) {
  uint32_t h = 2166136261u;
  for (const char* p = name; *p; ++p) h = (h ^ (uint8_t)*p) * 16777619u;
  const float hue = (float)(h % 360u) / 60.0f;
  const float c = 0.55f, x = c * (1.0f - fabsf(fmodf(hue, 2.0f) - 1.0f)), m = 0.30f;
  float r = 0, g = 0, b = 0;
  if (hue < 1) r = c, g = x;
  else if (hue < 2) r = x, g = c;
  else if (hue < 3) g = c, b = x;
  else if (hue < 4) g = x, b = c;
  else if (hue < 5) r = x, b = c;
  else r = c, b = x;
  return bui_rgba(r + m, g + m, b + m, 0.95f);
}

static void fill(bui_context* ui, float x0, float y0, float x1, float y1, bui_color c) {
  bui_fill(ui, bui_rect_make(x0, y0, x1, y1), c);
}

/* Text from `left`, its capitals centred on `y`. */
static void text(bui_context* ui, const char* s, float left, float y, float pixel, bui_color c) {
  bui_text(ui, s, left, y - 0.5f * BUI_GLYPH_H * pixel, pixel, c, 0.0f);
}

/* `s` cut (in place) to `w` wide. */
static void cut(char* s, float w, float pixel) {
  size_t n = strlen(s);
  while (n > 3 && bui_text_width(s, n, pixel) > w) s[--n] = '\0';
}

typedef struct label {
  float w, x, y;
  const bui_prof_event* e;
} label;
static int widest_first(const void* a, const void* b) {
  const float x = ((const label*)a)->w, y = ((const label*)b)->w;
  return x > y ? -1 : x < y ? 1 : 0;
}

typedef struct total {
  const char* name;
  bool gpu;
  double ms;
  int calls;
} total;
static int biggest_first(const void* a, const void* b) {
  const double x = ((const total*)a)->ms, y = ((const total*)b)->ms;
  return x > y ? -1 : x < y ? 1 : 0;
}

void bui_profiler(bui_context* ui, bui_rect area) {
  const size_t count = bui_prof_frame_count();
  const bui_prof_frame* sel = bui_prof_view_frame();
  if (!count || !sel) return;
  /* Sizes from the theme's pixel (2: the sizes this was laid out at). */
  const float px = bui_get_theme(ui)->pixel, u = px * 0.5f;
  /* (The built-in bitmap font: whole font pixels, or its glyphs go uneven.) */
  const bool bitmap = bui_get_font() == NULL;
#define SIZE(k) (bitmap ? fmaxf(1.0f, roundf((k) * px)) : (k) * px)
  const float pHead = SIZE(0.8f), pHint = SIZE(0.65f), pRow = SIZE(0.6f), pSmall = SIZE(0.55f), pTick = SIZE(0.5f);
#undef SIZE
  const bui_color kBack = bui_rgba(0.03f, 0.04f, 0.06f, 0.88f), kWhite = bui_rgba(1, 1, 1, 1),
                  kDim = bui_rgba(0.62f, 0.66f, 0.72f, 1), kCpu = bui_rgba(0.35f, 0.62f, 1.0f, 1),
                  kGpu = bui_rgba(1.0f, 0.62f, 0.25f, 1), kLine = bui_rgba(0.5f, 0.5f, 0.55f, 0.6f),
                  kSel = bui_rgba(1, 1, 1, 0.9f), kLaneBack = bui_rgba(1, 1, 1, 0.04f);
  const bool live = bui_prof_view_live();
  const float x0 = area.x0, y0 = area.y0, x1 = area.x1, y1 = area.y1;
  fill(ui, x0, y0, x1, y1, kBack);

  /* The header. */
  char buf[256], gpu[32] = "--";
  if (sel->gpu_ms >= 0.0) snprintf(gpu, sizeof gpu, "%.2f ms", sel->gpu_ms);
  snprintf(buf, sizeof buf, "PROFILER  frame %llu   CPU %.2f ms   GPU %s   %s   %llu dropped",
           (unsigned long long)sel->index, bui_prof_frame_cpu_ms(sel), gpu, live ? "LIVE" : "FROZEN",
           (unsigned long long)bui_prof_dropped());
  text(ui, buf, x0 + 10 * u, y0 + 14 * u, pHead, kWhite);
  const char* hint = "Left/Right: a frame (Shift: 10)   Up/Down: the slowest older/newer   End: live";
  text(ui, hint, x1 - 10 * u - bui_text_width(hint, BUI_NUL, pHint), y0 + 14 * u, pHint, kDim);

  /* The frame graph: every kept frame, CPU (blue) and GPU (orange). */
  const float gx0 = x0 + 10 * u, gx1 = x1 - 10 * u, gy0 = y0 + 30 * u, gy1 = gy0 + 90 * u;
  double top = 33.4;
  for (size_t i = 0; i < count; ++i) {
    const bui_prof_frame* f = bui_prof_frame_at(i);
    top = fmax(top, fmax(bui_prof_frame_cpu_ms(f), f->gpu_ms));
  }
  const float barW = (gx1 - gx0) / (float)BUI_PROF_HISTORY;
#define GY(ms) (gy1 - (float)((ms) / top) * (gy1 - gy0))
  const double lines[2] = {16.7, 33.3};
  for (int i = 0; i < 2; ++i) {
    fill(ui, gx0, GY(lines[i]) - 0.5f, gx1, GY(lines[i]) + 0.5f, kLine);
    snprintf(buf, sizeof buf, "%.1f ms", lines[i]);
    text(ui, buf, gx1 - bui_text_width(buf, BUI_NUL, pSmall), GY(lines[i]) - 7 * u, pSmall, kDim);
  }
  const size_t first = BUI_PROF_HISTORY - count;
  for (size_t i = 0; i < count; ++i) {
    const bui_prof_frame* f = bui_prof_frame_at(i);
    const float bx = gx0 + (float)(first + i) * barW;
    if (f == sel) fill(ui, bx - 1, gy0 - 2, bx + barW + 1, gy1 + 2, kSel);
    fill(ui, bx, GY(bui_prof_frame_cpu_ms(f)), bx + barW * 0.6f, gy1, kCpu);
    if (f->gpu_ms >= 0.0) fill(ui, bx + barW * 0.5f, GY(f->gpu_ms), bx + barW * 0.95f, gy1, kGpu);
  }
#undef GY
  snprintf(buf, sizeof buf, "max %.1f ms", top);
  text(ui, buf, gx0, gy0 - 6 * u, pSmall, kDim);

  /* The shown frame's timeline: its span (and the GPU's past it), a lane a
   * thread, each lane as deep as its deepest task. */
  const float tableH = 150 * u;
  const float tx0 = x0 + 120 * u, tx1 = x1 - 10 * u, ty0 = gy1 + 28 * u, tyEnd = y1 - tableH;
  const uint64_t t0 = sel->start;
  uint64_t t1 = sel->end > sel->start + 1 ? sel->end : sel->start + 1;
  for (size_t l = 0; l < sel->lane_count; ++l)
    for (size_t i = 0; i < sel->lanes[l].event_count; ++i)
      if (sel->lanes[l].events[i].end > t1) t1 = sel->lanes[l].events[i].end;
  const double spanMs = (double)(t1 - t0) * 1e-6;
#define TX(t) \
  (tx0 + (float)fmin((t) <= t0 ? 0.0 : (double)((t) - t0) / (double)(t1 - t0), 1.0) * (tx1 - tx0))
  /* The ruler: a tick every 1, 2, 5, ... ms. */
  double tick = 1000.0;
  const double nice[] = {0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 500.0};
  for (size_t i = 0; i < sizeof nice / sizeof *nice; ++i)
    if (spanMs / nice[i] <= 12.0) {
      tick = nice[i];
      break;
    }
  for (double ms = 0.0; ms <= spanMs; ms += tick) {
    const float x = TX(t0 + (uint64_t)(ms * 1e6));
    fill(ui, x - 0.5f, ty0 - 8 * u, x + 0.5f, tyEnd, kLine);
    snprintf(buf, sizeof buf, "%g", ms);
    text(ui, buf, x + 2 * u, ty0 - 10 * u, pTick, kDim);
  }
  const float row = 15.0f * u;
  enum { kMaxDepth = 5 };
  float ly = ty0;
  /* The bars, widest first, take the labels. */
  label* labels = NULL;
  size_t label_count = 0, label_cap = 0;
  for (size_t l = 0; l < sel->lane_count; ++l) {
    const bui_prof_lane* lane = &sel->lanes[l];
    int depth = 0;
    for (size_t i = 0; i < lane->event_count; ++i)
      if (lane->events[i].depth > depth) depth = lane->events[i].depth;
    if (depth > kMaxDepth) depth = kMaxDepth;
    const float laneH = (float)(depth + 1) * row + 4 * u;
    if (ly + laneH > tyEnd) break; /* (the table's below) */
    fill(ui, tx0, ly, tx1, ly + laneH - 2 * u, kLaneBack);
    snprintf(buf, sizeof buf, "%s", lane->thread);
    cut(buf, tx0 - x0 - 14 * u, pRow);
    text(ui, buf, x0 + 10 * u, ly + row * 0.5f + 1, pRow, lane->gpu ? kGpu : kWhite);
    for (size_t i = 0; i < lane->event_count; ++i) {
      const bui_prof_event* e = &lane->events[i];
      if (e->depth > kMaxDepth) continue;
      const float bx0 = TX(e->start), bx1 = fmaxf(TX(e->end), bx0 + 1.0f);
      const float by = ly + 1 + (float)e->depth * row;
      fill(ui, bx0, by, bx1, by + row - 2 * u, color_of(e->name));
      if (bx1 - bx0 > 60.0f * u) {
        if (label_count == label_cap) {
          label_cap = label_cap ? label_cap * 2 : 64;
          labels = (label*)realloc(labels, label_cap * sizeof *labels);
        }
        labels[label_count].w = bx1 - bx0, labels[label_count].x = bx0;
        labels[label_count].y = by + row * 0.5f - 1, labels[label_count].e = e;
        ++label_count;
      }
    }
    ly += laneH;
  }
#undef TX
  if (label_count) qsort(labels, label_count, sizeof *labels, widest_first);
  for (size_t i = 0; i < label_count; ++i) {
    const label* lb = &labels[i];
    snprintf(buf, sizeof buf, "%s %.2f", lb->e->name, (double)(lb->e->end - lb->e->start) * 1e-6);
    cut(buf, lb->w - 6 * u, pSmall);
    text(ui, buf, lb->x + 3 * u, lb->y, pSmall, kWhite);
  }
  free(labels);

  /* The top tasks of the frame, all threads: total, calls, share. (Names
   * are interned or literals: summed by pointer, then by text.) */
  total* totals = NULL;
  size_t total_count = 0, total_cap = 0;
  for (size_t l = 0; l < sel->lane_count; ++l) {
    const bui_prof_lane* lane = &sel->lanes[l];
    for (size_t i = 0; i < lane->event_count; ++i) {
      const bui_prof_event* e = &lane->events[i];
      total* t = NULL;
      for (size_t k = 0; k < total_count && !t; ++k)
        if (totals[k].gpu == lane->gpu && (totals[k].name == e->name || strcmp(totals[k].name, e->name) == 0))
          t = &totals[k];
      if (!t) {
        if (total_count == total_cap) {
          total_cap = total_cap ? total_cap * 2 : 64;
          totals = (total*)realloc(totals, total_cap * sizeof *totals);
        }
        t = &totals[total_count++];
        t->name = e->name, t->gpu = lane->gpu, t->ms = 0.0, t->calls = 0;
      }
      t->ms += (double)(e->end - e->start) * 1e-6;
      ++t->calls;
    }
  }
  if (total_count) qsort(totals, total_count, sizeof *totals, biggest_first);
  const float tableY = y1 - 140 * u;
  /* Two columns of six: each a name, then its ms, calls and share of the
   * frame at a fixed place. */
  const float half = (x1 - x0) * 0.5f, nameW = 290 * u, numAt = 300 * u;
  for (int c = 0; c < 2; ++c) {
    if (c == 0) text(ui, "TOP TASKS (this frame, all threads)", x0 + 10 * u, tableY, pRow, kDim);
    text(ui, "ms     calls   % frame", x0 + 10 * u + (float)c * half + numAt, tableY, pRow, kDim);
  }
  const double frameMs = fmax(bui_prof_frame_cpu_ms(sel), 1e-6);
  for (size_t i = 0; i < total_count && i < 12; ++i) {
    const total* t = &totals[i];
    const float left = x0 + 10 * u + (i < 6 ? 0.0f : half), y = tableY + 16 * u + (float)(i % 6) * 15 * u;
    snprintf(buf, sizeof buf, "%s%s", t->gpu ? "GPU: " : "", t->name);
    cut(buf, nameW, pRow);
    text(ui, buf, left, y, pRow, t->gpu ? kGpu : kWhite);
    snprintf(buf, sizeof buf, "%.2f     %d     %.1f%%", t->ms, t->calls, 100.0 * t->ms / frameMs);
    text(ui, buf, left + numAt, y, pRow, t->gpu ? kGpu : kWhite);
  }
  free(totals);
}
