# Modals

A modal is a window over the game's screen that must be answered before anything else.

While a modal is shown:

- the game's screen is still declared and drawn as usual, but receives no input;
- the modal has the focus, starting on its first button;
- it is read out as *"its title. its message. its first button. Item 1 of n, button"*.

A modal is answered by clicking a button, or by pressing Enter or A on one. Back presses the last button. Left and right move between buttons, as well as up and down. Afterwards, the focus returns to the item it was on before.

Several modals opened together are shown one after another.

## The game's own modals

```c
const char* buttons[] = {"Save", "Discard", "Cancel"};
bui_open_modal(ui, "quit", "Quit?", "You have unsaved changes.", buttons, 3);

/* later, each frame: */
int button;
if (bui_take_modal_result(ui, "quit", &button)) {
  if (button == 0) save_and_quit();
  else if (button == 1) quit();
}
```

- Passing `NULL` buttons, or a count of 0, gives a single "OK" button.
- `bui_modal_open(ui, id)` tells whether a modal is showing (`NULL` for any).
- `bui_close_modal(ui, id)` removes a modal without an answer.
- `bui_state.modal` is the id of the modal shown, or `NULL`.

Modals are centred on `bui_input.screen_size`. Set it each frame. When it is zero, BlueUI uses the extent of the frame's geometry.

The theme's `backdrop` colour is drawn over the screen behind a modal.

If the game routes input to BlueUI only on some screens, it should also route it whenever `bui_modal_open(ui, NULL)` is true. Otherwise a modal could appear where it cannot be answered, for example during gameplay.

## The narration offer

When a context is created, BlueUI opens a modal offering narration:

> **Narration available.** This game can read its menus and screens aloud. [Enable narrator] [Disable narrator]

- **Always spoken.** It is spoken even if speech is off, so a player who cannot see the screen hears the offer.
- **Applying the choice.** The choice switches speech from the next frame. `bui_state.speech_toggled` is true then, so a game that saves its narrator setting on that flag picks it up.
- **Reading the answer.** `bui_take_modal_result(ui, BUI_MODAL_NARRATOR, &button)` returns 0 for enable and 1 for disable.

To skip it, set `skip_narrator_support_modal` in `bui_config`. A game that only wants to offer narration on its first launch can skip it at creation and open it itself later, with the same id:

```c
const bui_strings* w = bui_get_strings(ui);
const char* buttons[2] = {w->narrator_enable, w->narrator_disable};
bui_open_modal(ui, BUI_MODAL_NARRATOR, w->narrator_title, w->narrator_text, buttons, 2);
```

A modal opened with the `BUI_MODAL_NARRATOR` id behaves like the built-in one: it is spoken even with speech off, and its choice switches speech.

## Controller modals

Each frame, the game reports its controllers in the input:

```c
in.gamepad_connected = any_controller_connected();
if (just_plugged_in) in.gamepad_event = BUI_GAMEPAD_CONNECTED;
if (just_unplugged)  in.gamepad_event = BUI_GAMEPAD_DISCONNECTED;
```

Reporting either one is enough. BlueUI also notices `gamepad_connected` changing between frames.

When a controller is unplugged, BlueUI shows *"Controller disconnected. Reconnect it, or carry on with the keyboard and mouse. [OK]"*. If one is plugged back in, that modal closes by itself. Its id is `BUI_MODAL_CONTROLLER`.

`bui_set_controller_modals` or `bui_config.controller_modals` chooses which changes open a modal:

| Value | Modals |
|---|---|
| `BUI_CONTROLLER_MODALS_DISCONNECT` | When a controller is unplugged. This is the default. |
| `BUI_CONTROLLER_MODALS_ALL` | When one is unplugged, and "Controller connected" when one is plugged in. |
| `BUI_CONTROLLER_MODALS_NONE` | Never. |

The words of every built-in modal are in `bui_strings`, so they can be translated. See [Speech and audio](speech-and-audio.md#customising-blueuis-own-words).
