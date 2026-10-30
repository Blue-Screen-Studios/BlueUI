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
int bui__voice_rate(const bui__voice* v);
/* An event handed to the game, after those before it. */
void bui__voice_push(bui__voice* v, bui_audio a);
/* A WAV round mono samples (-1..1), 16-bit, at `rate`. */
bool bui__make_wav(const char* text, const float* pcm, size_t n, int rate, bui_audio* a);

/* ----------------------------------------------------------- GUI sounds */

typedef struct bui__pack {
  float* pcm[BUI_EVENT_COUNT]; /* at the context's rate (NULL: silent) */
  size_t count[BUI_EVENT_COUNT];
} bui__pack;

/* The built-in packs, made at the context's rate (and let go of). */
void bui__make_packs(bui_context* ui);
void bui__free_packs(bui_context* ui);
/* An event BlueUI noticed itself: kept if it's the frame's most specific,
 * unless the game asked for a sound (bui_trigger) this frame. */
void bui__auto_event(bui_context* ui, bui_event sound);
/* The frame's sound (the most specific asked for), handed over. */
void bui__emit_event(bui_context* ui);

/* -------------------------------------------------------------- context */

/* The frame's input onto the surface, if there's one (surface.c). */
void bui__surface_input(bui_context* ui, bui_input* in);

typedef struct bui__entry {
  const char* id;
  bui_item item; /* (its strings in the frame's arena) */
  bool shown;
  int scroll;    /* its view (-1: none) */
} bui__entry;

#define BUI__MODAL_MAX 8
#define BUI__MODAL_BUTTONS 4

typedef struct bui__modal {
  char id[BUI_ID_MAX];
  char* title;
  char* message;
  char* buttons[BUI__MODAL_BUTTONS];
  int count;
} bui__modal;

typedef struct bui__scroll {
  char id[BUI_ID_MAX];
  float offset, content;
  bui_rect view;
} bui__scroll;

struct bui_context {
  bui__voice* voice;
  bui_surface surface; /* (bui_set_surface's) */
  bool has_surface;
  bui_strings strings;
  bui_theme theme;
  bool speech_on, speech_toggled;
  bool toggle_pending;  /* (switched, announcing, between frames: the next frame's) */
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
  int pending_vmove; /* (the part of pending_move Up and Down made: a grid's, to take) */
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
  /* GUI sounds. */
  bui__pack packs[BUI_SOUND_PACK_COUNT];
  int pending_event; /* (-1: none) */
  bool event_requested;
  /* The last frame's event and rumble (the state's). */
  bool has_frame_event, has_frame_rumble;
  bui_event frame_event;
  bui_rumble frame_rumble;
  double last_change;
  char sound_focus_id[BUI_ID_MAX];
  /* Modals: those waiting (the first shown), this frame's input held back
   * from the game's screen, the last answer, the focus to go back to. */
  bui__modal modals[BUI__MODAL_MAX];
  int modal_count;
  bool modal_frame;
  bui_input modal_in;
  char result_id[BUI_ID_MAX];
  int result_button;
  bool has_result;
  char saved_focus[BUI_ID_MAX];
  int saved_index;
  bool focus_saved, restore_pending, restore_focus;
  /* The narrator modal: speech kept on while it's shown, the choice applied
   * the next frame (speech_toggled then). */
  bool speech_choice_pending, speech_choice;
  /* Controllers. */
  bui_controller_modals controller_modals;
  bool pad_known, pad_was;
  /* Speech. */
  bui__str events, spoken_where, spoken_focus_id, last_line;
  bool holding, hold_heard;
  double hold_since;
};

/* Modals (modal.c): the controllers' changes; the shown modal declared and
 * drawn over the game's screen (at bui_end_frame); all let go of. */
void bui__controller_events(bui_context* ui);
void bui__modal_frame(bui_context* ui);
void bui__free_modals(bui_context* ui);
/* The frame's input taken: navigation, capture, the keys (context.c). */
void bui__take_input(bui_context* ui);

/* The frame's arena (strings good till the frame after next). */
static inline bui__arena* bui__frame_arena(bui_context* ui) { return &ui->arenas[ui->frame]; }

#endif
