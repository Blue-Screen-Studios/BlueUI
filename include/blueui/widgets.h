#ifndef BLUEUI_WIDGETS_H
#define BLUEUI_WIDGETS_H

/* BlueUI's widgets: bui_declare with a look (a theme's), each said as it
 * should be. A screen made of them needs nothing else to be heard and used
 * without a mouse. (A look of the game's own: bui_declare, drawn by it.) */

#include "blueui/blueui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bui_theme {
  float pixel; /* screen pixels a font pixel (every size below is in font pixels) */
  /* Text. */
  bui_color text, dim, blurb, value, off;
  /* Rows: every other one striped; the one under the mouse lit. */
  bui_color stripe, hover, rule;
  /* Windows: an edge, a panel, a title bar, its title, the words at its right. */
  bui_color edge, panel, title_bar, title, title_right;
  /* Tabs. */
  bui_color tab_on, tab_off, tab_hot, tab_text, tab_text_off;
  /* Toggles. */
  bui_color toggle_on, toggle_on_hot, toggle_off, toggle_off_hot, toggle_text_on, toggle_text_off;
  /* Choices: a well, its arrows. */
  bui_color well, arrow, arrow_hot, choice_text;
  /* Sliders. */
  bui_color track, fill, knob, knob_hot, knob_off, tick;
  /* Buttons: an edge, a face (lit under the mouse), a label. */
  bui_color button_edge, button, button_hot, button_text, button_text_off;
  bool button_edged;
  float control_width, arrow_width, blurb_at;
} bui_theme;

/* Green, like a swamp at night. */
BUI_API bui_theme bui_default_theme(void);

/* A row's setting: its name, what it does (shown when it fits), and what's
 * said of it (NULL: its name and value, and what it does while it's off). */
typedef struct bui_field {
  const char* name BUI_ZERO;
  const char* description BUI_ZERO;
  const char* spoken BUI_ZERO;
  bool disabled BUI_ZERO;
} bui_field;

/* One of a choice's: as shown, what it means (the row's description while
 * it's picked; NULL: the field's), as said (NULL: its shown name). */
typedef struct bui_option {
  const char* shown BUI_ZERO;
  const char* description BUI_ZERO;
  const char* spoken BUI_ZERO;
} bui_option;

/* A button's extras (NULL: none). */
typedef struct bui_button_opts {
  const char* spoken BUI_ZERO; /* (NULL: its label) */
  bool disabled BUI_ZERO;
  bool back BUI_ZERO;          /* pressed by Back too */
  float pixel BUI_ZERO;        /* its label's (0: the theme's) */
} bui_button_opts;

/* A value as shown or said ("80%"), written into `out`. */
typedef void (*bui_format_fn)(float value, char* out, size_t size, void* user);

/* Drawing helpers (pixel 0: the theme's). */
BUI_API void bui_label(bui_context* ui, const bui_theme* t, const char* text, float x, float y, bui_color c, float shadow, float pixel);
BUI_API void bui_centered(bui_context* ui, const bui_theme* t, bui_rect r, const char* text, bui_color c, float pixel);

/* A window: an edge, a panel, a title bar (its title; one at its right, or NULL). */
BUI_API void bui_window(bui_context* ui, const bui_theme* t, bui_rect r, const char* title, const char* right);

BUI_API bool bui_button(bui_context* ui, const bui_theme* t, const char* id, const char* text, bui_rect r, const bui_button_opts* opts);

/* Tabs along `area`'s top (each as wide as its name needs, `min_width`
 * at least; 0: 64), the page named for the one picked. `names` as said
 * ("Audio": shown upper case, numbered). True: another picked. */
BUI_API bool bui_tab_bar(bui_context* ui, const bui_theme* t, const char* id, bui_rect area, const char* const* names, int count,
                         int* current, float min_width);

/* A form's row: its name at the left, what it does, its value, its control
 * at the right end. True: its value changed (a button: pressed). */
typedef struct bui_row_parts {
  bui_rect row, control, prev, next;
} bui_row_parts;
BUI_API bui_row_parts bui_row_layout(const bui_theme* t, bui_rect row);

/* (on_text, off_text NULL: "ON", "OFF".) */
BUI_API bool bui_toggle_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, bool* on,
                            const char* on_text, const char* off_text);
/* (Each state its own words: shown, what it means, what's said.) */
BUI_API bool bui_toggle_row_ex(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, bool* on,
                               const bui_option* off, const bui_option* on_option);
BUI_API bool bui_choice_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, int* index,
                            const bui_option* options, int count);
/* (`shown`: a value as shown, and said; `spoken`, or NULL: what's said of
 * it at a value, whole.) */
BUI_API bool bui_slider_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, float* value, float lo,
                            float hi, float step, bui_format_fn shown, bui_format_fn spoken, void* user);
/* A slider of marked stops. */
BUI_API bool bui_stops_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, int* stop,
                           const bui_option* stops, int count);
BUI_API bool bui_button_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, const char* text);
/* A foldout's head: pressed, open or shut (what's in it the caller's, while open). */
BUI_API bool bui_foldout_row(bui_context* ui, const bui_theme* t, const char* id, bui_rect row, const bui_field* f, bool* open);

/* A rebinding: its item pressed, the wait begun and `prompt` said; the key
 * or button then pressed, or its cancelling. */
typedef struct bui_captured {
  bool started, cancelled;
  bool has_code;
  int64_t code;
  bool waiting; /* waiting now: draw it so */
} bui_captured;
BUI_API bui_captured bui_capture(bui_context* ui, const char* id, const bui_item* item, const char* prompt, double seconds);

#ifdef __cplusplus
}
#endif

#endif
