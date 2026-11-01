# Integration

## Adding BlueUI to a CMake project

### As a subdirectory

```cmake
add_subdirectory(external/BlueUI)
target_link_libraries(my_game PRIVATE blueui)
```

### With FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
  blueui
  GIT_REPOSITORY <repository URL>
  GIT_TAG        <tag or commit>
)
FetchContent_MakeAvailable(blueui)
target_link_libraries(my_game PRIVATE blueui)
```

When BlueUI is not the top-level project, its demo and tests are not built. Set options before adding it:

```cmake
set(BLUEUI_SHARED OFF CACHE BOOL "" FORCE)      # link statically
set(BLUEUI_WITH_FLITE OFF CACHE BOOL "" FORCE)  # no built-in voice
set(BLUEUI_PROFILER OFF CACHE BOOL "" FORCE)    # compile BUI_PROFILE scopes out
```

The `blueui` target carries its include directory and compile definitions. Include `<blueui/blueui.h>`, plus `<blueui/widgets.h>` for widgets and `<blueui/profiler.h>` for the [profiler](profiler.md). The `BUI_PROFILER` definition that `BLUEUI_PROFILER` sets is public, so it reaches every target that links `blueui`.

## Shared or static

| | Shared (default) | Static |
|---|---|---|
| Output | `libblueui.so` with soname `libblueui.so.0` | `libblueui.a` |
| Exported symbols | `bui_*` only; Flite's are hidden | — |
| At run time | The library must be found by the loader | Nothing extra |

A game that ships the shared library usually copies it next to its executable and sets an `$ORIGIN` run path:

```cmake
set_property(TARGET my_game APPEND PROPERTY BUILD_RPATH "$ORIGIN")
add_custom_command(TARGET my_game POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_SONAME_FILE:blueui>" "$<TARGET_FILE_DIR:my_game>"
  VERBATIM)
```

On Windows, the shared build defines `BLUEUI_DLL` for consumers so that functions are imported correctly. No action is needed when linking the CMake target.

## Shipping the voice

Flite is distributed under its own BSD-style licence, which requires its copyright notice to be kept. When BlueUI is built with the voice, CMake sets `BLUEUI_FLITE_LICENSE` to the path of Flite's `COPYING` file. Ship that file with your game:

```cmake
add_custom_command(TARGET my_game POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${BLUEUI_FLITE_LICENSE}" "$<TARGET_FILE_DIR:my_game>/flite_LICENSE.txt"
  VERBATIM)
```

Crediting "Flite, Language Technologies Institute, Carnegie Mellon University" in the game's credits is also good practice.

## Using BlueUI from C++

The headers are wrapped in `extern "C"` and work unchanged from C++. When compiled as C++, the input structs (`bui_item`, `bui_field`, `bui_option`, `bui_button_opts`, `bui_hints`, `bui_config`) have zero default member initialisers. Designated and partial brace initialisation therefore compile cleanly, even with `-Wextra`:

```cpp
const bui_field volume{"VOLUME", "Everything the game plays"};
const bui_button_opts back{.spoken = "Back", .back = true};
bui_item it{};
```

Strings passed in are copied, so `std::string` temporaries are safe:

```cpp
const std::string spoken = name + ", " + value;
it.spoken = spoken.c_str();
bui_declare(ui, ("row." + std::to_string(i)).c_str(), &it);
```

The draw command `bui_draw_cmd` is twelve floats. A renderer with its own rectangle type of the same layout can copy the geometry in one `memcpy`.

## Feeding input from a platform layer

BlueUI has no platform code. The demo's SDL3 event handling in [`demo/main.c`](../demo/main.c) is a complete reference for:

- keyboard keys mapped to navigation keys, with Escape doubling as capture cancel;
- controller buttons, with D-pad repeat while held;
- the mouse position, button state, clicks and wheel;
- switching `device` between keyboard and controller as the player switches;
- every raw key and button reported with `bui_input_pressed` for rebinding.

Games with their own rebindable controls should map their *actions* to BlueUI keys, not physical keys.

## Testing a game's menus

BlueUI's state lets a game test its menus the way a player experiences them: by what is focused and said, rather than by pixel positions. A typical test drives frames with synthetic input:

```c
bui_reset(ui);                                  /* forget focus and what was spoken */
bui_input in;
bui_input_clear(&in);
bui_input_key(&in, BUI_KEY_DOWN);
bui_begin_frame(ui, &in);
declare_main_menu(ui);
bui_end_frame(ui);

assert(strcmp(bui_focused_id(ui), "play") == 0);
assert(strcmp(bui_get_state(ui)->last_line, "PLAY. Item 1 of 4, button") == 0);
```

Useful tools:

- **`bui_find(ui, id_or_text)`** returns an item from the last frame by id, or by the start of its spoken text. Its `rect` gives a point to click; `bui_row_layout` gives a form row's control.
- **`bui_get_state(ui)->items`** lists every item, with its `value`, `enabled`, `shown` and `index`.
- **`last_line`** is the last line given to the voice.
- **Text-only mode** (`bui_config.no_voice`) delivers every line immediately as text, without timing differences.
- **`bui_reveal(ui, id)`** scrolls an item into view before a test clicks it.

BlueUI's own test suite, [`tests/tests.c`](../tests/tests.c), shows these techniques in use.
