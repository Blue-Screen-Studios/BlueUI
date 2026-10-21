# API reference

All public declarations are in two headers:

- `<blueui/blueui.h>`: the core.
- `<blueui/widgets.h>`: themed widgets. It includes the core.

Every function takes the context as its first argument unless noted. Strings passed in may be `NULL` where a field is optional; `NULL` is treated as empty.

- [Basic types](#basic-types)
- [Context](#context)
- [Theme](#theme)
- [Input](#input)
- [Frames and screens](#frames-and-screens)
- [Items](#items)
- [State](#state)
- [Focus, navigation and tabs](#focus-navigation-and-tabs)
- [Scrolling](#scrolling)
- [Capture](#capture)
- [Speech](#speech)
- [Audio](#audio)
- [GUI sounds](#gui-sounds)
- [Drawing and text](#drawing-and-text)
- [Widgets](#widgets)

---

## Basic types

### `bui_vec2`, `bui_rect`, `bui_color`

```c
typedef struct bui_vec2  { float x, y; } bui_vec2;
typedef struct bui_rect  { float x0, y0, x1, y1; } bui_rect;
typedef struct bui_color { float r, g, b, a; } bui_color;
```

Coordinates are pixels from the top-left corner. A rectangle includes `x0, y0` and excludes `x1, y1`. Colours are linear RGB with straight alpha.

Inline helpers:

| Function | Returns |
|---|---|
| `bui_rect_make(x0, y0, x1, y1)` | A rectangle. |
| `bui_rgb(r, g, b)`, `bui_rgba(r, g, b, a)` | A colour; `bui_rgb` has alpha 1. |
| `bui_rect_empty(r)` | Whether `r` has no area. |
| `bui_rect_contains(r, p)` | Whether point `p` is inside `r`. |
| `bui_rect_inside(r, o)` | Whether `r` lies within `o`, with half a pixel of tolerance. |
| `bui_rect_w(r)`, `bui_rect_h(r)` | Width, height. |
| `bui_rect_center(r)` | The centre point. |
| `bui_rect_grow(r, d)` | `r` expanded by `d` on every side. |

### `bui_draw_cmd`

```c
typedef struct bui_draw_cmd {
  float x0, y0, x1, y1;   /* the rectangle, in pixels */
  float r, g, b, a;       /* linear colour, straight alpha */
  float u0, v0, u1, v1;   /* texture box; untextured when u1 <= u0 */
} bui_draw_cmd;
```

One rectangle of geometry, drawn in order and alpha-blended. When `u1 > u0`, the rectangle samples the game's UI texture over the box `(u0, v0)`–`(u1, v1)`, multiplied by the colour.

---

## Context

### `bui_config`

```c
typedef struct bui_config {
  int   sample_rate;   /* audio sample rate; 0 means 48000 */
  float speech_rate;   /* speaking speed; 0 means 1.15, 1.0 is the voice's natural speed */
  bool  no_voice;      /* deliver lines as text only */
  const bui_theme* theme;  /* the game's look (copied); NULL uses bui_default_theme() */
} bui_config;
```

### Functions

| Function | Description |
|---|---|
| `bui_context* bui_create(const bui_config* config)` | Creates a context. `config` may be `NULL` for defaults. Loads the voice and starts its thread. |
| `void bui_destroy(bui_context* ui)` | Stops the voice and frees everything. Accepts `NULL`. |
| `void bui_reset(bui_context* ui)` | Forgets focus, spoken history, hints, scroll offsets and the last frame's state. Keeps the voice and the speech setting. |
| `bool bui_has_voice(const bui_context* ui)` | Whether a voice is loaded. False in text-only mode. |
| `void bui_set_strings(bui_context* ui, const bui_strings* s)` | Replaces BlueUI's own phrases. `NULL` restores the defaults. The pointers are kept, not copied. |
| `const bui_strings* bui_get_strings(const bui_context* ui)` | The phrases in use. |
| `void bui_set_theme(bui_context* ui, const bui_theme* theme)` | Replaces the theme (copied). Applies to everything declared afterwards; the focus outline uses the theme in effect at `bui_end_frame`. `NULL` restores the default. |
| `const bui_theme* bui_get_theme(const bui_context* ui)` | The theme in effect. |
| `bui_theme bui_default_theme(void)` | BlueUI's default blue theme. |

### `bui_strings`

```c
typedef struct bui_strings {
  const char* item_of;     /* "Item {i} of {n}" */
  const char* speech_on;   /* "Narrator on" */
  const char* speech_off;  /* "Narrator off. Press N to switch it back on" */
  const char* tab;         /* "tab", as in "Settings, Audio tab" */
  const char* cancelled;   /* "Cancelled" */
  const char* kinds[BUI_KIND_COUNT];  /* "button", "tab", "toggle", "slider", "selector", "text", "" */
} bui_strings;
```

---

## Theme

`bui_theme`, declared in `<blueui/blueui.h>`. Sizes are in font pixels and multiplied by `pixel`. Colours are linear RGB with straight alpha.

| Fields | Used for |
|---|---|
| `pixel` | Screen pixels per font pixel. |
| `text`, `dim`, `blurb`, `value`, `off` | Labels, secondary text, descriptions, values, disabled text. |
| `stripe`, `hover`, `rule` | Alternate row stripes, the hovered row, separator rules. |
| `edge`, `panel`, `title_bar`, `title`, `title_right` | Windows. |
| `tab_on`, `tab_off`, `tab_hot`, `tab_text`, `tab_text_off` | Tab bars. |
| `toggle_on`, `toggle_on_hot`, `toggle_off`, `toggle_off_hot`, `toggle_text_on`, `toggle_text_off` | Toggles. |
| `well`, `arrow`, `arrow_hot`, `choice_text` | Choices and their arrow buttons. |
| `track`, `fill`, `knob`, `knob_hot`, `knob_off`, `tick` | Sliders and stops. |
| `button_edge`, `button`, `button_hot`, `button_text`, `button_text_off`, `button_edged` | Buttons, including button rows. |
| `control_width`, `arrow_width`, `blurb_at` | Form row layout. |
| `focus`, `focus_width`, `focus_gap` | The focus outline: colour, thickness, distance from the item. |
| `sound_pack`, `sound_volume` | The built-in GUI sound pack (`BUI_SOUNDS_NONE` by default, `BUI_SOUNDS_SOFT` or `BUI_SOUNDS_CRISP`) and its volume, 0 to 1. |

---

## Input

### `bui_input`

| Field | Type | Description |
|---|---|---|
| `mouse` | `bui_vec2` | Mouse position. `bui_input_clear` sets it far off screen. |
| `mouse_down` | `bool` | Left button held. |
| `clicked` | `bool` | Left button pressed this frame. |
| `click_at` | `bui_vec2` | Where it was pressed. |
| `wheel` | `float` | Wheel movement this frame; positive is up. |
| `keys`, `key_count` | `bui_key[BUI_MAX_KEYS]`, `int` | Navigation keys pressed this frame, in order. |
| `tab_hotkey` | `int` | A tab chosen by number: 1 is the first, 0 is none. |
| `pressed`, `pressed_count` | `int64_t[BUI_MAX_PRESSED]`, `int` | Raw codes pressed this frame, in the game's own numbering, for capture. |
| `device` | `bui_device` | `BUI_DEVICE_KEYBOARD` or `BUI_DEVICE_GAMEPAD`. Chooses which hint is spoken. |
| `time` | `double` | Seconds on a steady clock. |
| `speech_playing` | `bool` | Whether the game is still playing a speech line. |

`BUI_MAX_KEYS` is 32 and `BUI_MAX_PRESSED` is 16.

### `bui_key`

`BUI_KEY_UP`, `BUI_KEY_DOWN`, `BUI_KEY_LEFT`, `BUI_KEY_RIGHT`, `BUI_KEY_NEXT`, `BUI_KEY_PREV`, `BUI_KEY_ACTIVATE`, `BUI_KEY_BACK`, `BUI_KEY_TAB_NEXT`, `BUI_KEY_TAB_PREV`, `BUI_KEY_TOGGLE_SPEECH`, `BUI_KEY_REPEAT`, `BUI_KEY_CANCEL_CAPTURE`. [Concepts](concepts.md#focus-and-navigation) describes what each does.

### Functions

| Function | Description |
|---|---|
| `void bui_input_clear(bui_input* in)` | Zeroes the input and moves the mouse off screen. |
| `bool bui_input_key(bui_input* in, bui_key key)` | Appends a key. False when full. |
| `bool bui_input_pressed(bui_input* in, int64_t code)` | Appends a raw code. False when full. |

---

## Frames and screens

| Function | Description |
|---|---|
| `void bui_begin_frame(bui_context* ui, const bui_input* in)` | Starts a frame with this input. `in` is copied and may be `NULL`. Clears the geometry and interprets the keys. |
| `void bui_end_frame(bui_context* ui)` | Resolves focus and scrolling, draws the focus outline, speaks, and publishes the state. |
| `const bui_input* bui_get_input(const bui_context* ui)` | This frame's input. |
| `void bui_screen(bui_context* ui, const char* id, const char* name, const bui_hints* hints)` | Names the screen. Call once per frame, before its items. `hints` may be `NULL`. |
| `void bui_page(bui_context* ui, const char* name)` | Names the page within the screen. `bui_tab_bar` calls this. |

### `bui_hints`

```c
typedef struct bui_hints {
  const char* keyboard;   /* spoken on the first visit with a keyboard */
  const char* gamepad;    /* spoken on the first visit with a controller; NULL uses keyboard */
} bui_hints;
```

---

## Items

### `bui_item`

| Field | Description |
|---|---|
| `kind` | A `bui_kind`. Decides the word spoken after the position. |
| `rect` | The item's area: hover, clicks, the focus outline. |
| `spoken` | What is said when the item is reached. |
| `value` | Its state as text. Compared between frames to detect change; never spoken. |
| `changed` | What is said when `value` changes. `NULL` uses `spoken`. |
| `role` | For `BUI_CUSTOM`, the word spoken in place of a kind. |
| `disabled` | Reachable and spoken, but not activated, stepped or dragged. |
| `unfocusable` | Skipped by the keys; still hoverable and clickable. |
| `back` | Also activated by `BUI_KEY_BACK`. |
| `press`, `prev`, `next` | Click areas: activate, step -1, step +1. When all three are empty, a click anywhere on the item activates it. |
| `track` | Drag area. Holding the button over it reports `dragging` and `drag`. |

### `bui_kind`

`BUI_BUTTON`, `BUI_TAB`, `BUI_TOGGLE`, `BUI_SLIDER`, `BUI_CHOICE`, `BUI_TEXT`, `BUI_CUSTOM`.

### `bui_result`

| Field | Description |
|---|---|
| `hovered` | Under the mouse in mouse mode; focused in navigation mode. |
| `focused` | Has the keyboard or controller focus. |
| `activated` | Pressed this frame: click, Activate, or Back for a back item. |
| `step` | -1 or +1 from Left or Right, or a click on `prev` or `next`; otherwise 0. |
| `dragging` | The mouse is held on the track. |
| `drag` | Position along the track, from 0 to 1, while dragging. |

### Functions

| Function | Description |
|---|---|
| `bui_result bui_declare(bui_context* ui, const char* id, const bui_item* item)` | Declares an item. `id` must be unique on the screen and shorter than 64 bytes. All strings are copied. |
| `void bui_revise(bui_context* ui, const char* value, const char* spoken)` | Updates the item just declared after the game changed it, so the change is spoken this frame. |

---

## State

### `bui_state`

Returned by `const bui_state* bui_get_state(const bui_context* ui)`. It describes the last completed frame and stays valid until the frame after next begins.

| Field | Description |
|---|---|
| `items`, `item_count` | Every item declared, in order. |
| `screen` | The screen's id. |
| `page` | The page's name, or empty. |
| `focused` | The focused item's id, or empty. |
| `capturing` | The id of the item waiting for a key, or `NULL`. |
| `last_line` | The last line given to the voice. |
| `navigating` | Navigation mode is on. |
| `speech_enabled` | Speech is on. |
| `speech_toggled` | Speech was switched on or off during this frame. |
| `back_pressed` | Back was pressed and no item took it. |
| `device` | The input device of the frame. |

### `bui_item_state`

| Field | Description |
|---|---|
| `id`, `kind`, `rect`, `spoken`, `value` | As declared, after any `bui_revise`. |
| `enabled`, `focusable` | The opposites of `disabled` and `unfocusable`. |
| `shown` | Wholly inside its scroll view, or not in one. |
| `focused`, `hovered` | Had the focus, or was under the mouse. |
| `index` | Position among focusable items, from 0; -1 if not focusable. |

### Functions

| Function | Description |
|---|---|
| `const bui_item_state* bui_find(const bui_context* ui, const char* id_or_spoken)` | The item with this id or, failing that, the first whose spoken text starts with it. `NULL` if none. |

---

## Focus, navigation and tabs

| Function | Description |
|---|---|
| `const char* bui_focused_id(const bui_context* ui)` | The focused item's id, or empty. |
| `void bui_focus(bui_context* ui, const char* id)` | Moves focus to an item. Items declared after the call see the new focus. |
| `bool bui_navigating(const bui_context* ui)` | Whether navigation mode is on. |
| `void bui_begin_navigating(bui_context* ui)` | Turns navigation mode on and shows the outline. |
| `bool bui_back_pressed(const bui_context* ui)` | Back was pressed this frame and no item declared so far took it. |
| `void bui_mark_arrived(bui_context* ui)` | The current screen counts as already announced, with its hints. |
| `int bui_row_stripe(bui_context* ui)` | A counter for striping rows; returns 0, 1, 2, ... through the frame. |
| `int bui_take_tab_step(bui_context* ui)` | This frame's tab step (-1, 0 or +1), consumed by the call. Marks the screen as having tabs. |
| `int bui_take_tab_jump(bui_context* ui)` | This frame's tab hotkey as an index from 0, or -1; consumed by the call. |

---

## Scrolling

| Function | Description |
|---|---|
| `float bui_begin_scroll(bui_context* ui, const char* id, bui_rect view, float content)` | Starts a scroll view of height `content` shown in `view`. Applies the wheel and returns the offset to subtract from item positions. |
| `void bui_end_scroll(bui_context* ui)` | Ends the scroll view. |
| `bool bui_visible(const bui_context* ui, bui_rect r)` | Whether `r` is inside the current scroll view, or true outside any. |
| `float bui_scroll_of(const bui_context* ui, const char* id)` | A view's current offset. |
| `void bui_set_scroll(bui_context* ui, const char* id, float offset)` | Sets a view's offset. It is clamped on the next `bui_begin_scroll`. |
| `void bui_reveal(bui_context* ui, const char* id)` | Scrolls an item into view at the end of this frame. |

---

## Capture

| Function | Description |
|---|---|
| `void bui_start_capture(bui_context* ui, const char* id, double seconds)` | Waits for a raw code on behalf of item `id`, for up to `seconds`. |
| `bool bui_capturing(const bui_context* ui, const char* id)` | Whether `id` is waiting; with `NULL`, whether anything is. |
| `bool bui_take_captured(bui_context* ui, const char* id, int64_t* code, bool* cancelled)` | Once the wait for `id` ends, returns true a single time. `*code` is the code pressed, unless `*cancelled` is set. Either pointer may be `NULL`. |

---

## Speech

### `bui_priority`

`BUI_SAY_NORMAL`, `BUI_SAY_INTERRUPT`, `BUI_SAY_QUEUE`, `BUI_SAY_HOLD`. See [Speech and audio](speech-and-audio.md#priorities).

### Functions

| Function | Description |
|---|---|
| `void bui_announce(bui_context* ui, const char* text, bui_priority priority)` | Speaks a game event, unless speech is off. |
| `void bui_say(bui_context* ui, const char* text, bool queued)` | Speaks even when speech is off. |
| `void bui_stop_speech(bui_context* ui)` | Drops pending lines and sends a stop event. |
| `void bui_set_speech_enabled(bui_context* ui, bool on, bool announce)` | Turns speech on or off. With `announce`, the change is spoken as the toggle key would speak it. |
| `bool bui_speech_enabled(const bui_context* ui)` | Whether speech is on. |

---

## Audio

### `bui_audio`

| Field | Description |
|---|---|
| `op` | `BUI_AUDIO_PLAY`, `BUI_AUDIO_QUEUE` or `BUI_AUDIO_STOP` for speech; `BUI_AUDIO_SOUND` for a GUI sound. |
| `text` | The line, or a GUI sound's event name. `NULL` for a stop. |
| `wav`, `wav_size` | A WAV file in memory: RIFF, PCM, 16-bit, mono. `NULL` for a stop or in text-only mode. |
| `samples`, `sample_count` | The samples inside `wav`. |
| `sample_rate` | Samples per second. |

### Functions

| Function | Description |
|---|---|
| `bool bui_next_audio(bui_context* ui, bui_audio* out)` | Takes the next event. True if there was one; the caller then owns it. |
| `void bui_audio_free(bui_audio* audio)` | Frees an event and zeroes it. |
| `bool bui_synthesize(bui_context* ui, const char* text, bui_audio* out)` | Synthesizes a line synchronously. False without a voice. |

---

## GUI sounds

See [Speech and audio](speech-and-audio.md#gui-sounds).

### `bui_sound`

`BUI_SOUND_MOVE`, `BUI_SOUND_PRESS`, `BUI_SOUND_BACK`, `BUI_SOUND_CHANGE`, `BUI_SOUND_ON`, `BUI_SOUND_OFF`, `BUI_SOUND_TAB`, `BUI_SOUND_SCREEN`, `BUI_SOUND_DENIED`; `BUI_SOUND_COUNT`.

### `bui_sound_pack`

`BUI_SOUNDS_NONE`, `BUI_SOUNDS_SOFT`, `BUI_SOUNDS_CRISP`; `BUI_SOUND_PACK_COUNT`. Chosen in `bui_theme.sound_pack`.

### Functions

| Function | Description |
|---|---|
| `void bui_play_sound(bui_context* ui, bui_sound sound)` | Plays an event's sound at this frame's end, in place of BlueUI's own choice. |

---

## Drawing and text

| Function | Description |
|---|---|
| `const bui_draw_cmd* bui_get_geometry(const bui_context* ui, size_t* count)` | The frame's rectangles, in drawing order. |
| `void bui_draw(bui_context* ui, const bui_draw_cmd* cmds, size_t count)` | Appends rectangles. |
| `void bui_fill(bui_context* ui, bui_rect r, bui_color c)` | Appends a solid rectangle. |
| `void bui_text(bui_context* ui, const char* text, float x, float y, float pixel, bui_color color, float shadow)` | Appends text, top-left at `(x, y)`, each font pixel `pixel` screen pixels. `shadow > 0` adds a drop shadow of that opacity. `\n` starts a new line. |
| `size_t bui_text_rects(const char* text, size_t len, float x, float y, float pixel, bui_color color, float shadow, bui_draw_cmd* out, size_t cap)` | Text as rectangles into a caller's buffer, without a context. Returns the number needed and writes at most `cap`. `len` may be `BUI_NUL` for a NUL-terminated string. |
| `float bui_text_width(const char* text, size_t len, float pixel)` | Width of the longest line. |
| `bool bui_has_glyph(char c)` | Whether the font has a glyph for `c`. Other characters draw as a box. |
| `const unsigned char* bui_glyph_rows(char c)` | The glyph's 7 rows of 5 bits, top first, leftmost pixel in the highest bit. |
| `const char* bui_format(bui_context* ui, const char* fmt, ...)` | A `printf`-formatted string that stays valid until the frame after next begins. |

Font constants: `BUI_GLYPH_W` (5), `BUI_GLYPH_H` (7), `BUI_GLYPH_ADVANCE` (6), `BUI_LINE_ADVANCE` (9), all in font pixels. Lower-case letters are drawn as upper case.

---

## Widgets

Declared in `<blueui/widgets.h>`. See [Widgets](widgets.md) for behaviour and examples. Every widget draws with the context's theme.

### Types

| Type | Fields |
|---|---|
| `bui_field` | `name`, `description`, `spoken`, `disabled`. |
| `bui_option` | `shown`, `description`, `spoken`. |
| `bui_button_opts` | `spoken`, `disabled`, `back`, `pixel`. |
| `bui_row_parts` | `row`, `control`, `prev`, `next`. |
| `bui_captured` | `started`, `cancelled`, `has_code`, `code`, `waiting`. |
| `bui_format_fn` | `void (*)(float value, char* out, size_t size, void* user)`. |

### Functions

| Function | Returns true when |
|---|---|
| `void bui_label(ui, text, x, y, color, shadow, pixel)` | — |
| `void bui_centered(ui, rect, text, color, pixel)` | — |
| `void bui_window(ui, rect, title, right)` | — |
| `bool bui_button(ui, id, text, rect, opts)` | Pressed. |
| `bool bui_tab_bar(ui, id, area, names, count, current, min_width)` | A different tab was chosen. |
| `bui_row_parts bui_row_layout(const bui_theme* theme, bui_rect row)` | — |
| `bool bui_toggle_row(ui, id, row, field, on, on_text, off_text)` | Flipped. |
| `bool bui_toggle_row_ex(ui, id, row, field, on, off_option, on_option)` | Flipped. |
| `bool bui_choice_row(ui, id, row, field, index, options, count)` | Changed. |
| `bool bui_slider_row(ui, id, row, field, value, lo, hi, step, shown, spoken, user)` | Changed. |
| `bool bui_stops_row(ui, id, row, field, stop, stops, count)` | Changed. |
| `bool bui_button_row(ui, id, row, field, text)` | Pressed. |
| `bool bui_foldout_row(ui, id, row, field, open)` | Opened or closed. |
| `bui_captured bui_capture(ui, id, item, prompt, seconds)` | — |
