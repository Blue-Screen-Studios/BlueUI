# Widgets

`blueui/widgets.h` provides themed widgets built on `bui_declare`. Each widget declares its item, applies the frame's input, speaks itself correctly and draws itself. A screen built from them needs no extra accessibility code.

All widgets take the context, a unique id and a rectangle in pixels, and draw with the context's theme. Widgets that change a value take it by pointer, update it in place, and return `true` when it changed. Buttons return `true` when pressed.

## Themes

A `bui_theme` is a game's whole look and feel: every colour, the size multiplier, the focus outline, and which built-in [GUI sound pack](speech-and-audio.md#gui-sounds) plays (None, Soft or Crisp). Each game defines its theme once and passes it to BlueUI. Widgets take no styling arguments.

```c
bui_theme theme = bui_default_theme();      /* start from BlueUI's blue */
theme.pixel = 3.0f;                         /* screen pixels per font pixel */
theme.panel = bui_rgba(0.08f, 0.03f, 0.02f, 0.97f);
theme.fill = bui_rgb(1.0f, 0.45f, 0.15f);
theme.focus = bui_rgb(0.4f, 0.9f, 1.0f);    /* the focus outline */

bui_config config = {0};
config.theme = &theme;
bui_context* ui = bui_create(&config);
```

- **Units.** Sizes in the theme (`control_width`, `arrow_width`, `blurb_at`, `focus_width`, `focus_gap`) are in font pixels and are multiplied by `pixel`.
- **Colours.** All colours are linear RGB with straight alpha.
- **Copying.** The theme is copied, so the original can be discarded.

`bui_set_theme(ui, &theme)` replaces the theme at any time, also copying it. It applies to everything declared after the call, so a game can:

- update `pixel` when the window is resized;
- give each screen its own variant (a shop's colours, a pause menu's);
- style a single widget differently, then switch back:

```c
bui_theme flat = *bui_get_theme(ui);
flat.button_edged = false;
bui_set_theme(ui, &flat);
bui_button(ui, "back", "BACK", rect, &opts);
bui_set_theme(ui, &theme);
```

The focus outline is drawn at `bui_end_frame` with the theme in effect at that point. `bui_set_theme(ui, NULL)` restores the default.

### The default theme

`bui_default_theme()` is BlueUI's own look: deep navy panels, a bright blue accent for fills and selected tabs, and a warm gold focus outline. Gold stands out clearly against the blues, which helps players with low vision follow the focus. The [feature tour](../demo/main.c) includes three more themes to start from: green, warm red, and high contrast.

## Drawing helpers

| Function | Draws |
|---|---|
| `bui_fill(ui, rect, color)` | A solid rectangle. |
| `bui_text(ui, text, x, y, pixel, color, shadow)` | Text at a position. `shadow > 0` adds a one-font-pixel drop shadow of that opacity. |
| `bui_label(ui, text, x, y, color, shadow, pixel)` | Text at the theme's pixel size, or `pixel` when non-zero. |
| `bui_centered(ui, rect, text, color, pixel)` | Text centred in a rectangle. |
| `bui_window(ui, rect, title, right)` | A panel with an edge, a title bar, a title, and optional text at the right of the title bar. |

## Buttons

```c
bui_button_opts opts = {0};
opts.spoken = "Back";        /* spoken text; NULL uses the label */
opts.back = true;            /* also pressed by BUI_KEY_BACK */
if (bui_button(ui, "back", "BACK", rect, &opts)) go_back();
```

Pass `NULL` for default options. A disabled button (`opts.disabled`) is still reachable and spoken, but cannot be pressed.

Spoken as: *"Back. Item 3 of 3, button"*.

## Tab bars

```c
static const char* tabs[] = {"Audio", "Video", "Controls"};
static int tab = 0;
if (bui_tab_bar(ui, "tabs", bar_rect, tabs, 3, &tab, 0.0f)) reset_scroll();
```

Tabs are laid out left to right along the top of `bar_rect`. Each tab is as wide as its label needs, and at least `min_width` font pixels wide (64 when 0). Labels are shown upper case and numbered, such as "1 AUDIO". The tab bar:

- switches on Tab and Shift+Tab, the bumper keys, number hotkeys, and clicks;
- names the current page (`bui_page`), so arriving on a tab is spoken as "Settings, Audio tab";
- declares its tabs as unfocusable items with ids `"<id>#0"`, `"<id>#1"`, and so on.

## Form rows

The row widgets lay out a settings-style row: the name at the left, a description when it fits, the value, and a control at the right end. Every other row is striped, and the row under the mouse is highlighted.

Each takes a `bui_field`:

```c
bui_field f = {0};
f.name = "Filter strength";               /* shown upper case */
f.description = "How much of the filter"; /* shown when it fits */
f.spoken = NULL;                          /* NULL: generated from name and value */
f.disabled = !filter_on;                  /* reachable and spoken, not changeable */
```

When a row is disabled and has a description, the description is spoken as the reason. For example: *"Filter strength, 100%. Pick a filter first"*.

`bui_row_layout(bui_get_theme(ui), row)` returns the rectangles a row uses (`row`, `control`, `prev`, `next`). Use it to align custom rows or to locate a control in tests.

### Toggle

```c
bui_toggle_row(ui, "fullscreen", rect, &f, &fullscreen, NULL, NULL);   /* "ON" / "OFF" */
bui_toggle_row(ui, "audio", rect, &f, &headphones, "HEADPHONES", "SPEAKERS");
```

Activate, Left, Right or a click on the control flips it. It is spoken as *"Fullscreen, on"*.

`bui_toggle_row_ex` takes a `bui_option` for each state, so each state can have its own label, description and spoken text:

```c
bui_option off = {"OFF", "Light passes glass plainly", "Colored shadows, off. A little faster"};
bui_option on  = {"ON",  "Glass tints the light",      "Colored shadows, on"};
bui_toggle_row_ex(ui, "colored", rect, &f, &colored, &off, &on);
```

### Choice

```c
bui_option modes[] = {
  {"SINGLE", "1 ray a face: the fastest", NULL},
  {"4 CORNERS", "4 rays a face: cleaner edges", NULL},
};
bui_choice_row(ui, "mode", rect, &f, &mode, modes, 2);
```

Cycles through options with Left and Right, the arrow buttons, or Activate. The selected option's description replaces the row's while it is selected. It is spoken as *"Sample mode, SINGLE. 1 ray a face: the fastest"*, or as the option's own `spoken` text.

### Slider

```c
static void percent(float v, char* out, size_t size, void* user) {
  snprintf(out, size, "%d%%", (int)(v * 100.0f + 0.5f));
}
bui_slider_row(ui, "volume", rect, &f, &volume, 0.0f, 1.0f, 0.05f, percent, NULL, NULL);
```

- Left and Right step by `step`, snapped to multiples of it from `lo`.
- Dragging on the track sets the value directly.
- `shown` formats the value for display and speech, such as "80%".
- `spoken`, when given, formats the whole spoken line, such as "Mouse sensitivity, 1.50 times".
- `user` is passed to both formatters.

### Stops

```c
bui_option ranges[] = {{"3.2 M"}, {"6.4 M"}, {"12.8 M"}, {"25.6 M"}};
bui_stops_row(ui, "range", rect, &f, &stop, ranges, 4);
```

A slider with a fixed number of marked positions. It steps without wrapping, and dragging snaps to the nearest stop. Each stop is a `bui_option` with its own label, description and spoken text.

### Button row

```c
bui_field reset = {"Reset controls", "Every key back to its default"};
if (bui_button_row(ui, "reset", rect, &reset, "RESET")) reset_bindings();
```

A row whose control is a button. It is spoken as *"Reset controls: Every key back to its default"*.

### Foldout

```c
static bool open = false;
bui_field shadows = {"Shadows", "How shadows are worked out"};
bui_foldout_row(ui, "shadows", rect, &shadows, &open);
if (open) { /* declare the rows inside */ }
```

A heading that opens and closes a section. It is spoken as *"Shadows, expanded"* or *"Shadows, collapsed"*.

## Rebinding

`bui_capture` turns an item into a rebinding slot. When the item is pressed, BlueUI waits for the next code reported with `bui_input_pressed`, and announces `prompt`. While it waits, the frame's keys do not navigate.

```c
bui_item slot = {0};
slot.rect = rect;
slot.spoken = "Jump, Space";
bui_captured c = bui_capture(ui, "bind.jump", &slot, "Press a key for jump. Escape to cancel", 8.0);
if (c.has_code) bind_jump(c.code);
draw_slot(rect, c.waiting ? "PRESS A KEY" : "SPACE");
```

The wait ends with the first pressed code, with `BUI_KEY_CANCEL_CAPTURE`, or after `seconds` (8 when 0). A cancel or timeout sets `cancelled` and speaks "Cancelled". The lower-level calls `bui_start_capture`, `bui_capturing` and `bui_take_captured` are available for custom flows.

## Tech trees

A tech tree draws text boxes joined by lines, with a button under each box. The canvas pans when dragged and zooms with the wheel about the cursor. A strip under it holds **-**, **+** and **FIT** buttons for keyboard and controller players. The widget decides nothing: when a node's button is pressed it returns that node's index, and the game checks costs and prerequisites and updates the node's state for the next frame.

```c
static bui_tech_view view;                   /* zeroed: fitted on the first frame */
bui_tech_node nodes[] = {
  {"Bucket", "UNLOCKED", NULL, 0, 0, BUI_TECH_UNLOCKED, 0, NULL, false},
  {"Bins", "$100  1H", NULL, 0, 100, BUI_TECH_AVAILABLE, 0, "UNLOCK", false},
};
bui_tech_link links[] = {{0, 1}};            /* parent -> child: drawn as an elbow line */
int asked = bui_tech_tree(ui, "tree", area, nodes, 2, links, 1, &view, NULL);
if (asked >= 0) try_unlock(asked);           /* the game's rules */
if (view.selected >= 0) show_details(view.selected);
```

- Node positions are box centres in tree units (one unit is one pixel at zoom 1). Boxes default to 120 x 40 units.
- Clicking a node, pressing Enter on it, or moving the focus to it selects it (`view.selected`). Clicking empty canvas without dragging clears the selection.
- A press that moves more than 4 pixels pans and does not select.
- Everything is clipped to the canvas. Nodes outside it can't be hovered or clicked, and moving the focus to one pans it into view.
- Text is drawn at a whole font pixel (`theme.pixel * zoom`, rounded down) and hidden when that falls below 1. The detail line is dropped when it no longer fits.
- A disabled button (`button_disabled`) is still focusable and spoken, but pressing it does nothing.

Spoken as: *"Bins, $100 1H. Item 2 of 5, tech"* and *"UNLOCK, Bins. Item 3 of 5, button"*.

## Grids

A grid lays out cells: squares, or hexagons in a honeycomb. Pointy-top hexagons sit in rows, every other row shifted half a cell. Flat-top hexagons sit in columns, every other column shifted down half a cell. Use it for inventories, palettes and boards.

```c
bui_grid_opts o = {0};
o.shape = BUI_GRID_HEX_POINTY;   /* or BUI_GRID_SQUARE, BUI_GRID_HEX_FLAT */
o.columns = 10, o.rows = 3;      /* count 0: columns x rows */
o.cell = 40.0f, o.gap = 2.0f;    /* a pointy hexagon's width, flat side to flat side */
o.selected = hotbar_slot;        /* lit as selected; -1 for none */
o.spoken = slot_name;            /* what's said of a cell */
o.draw_cell = draw_item;         /* the cell's contents, in its inscribed square */
o.user = &inventory;
o.hex_uv = atlas_hex, o.ring_uv = atlas_hex_ring;  /* optional masks in the UI texture */
bui_grid_result r = bui_grid(ui, "backpack", bui_rect_make(x, y, x + 1, y + 1), &o);
if (r.activated >= 0) pick_up(r.activated);        /* click, Enter, A */
if (r.alt_activated >= 0) split(r.alt_activated);  /* right-click (bui_input.alt_clicked) */
```

- Each cell is an item of its own, with the id `"<id>#<n>"`, the role `slot` (or `o.role`) and the text from `o.spoken`. Its value is the same text, so a change to what's in a cell is spoken while the cell has the focus.
- The arrow keys move between cells in two dimensions. Up or Down off the grid's edge moves on to the item before or after it. Down into a short last row goes to its last cell.
- Clicks and hovering go by the cell's exact shape, so a click between hexagons hits nothing. `bui_grid_hit` does the same test for any point.
- Without masks, hexagons are drawn from rectangles (`bui_hex_rects`). With `hex_uv` and `ring_uv`, each takes one textured rectangle.
- `inert` draws the grid without declaring items: a hotbar shown during play, for example. `no_faces` leaves all the drawing to `draw_cell`.
- In a scroll view, pass `area` already moved up by the offset. Cells not wholly in the view are not drawn.
- `bui_grid_size`, `bui_grid_cell_rect` and `bui_grid_neighbor` give the layout to the game, for centring the grid or drawing a dragged item.

The theme colours are `grid_cell`, `grid_ring`, `grid_selected`, `grid_selected_face` and `grid_hover`.

Spoken as: *"Pickaxe. Item 4 of 40, slot"*.

## File picker

A file picker lists a folder's subfolders, then its files with one of the given extensions. It reads the disk itself, so a game needs no platform dialog. The game keeps its state between frames and shows it every frame until it returns.

```c
static bui_file_picker picker;                 /* zeroed: the home folder */
picker.extensions = ".hxlicense;.txt";         /* NULL: every file */
switch (bui_file_picker_show(ui, "pick", area, &picker, "IMPORT LICENSE")) {
  case BUI_PICK_CHOSEN: load(picker.chosen); bui_file_picker_free(&picker); break;
  case BUI_PICK_CANCELLED: bui_file_picker_free(&picker); break;
  case BUI_PICK_OPEN: break;                   /* still choosing */
}
```

- Activating a folder opens it; **UP** goes to its parent and **HOME** to the home folder. Activating a file chooses it.
- **CANCEL**, Escape and B cancel.
- Entries are spoken as *"Folder saves"* or *"File key.hxlicense"*. Opening a folder announces where you are.
- Dot files are hidden unless `show_hidden` is set. A folder that can't be read says so.

## Custom widgets

Any rectangle becomes an accessible item with `bui_declare`. Use it for screens with their own look: inventory grids, character pickers, shop rows.

```c
bui_item it = {0};
it.kind = BUI_CHOICE;
it.rect = row;
it.prev = left_arrow;                   /* clicks here step -1 */
it.next = right_arrow;                  /* clicks here step +1 */
it.value = hat_name(hat);               /* compared between frames */
it.spoken = bui_format(ui, "Hat, %s. Left and right change it", hat_name(hat));
it.changed = bui_format(ui, "Hat, %s", hat_name(hat));

bui_result r = bui_declare(ui, "hat", &it);
int step = r.step ? r.step : r.activated ? 1 : 0;
if (step) {
  hat = (hat + step + HAT_COUNT) % HAT_COUNT;
  bui_revise(ui, hat_name(hat), bui_format(ui, "Hat, %s", hat_name(hat)));   /* spoken this frame */
}
draw_hat_row(row, hat, r.hovered);
```

How clicks are interpreted:

| Fields set | A click |
|---|---|
| `prev` or `next` | Inside them, sets `step` to -1 or +1. |
| `press` | Inside it, activates. Elsewhere on the item, only focuses. |
| `track` | Inside it while the button is held, drags (`dragging`, `drag` from 0 to 1). |
| None | Anywhere on the item, activates. |

`bui_result.hovered` is true for the item under the mouse in mouse mode, and for the focused item in navigation mode. Use it for the highlight in both modes.

`kind` decides the word spoken after the position: button, tab, toggle, slider, selector or text. For `BUI_CUSTOM`, `role` is spoken instead, or nothing when it is empty. Use `BUI_TEXT` for read-only content, such as pages of a licence or credits, that the player should be able to step through and hear.
