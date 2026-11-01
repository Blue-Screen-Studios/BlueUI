# API reference

All public declarations are in three headers:

- `<blueui/blueui.h>`: the core.
- `<blueui/widgets.h>`: themed widgets. It includes the core.
- `<blueui/profiler.h>`: the frame profiler. It includes the core. Its functions take no context, except `bui_profiler`.

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
- [Feedback: events, sounds, rumble](#feedback-events-sounds-rumble)
- [Modals](#modals)
- [Drawing and text](#drawing-and-text)
- [Widgets](#widgets)
- [Profiler](#profiler)

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
  bool  skip_narrator_support_modal;           /* no narration offer at startup */
  bui_controller_modals controller_modals;     /* which controller changes open a modal */
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
  const char* speech_off;  /* "Narrator off" */
  const char* tab;         /* "tab", as in "Settings, Audio tab" */
  const char* cancelled;   /* "Cancelled" */
  const char* kinds[BUI_KIND_COUNT];  /* "button", "tab", "toggle", "slider", "selector", "text", "" */
  const char* ok;                     /* "OK" */
  const char* narrator_title;         /* "Narration available" */
  const char* narrator_text;          /* "This game can read its menus and screens aloud." */
  const char* narrator_enable;        /* "Enable narrator" */
  const char* narrator_disable;       /* "Disable narrator" */
  const char* pad_lost_title;         /* "Controller disconnected" */
  const char* pad_lost_text;          /* "Reconnect it, or carry on with the keyboard and mouse." */
  const char* pad_found_title;        /* "Controller connected" */
  const char* pad_found_text;         /* "The controller is ready to use." */
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
| `backdrop` | Drawn over the whole screen behind a modal. |
| `tech_canvas`, `tech_node`, `tech_locked`, `tech_available`, `tech_researching`, `tech_unlocked`, `tech_selected`, `tech_link`, `tech_link_lit` | Tech trees: the canvas, a node's face, its edge by state (and when selected), and the lines between nodes (lit when the node they lead to is unlocked). |
| `sound_pack`, `sound_volume` | The built-in GUI sound pack (`BUI_SOUNDS_NONE` by default, `BUI_SOUNDS_SOFT` or `BUI_SOUNDS_CRISP`) and its volume, 0 to 1. |
| `rumble_pack`, `rumble_strength` | The built-in rumble pack (`BUI_RUMBLE_NONE` by default, `BUI_RUMBLE_SOFT` or `BUI_RUMBLE_CRISP`) and its strength, 0 to 1. |

---

## Input

### `bui_input`

| Field | Type | Description |
|---|---|---|
| `mouse` | `bui_vec2` | Mouse position. `bui_input_clear` sets it far off screen. |
| `mouse_down` | `bool` | Left button held. |
| `clicked` | `bool` | Left button pressed this frame. |
| `click_at` | `bui_vec2` | Where it was pressed. |
| `alt_clicked`, `alt_click_at` | `bool`, `bui_vec2` | The right button pressed this frame, and where. Grids report it as `alt_activated`. |
| `wheel` | `float` | Wheel movement this frame; positive is up. |
| `keys`, `key_count` | `bui_key[BUI_MAX_KEYS]`, `int` | Navigation keys pressed this frame, in order. |
| `tab_hotkey` | `int` | A tab chosen by number: 1 is the first, 0 is none. |
| `pressed`, `pressed_count` | `int64_t[BUI_MAX_PRESSED]`, `int` | Raw codes pressed this frame, in the game's own numbering, for capture. |
| `device` | `bui_device` | `BUI_DEVICE_KEYBOARD` or `BUI_DEVICE_GAMEPAD`. Chooses which hint is spoken. |
| `time` | `double` | Seconds on a steady clock. |
| `speech_playing` | `bool` | Whether the game is still playing a speech line. |
| `gamepad_connected` | `bool` | Whether a controller is connected. Changes between frames count as plug events. |
| `gamepad_event` | `bui_gamepad_event` | `BUI_GAMEPAD_CONNECTED` or `BUI_GAMEPAD_DISCONNECTED` when one happened this frame; otherwise `BUI_GAMEPAD_NONE`. |
| `screen_size` | `bui_vec2` | The screen's size in pixels, for centring modals. 0 uses the extent of the geometry. |

`BUI_MAX_KEYS` is 32 and `BUI_MAX_PRESSED` is 16.

### `bui_key`

`BUI_KEY_UP`, `BUI_KEY_DOWN`, `BUI_KEY_LEFT`, `BUI_KEY_RIGHT`, `BUI_KEY_NEXT`, `BUI_KEY_PREV`, `BUI_KEY_ACTIVATE`, `BUI_KEY_BACK`, `BUI_KEY_TAB_NEXT`, `BUI_KEY_TAB_PREV`, `BUI_KEY_REPEAT`, `BUI_KEY_CANCEL_CAPTURE`. [Concepts](concepts.md#focus-and-navigation) describes what each does.

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

### Surfaces

A surface lays the UI out on a canvas of its own size that appears somewhere other than flat on the screen, such as a laptop screen inside a 3D world. BlueUI stays 2D: the geometry is still emitted in canvas pixels, and the game draws it through `to_clip`. BlueUI maps the mouse back onto the canvas.

```c
typedef struct bui_surface {
  bui_vec2 size;      /* the canvas, in pixels */
  float to_clip[16];  /* canvas (x, y, 0, 1) to clip space, column-major; NDC x right, y down */
  bui_vec2 viewport;  /* the screen's pixels (the mouse's space) */
} bui_surface;
```

| Function | Description |
|---|---|
| `bui_surface bui_surface_on_plane(bui_vec2 size, const float origin[3], const float right[3], const float down[3], const float view_proj[16], bui_vec2 viewport)` | A canvas on a world plane: its top-left corner at `origin`, its full width along `right` and its full height along `down`, seen through `view_proj`. |
| `bool bui_surface_unproject(const bui_surface* s, bui_vec2 screen, bui_vec2* canvas)` | A screen pixel to canvas pixels. Returns `false` if the pixel is off the canvas or the canvas is behind the camera. |
| `bool bui_surface_project(const bui_surface* s, bui_vec2 canvas, bui_vec2* screen)` | A canvas point to screen pixels. Returns `false` if the point is behind the camera. |
| `void bui_set_surface(bui_context* ui, const bui_surface* s)` | Lays out the frames from the next `bui_begin_frame` on `s` (copied; `NULL` returns to the screen). The input's mouse and click are unprojected onto the canvas (off it, they're nowhere), and `screen_size` becomes the canvas size. |
| `const bui_surface* bui_get_surface(const bui_context* ui)` | The current surface, or `NULL`. |

To draw the geometry, apply `to_clip` to each rectangle's corners in the vertex shader (`gl_Position = to_clip * vec4(x, y, 0, 1)`). The clip-space `w` keeps text and icons perspective-correct.

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
| `has_event`, `event` | The frame's UI event, if there was one. |
| `has_rumble`, `rumble` | Its rumble, scaled by the theme's strength. Only while a controller is in use and the theme has a rumble pack. |
| `modal` | The id of the modal shown, or `NULL`. |
| `gamepad_connected` | As reported in the input. |

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
| `void bui_set_speech_enabled(bui_context* ui, bool on, bool announce)` | Turns speech on or off. With `announce`, the change is spoken ("Narrator on", then where the player is; or "Narrator off") and `speech_toggled` is set that frame. |
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

## Feedback: events, sounds, rumble

See [Feedback](feedback.md).

### Types

| Type | Values or fields |
|---|---|
| `bui_event` | `BUI_EVENT_MOVE`, `BUI_EVENT_PRESS`, `BUI_EVENT_BACK`, `BUI_EVENT_CHANGE`, `BUI_EVENT_ON`, `BUI_EVENT_OFF`, `BUI_EVENT_TAB`, `BUI_EVENT_SCREEN`, `BUI_EVENT_DENIED`; `BUI_EVENT_COUNT`. |
| `bui_sound_pack` | `BUI_SOUNDS_NONE`, `BUI_SOUNDS_SOFT`, `BUI_SOUNDS_CRISP`. |
| `bui_rumble_pack` | `BUI_RUMBLE_NONE`, `BUI_RUMBLE_SOFT`, `BUI_RUMBLE_CRISP`. |
| `bui_rumble_step` | `low`, `high` (motors, 0 to 1), `ms`. |
| `bui_rumble` | `steps[BUI_RUMBLE_MAX_STEPS]` (4), `step_count`. |

### Functions

| Function | Description |
|---|---|
| `void bui_trigger(bui_context* ui, bui_event event)` | Uses this event for the frame's sound and rumble, in place of BlueUI's own choice. |

---

## Modals

See [Modals](modals.md).

| Name | Description |
|---|---|
| `BUI_MODAL_NARRATOR` | `"bui.narrator"`: the narration offer. Buttons: 0 enable, 1 disable. |
| `BUI_MODAL_CONTROLLER` | `"bui.controller"`: the controller notices. Button: 0 OK. |
| `bui_gamepad_event` | `BUI_GAMEPAD_NONE`, `BUI_GAMEPAD_CONNECTED`, `BUI_GAMEPAD_DISCONNECTED`. |
| `bui_controller_modals` | `BUI_CONTROLLER_MODALS_DISCONNECT` (default), `BUI_CONTROLLER_MODALS_ALL`, `BUI_CONTROLLER_MODALS_NONE`. |

| Function | Description |
|---|---|
| `void bui_open_modal(bui_context* ui, const char* id, const char* title, const char* message, const char* const* buttons, int count)` | Opens a modal (up to 4 buttons; none means one "OK"). Strings are copied. Opening an id already open replaces it. |
| `void bui_close_modal(bui_context* ui, const char* id)` | Removes a modal without an answer. |
| `bool bui_modal_open(const bui_context* ui, const char* id)` | Whether that modal (or, with `NULL`, any) is open. |
| `bool bui_take_modal_result(bui_context* ui, const char* id, int* button)` | Once a modal is answered, returns true a single time with the button chosen. |
| `void bui_set_controller_modals(bui_context* ui, bui_controller_modals which)` | Which controller changes open a modal. |

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

| `void bui_set_font(const bui_font* font)` | Replaces the built-in font, for every context (`NULL` restores it). The struct is kept, not copied. |
| `const bui_font* bui_get_font(void)` | The font set, or `NULL`. |

`bui_font` holds `user`, `width(user, text, len, pixel)` and `rects(user, text, len, x, y, pixel, color, shadow, out, cap)`. They take the place of `bui_text_width` and `bui_text_rects`, and keep the built-in font's measures so layouts don't change: the text's top at `y`, capitals `BUI_GLYPH_H * pixel` tall, lines `BUI_LINE_ADVANCE * pixel` apart. A game's TTF font returns rectangles textured from its UI texture.

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
| `bui_tech_node` | `title`, `detail`, `spoken`, `x`, `y`, `state`, `progress`, `button`, `button_disabled`. |
| `bui_tech_state` | `BUI_TECH_LOCKED`, `BUI_TECH_AVAILABLE`, `BUI_TECH_RESEARCHING`, `BUI_TECH_UNLOCKED`. |
| `bui_tech_link` | `from`, `to` (node indices). |
| `bui_tech_view` | `x`, `y`, `zoom`, `selected` (plus internal drag state). Zero it to start fitted. |
| `bui_tech_tree_opts` | `node_w`, `node_h`, `min_zoom`, `max_zoom` (0: 120, 40, 0.35, 2.5). |
| `bui_grid_shape` | `BUI_GRID_SQUARE`, `BUI_GRID_HEX_POINTY`, `BUI_GRID_HEX_FLAT`. |
| `bui_grid_opts` | `shape`, `columns`, `rows`, `count`, `cell`, `gap`, `stagger_first`, `selected`, `inert`, `no_faces`, `role`, `spoken`, `draw_cell`, `user`, `hex_uv`, `ring_uv`, `ring`. |
| `bui_grid_result` | `hovered`, `focused`, `activated`, `alt_activated` (cell indices, -1 for none). `draw_cell` state bits: `BUI_CELL_HOVERED`, `BUI_CELL_FOCUSED`, `BUI_CELL_SELECTED`. |

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
| `int bui_tech_tree(ui, id, area, nodes, count, links, link_count, view, opts)` | Returns the node whose button was pressed (an unlock request), else -1. |
| `void bui_tech_fit(ui, view, area, nodes, count, opts)` | — |
| `bui_grid_result bui_grid(ui, id, area, opts)` | Returns the cells hovered, focused, activated and right-clicked this frame. |
| `bui_vec2 bui_grid_size(opts)` | The grid's whole size. |
| `bui_rect bui_grid_cell_rect(opts, area, index)` | A cell's bounding box. |
| `int bui_grid_hit(opts, area, point)` | The cell under a point by its exact shape, else -1. |
| `int bui_grid_neighbor(opts, index, dx, dy)` | The cell the arrow keys move to, else -1. |
| `size_t bui_hex_rects(box, pointy, color, out, cap)` | A hexagon filling `box` as rectangles. |
| `bui_pick bui_file_picker_show(ui, id, area, picker, title)` | `BUI_PICK_OPEN` while choosing, then `BUI_PICK_CHOSEN` (the path in `picker->chosen`) or `BUI_PICK_CANCELLED`. |
| `void bui_file_picker_free(picker)` | Frees its entries. |
| `void bui_path_parent(path, out, size)` | A folder's parent; a root stays as it is. |

---

## Profiler

See [Profiler](profiler.md).

### Types

| Type | Fields |
|---|---|
| `bui_prof_event` | `name`, `start`, `end` (ns, `bui_prof_now_ns`), `depth` |
| `bui_prof_lane` | `thread`, `gpu`, `events`, `event_count` (events in start order) |
| `bui_prof_frame` | `index`, `start`, `end` (ns), `gpu_ms` (-1: none), `lanes`, `lane_count` (`"Main"` first, the GPU's last) |
| `bui_prof_gpu_span` | `name`, `start_ms`, `end_ms`, `depth` |
| `bui_prof_step` | `BUI_PROF_PREV`, `BUI_PROF_NEXT`, `BUI_PROF_PREV_10`, `BUI_PROF_NEXT_10`, `BUI_PROF_SLOWEST_OLDER`, `BUI_PROF_SLOWEST_NEWER`, `BUI_PROF_LIVE` |

### Functions

| Function | Description |
|---|---|
| `void bui_prof_set_enabled(bool on)` | Turns recording on from the next frame (and returns the view to live) or off. Kept frames stay. |
| `bool bui_prof_enabled(void)` | Whether it's recording. |
| `void bui_prof_name_thread(const char* name)` | This thread's lane name (copied). |
| `const char* bui_prof_intern(const char* name)` | A copy kept for good; equal text gives the same pointer. |
| `uint64_t bui_prof_now_ns(void)` | The steady clock in nanoseconds. |
| `bool bui_prof_begin(const char* name)` | Starts a task on this thread. Returns whether it's recorded; call `bui_prof_end` only if so. |
| `void bui_prof_end(void)` | Ends this thread's innermost task. |
| `void bui_prof_frame_begin(void)`, `void bui_prof_frame_end(void)` | Bracket the game's frame on the main thread. The end gathers every thread's tasks into a record. |
| `uint64_t bui_prof_current_frame(void)` | The index of the frame being recorded. |
| `void bui_prof_add_gpu(uint64_t frame, uint64_t start_ns, const bui_prof_gpu_span* spans, size_t count)` | Adds a GPU lane to a kept frame. Spans are in ms from `start_ns`. |
| `size_t bui_prof_frame_count(void)`, `const bui_prof_frame* bui_prof_frame_at(size_t i)` | The kept frames, oldest first, at most `BUI_PROF_HISTORY` (240). Valid until the next frame end or `add_gpu`. |
| `double bui_prof_frame_cpu_ms(const bui_prof_frame* f)` | The frame's main-thread time (inline). |
| `void bui_prof_set_frozen(bool)`, `bool bui_prof_frozen(void)` | Frozen, no new frames are kept. |
| `uint64_t bui_prof_dropped(void)` | Tasks lost to a full ring. |
| `void bui_prof_step_view(bui_prof_step step)` | Moves the overlay's frame (freezing), or back to live. |
| `const bui_prof_frame* bui_prof_view_frame(void)`, `bool bui_prof_view_live(void)` | The frame the overlay shows, and whether it's live. |
| `void bui_profiler(bui_context* ui, bui_rect area)` | Draws the overlay over `area`. Declares no items. |

Macros: `BUI_PROFILE(name)`, `BUI_PROFILE_DYN(name)` (C++, or C on GCC and Clang). Both do nothing when `BUI_PROFILER` is 0.
