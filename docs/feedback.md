# Feedback: events, sounds and rumble

Besides speech, BlueUI gives two kinds of feedback for what happens on screen: a short **sound** and, on a controller, a **rumble**. Both come from the same UI events, and both are chosen in the theme from built-in packs.

```c
bui_theme theme = bui_default_theme();
theme.sound_pack = BUI_SOUNDS_SOFT;     /* None, Soft or Crisp */
theme.sound_volume = 0.8f;              /* 0..1 */
theme.rumble_pack = BUI_RUMBLE_SOFT;    /* None, Soft or Crisp */
theme.rumble_strength = 1.0f;           /* 0..1 */
```

The default theme has both turned off.

## Events

| Event | When |
|---|---|
| `BUI_EVENT_MOVE` | The focus moves, or the mouse enters an item. |
| `BUI_EVENT_PRESS` | An item is pressed. |
| `BUI_EVENT_BACK` | A back item is pressed, by Escape, B or a click. |
| `BUI_EVENT_CHANGE` | A value changes: a slider, a choice, or a custom item's `value`. While dragging, at most every 70 ms. |
| `BUI_EVENT_ON`, `BUI_EVENT_OFF` | A toggle switches on or off; a foldout opens or closes. |
| `BUI_EVENT_TAB` | The tab or page changes. |
| `BUI_EVENT_SCREEN` | A new screen is shown, including a [modal](modals.md). |
| `BUI_EVENT_DENIED` | A disabled item is pressed or stepped. |

There is at most one event per frame. When several happen together, the most specific wins. For example, clicking a toggle gives *on*, not *press*. The order of precedence is: denied, back, on/off, tab, screen, press, change, move.

Custom widgets get events automatically through `bui_declare` and value-change detection. To choose one explicitly, call `bui_trigger(ui, BUI_EVENT_ON)`; it replaces BlueUI's own choice for that frame.

The frame's event, if any, is in `bui_state.has_event` and `bui_state.event`.

## Sounds

| `bui_sound_pack` | Sound |
|---|---|
| `BUI_SOUNDS_NONE` | No GUI sounds. |
| `BUI_SOUNDS_SOFT` | Rounded sine tones. |
| `BUI_SOUNDS_CRISP` | Clicks and square-wave ticks. |

The packs are generated in code when a context is created, at the context's sample rate. BlueUI ships and loads no sound files.

Sounds arrive from `bui_next_audio` as `BUI_AUDIO_SOUND` events:

- `text` is the event's name, such as `"press"`.
- `wav` and `samples` hold the clip at the context's sample rate, with `sound_volume` applied.

Play them on a channel separate from speech, so neither cuts the other off. Sounds play whether or not speech is enabled.

## Rumble

| `bui_rumble_pack` | Feel |
|---|---|
| `BUI_RUMBLE_NONE` | No rumble. |
| `BUI_RUMBLE_SOFT` | Gentle, rounded pulses, mostly on the heavy motor. |
| `BUI_RUMBLE_CRISP` | Short, sharp ticks, mostly on the light motor. |

BlueUI never drives a controller. A rumble is only timings, which the game plays on its own motors:

```c
typedef struct bui_rumble_step {
  float low;   /* the low-frequency (heavy) motor, 0..1 */
  float high;  /* the high-frequency (light) motor, 0..1 */
  float ms;    /* how long */
} bui_rumble_step;

typedef struct bui_rumble {
  bui_rumble_step steps[BUI_RUMBLE_MAX_STEPS];  /* up to 4 */
  int step_count;
} bui_rumble;
```

When a frame's event has a rumble, `bui_state.has_rumble` is true and `bui_state.rumble` holds it, already scaled by `rumble_strength`. Rumbles are only given while a controller is in use, meaning `bui_input.device` is `BUI_DEVICE_GAMEPAD`.

Play the steps in order, starting from that frame. A new rumble replaces one still playing:

```c
const bui_state* s = bui_get_state(ui);
if (s->has_rumble) { pattern = s->rumble; started = now; step = -1; }

/* every frame: */
double end = started;
int due = -1;
for (int k = 0; k < pattern.step_count; ++k)
  if (now < (end += pattern.steps[k].ms / 1000.0)) { due = k; break; }
if (due != step) {
  step = due;
  if (due >= 0) set_motors(pattern.steps[due].low, pattern.steps[due].high, pattern.steps[due].ms);
  else set_motors(0, 0, 0);
}
```

With SDL3, `set_motors` is `SDL_RumbleGamepad(pad, low * 65535, high * 65535, ms)`. The [demo](../demo/main.c) does exactly this.
