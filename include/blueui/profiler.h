#ifndef BLUEUI_PROFILER_H
#define BLUEUI_PROFILER_H

/* BlueUI's profiler (docs/profiler.md): how long each task takes, on every
 * thread (and the GPU, from the game's timestamps), a frame at a time, and
 * a widget that draws it.
 *
 * Mark a task with BUI_PROFILE("Name") at the top of its scope (C++, or C
 * on GCC and Clang; elsewhere bui_prof_begin / bui_prof_end). The name must
 * live as long as the program (a string literal); BUI_PROFILE_DYN for a
 * name made at run time (interned). Off, a scope is one call: a relaxed
 * load and a return. Built with BUI_PROFILER=0 (CMake's
 * -DBLUEUI_PROFILER=OFF), the macros are nothing at all.
 *
 * Each thread writes its own ring of finished tasks (one writer, no locks);
 * at each frame's end the game's main thread gathers every ring into a
 * frame record (a lane a thread, the GPU's last) and the last
 * BUI_PROF_HISTORY of them are kept.
 *
 *   bui_prof_name_thread("Main");
 *   ...each frame:
 *   bui_prof_frame_begin();
 *   { BUI_PROFILE("Update"); update(); }
 *   bui_begin_frame(ui, &input);
 *   ...
 *   if (shown) bui_profiler(ui, area);
 *   bui_end_frame(ui);
 *   bui_prof_frame_end(); */

#include "blueui.h"

#ifndef BUI_PROFILER
#define BUI_PROFILER 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BUI_PROF_HISTORY 240

/* On: recording (from the next frame), the view live again. Off: nothing's
 * recorded; what's kept stays. */
BUI_API void bui_prof_set_enabled(bool on);
BUI_API bool bui_prof_enabled(void);

/* This thread's name in the lanes ("Worker 3"); copied. */
BUI_API void bui_prof_name_thread(const char* name);
/* A name made at run time, kept for good (the same text: the same pointer). */
BUI_API const char* bui_prof_intern(const char* name);
/* The steady clock, in nanoseconds. */
BUI_API uint64_t bui_prof_now_ns(void);

/* A task's start and end on this thread (nested: they stack). begin: true
 * if it's recorded (the profiler on), and only then its end. */
BUI_API bool bui_prof_begin(const char* name);
BUI_API void bui_prof_end(void);

/* The main thread, round its frame: begun (the frame's start), ended (every
 * thread's new tasks gathered into its record). */
BUI_API void bui_prof_frame_begin(void);
BUI_API void bui_prof_frame_end(void);
/* The frame being recorded now (its index). */
BUI_API uint64_t bui_prof_current_frame(void);

/* The GPU's tasks of frame `frame` (when its timestamps came back: frames
 * late), placed from `start_ns` (its commands' submission, bui_prof_now_ns):
 * each name (living as long as the program), start and end from it (ms) and
 * depth. */
typedef struct bui_prof_gpu_span {
  const char* name;
  double start_ms, end_ms;
  uint16_t depth;
} bui_prof_gpu_span;
BUI_API void bui_prof_add_gpu(uint64_t frame, uint64_t start_ns, const bui_prof_gpu_span* spans, size_t count);

/* ---------------------------------------------------- the frames kept */

typedef struct bui_prof_event {
  const char* name;
  uint64_t start, end; /* bui_prof_now_ns */
  uint16_t depth;      /* how deep in its thread's tasks */
} bui_prof_event;

typedef struct bui_prof_lane {
  const char* thread;
  bool gpu;
  const bui_prof_event* events; /* by start */
  size_t event_count;
} bui_prof_lane;

typedef struct bui_prof_frame {
  uint64_t index;
  uint64_t start, end;   /* the main thread's frame (bui_prof_now_ns) */
  double gpu_ms;         /* the GPU's, start to end of its spans (-1: none) */
  const bui_prof_lane* lanes; /* the main thread's ("Main") first, the GPU's last */
  size_t lane_count;
} bui_prof_frame;

/* The kept frames, oldest first (the main thread's to read; good till the
 * next bui_prof_frame_end or bui_prof_add_gpu). */
BUI_API size_t bui_prof_frame_count(void);
BUI_API const bui_prof_frame* bui_prof_frame_at(size_t i);
static inline double bui_prof_frame_cpu_ms(const bui_prof_frame* f) { return (double)(f->end - f->start) * 1e-6; }

/* Frozen: nothing more's kept (the history holds still to look through). */
BUI_API void bui_prof_set_frozen(bool frozen);
BUI_API bool bui_prof_frozen(void);
/* Tasks lost since the profiler came on (a thread's ring overrun). */
BUI_API uint64_t bui_prof_dropped(void);

/* ---------------------------------------------------------- the view */

/* Which frame the view shows: the newest (live), or one stepped to (which
 * freezes the history). */
typedef enum bui_prof_step {
  BUI_PROF_PREV,           /* a frame back */
  BUI_PROF_NEXT,           /* a frame on */
  BUI_PROF_PREV_10,
  BUI_PROF_NEXT_10,
  BUI_PROF_SLOWEST_OLDER,  /* the slowest frame older than this one */
  BUI_PROF_SLOWEST_NEWER,  /* newer */
  BUI_PROF_LIVE            /* back to the newest, recording again */
} bui_prof_step;
BUI_API void bui_prof_step_view(bui_prof_step step);
/* The frame shown (NULL: none kept). */
BUI_API const bui_prof_frame* bui_prof_view_frame(void);
BUI_API bool bui_prof_view_live(void);

/* The profiler drawn over `area`, into the frame's geometry: the frame
 * graph (each kept frame's CPU and GPU time), the shown frame's timeline (a
 * lane a thread, its tasks as bars, nested under their parents) and its top
 * tasks. Drawing only: no items declared, nothing said. */
BUI_API void bui_profiler(bui_context* ui, bui_rect area);

#ifdef __cplusplus
}
#endif

/* ---------------------------------------------------------- the scopes */

#define BUI_PROF_CAT2(a, b) a##b
#define BUI_PROF_CAT(a, b) BUI_PROF_CAT2(a, b)

#ifdef __cplusplus
namespace bui {
/* A task for its scope. */
class ProfScope {
 public:
  explicit ProfScope(const char* name) : on_(bui_prof_begin(name)) {}
  ~ProfScope() {
    if (on_) bui_prof_end();
  }
  ProfScope(const ProfScope&) = delete;
  ProfScope& operator=(const ProfScope&) = delete;

 private:
  bool on_;
};
}  // namespace bui
#if BUI_PROFILER
#define BUI_PROFILE(name) ::bui::ProfScope BUI_PROF_CAT(buiProfScope_, __LINE__)(name)
#define BUI_PROFILE_DYN(name) \
  ::bui::ProfScope BUI_PROF_CAT(buiProfScope_, __LINE__)(bui_prof_enabled() ? bui_prof_intern(name) : "")
#endif
#elif defined(__GNUC__)
static inline void bui__prof_scope_end(bool* on) {
  if (*on) bui_prof_end();
}
#if BUI_PROFILER
#define BUI_PROFILE(name) \
  __attribute__((cleanup(bui__prof_scope_end))) bool BUI_PROF_CAT(bui_prof_scope_, __LINE__) = bui_prof_begin(name)
#define BUI_PROFILE_DYN(name) BUI_PROFILE(bui_prof_enabled() ? bui_prof_intern(name) : "")
#endif
#endif

#ifndef BUI_PROFILE
/* (The name unevaluated, but used: no unused-parameter warnings.) */
#define BUI_PROFILE(name) ((void)sizeof(name))
#define BUI_PROFILE_DYN(name) ((void)sizeof(name))
#endif

#endif
