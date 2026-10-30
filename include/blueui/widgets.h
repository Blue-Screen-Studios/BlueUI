#ifndef BLUEUI_WIDGETS_H
#define BLUEUI_WIDGETS_H

/* BlueUI's widgets: bui_declare with a look (a theme's), each said as it
 * should be. A screen made of them needs nothing else to be heard and used
 * without a mouse. (A look of the game's own: bui_declare, drawn by it.) */

#include "blueui/blueui.h"

#ifdef __cplusplus
extern "C" {
#endif

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
BUI_API void bui_label(bui_context* ui, const char* text, float x, float y, bui_color c, float shadow, float pixel);
BUI_API void bui_centered(bui_context* ui, bui_rect r, const char* text, bui_color c, float pixel);

/* A window: an edge, a panel, a title bar (its title; one at its right, or NULL). */
BUI_API void bui_window(bui_context* ui, bui_rect r, const char* title, const char* right);

BUI_API bool bui_button(bui_context* ui, const char* id, const char* text, bui_rect r, const bui_button_opts* opts);

/* Tabs along `area`'s top (each as wide as its name needs, `min_width`
 * at least; 0: 64), the page named for the one picked. `names` as said
 * ("Audio": shown upper case, numbered). True: another picked. */
BUI_API bool bui_tab_bar(bui_context* ui, const char* id, bui_rect area, const char* const* names, int count,
                         int* current, float min_width);

/* A form's row: its name at the left, what it does, its value, its control
 * at the right end. True: its value changed (a button: pressed). */
typedef struct bui_row_parts {
  bui_rect row, control, prev, next;
} bui_row_parts;
BUI_API bui_row_parts bui_row_layout(const bui_theme* t, bui_rect row);

/* (on_text, off_text NULL: "ON", "OFF".) */
BUI_API bool bui_toggle_row(bui_context* ui, const char* id, bui_rect row, const bui_field* f, bool* on,
                            const char* on_text, const char* off_text);
/* (Each state its own words: shown, what it means, what's said.) */
BUI_API bool bui_toggle_row_ex(bui_context* ui, const char* id, bui_rect row, const bui_field* f, bool* on,
                               const bui_option* off, const bui_option* on_option);
BUI_API bool bui_choice_row(bui_context* ui, const char* id, bui_rect row, const bui_field* f, int* index,
                            const bui_option* options, int count);
/* (`shown`: a value as shown, and said; `spoken`, or NULL: what's said of
 * it at a value, whole.) */
BUI_API bool bui_slider_row(bui_context* ui, const char* id, bui_rect row, const bui_field* f, float* value, float lo,
                            float hi, float step, bui_format_fn shown, bui_format_fn spoken, void* user);
/* A slider of marked stops. */
BUI_API bool bui_stops_row(bui_context* ui, const char* id, bui_rect row, const bui_field* f, int* stop,
                           const bui_option* stops, int count);
BUI_API bool bui_button_row(bui_context* ui, const char* id, bui_rect row, const bui_field* f, const char* text);
/* A foldout's head: pressed, open or shut (what's in it the caller's, while open). */
BUI_API bool bui_foldout_row(bui_context* ui, const char* id, bui_rect row, const bui_field* f, bool* open);

/* A rebinding: its item pressed, the wait begun and `prompt` said; the key
 * or button then pressed, or its cancelling. */
typedef struct bui_captured {
  bool started, cancelled;
  bool has_code;
  int64_t code;
  bool waiting; /* waiting now: draw it so */
} bui_captured;
BUI_API bui_captured bui_capture(bui_context* ui, const char* id, const bui_item* item, const char* prompt, double seconds);

/* ----------------------------------------------------------------- grids */

/* A grid of cells: an inventory's slots, a palette, a board. Square cells,
 * or hexagons in a honeycomb: pointy-top (rows side by side, every other
 * row shifted half a cell) or flat-top (columns, every other one shifted
 * down half a cell). Each cell is an item of its own (hovered, clicked,
 * focused, said); the arrow keys move between them in two dimensions, and
 * Up or Down off its edge go on to the screen's items before or after it.
 * It draws each cell's face and ring; what's in a cell is the game's to
 * draw (draw_cell). Clicks land on the hexagon under the mouse exactly. */
typedef enum bui_grid_shape { BUI_GRID_SQUARE, BUI_GRID_HEX_POINTY, BUI_GRID_HEX_FLAT } bui_grid_shape;

/* draw_cell's state bits. */
#define BUI_CELL_HOVERED 1u  /* under the mouse (or focused, while navigating) */
#define BUI_CELL_FOCUSED 2u
#define BUI_CELL_SELECTED 4u

typedef struct bui_grid_opts {
  bui_grid_shape shape BUI_ZERO;
  int columns BUI_ZERO, rows BUI_ZERO;  /* (rows 0: as many as count needs) */
  int count BUI_ZERO;                   /* cells, row by row (0: columns x rows) */
  /* A cell's size: a square's side; a pointy hexagon's width (flat side to
   * flat side: its height 2 / sqrt(3) of it); a flat one's height. */
  float cell BUI_ZERO;
  float gap BUI_ZERO;                   /* between cells */
  bool stagger_first BUI_ZERO;          /* hexagons: the first row (column) shifted, not the second */
  int selected BUI_ZERO;                /* the cell lit as selected (-1: none; zeroed: the first) */
  bool inert BUI_ZERO;                  /* drawn only: no items (neither focused, hovered nor clicked) */
  bool no_faces BUI_ZERO;               /* the game draws the cells whole (draw_cell alone) */
  const char* role BUI_ZERO;            /* each cell's kind, as said (NULL: "slot") */
  /* What's said of cell `index` (NULL: "Slot n"); the string the game's
   * till the call returns. */
  const char* (*spoken)(void* user, int index);
  /* Cell `index`'s contents, drawn over its face, in `inner` (its square,
   * or the hexagon's inscribed square), BUI_CELL_* `state`. */
  void (*draw_cell)(void* user, int index, bui_rect inner, unsigned state);
  void* user;
  /* The hexagon's shape in the game's UI texture (a white mask filling the
   * box; a pointy one for pointy cells, a flat one for flat cells) and its
   * ring's. Empty: drawn from rectangles. */
  bui_rect hex_uv BUI_ZERO, ring_uv BUI_ZERO;
  float ring BUI_ZERO;                  /* the ring's thickness (0: the theme's pixel) */
} bui_grid_opts;

typedef struct bui_grid_result {
  int hovered;        /* the cell under the mouse, or focused (-1: none) */
  int focused;        /* (-1: none) */
  int activated;      /* clicked, Enter, A (-1: none) */
  int alt_activated;  /* right-clicked (-1: none) */
} bui_grid_result;

/* The grid at `area`'s top left (its size: bui_grid_size). In a scroll
 * view, `area` as laid out, scrolled up by its offset. */
BUI_API bui_grid_result bui_grid(bui_context* ui, const char* id, bui_rect area, const bui_grid_opts* opts);
/* Its whole size, every cell in it. */
BUI_API bui_vec2 bui_grid_size(const bui_grid_opts* opts);
/* Where cell `index` is: its bounding box (hexagons: point to point). */
BUI_API bui_rect bui_grid_cell_rect(const bui_grid_opts* opts, bui_rect area, int index);
/* The cell under `p` (-1: none): hexagons by their exact shape. */
BUI_API int bui_grid_hit(const bui_grid_opts* opts, bui_rect area, bui_vec2 p);
/* The cell the arrow keys go to from `index` (dx, dy: -1, 0, +1): -1 off its edge. */
BUI_API int bui_grid_neighbor(const bui_grid_opts* opts, int index, int dx, int dy);
/* A hexagon (pointy or flat, filling `box`) as rectangles, the first `cap`
 * written to `out`: how many there are. */
BUI_API size_t bui_hex_rects(bui_rect box, bool pointy, bui_color c, bui_draw_cmd* out, size_t cap);

/* ------------------------------------------------------------ tech tree */

/* A tech tree: boxes of text joined by lines, a button under each, on a
 * canvas dragged to pan and wheeled to zoom (and a strip of -, +, FIT
 * under it, for the keys). It draws and reports; what a node costs, and
 * whether it may be had, is the game's: its button pressed, the game asked. */
typedef enum bui_tech_state { BUI_TECH_LOCKED, BUI_TECH_AVAILABLE, BUI_TECH_RESEARCHING, BUI_TECH_UNLOCKED } bui_tech_state;

typedef struct bui_tech_node {
  const char* title BUI_ZERO;    /* its box's first line */
  const char* detail BUI_ZERO;   /* its second ("$400  2H", "UNLOCKED"; NULL: none) */
  const char* spoken BUI_ZERO;   /* (NULL: its title and detail) */
  float x BUI_ZERO, y BUI_ZERO;  /* its box's centre, in the tree's units (a pixel at zoom 1) */
  bui_tech_state state BUI_ZERO;
  float progress BUI_ZERO;       /* 0..1, while researching: a bar along its foot */
  const char* button BUI_ZERO;   /* the button under it, its label (NULL: none) */
  bool button_disabled BUI_ZERO; /* (heard, not pressed) */
  bool pressable BUI_ZERO;       /* its box the button (no button under it): clicked (let go on it, not dragged), Enter */
} bui_tech_node;

/* A line from a node down to one that needs it (indices). */
typedef struct bui_tech_link {
  int from BUI_ZERO, to BUI_ZERO;
} bui_tech_link;

/* Where the tree's looked at: the game's, kept between frames. Zeroed: the
 * whole tree fitted in at its first frame, its first node selected. */
typedef struct bui_tech_view {
  float x BUI_ZERO, y BUI_ZERO;  /* the point at the canvas's centre */
  float zoom BUI_ZERO;           /* screen pixels per unit (0: fitted, next frame) */
  int selected BUI_ZERO;         /* the node picked (-1: none) */
  /* (Its own: a press, a drag, the focus last revealed.) */
  bool pressing BUI_ZERO, panning BUI_ZERO, press_empty BUI_ZERO;
  bui_vec2 press_at BUI_ZERO, last BUI_ZERO;
  int revealed BUI_ZERO;
  int press_node BUI_ZERO;  /* (the node pressed on, + 1: 0 none) */
} bui_tech_view;

/* Its sizes, in units (0: the defaults: 120 x 40 boxes, 16 tall buttons,
 * 0.35..2.5 zoom). Across: its nodes left to right (the lines from a
 * node's right side to the next's left), panned and zoomed as any tree,
 * Left and Right panning it too. */
typedef struct bui_tech_tree_opts {
  float node_w BUI_ZERO, node_h BUI_ZERO;
  float min_zoom BUI_ZERO, max_zoom BUI_ZERO;
  float button_h BUI_ZERO;
  bool horizontal BUI_ZERO;
} bui_tech_tree_opts;

/* The tree in `area` (its canvas, the strip of zoom buttons at its foot).
 * The node whose button was pressed this frame (or, pressable, the node
 * itself: the game asked to unlock it), else -1. `view->selected`: the node clicked or chosen (Enter). */
BUI_API int bui_tech_tree(bui_context* ui, const char* id, bui_rect area, const bui_tech_node* nodes, int count,
                          const bui_tech_link* links, int link_count, bui_tech_view* view, const bui_tech_tree_opts* opts);
/* The whole tree fitted in `area`'s canvas. */
BUI_API void bui_tech_fit(bui_context* ui, bui_tech_view* view, bui_rect area, const bui_tech_node* nodes, int count, const bui_tech_tree_opts* opts);

#ifdef __cplusplus
}
#endif

#endif
