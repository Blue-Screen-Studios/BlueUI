# Concepts

## Immediate mode

BlueUI keeps no tree of widgets. Every frame the game declares the current screen from scratch, and BlueUI compares it with the previous frame. That comparison is how it knows what is new, what moved and what changed.

A frame always has this shape:

```c
bui_begin_frame(ui, &input);   /* the frame's input; the frame's keys are interpreted here */
bui_screen(ui, ...);           /* which screen this is */
/* ...items, in focus order... */
bui_end_frame(ui);             /* focus, scrolling, the focus outline, speech */
```

The game's own data is the single source of truth. Widgets read a value through a pointer, apply this frame's input to it, and return whether it changed.

## Screens and pages

`bui_screen(ui, id, name, hints)` names the screen being shown.

- **`id`** identifies the screen. When it differs from the previous frame's, the player has *arrived*: focus moves to the first item and the screen's name is spoken.
- **`name`** is what is spoken on arrival, such as "Settings".
- **`hints`** are spoken after the name the first time the screen is visited, such as "Up and down to move, Enter to choose". Keyboard and controller hints are tracked separately. A player who switches to a controller hears the controller hint once too.

`bui_page(ui, name)` names a page within the screen, usually the selected tab. `bui_tab_bar` calls it for you. A change of page counts as arriving, and the page is spoken as "Settings, Audio tab".

## Items and ids

Everything the player can reach is an *item*, declared with `bui_declare` or by a widget built on it. Each item has an id that must be unique on the screen and shorter than 64 bytes. BlueUI uses ids to:

- remember which item has focus across frames,
- compare an item's value with its value on the previous frame, and
- find items for the game or its tests (`bui_find`).

Ids only need to be stable while an item stays on screen. Generated ids such as `"bind.3"` or `"row.12"` are fine. Use `bui_format` to build them without managing buffers.

## Focus and navigation

Focus order is declaration order. Items marked `unfocusable`, such as tabs, are skipped by the keys but can still be hovered and clicked.

BlueUI distinguishes two modes:

- **Mouse mode** (`bui_navigating` returns false). Items react to hover and clicks. The item under the mouse is spoken. No focus outline is drawn.
- **Navigation mode** (`bui_navigating` returns true). The keys move the focus, an outline is drawn around the focused item, and `hovered` in a `bui_result` means focused.

The first navigation key in mouse mode only switches to navigation mode, showing where the focus is without acting. Moving the mouse more than 8 pixels, or clicking, switches back to mouse mode. Call `bui_begin_navigating` to start in navigation mode, for example when a screen opens from a controller.

| Key | Effect |
|---|---|
| Up, Down | Move focus, wrapping at the ends. |
| Next, Prev | Switch tabs if the screen had a tab bar on the previous frame; otherwise move focus like Down and Up. |
| Left, Right | Step the focused item's value (`bui_result.step`). |
| Activate | Press the focused item (`bui_result.activated`). |
| Back | Press the first declared item marked `back`. If no item takes it, `bui_back_pressed` returns true for the rest of the frame. |
| Tab Next, Tab Prev | Always switch tabs. |
| Repeat | Speak the last line again. |

When the focused item disappears, for example because its foldout closed, focus moves to the item now at the same position. When focus moves onto an item outside its scroll view, the view scrolls to show it on the next frame.

## What is spoken, and when

At the end of each frame, BlueUI builds at most one line from these parts, in order. The parts are joined with ". ".

1. **"Narrator on"**, if speech was switched on this frame.
2. **Arrival.** The screen and page name, plus the hint the first time. The focused item's description follows, unless it is the tab that was just clicked.
3. **A new focus.** When the focused or hovered item is different from the one last spoken, its description is spoken. It has the form *"its text. Item i of n, its kind"*, such as "Volume, 80%. Item 1 of 4, slider".
4. **A changed value.** When the spoken item's `value` differs from the previous frame's, its `changed` text is spoken, or its `spoken` text if there is none. The position and kind are left out, so a slider being nudged says only "Volume, 85%".
5. **Announcements** made with `BUI_SAY_NORMAL` since the last frame.

The line interrupts whatever was being said. Nothing is repeated while focus stays still and nothing changes.

An item's `value` is a string the game chooses to represent its state. BlueUI never speaks it directly. It only compares it between frames to detect change. When a widget changes its own value during the frame, it calls `bui_revise` so the change is spoken in the same frame rather than the next.

## Speech on and off

When speech is off, nothing is spoken except the line confirming that it was turned off. The game's setting should drive `bui_set_speech_enabled(ui, on, false)` at startup. The [narration offer](modals.md#the-narration-offer) shown when a game starts switches speech by the player's choice; `bui_state.speech_toggled` is true that frame, so the game can save the new setting. A settings screen can switch it with `bui_set_speech_enabled(ui, on, true)`, which announces the change.

## Scroll views

```c
float offset = bui_begin_scroll(ui, "list", view_rect, content_height);
for (int i = 0; i < count; ++i) {
  bui_rect r = bui_rect_make(view_rect.x0, view_rect.y0 + i * row_h - offset, view_rect.x1, view_rect.y0 + (i + 1) * row_h - offset);
  /* declare an item at r */
}
bui_end_scroll(ui);
```

Lay items out at their scrolled positions using the returned offset. Items not wholly inside the view are still declared, so they are counted and reachable, but they cannot be hovered or clicked. Their `bui_item_state.shown` is false. Widgets skip drawing them; use `bui_visible` to do the same in custom drawing. The mouse wheel scrolls the view when the mouse is over it. `bui_set_scroll` and `bui_reveal` move it programmatically.

## Memory and lifetimes

- **Strings passed in** (ids, item text, hints, options) are copied during the call. They can be temporaries.
- **`bui_strings`** passed to `bui_set_strings` is the exception. Its pointers are kept, so they must stay valid.
- **The state, the geometry and strings from `bui_format`** stay valid until the frame after next begins. The state read after frame *N* remains readable throughout frame *N+1*.
- **Audio buffers** from `bui_next_audio` and `bui_synthesize` belong to the caller. Free them with `bui_audio_free`.

BlueUI allocates as the frame's content grows and reuses the memory afterwards. A steady screen does not allocate.

## Threading

A context is not thread-safe. Call every function for a context from the same thread, normally the game's main thread. Speech synthesis runs on a background thread owned by the context. Its results are collected by `bui_next_audio` on the calling thread. Separate contexts are independent.

The [profiler](profiler.md) is the exception: it belongs to no context, and `BUI_PROFILE` scopes (with `bui_prof_begin`, `bui_prof_end` and `bui_prof_name_thread`) can be used from any thread. Frame begin and end, `bui_prof_add_gpu`, reading the kept frames, stepping the view and `bui_profiler` belong to the game's main thread.
