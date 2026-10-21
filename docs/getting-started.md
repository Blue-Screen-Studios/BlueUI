# Getting started

This guide builds a small two-screen menu and connects BlueUI's three outputs: state, geometry and audio. A complete working version, using SDL3 for the window, renderer and audio, is in [`demo/main.c`](../demo/main.c).

## 1. Create a context

```c
#include <blueui/widgets.h>

bui_config config = {0};
config.sample_rate = 48000;        /* match your mixer; 0 means 48000 */

bui_context* ui = bui_create(&config);
if (!bui_has_voice(ui)) {
  /* Built without Flite, or the voice failed to load. Lines still arrive,
     as text only; see "Speech and audio". */
}

bui_theme theme = bui_default_theme();
theme.pixel = 3.0f;                /* screen pixels per font pixel */
```

A context holds everything BlueUI remembers between frames: focus, scroll offsets, which hints have been spoken, and the voice. Create one per UI and destroy it with `bui_destroy`.

## 2. Fill in the input each frame

BlueUI never reads devices itself. Each frame, start from `bui_input_clear` and fill in the fields from your platform's events:

```c
bui_input in;
bui_input_clear(&in);

in.mouse = (bui_vec2){mouse_x, mouse_y};
in.mouse_down = left_button_held;
in.clicked = left_button_pressed_this_frame;
in.click_at = (bui_vec2){click_x, click_y};
in.wheel = wheel_delta;                 /* positive scrolls up */
in.time = seconds_since_start;          /* steady clock */
in.device = last_input_was_gamepad ? BUI_DEVICE_GAMEPAD : BUI_DEVICE_KEYBOARD;
in.speech_playing = my_audio_is_playing_speech();

/* Keys pressed this frame, in order. */
if (pressed(KEY_DOWN))   bui_input_key(&in, BUI_KEY_DOWN);
if (pressed(KEY_ENTER))  bui_input_key(&in, BUI_KEY_ACTIVATE);
if (pressed(KEY_ESCAPE)) bui_input_key(&in, BUI_KEY_BACK);
/* ... */
```

A typical mapping:

| BlueUI key | Keyboard | Controller |
|---|---|---|
| `BUI_KEY_UP`, `BUI_KEY_DOWN` | Arrow keys | D-pad up/down |
| `BUI_KEY_LEFT`, `BUI_KEY_RIGHT` | Arrow keys | D-pad left/right |
| `BUI_KEY_NEXT`, `BUI_KEY_PREV` | Tab, Shift+Tab | — |
| `BUI_KEY_ACTIVATE` | Enter, Space | A / Cross |
| `BUI_KEY_BACK` | Escape | B / Circle |
| `BUI_KEY_TAB_NEXT`, `BUI_KEY_TAB_PREV` | — | Right and left bumper |
| `BUI_KEY_TOGGLE_SPEECH` | A key of your choice | View / Share |
| `BUI_KEY_REPEAT` | A key of your choice | — |
| `BUI_KEY_CANCEL_CAPTURE` | Escape | Start |

Number keys can select tabs directly: set `in.tab_hotkey` to 1 for the first tab, 2 for the second, and so on. Zero means none.

For key rebinding, also report every raw key or button pressed with `bui_input_pressed(&in, code)`, using whatever codes your game uses. BlueUI only passes them back to you. See [Rebinding](widgets.md#rebinding).

## 3. Declare the screen

Between `bui_begin_frame` and `bui_end_frame`, declare the screen and its items in the order the player should move through them:

```c
typedef enum { MENU, SETTINGS } screen_id;
static screen_id screen = MENU;
static float volume = 0.8f;
static bool fullscreen = false;

static void percent(float v, char* out, size_t size, void* user) {
  (void)user;
  snprintf(out, size, "%d%%", (int)(v * 100.0f + 0.5f));
}

static void declare(bui_context* ui, const bui_theme* t) {
  if (screen == MENU) {
    bui_hints hints = {"Up and down to move, Enter to choose", "D-pad to move, A to choose"};
    bui_screen(ui, "menu", "Main menu", &hints);
    if (bui_button(ui, t, "settings", "SETTINGS", bui_rect_make(100, 100, 300, 130), NULL)) screen = SETTINGS;
    if (bui_button(ui, t, "quit", "QUIT", bui_rect_make(100, 140, 300, 170), NULL)) quit_game();
  } else {
    bui_hints hints = {"Left and right to change a setting, Escape to go back", NULL};
    bui_screen(ui, "settings", "Settings", &hints);

    bui_field vol = {"Volume", "Everything the game plays"};
    bui_slider_row(ui, t, "volume", bui_rect_make(40, 60, 600, 90), &vol, &volume, 0.0f, 1.0f, 0.05f, percent, NULL, NULL);

    bui_field full = {"Fullscreen", "The whole screen, or a window"};
    if (bui_toggle_row(ui, t, "fullscreen", bui_rect_make(40, 96, 600, 126), &full, &fullscreen, NULL, NULL))
      apply_fullscreen(fullscreen);

    bui_button_opts back = {0};
    back.back = true;                         /* Escape or B also presses it */
    if (bui_button(ui, t, "back", "BACK", bui_rect_make(40, 200, 160, 230), &back)) screen = MENU;
  }
}
```

Widgets return `true` when they were pressed or their value changed, so the game reacts right there. Values are passed by pointer and updated in place.

## 4. Run the frame and use the outputs

```c
bui_begin_frame(ui, &in);
declare(ui, &theme);
bui_end_frame(ui);

/* Geometry: draw in order, alpha-blended. Colours are linear. */
size_t n;
const bui_draw_cmd* rects = bui_get_geometry(ui, &n);
for (size_t i = 0; i < n; ++i) draw_rect(&rects[i]);

/* Audio: hand every queued line to your mixer. */
bui_audio a;
while (bui_next_audio(ui, &a)) {
  switch (a.op) {
    case BUI_AUDIO_PLAY:  stop_speech(); if (a.samples) play_speech(a.samples, a.sample_count, a.sample_rate); break;
    case BUI_AUDIO_QUEUE: if (a.samples) queue_speech(a.samples, a.sample_count, a.sample_rate); break;
    case BUI_AUDIO_STOP:  stop_speech(); break;
  }
  bui_audio_free(&a);
}

/* State: whatever the game wants to know. */
const bui_state* s = bui_get_state(ui);
if (s->speech_toggled) save_setting("narrator", s->speech_enabled);
```

Each `bui_draw_cmd` is a rectangle in pixels from the top-left corner, with a straight-alpha linear colour. When `u1 > u0`, the rectangle samples your UI texture over that UV box, multiplied by the colour. BlueUI only produces textured rectangles that you add yourself, for example icons.

## 5. Add the game's own drawing

Anything drawn with `bui_fill`, `bui_text` or `bui_draw` between `bui_begin_frame` and `bui_end_frame` goes into the same list, in order with the widgets. Backgrounds, titles and decorations stay correctly layered:

```c
bui_fill(ui, bui_rect_make(0, 0, width, height), bui_rgba(0.0f, 0.0f, 0.0f, 0.6f));
bui_text(ui, "MY GAME", 100, 40, 4.0f, bui_rgb(1, 1, 1), 0.7f);
```

## 6. Say something

Game events can be announced at any time:

```c
bui_announce(ui, "Game saved", BUI_SAY_NORMAL);                   /* joins this frame's line */
bui_announce(ui, "Controller connected", BUI_SAY_INTERRUPT);      /* cuts in immediately */
bui_announce(ui, "Welcome. Press N to turn the narrator off.", BUI_SAY_HOLD);  /* an opening line */
```

See [Speech and audio](speech-and-audio.md) for what each priority does.

## Next steps

- [Concepts](concepts.md) explains how focus moves and what gets spoken.
- [Widgets](widgets.md) covers every widget and building your own.
- [Integration](integration.md) covers adding BlueUI to a CMake project and shipping it.
