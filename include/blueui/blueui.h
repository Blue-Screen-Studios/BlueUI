#ifndef BLUEUI_H
#define BLUEUI_H

/* BlueUI: a game UI that talks, in C. Immediate mode: each frame the game
 * hands in its input, declares its screen (its items, in order), and gets
 * back three things:
 *
 *   - the UI's state (bui_get_state): every item, what has the focus, what
 *     was said; for the game to act on and test against;
 *   - its geometry (bui_get_geometry): rectangles to draw, with any renderer;
 *   - its voice (bui_next_audio): WAV buffers of what's said, synthesized
 *     by the Flite voice built in, for the game to play.
 *
 * It works out focus, keyboard and controller navigation, clicks, scrolling
 * and what changed. It knows nothing of SDL or any platform: the game turns
 * its own events into a bui_input.
 *
 *   bui_context* ui = bui_create(NULL);
 *   ...each frame:
 *   bui_begin_frame(ui, &input);
 *   bui_screen(ui, "menu", "Main menu", &hints);
 *   if (bui_button(ui, &theme, "play", "PLAY", rect, NULL)) start_game();
 *   bui_end_frame(ui);
 *   draw(bui_get_geometry(ui, &count), count);
 *   bui_audio a;
 *   while (bui_next_audio(ui, &a)) { play(&a); bui_audio_free(&a); }
 *
 * Everything declared is reachable without a mouse and spoken: "Main menu.
 * Up and down to move... Play. Item 1 of 4, button". */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  if defined(BLUEUI_BUILDING)
#    define BUI_API __declspec(dllexport)
#  elif defined(BLUEUI_DLL)
#    define BUI_API __declspec(dllimport)
#  else
#    define BUI_API
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define BUI_API __attribute__((visibility("default")))
#else
#  define BUI_API
#endif

/* (C++: the fields a brace list leaves out zeroed, without a warning; C
 * zeroes them anyway.) */
#ifdef __cplusplus
#define BUI_ZERO = {}
#else
#define BUI_ZERO
#endif

/* ------------------------------------------------------------- geometry */

typedef struct bui_vec2 {
  float x, y;
} bui_vec2;

/* Pixels from the top left: x0, y0 inclusive, x1, y1 exclusive. */
typedef struct bui_rect {
  float x0, y0, x1, y1;
} bui_rect;

/* Linear color, straight alpha. */
typedef struct bui_color {
  float r, g, b, a;
} bui_color;

/* A rectangle to draw, blended over what's there. With a texture box
 * (u1 > u0: 0..1 in the game's UI texture, its icons) the texture there
 * instead, times the color. */
typedef struct bui_draw_cmd {
  float x0, y0, x1, y1;
  float r, g, b, a;
  float u0, v0, u1, v1;
} bui_draw_cmd;

static inline bui_rect bui_rect_make(float x0, float y0, float x1, float y1) {
  bui_rect r;
  r.x0 = x0, r.y0 = y0, r.x1 = x1, r.y1 = y1;
  return r;
}
static inline bui_color bui_rgba(float r, float g, float b, float a) {
  bui_color c;
  c.r = r, c.g = g, c.b = b, c.a = a;
  return c;
}
static inline bui_color bui_rgb(float r, float g, float b) { return bui_rgba(r, g, b, 1.0f); }
static inline bool bui_rect_empty(bui_rect r) { return r.x1 <= r.x0 || r.y1 <= r.y0; }
static inline bool bui_rect_contains(bui_rect r, bui_vec2 p) { return p.x >= r.x0 && p.x < r.x1 && p.y >= r.y0 && p.y < r.y1; }
/* (r wholly in o, give or take half a pixel) */
static inline bool bui_rect_inside(bui_rect r, bui_rect o) {
  return r.x0 >= o.x0 - 0.5f && r.y0 >= o.y0 - 0.5f && r.x1 <= o.x1 + 0.5f && r.y1 <= o.y1 + 0.5f;
}
static inline float bui_rect_w(bui_rect r) { return r.x1 - r.x0; }
static inline float bui_rect_h(bui_rect r) { return r.y1 - r.y0; }
static inline bui_vec2 bui_rect_center(bui_rect r) {
  bui_vec2 c;
  c.x = 0.5f * (r.x0 + r.x1), c.y = 0.5f * (r.y0 + r.y1);
  return c;
}
static inline bui_rect bui_rect_grow(bui_rect r, float d) { return bui_rect_make(r.x0 - d, r.y0 - d, r.x1 + d, r.y1 + d); }

/* ---------------------------------------------------------------- input */

typedef enum bui_key {
  BUI_KEY_UP,
  BUI_KEY_DOWN,
  BUI_KEY_LEFT,
  BUI_KEY_RIGHT,
  BUI_KEY_NEXT,           /* Tab: the next tab (a screen with tabs), else the next item */
  BUI_KEY_PREV,           /* Shift+Tab */
  BUI_KEY_ACTIVATE,       /* Enter, Space, A / Cross */
  BUI_KEY_BACK,           /* Escape, B / Circle */
  BUI_KEY_TAB_NEXT,       /* the bumpers: always the tabs */
  BUI_KEY_TAB_PREV,
  BUI_KEY_TOGGLE_SPEECH,  /* speech on and off */
  BUI_KEY_REPEAT,         /* the last line again */
  BUI_KEY_CANCEL_CAPTURE  /* a rebinding's wait given up (Escape, Start) */
} bui_key;

typedef enum bui_device { BUI_DEVICE_KEYBOARD, BUI_DEVICE_GAMEPAD } bui_device;

#define BUI_MAX_KEYS 32
#define BUI_MAX_PRESSED 16

/* A frame's input, from the game's own events. Start it with
 * bui_input_clear (the mouse nowhere, nothing pressed). */
typedef struct bui_input {
  bui_vec2 mouse;
  bool mouse_down;       /* the left button held */
  bool clicked;          /* pressed this frame, at click_at */
  bui_vec2 click_at;
  float wheel;           /* up positive */
  bui_key keys[BUI_MAX_KEYS];
  int key_count;
  int tab_hotkey;        /* a tab by its number (1: the first); 0: none */
  int64_t pressed[BUI_MAX_PRESSED];  /* every key and button pressed, in the game's codes (a rebinding's capture) */
  int pressed_count;
  bui_device device;
  double time;           /* seconds, steady */
  bool speech_playing;   /* the game still playing a line BlueUI handed it (bui_next_audio) */
} bui_input;

BUI_API void bui_input_clear(bui_input* in);
/* A key added (false: full). */
BUI_API bool bui_input_key(bui_input* in, bui_key key);
BUI_API bool bui_input_pressed(bui_input* in, int64_t code);

/* ---------------------------------------------------------------- items */

/* What an item is, as said after it ("Item 2 of 5, toggle"). */
typedef enum bui_kind {
  BUI_BUTTON,
  BUI_TAB,
  BUI_TOGGLE,
  BUI_SLIDER,
  BUI_CHOICE,
  BUI_TEXT,
  BUI_CUSTOM,
  BUI_KIND_COUNT
} bui_kind;

/* A thing on the screen: what bui_item works with (blueui/widgets.h's are
 * made of these; a game's own looks are too). Zero it, then fill it in;
 * its strings are copied (NULL: none). */
typedef struct bui_item {
  bui_kind kind BUI_ZERO;
  bui_rect rect BUI_ZERO;         /* its area: hovered, outlined when it has the focus, clicked */
  const char* spoken BUI_ZERO;    /* what's said when it's come to (its name, its value) */
  const char* value BUI_ZERO;     /* its state (as text): changed while it has the focus, `changed` said */
  const char* changed BUI_ZERO;   /* said when `value` changes (NULL: `spoken`) */
  const char* role BUI_ZERO;      /* a BUI_CUSTOM's kind, as said */
  bool disabled BUI_ZERO;         /* not clicked or changed; still reachable, so it can be heard */
  bool unfocusable BUI_ZERO;      /* hovered and clicked only (a tab) */
  bool back BUI_ZERO;             /* pressed by Back (Escape, B) */
  bui_rect press BUI_ZERO, prev BUI_ZERO, next BUI_ZERO;  /* clicked there: pressed, stepped back, stepped on (all empty: anywhere on it presses) */
  bui_rect track BUI_ZERO;        /* a slider's: the mouse held on it drags it */
} bui_item;

/* What happened to it this frame. */
typedef struct bui_result {
  bool hovered;
  bool focused;          /* has the keyboard's or controller's focus */
  bool activated;        /* clicked, Enter, A; Back for a back item */
  int step;              /* -1, +1: left, right (or its prev, next clicked) */
  bool dragging;
  float drag;            /* 0..1 along its track, while dragging */
} bui_result;

/* A screen's keys, said the first time it's come to. */
typedef struct bui_hints {
  const char* keyboard BUI_ZERO;
  const char* gamepad BUI_ZERO;   /* (NULL: the keyboard's) */
} bui_hints;

/* ----------------------------------------------------------------- state */

/* An item as it was at the last frame's end. */
typedef struct bui_item_state {
  const char* id;
  bui_kind kind;
  bui_rect rect;
  const char* spoken;
  const char* value;
  bool enabled, focusable;
  bool shown;            /* in its scroll view */
  bool focused, hovered;
  int index;             /* its place among the focusable ones (0: the first; -1: not one) */
} bui_item_state;

/* The UI as the last frame left it. Its strings stay good till the frame
 * after next begins. */
typedef struct bui_state {
  const bui_item_state* items;
  size_t item_count;
  const char* screen;    /* the screen's id */
  const char* page;      /* its page (tab), as named; "" none */
  const char* focused;   /* the focused item's id ("" none) */
  const char* capturing; /* the item waiting for a key (NULL: none) */
  const char* last_line; /* the last line given to the voice */
  bool navigating;       /* the keys moving the focus (outlined); else the mouse */
  bool speech_enabled;
  bool speech_toggled;   /* switched this frame (its key, or bui_set_speech_enabled's announcing) */
  bool back_pressed;     /* Back pressed, and no item took it */
  bui_device device;
} bui_state;

/* --------------------------------------------------------------- speech */

/* How an announcement is said: with the frame's line (after what changed
 * on screen), at once (cutting off what's being said), after what's being
 * said, or at once and held (an opening line: what the screen says waits
 * for it to finish, or 15 s, or a key). */
typedef enum bui_priority { BUI_SAY_NORMAL, BUI_SAY_INTERRUPT, BUI_SAY_QUEUE, BUI_SAY_HOLD } bui_priority;

/* What to do with it: play it at once (what's playing cut off), play it
 * after what's playing, or stop what's playing. */
typedef enum bui_audio_op { BUI_AUDIO_PLAY, BUI_AUDIO_QUEUE, BUI_AUDIO_STOP } bui_audio_op;

/* A line, spoken: a WAV file in memory (RIFF, PCM, 16 bits, mono), its
 * samples inside it. The game's, once taken: bui_audio_free it. */
typedef struct bui_audio {
  bui_audio_op op;
  char* text;            /* what's said (NULL: a STOP) */
  uint8_t* wav;          /* (NULL: a STOP, or no voice built in: the text alone, for a screen reader of the game's) */
  size_t wav_size;
  const int16_t* samples;
  size_t sample_count;
  int sample_rate;
} bui_audio;

/* Words BlueUI says itself (translate them, or reword). The pointers are
 * kept, not copied. */
typedef struct bui_strings {
  const char* item_of;   /* "Item {i} of {n}" */
  const char* speech_on;
  const char* speech_off;
  const char* tab;       /* "Settings, Audio tab" */
  const char* cancelled;
  const char* kinds[BUI_KIND_COUNT];
} bui_strings;

/* -------------------------------------------------------------- context */

typedef struct bui_config {
  int sample_rate BUI_ZERO;       /* the voice's WAVs (0: 48000) */
  float speech_rate BUI_ZERO;     /* how fast it speaks (0: 1.15; 1: the voice's own) */
  bool no_voice BUI_ZERO;         /* lines as text only (tests; a screen reader of the game's) */
} bui_config;

typedef struct bui_context bui_context;

/* (NULL: the defaults.) */
BUI_API bui_context* bui_create(const bui_config* config);
BUI_API void bui_destroy(bui_context* ui);
/* Everything about the screens forgotten (focus, what's been said, its
 * hints, scrolling); the voice kept. */
BUI_API void bui_reset(bui_context* ui);
BUI_API bool bui_has_voice(const bui_context* ui);

/* The frame: its input in, then the screen and its items declared, then
 * its end (navigation, the focus's outline, the line said). */
BUI_API void bui_begin_frame(bui_context* ui, const bui_input* in);
BUI_API void bui_end_frame(bui_context* ui);
BUI_API const bui_input* bui_get_input(const bui_context* ui);

/* The outputs. */
BUI_API const bui_state* bui_get_state(const bui_context* ui);
BUI_API const bui_draw_cmd* bui_get_geometry(const bui_context* ui, size_t* count);
/* The next line to play (true: one taken, the game's to free). */
BUI_API bool bui_next_audio(bui_context* ui, bui_audio* out);
BUI_API void bui_audio_free(bui_audio* audio);

/* The screen shown (its id: another, the focus on its first item; its
 * name: said when it's come to; its keys: said the first time). A page
 * within it (a tab): its name after the screen's. */
BUI_API void bui_screen(bui_context* ui, const char* id, const char* name, const bui_hints* hints);
BUI_API void bui_page(bui_context* ui, const char* name);

/* An item: its id (unique on the screen, under 64 bytes), what it is.
 * Declared in the order the keys go through them. */
BUI_API bui_result bui_declare(bui_context* ui, const char* id, const bui_item* item);
/* The item just declared, as it is now (its call changed it: its value,
 * what's said of it), so the change is said this frame. */
BUI_API void bui_revise(bui_context* ui, const char* value, const char* spoken);

/* A screen's tabs: switched by Tab, Shift+Tab, the bumpers, their numbers;
 * this frame's step and jump (taken by the call). */
BUI_API int bui_take_tab_step(bui_context* ui); /* -1, +1, 0 */
BUI_API int bui_take_tab_jump(bui_context* ui); /* a tab's index (0: the first); -1: none */

/* Said: with this frame's line, or by `priority` (not while speech is off). */
BUI_API void bui_announce(bui_context* ui, const char* text, bui_priority priority);
/* Said whether speech is on or not (an offer to switch it on). */
BUI_API void bui_say(bui_context* ui, const char* text, bool queued);
BUI_API void bui_stop_speech(bui_context* ui);

/* Speech on or off (the game's setting: off, nothing's said). Toggled by
 * BUI_KEY_TOGGLE_SPEECH, its line said either way. `announce`: said as the
 * key's would be ("Narrator on. Where it is"). */
BUI_API void bui_set_speech_enabled(bui_context* ui, bool on, bool announce);
BUI_API bool bui_speech_enabled(const bui_context* ui);

BUI_API bool bui_back_pressed(const bui_context* ui);
BUI_API bool bui_navigating(const bui_context* ui);
/* The focus shown from the start (the keys', not the mouse's, till it moves). */
BUI_API void bui_begin_navigating(bui_context* ui);
/* The screen come to already said (an opening line): not again. */
BUI_API void bui_mark_arrived(bui_context* ui);
/* A form's rows, counted through the frame (every other one striped). */
BUI_API int bui_row_stripe(bui_context* ui);

/* A scrolled view: the items declared till bui_end_scroll in `view`
 * (their rects as laid out, scrolled up by the offset returned); those not
 * wholly in it neither hovered nor clicked. The wheel scrolls it, the focus
 * kept in it. `content`: how tall what's in it is. */
BUI_API float bui_begin_scroll(bui_context* ui, const char* id, bui_rect view, float content);
BUI_API void bui_end_scroll(bui_context* ui);
BUI_API bool bui_visible(const bui_context* ui, bui_rect r); /* (in the current view, if there's one) */
BUI_API float bui_scroll_of(const bui_context* ui, const char* id);
BUI_API void bui_set_scroll(bui_context* ui, const char* id, float offset);
/* Item `id` scrolled into its view (this frame's end: shown the next). */
BUI_API void bui_reveal(bui_context* ui, const char* id);

/* A rebinding's wait: `id`'s, till a key or button (bui_input.pressed),
 * BUI_KEY_CANCEL_CAPTURE, or `seconds`. */
BUI_API void bui_start_capture(bui_context* ui, const char* id, double seconds);
BUI_API bool bui_capturing(const bui_context* ui, const char* id); /* (NULL: any) */
/* Its outcome, once (for `id`): true, with the code pressed, or cancelled. */
BUI_API bool bui_take_captured(bui_context* ui, const char* id, int64_t* code, bool* cancelled);

/* The focus. */
BUI_API const char* bui_focused_id(const bui_context* ui);
BUI_API void bui_focus(bui_context* ui, const char* id);
/* The last frame's item, by id or by what's said (its start); NULL: none. */
BUI_API const bui_item_state* bui_find(const bui_context* ui, const char* id_or_spoken);

BUI_API void bui_set_strings(bui_context* ui, const bui_strings* strings);
BUI_API const bui_strings* bui_get_strings(const bui_context* ui);
/* The focus's outline (round the item with it, while navigating). */
BUI_API void bui_set_focus_style(bui_context* ui, bui_color color, float width, float gap);

/* Drawing, into the frame's geometry (in order with the widgets'). */
BUI_API void bui_draw(bui_context* ui, const bui_draw_cmd* cmds, size_t count);
BUI_API void bui_fill(bui_context* ui, bui_rect r, bui_color c);
/* A string good till the frame after next (printf's format). */
BUI_API const char* bui_format(bui_context* ui, const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/* ----------------------------------------------------------------- font */

/* The built-in font: 5 x 7 pixel bitmap glyphs (ASCII's printable
 * characters that matter: letters (lower case drawn as upper), digits,
 * punctuation), each font pixel `pixel` screen pixels; glyphs 6 font
 * pixels apart, lines 9. Unknown characters draw as a box. */
#define BUI_GLYPH_W 5
#define BUI_GLYPH_H 7
#define BUI_GLYPH_ADVANCE 6
#define BUI_LINE_ADVANCE 9
#define BUI_NUL ((size_t)-1) /* (a length: up to its NUL) */

/* Text at (x, y) (its top left) as rectangles, with a one-font-pixel drop
 * shadow when `shadow` > 0 (its darkness): how many there are, the first
 * `cap` written to `out`. */
BUI_API size_t bui_text_rects(const char* text, size_t len, float x, float y, float pixel, bui_color color, float shadow,
                              bui_draw_cmd* out, size_t cap);
BUI_API void bui_text(bui_context* ui, const char* text, float x, float y, float pixel, bui_color color, float shadow);
BUI_API float bui_text_width(const char* text, size_t len, float pixel); /* (its longest line) */
BUI_API bool bui_has_glyph(char c);
/* Its rows (BUI_GLYPH_H, top first; BUI_GLYPH_W bits each, the leftmost
 * the highest): the font's, or a box. */
BUI_API const unsigned char* bui_glyph_rows(char c);

/* ---------------------------------------------------------------- voice */

/* `text` spoken now, on this thread: its WAV (the game's: bui_audio_free).
 * False: no voice. */
BUI_API bool bui_synthesize(bui_context* ui, const char* text, bui_audio* out);

#ifdef __cplusplus
}
#endif

#endif
