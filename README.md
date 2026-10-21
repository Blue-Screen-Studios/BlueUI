# BlueUI

**An accessible, self-narrating game UI library in C.**

BlueUI is an immediate-mode UI library for games. Every widget you declare is automatically reachable by keyboard and controller, and every interaction is spoken aloud through a built-in text-to-speech voice. A blind or low-vision player can use a BlueUI menu without the game doing any extra work.

BlueUI does not own a window, a renderer or an audio device. Each frame the game passes in its input, and BlueUI hands back three outputs:

| Output | Accessor | What the game does with it |
|---|---|---|
| **UI state** | `bui_get_state` | Read the focused item, every item's value and position, and the last line spoken. |
| **Geometry** | `bui_get_geometry` | Draw a list of coloured rectangles with any renderer. |
| **Audio** | `bui_next_audio` | Play WAV buffers of synthesized speech, and GUI sounds, through the game's own mixer. |

## Features

- **Screen reader built in.** Screens, pages, items, values and changes are announced automatically. For example, "Settings, Audio tab. Volume, 80%. Item 1 of 4, slider."
- **Speech synthesis included.** CMU Flite is compiled into the library and runs on a background thread. No system speech service is required.
- **Keyboard, controller and mouse navigation.** Focus order follows declaration order. Tab switching, value stepping, slider dragging and a back action all work without game code.
- **Change detection.** When a value changes, by input or by the game's own code, only the new value is spoken.
- **Platform independent.** The library has no dependency on SDL or any windowing or audio API. The game translates its own events into a plain `bui_input` struct.
- **Renderer independent.** Output is a flat list of rectangles, optionally textured, with a built-in 5×7 bitmap font.
- **GUI sounds.** Moving, pressing, toggling, changing values, switching tabs and pressing disabled items can each play a sound. The theme picks a built-in pack (None, Soft or Crisp). The packs are generated in code, so no sound files are shipped.
- **Themed widgets.** Includes buttons, tab bars, toggles, choices, sliders, stepped sliders, foldouts, rebinding capture and scroll views. Each game passes one `bui_theme` (colours, sizes, focus outline) and every widget follows it. The default theme is blue.
- **Testable.** Menus can be tested by what is said and focused, not by pixel positions. Text-only mode makes tests deterministic.
- **C11 with a small surface.** A single shared library exporting only `bui_*` symbols, and headers that work from C and C++.

## Quick start

```c
#include <blueui/widgets.h>

bui_theme theme = bui_default_theme();        /* BlueUI's blue look; change any field */
theme.pixel = 3.0f;

bui_config config = {0};
config.theme = &theme;                        /* your game's look, set once */
bui_context* ui = bui_create(&config);        /* 48 kHz voice */

/* Every frame: */
bui_input in;
bui_input_clear(&in);
/* ...fill `in` from your platform's events: mouse, keys, time... */

bui_begin_frame(ui, &in);

bui_hints hints = {"Up and down to move, Enter to choose", NULL};
bui_screen(ui, "menu", "Main menu", &hints);
if (bui_button(ui, "play", "PLAY", play_rect, NULL)) start_game();
if (bui_button(ui, "quit", "QUIT", quit_rect, NULL)) quit_game();

bui_end_frame(ui);

/* Draw. */
size_t count;
const bui_draw_cmd* rects = bui_get_geometry(ui, &count);
my_renderer_draw(rects, count);

/* Speak. */
bui_audio line;
while (bui_next_audio(ui, &line)) {
  my_audio_handle(line.op, line.samples, line.sample_count, line.sample_rate);
  bui_audio_free(&line);
}

/* On shutdown: */
bui_destroy(ui);
```

When the player opens this menu, BlueUI says *"Main menu. Up and down to move, Enter to choose"*. Pressing Down then says *"PLAY. Item 1 of 2, button"*.

## Building

Requirements:

- CMake 3.21 or newer
- A C11 compiler with `<threads.h>` (GCC or Clang with glibc 2.28+; MSVC 2022 17.8+)
- Network access on first configure, to download Flite 2.2
- SDL3, for the demo only

```sh
cmake -B build
cmake --build build
ctest --test-dir build        # run the test suite
./build/blueui_demo           # a tour of every feature (SDL3)
```

| Option | Default | Description |
|---|---|---|
| `BLUEUI_WITH_FLITE` | `ON` | Build the Flite voice into the library. When off, speech is delivered as text only. |
| `BLUEUI_SHARED` | `ON` | Build `libblueui.so`. Turn off for a static library. |
| `BLUEUI_BUILD_DEMO` | `ON` when top level | Build `blueui_demo`. Requires SDL3. |
| `BLUEUI_BUILD_TESTS` | `ON` when top level | Build `blueui_tests` and register it with CTest. |

To use BlueUI in a game, add it with `add_subdirectory` or `FetchContent` and link the `blueui` target. See [Integration](docs/integration.md).

## Documentation

| Guide | Contents |
|---|---|
| [Getting started](docs/getting-started.md) | A complete frame loop: input, declaring a screen, drawing, playing speech. |
| [Concepts](docs/concepts.md) | Immediate mode, ids, focus and navigation, what is spoken and when, memory lifetimes, threading. |
| [Widgets](docs/widgets.md) | Themes, every built-in widget, and building custom widgets with `bui_declare`. |
| [Speech and audio](docs/speech-and-audio.md) | The voice, the audio queue, the WAV format, priorities, text-only mode, GUI sounds and sound packs. |
| [Integration](docs/integration.md) | CMake, shared and static builds, shipping, C++ use, testing a game's menus. |
| [API reference](docs/api-reference.md) | Every type and function. |

## Platform support

BlueUI is developed and tested on Linux with GCC. The library code is portable C11, but it needs `<threads.h>`. That header is available with glibc 2.28+ and with recent MSVC. Apple's libc does not provide it, so macOS is not supported at this time.

## Project layout

```
include/blueui/blueui.h    Core: context, input, state, items, speech, font
include/blueui/widgets.h   Themed widgets built on the core
src/                       Implementation
tests/tests.c              Test suite (CTest)
demo/main.c                Feature tour (SDL3): every widget, speech, rebinding and themes
docs/                      Documentation
```

## License

BlueUI is released under the [MIT License](LICENSE.md).

## Acknowledgements

The built-in voice is [Flite](https://github.com/festvox/flite) 2.2, from the Language Technologies Institute at Carnegie Mellon University. Flite has its own BSD-style licence that requires its copyright notice to be kept. Games that ship BlueUI with the voice enabled should include Flite's `COPYING` file. CMake exposes its path as `BLUEUI_FLITE_LICENSE`.
