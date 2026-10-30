#include <math.h>
#include <string.h>

#include "internal.h"

/* A surface's homography: its to_clip's rows x, y, w and columns x, y, 1
 * (the canvas's plane, z 0), canvas (x, y, 1) to clip (x, y, w). Row-major. */
static void homography(const bui_surface* s, float h[9]) {
  const float* m = s->to_clip;
  static const int rows[3] = {0, 1, 3}, cols[3] = {0, 1, 3};
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c) h[r * 3 + c] = m[cols[c] * 4 + rows[r]];
}

static bool invert3(const float a[9], float out[9]) {
  const float c0 = a[4] * a[8] - a[5] * a[7], c1 = a[5] * a[6] - a[3] * a[8], c2 = a[3] * a[7] - a[4] * a[6];
  const float det = a[0] * c0 + a[1] * c1 + a[2] * c2;
  if (fabsf(det) < 1e-20f) return false;
  const float k = 1.0f / det;
  out[0] = c0 * k, out[1] = (a[2] * a[7] - a[1] * a[8]) * k, out[2] = (a[1] * a[5] - a[2] * a[4]) * k;
  out[3] = c1 * k, out[4] = (a[0] * a[8] - a[2] * a[6]) * k, out[5] = (a[2] * a[3] - a[0] * a[5]) * k;
  out[6] = c2 * k, out[7] = (a[1] * a[6] - a[0] * a[7]) * k, out[8] = (a[0] * a[4] - a[1] * a[3]) * k;
  return true;
}

bui_surface bui_surface_on_plane(bui_vec2 size, const float origin[3], const float right[3], const float down[3],
                                 const float view_proj[16], bui_vec2 viewport) {
  bui_surface s;
  memset(&s, 0, sizeof s);
  s.size = size, s.viewport = viewport;
  /* Canvas to world: x along right / width, y along down / height, the origin. */
  float place[16] = {0};
  const float sx = size.x > 0.0f ? 1.0f / size.x : 0.0f, sy = size.y > 0.0f ? 1.0f / size.y : 0.0f;
  for (int k = 0; k < 3; ++k) place[0 * 4 + k] = right[k] * sx, place[1 * 4 + k] = down[k] * sy, place[3 * 4 + k] = origin[k];
  place[2 * 4 + 2] = 1.0f, place[15] = 1.0f;
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r) {
      float v = 0.0f;
      for (int k = 0; k < 4; ++k) v += view_proj[k * 4 + r] * place[c * 4 + k];
      s.to_clip[c * 4 + r] = v;
    }
  return s;
}

bool bui_surface_project(const bui_surface* s, bui_vec2 canvas, bui_vec2* screen) {
  float h[9];
  homography(s, h);
  const float x = h[0] * canvas.x + h[1] * canvas.y + h[2], y = h[3] * canvas.x + h[4] * canvas.y + h[5],
              w = h[6] * canvas.x + h[7] * canvas.y + h[8];
  if (w <= 1e-6f) return false;
  if (screen) screen->x = (x / w * 0.5f + 0.5f) * s->viewport.x, screen->y = (y / w * 0.5f + 0.5f) * s->viewport.y;
  return true;
}

bool bui_surface_unproject(const bui_surface* s, bui_vec2 screen, bui_vec2* canvas) {
  float h[9], inv[9];
  homography(s, h);
  if (!invert3(h, inv) || s->viewport.x <= 0.0f || s->viewport.y <= 0.0f) return false;
  const float nx = screen.x / s->viewport.x * 2.0f - 1.0f, ny = screen.y / s->viewport.y * 2.0f - 1.0f;
  const float x = inv[0] * nx + inv[1] * ny + inv[2], y = inv[3] * nx + inv[4] * ny + inv[5], w = inv[6] * nx + inv[7] * ny + inv[8];
  if (fabsf(w) < 1e-12f) return false;
  const bui_vec2 c = {x / w, y / w};
  /* (Behind the camera: the ray's back half, the clip w there below 0.) */
  if (h[6] * c.x + h[7] * c.y + h[8] <= 0.0f) return false;
  if (c.x < 0.0f || c.y < 0.0f || c.x >= s->size.x || c.y >= s->size.y) return false;
  if (canvas) *canvas = c;
  return true;
}

void bui_set_surface(bui_context* ui, const bui_surface* s) {
  ui->has_surface = s != NULL;
  if (s) ui->surface = *s;
}

const bui_surface* bui_get_surface(const bui_context* ui) { return ui->has_surface ? &ui->surface : NULL; }

void bui__surface_input(bui_context* ui, bui_input* in) {
  if (!ui->has_surface) return;
  const bui_vec2 nowhere = {-1.0f, -1.0f};
  bui_vec2 c;
  in->mouse = bui_surface_unproject(&ui->surface, in->mouse, &c) ? c : nowhere;
  if (in->clicked) {
    if (bui_surface_unproject(&ui->surface, in->click_at, &c)) in->click_at = c;
    else in->clicked = false, in->click_at = nowhere;
  }
  in->screen_size = ui->surface.size;
}
