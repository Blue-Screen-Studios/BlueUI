# Widgets

`blueui/widgets.h` provides themed widgets built on `bui_declare`. Each widget declares its item, applies the frame's input, speaks itself correctly and draws itself. A screen built from them needs no extra accessibility code.

All widgets take the context, a theme, a unique id and a rectangle in pixels. Widgets that change a value take it by pointer, update it in place, and return `true` when it changed. Buttons return `true` when pressed.

## Themes

```c
bui_theme theme = bui_default_theme();
theme.pixel = 3.0f;                         /* screen pixels per font pixel */
theme.panel = bui_rgba(0.03f, 0.05f, 0.09f, 0.97f);
```

A `bui_theme` holds every colour and the size multiplier. Sizes in the theme, such as `control_width`, `arrow_width` and `blurb_at`, are in font pixels and are multiplied by `pixel`. All colours are linear RGB with straight alpha. Themes are plain values, so copy one and change a few fields for a variant look:

```c
bui_theme flat = theme;
flat.button_edged = false;
```

## Drawing helpers

| Function | Draws |
|---|---|
| `bui_fill(ui, rect, color)` | A solid rectangle. |
| `bui_text(ui, text, x, y, pixel, color, shadow)` | Text at a position. `shadow > 0` adds a one-font-pixel drop shadow of that opacity. |
| `bui_label(ui, theme, text, x, y, color, shadow, pixel)` | Text at the theme's pixel size, or `pixel` when non-zero. |
| `bui_centered(ui, theme, rect, text, color, pixel)` | Text centred in a rectangle. |
| `bui_window(ui, theme, rect, title, right)` | A panel with an edge, a title bar, a title, and optional text at the right of the title bar. |

## Buttons

```c
bui_button_opts opts = {0};
opts.spoken = "Back";        /* spoken text; NULL uses the label */
opts.back = true;            /* also pressed by BUI_KEY_BACK */
if (bui_button(ui, &theme, "back", "BACK", rect, &opts)) go_back();
```

Pass `NULL` for default options. A disabled button (`opts.disabled`) is still reachable and spoken, but cannot be pressed.

Spoken as: *"Back. Item 3 of 3, button"*.

## Tab bars

```c
static const char* tabs[] = {"Audio", "Video", "Controls"};
static int tab = 0;
if (bui_tab_bar(ui, &theme, "tabs", bar_rect, tabs, 3, &tab, 0.0f)) reset_scroll();
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

`bui_row_layout(theme, row)` returns the rectangles a row uses (`row`, `control`, `prev`, `next`). Use it to align custom rows or to locate a control in tests.

### Toggle

```c
bui_toggle_row(ui, &theme, "fullscreen", rect, &f, &fullscreen, NULL, NULL);   /* "ON" / "OFF" */
bui_toggle_row(ui, &theme, "audio", rect, &f, &headphones, "HEADPHONES", "SPEAKERS");
```

Activate, Left, Right or a click on the control flips it. It is spoken as *"Fullscreen, on"*.

`bui_toggle_row_ex` takes a `bui_option` for each state, so each state can have its own label, description and spoken text:

```c
bui_option off = {"OFF", "Light passes glass plainly", "Colored shadows, off. A little faster"};
bui_option on  = {"ON",  "Glass tints the light",      "Colored shadows, on"};
bui_toggle_row_ex(ui, &theme, "colored", rect, &f, &colored, &off, &on);
```

### Choice

```c
bui_option modes[] = {
  {"SINGLE", "1 ray a face: the fastest", NULL},
  {"4 CORNERS", "4 rays a face: cleaner edges", NULL},
};
bui_choice_row(ui, &theme, "mode", rect, &f, &mode, modes, 2);
```

Cycles through options with Left and Right, the arrow buttons, or Activate. The selected option's description replaces the row's while it is selected. It is spoken as *"Sample mode, SINGLE. 1 ray a face: the fastest"*, or as the option's own `spoken` text.

### Slider

```c
static void percent(float v, char* out, size_t size, void* user) {
  snprintf(out, size, "%d%%", (int)(v * 100.0f + 0.5f));
}
bui_slider_row(ui, &theme, "volume", rect, &f, &volume, 0.0f, 1.0f, 0.05f, percent, NULL, NULL);
```

- Left and Right step by `step`, snapped to multiples of it from `lo`.
- Dragging on the track sets the value directly.
- `shown` formats the value for display and speech, such as "80%".
- `spoken`, when given, formats the whole spoken line, such as "Mouse sensitivity, 1.50 times".
- `user` is passed to both formatters.

### Stops

```c
bui_option ranges[] = {{"3.2 M"}, {"6.4 M"}, {"12.8 M"}, {"25.6 M"}};
bui_stops_row(ui, &theme, "range", rect, &f, &stop, ranges, 4);
```

A slider with a fixed number of marked positions. It steps without wrapping, and dragging snaps to the nearest stop. Each stop is a `bui_option` with its own label, description and spoken text.

### Button row

```c
bui_field reset = {"Reset controls", "Every key back to its default"};
if (bui_button_row(ui, &theme, "reset", rect, &reset, "RESET")) reset_bindings();
```

A row whose control is a button. It is spoken as *"Reset controls: Every key back to its default"*.

### Foldout

```c
static bool open = false;
bui_field shadows = {"Shadows", "How shadows are worked out"};
bui_foldout_row(ui, &theme, "shadows", rect, &shadows, &open);
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
