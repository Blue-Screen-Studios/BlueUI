# Profiler

BlueUI includes a frame profiler. It records how long each task takes on every thread, plus the GPU if the game supplies timestamps, one frame at a time, and draws the result as an overlay. Everything is declared in `<blueui/profiler.h>`.

Profiling is process-wide. It does not belong to a context, so any thread can record whether or not a UI exists. Only the overlay, `bui_profiler`, needs a context.

## Recording

```c
#include <blueui/profiler.h>

bui_prof_name_thread("Main");            /* this thread's lane */
bui_prof_set_enabled(true);              /* recording starts with the next frame */

/* each frame, on the main thread: */
bui_prof_frame_begin();
{
  BUI_PROFILE("Update");                 /* a task, to the end of this scope */
  update();
}
...
bui_prof_frame_end();                    /* every thread's finished tasks gathered */
```

- **Scopes.** `BUI_PROFILE(name)` marks a task from that point to the end of the enclosing scope. Scopes nest. It works in C++ (an RAII `bui::ProfScope`) and in C on GCC and Clang (`__attribute__((cleanup))`). Elsewhere, pair `bui_prof_begin` with `bui_prof_end` yourself, and call `end` only if `begin` returned true.
- **Names.** A name must live as long as the program, such as a string literal. For a name built at run time, use `BUI_PROFILE_DYN(name)`, which interns it (`bui_prof_intern`). Equal text always gives the same pointer.
- **Threads.** Any thread can record. Each thread gets a lane, named by `bui_prof_name_thread` and otherwise "Thread N". When a thread exits, its buffer is reused by the next thread that records.
- **The main lane.** Put frame begin and end around the game's frame on its main thread. The lane named `"Main"` is always listed first.
- **Long tasks.** A task that started before the current frame, such as a worker job still running at the last frame's end, is filed with the frame it started in.

## The GPU lane

BlueUI does not know your graphics API. Write timestamps around your passes, read them back when the frame's fence signals, and hand them over:

```c
uint64_t frame = bui_prof_current_frame();   /* when recording the commands */
uint64_t start = bui_prof_now_ns();          /* when submitting them */
...
/* later, the timestamps read back: */
bui_prof_gpu_span spans[] = {{"Shadows", 0.00, 0.41, 1}, {"Lighting", 0.41, 1.30, 1}};
bui_prof_add_gpu(frame, start, spans, 2);
```

Each span gives its start and end in milliseconds from `start`, plus its nesting depth. The spans become a `GPU` lane, always last, in that frame's record. The frame's GPU time runs from the first span's start to the last span's end. If the frame has already left the history, the spans are ignored.

## The overlay

```c
bui_begin_frame(ui, &input);
...                                       /* the game's own screen */
if (shown) bui_profiler(ui, bui_rect_make(16, 16, w - 16, h * 0.62f));
bui_end_frame(ui);
```

`bui_profiler` draws into the frame's geometry with `bui_fill` and `bui_text`, so the game's font is used if one is set with `bui_set_font`. Text sizes scale with the theme's `pixel`, rounded to whole font pixels when the built-in bitmap font is in use. The layout is sized for a `pixel` of 2 on a 1280×720 area. For a larger `pixel`, give it a taller area. It declares no items, so it takes no focus and nothing is spoken. Draw it last so it sits on top. If the game's main context is sometimes drawn somewhere other than flat on the screen (a `bui_surface`), give the overlay a context of its own (`no_voice`, `skip_narrator_support_modal`, `BUI_CONTROLLER_MODALS_NONE`) and append its geometry after the rest. It shows:

- **The frame graph.** One pair of bars per kept frame: CPU time in blue (the main thread's frame, start to end) and GPU time in orange. Lines mark 16.7 ms and 33.3 ms. The shown frame is outlined.
- **The timeline.** The shown frame, a lane per thread. Each task is a bar, with nested tasks under their parents (up to six deep). Bars are coloured by name, so a task keeps its colour from frame to frame. The widest bars are labelled with their name and milliseconds. A ruler of milliseconds runs along the top.
- **Top tasks.** The shown frame's tasks summed by name across every thread (GPU tasks prefixed `GPU:`): milliseconds, calls and share of the frame, the twelve biggest.
- **The header.** The frame's number, CPU and GPU milliseconds, LIVE or FROZEN, and how many tasks were dropped.

## Looking through frames

The overlay shows the newest frame (live) until you step it with `bui_prof_step_view`. Stepping freezes the history, so the kept frames hold still while the game keeps running. `BUI_PROF_LIVE` goes back to the newest frame and starts keeping frames again. BlueUI doesn't bind any keys to this; map your own:

| Step | |
|---|---|
| `BUI_PROF_PREV`, `BUI_PROF_NEXT` | One frame back or forward. |
| `BUI_PROF_PREV_10`, `BUI_PROF_NEXT_10` | Ten frames. |
| `BUI_PROF_SLOWEST_OLDER`, `BUI_PROF_SLOWEST_NEWER` | The slowest frame older or newer than the shown one. |
| `BUI_PROF_LIVE` | Back to the newest, recording again. |

Turning the profiler on with `bui_prof_set_enabled(true)` also returns the view to live.

## Reading the records

`bui_prof_frame_count` and `bui_prof_frame_at(i)` (oldest first) give the kept frames, at most `BUI_PROF_HISTORY` (240). Each `bui_prof_frame` holds its lanes, and each `bui_prof_lane` its events in start order. The pointers stay valid until the next `bui_prof_frame_end` or `bui_prof_add_gpu`. Read them on the thread that calls those.

## What it costs

- **Off** (`bui_prof_enabled()` false): a scope is one call into the library, a relaxed atomic load and a return. No clock is read, nothing is written and no lock is taken. Frame begin and end return at once. A thread's ring (768 KiB) is not allocated until the thread first records.
- **Compiled out:** with `-DBLUEUI_PROFILER=OFF` (which defines `BUI_PROFILER=0` for whatever links `blueui`), `BUI_PROFILE` and `BUI_PROFILE_DYN` do nothing, and their argument is not evaluated. The functions are still built.
- **On:** two clock reads and a ring write per scope. Each thread has its own ring, so no locks are taken. Once a frame, the main thread gathers every ring into the frame's record.

## How it works

Each thread writes its finished tasks (name, start, end, depth) into its own ring of 32,768 entries. It is the only writer and publishes its write count with a release store. At `bui_prof_frame_end` the main thread reads every ring up to that count into a frame record. If a thread records more than a ring holds between two frame ends, the oldest entries are lost and counted in `bui_prof_dropped`. Thread exit is detected with a C11 `tss_t` destructor.
