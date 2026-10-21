#ifndef BLUEUI_INTERNAL_H
#define BLUEUI_INTERNAL_H

#include <stdarg.h>

#include "blueui/blueui.h"

#define BUI_ID_MAX 64

/* (NULL as "".) */
static inline const char* bui__s(const char* s) { return s ? s : ""; }

/* ------------------------------------------------------------- an arena */

/* A frame's strings: blocks that stay put, all let go of at once. */
typedef struct bui__block {
  struct bui__block* next;
  size_t used, cap;
  char data[];
} bui__block;

typedef struct bui__arena {
  bui__block* head;
  bui__block* at;
} bui__arena;

void bui__arena_reset(bui__arena* a);
void bui__arena_free(bui__arena* a);
char* bui__arena_dup(bui__arena* a, const char* s);
char* bui__arena_vprintf(bui__arena* a, const char* fmt, va_list args);

/* --------------------------------------------------- a growable string */

typedef struct bui__str {
  char* data;
  size_t len, cap;
} bui__str;

void bui__str_set(bui__str* s, const char* text);
void bui__str_add(bui__str* s, const char* text);
void bui__str_free(bui__str* s);
static inline const char* bui__str_c(const bui__str* s) { return s->data ? s->data : ""; }

/* ---------------------------------------------------------------- voice */

typedef struct bui__voice bui__voice;

/* (no_voice, or none built in: lines as text alone.) */
bui__voice* bui__voice_create(const bui_config* config);
void bui__voice_destroy(bui__voice* v);
bool bui__voice_real(const bui__voice* v);
void bui__voice_say(bui__voice* v, const char* text, bool queued);
void bui__voice_stop(bui__voice* v);
bool bui__voice_busy(bui__voice* v); /* (still to be handed over) */
bool bui__voice_next(bui__voice* v, bui_audio* out);
bool bui__voice_synthesize(bui__voice* v, const char* text, bui_audio* out);

/* -------------------------------------------------------------- context */

typedef struct bui__entry {
  const char* id;
  bui_item item; /* (its strings in the frame's arena) */
  bool shown;
  int scroll;    /* its view (-1: none) */
} bui__entry;

typedef struct bui__scroll {
  char id[BUI_ID_MAX];
  float offset, content;
  bui_rect view;
} bui__scroll;

struct bui_context {
  bui__voice* voice;
  bui_strings strings;
  bui_theme theme;
  bool speech_on, speech_toggled;
  bui_input in;
  /* The geometry. */
  bui_draw_cmd* draw;
  size_t draw_count, draw_cap;
  /* This frame's items (their strings in arenas[frame]); the last frame's, as state. */
  bui__arena arenas[2];
  int frame;
  bui__entry* entries;
  size_t entry_count, entry_cap;
  bui_item_state* found;
  size_t found_count, found_cap;
  bui_state state;
  /* The screen. */
  char screen_id[BUI_ID_MAX], last_screen_id[BUI_ID_MAX];
  bui__str screen_name, page_name, last_page_name, hint_keyboard, hint_gamepad;
  char (*hinted)[BUI_ID_MAX + 8];
  size_t hinted_count, hinted_cap;
  char focus_id[BUI_ID_MAX], hovered_id[BUI_ID_MAX];
  int focus_index, rows;
  bool navigating, nav_anchored, skip_arrival;
  bui_vec2 nav_mouse;
  /* This frame's keys, for the items to take. */
  bool pending_activate, pending_back;
  int pending_step, pending_move, tab_step, tab_jump;
  bool had_tabs, has_tabs;
  /* Scrolling. */
  bui__scroll* scrolls;
  size_t scroll_count, scroll_cap;
  int scroll_at; /* (the view being declared: -1 none) */
  char reveal_id[BUI_ID_MAX];
  /* Capturing. */
  char capture_id[BUI_ID_MAX], captured_id[BUI_ID_MAX];
  double capture_until;
  bool has_captured, capture_cancelled;
  int64_t captured;
  /* Speech. */
  bui__str events, spoken_where, spoken_focus_id, last_line;
  bool holding, hold_heard;
  double hold_since;
};

/* The frame's arena (strings good till the frame after next). */
static inline bui__arena* bui__frame_arena(bui_context* ui) { return &ui->arenas[ui->frame]; }

#endif
