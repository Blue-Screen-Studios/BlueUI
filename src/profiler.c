/* The profiler's recording (blueui/profiler.h, docs/profiler.md): a ring of
 * finished tasks a thread, gathered by the main thread at each frame's end
 * into a frame record, the last BUI_PROF_HISTORY of them kept. The view's
 * drawing is profilerview.c's. */

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#include "blueui/profiler.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#define RING (1u << 15) /* finished tasks a thread can hold between frames */

static atomic_bool g_enabled = false;

/* ------------------------------------------------------------ the rings */

typedef struct open_task {
  const char* name;
  uint64_t start;
} open_task;

/* A thread's ring: one writer (its thread), read by the main thread at each
 * frame's end. Kept for good (a thread that's gone hands it to the next one
 * that comes). */
typedef struct thread_buf {
  const char* name;       /* (interned) */
  bui_prof_event* ring;   /* (RING of them, made at its first task: none while it records nothing) */
  atomic_uint_fast64_t written;
  uint64_t read;          /* the gatherer's */
  open_task* open;        /* the writer's open tasks */
  size_t open_count, open_cap;
  bool free;
} thread_buf;

/* A kept frame: its public record, and the arrays behind it (reused). */
typedef struct frame_slot {
  bui_prof_frame f;
  bui_prof_lane* lanes;
  size_t* event_caps;
  size_t lane_cap;
} frame_slot;

static struct {
  once_flag once;
  mtx_t mutex;
  tss_t key;
  thread_buf** bufs;
  size_t buf_count, buf_cap;
  /* intern's: an open-addressed set of strings */
  char** names;
  size_t name_count, name_cap;
  /* the history: hist[(head + i) % BUI_PROF_HISTORY], i < count */
  frame_slot hist[BUI_PROF_HISTORY];
  size_t head, count;
  frame_slot building;
  uint64_t frame, frame_start;
  bool frozen;
  uint64_t dropped;
  /* the view */
  uint64_t view_sel;
  bool view_live;
} R = {.once = ONCE_FLAG_INIT, .view_live = true};

static _Thread_local thread_buf* t_buf;

static void on_thread_exit(void* p) {
  thread_buf* b = (thread_buf*)p;
  mtx_lock(&R.mutex);
  b->open_count = 0;
  b->free = true;
  mtx_unlock(&R.mutex);
}

static void init_once(void) {
  mtx_init(&R.mutex, mtx_plain);
  tss_create(&R.key, on_thread_exit);
}
static void init(void) { call_once(&R.once, init_once); }

static const char* intern_locked(const char* s);

/* This thread's ring (made, or a gone thread's taken, the first time). */
static thread_buf* mine(void) {
  if (t_buf) return t_buf;
  init();
  mtx_lock(&R.mutex);
  thread_buf* b = NULL;
  for (size_t i = 0; i < R.buf_count && !b; ++i)
    if (R.bufs[i]->free) b = R.bufs[i], b->free = false;
  if (!b) {
    if (R.buf_count == R.buf_cap) {
      R.buf_cap = R.buf_cap ? R.buf_cap * 2 : 16;
      R.bufs = (thread_buf**)realloc(R.bufs, R.buf_cap * sizeof *R.bufs);
    }
    b = (thread_buf*)calloc(1, sizeof *b);
    char name[32];
    snprintf(name, sizeof name, "Thread %zu", R.buf_count + 1);
    b->name = intern_locked(name);
    R.bufs[R.buf_count++] = b;
  }
  mtx_unlock(&R.mutex);
  t_buf = b;
  tss_set(R.key, b);
  return b;
}

/* ------------------------------------------------------------- interning */

static uint64_t hash(const char* s) {
  uint64_t h = 1469598103934665603ull;
  for (; *s; ++s) h = (h ^ (unsigned char)*s) * 1099511628211ull;
  return h;
}

static const char* intern_locked(const char* s) {
  if (R.name_count * 2 >= R.name_cap) {
    const size_t cap = R.name_cap ? R.name_cap * 2 : 256;
    char** names = (char**)calloc(cap, sizeof *names);
    for (size_t i = 0; i < R.name_cap; ++i)
      if (R.names[i]) {
        size_t at = hash(R.names[i]) & (cap - 1);
        while (names[at]) at = (at + 1) & (cap - 1);
        names[at] = R.names[i];
      }
    free(R.names);
    R.names = names, R.name_cap = cap;
  }
  size_t at = hash(s) & (R.name_cap - 1);
  for (; R.names[at]; at = (at + 1) & (R.name_cap - 1))
    if (strcmp(R.names[at], s) == 0) return R.names[at];
  const size_t n = strlen(s) + 1;
  R.names[at] = (char*)malloc(n);
  memcpy(R.names[at], s, n);
  ++R.name_count;
  return R.names[at];
}

const char* bui_prof_intern(const char* name) {
  init();
  mtx_lock(&R.mutex);
  const char* s = intern_locked(name ? name : "");
  mtx_unlock(&R.mutex);
  return s;
}

/* -------------------------------------------------------------- recording */

uint64_t bui_prof_now_ns(void) {
#if defined(_WIN32)
  static LARGE_INTEGER freq;
  if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  return (uint64_t)((double)c.QuadPart * 1e9 / (double)freq.QuadPart);
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

bool bui_prof_enabled(void) { return atomic_load_explicit(&g_enabled, memory_order_relaxed); }

void bui_prof_set_enabled(bool on) {
  if (on == bui_prof_enabled()) return;
  if (on) {
    /* Nothing from before (each ring read up to now); the view live. */
    init();
    mtx_lock(&R.mutex);
    for (size_t i = 0; i < R.buf_count; ++i)
      R.bufs[i]->read = atomic_load_explicit(&R.bufs[i]->written, memory_order_acquire);
    mtx_unlock(&R.mutex);
    R.frame_start = bui_prof_now_ns();
    R.view_live = true;
    R.frozen = false;
  }
  atomic_store_explicit(&g_enabled, on, memory_order_relaxed);
}

void bui_prof_name_thread(const char* name) {
  thread_buf* b = mine();
  mtx_lock(&R.mutex);
  b->name = intern_locked(name ? name : "");
  mtx_unlock(&R.mutex);
}

bool bui_prof_begin(const char* name) {
  if (!bui_prof_enabled()) return false;
  thread_buf* b = mine();
  if (b->open_count == b->open_cap) {
    b->open_cap = b->open_cap ? b->open_cap * 2 : 32;
    b->open = (open_task*)realloc(b->open, b->open_cap * sizeof *b->open);
  }
  b->open[b->open_count].name = name;
  b->open[b->open_count].start = bui_prof_now_ns();
  ++b->open_count;
  return true;
}

void bui_prof_end(void) {
  thread_buf* b = mine();
  if (!b->open_count) return;
  const open_task t = b->open[--b->open_count];
  const uint64_t w = atomic_load_explicit(&b->written, memory_order_relaxed);
  if (!b->ring) {
    /* (Made under the lock: the gatherer looks for it.) */
    bui_prof_event* ring = (bui_prof_event*)calloc(RING, sizeof *ring);
    mtx_lock(&R.mutex);
    b->ring = ring;
    mtx_unlock(&R.mutex);
  }
  bui_prof_event* e = &b->ring[w % RING];
  e->name = t.name, e->start = t.start, e->end = bui_prof_now_ns(), e->depth = (uint16_t)b->open_count;
  atomic_store_explicit(&b->written, w + 1, memory_order_release);
}

/* ----------------------------------------------------------- the frames */

static frame_slot* slot_at(size_t i) { return &R.hist[(R.head + i) % BUI_PROF_HISTORY]; }

static void slot_clear(frame_slot* s) {
  s->f.lane_count = 0;
  for (size_t i = 0; i < s->lane_cap; ++i) s->lanes[i].event_count = 0;
}

/* A lane put in at `at` (the ones from there moved on): its index. */
static size_t slot_insert_lane(frame_slot* s, size_t at, const char* thread, bool gpu) {
  if (s->f.lane_count == s->lane_cap) {
    const size_t cap = s->lane_cap ? s->lane_cap * 2 : 8;
    s->lanes = (bui_prof_lane*)realloc(s->lanes, cap * sizeof *s->lanes);
    s->event_caps = (size_t*)realloc(s->event_caps, cap * sizeof *s->event_caps);
    for (size_t i = s->lane_cap; i < cap; ++i) {
      memset(&s->lanes[i], 0, sizeof s->lanes[i]);
      s->event_caps[i] = 0;
    }
    s->lane_cap = cap;
  }
  /* (The spare lane's arrays, at the end, carried round to `at`.) */
  const bui_prof_lane spare = s->lanes[s->f.lane_count];
  const size_t spare_cap = s->event_caps[s->f.lane_count];
  memmove(&s->lanes[at + 1], &s->lanes[at], (s->f.lane_count - at) * sizeof *s->lanes);
  memmove(&s->event_caps[at + 1], &s->event_caps[at], (s->f.lane_count - at) * sizeof *s->event_caps);
  s->lanes[at] = spare;
  s->event_caps[at] = spare_cap;
  s->lanes[at].thread = thread;
  s->lanes[at].gpu = gpu;
  s->lanes[at].event_count = 0;
  ++s->f.lane_count;
  s->f.lanes = s->lanes;
  return at;
}

static void slot_push_event(frame_slot* s, size_t lane, bui_prof_event e) {
  bui_prof_lane* l = &s->lanes[lane];
  if (l->event_count == s->event_caps[lane]) {
    s->event_caps[lane] = s->event_caps[lane] ? s->event_caps[lane] * 2 : 64;
    l->events = (bui_prof_event*)realloc((void*)l->events, s->event_caps[lane] * sizeof *l->events);
  }
  ((bui_prof_event*)l->events)[l->event_count++] = e;
}

static int by_start(const void* a, const void* b) {
  const bui_prof_event* x = (const bui_prof_event*)a;
  const bui_prof_event* y = (const bui_prof_event*)b;
  if (x->start != y->start) return x->start < y->start ? -1 : 1;
  return (int)x->depth - (int)y->depth;
}

uint64_t bui_prof_current_frame(void) { return R.frame; }

void bui_prof_frame_begin(void) {
  if (!bui_prof_enabled()) return;
  R.frame_start = bui_prof_now_ns();
}

/* A task begun before the frame (a worker's long task): kept with the frame
 * it started in. */
static void file_late(const char* thread, bui_prof_event e) {
  for (size_t k = R.count; k-- > 0;) {
    frame_slot* s = slot_at(k);
    if (e.start < s->f.start) continue;
    size_t lane = s->f.lane_count;
    for (size_t i = 0; i < s->f.lane_count; ++i)
      if (!s->lanes[i].gpu && s->lanes[i].thread == thread) lane = i;
    if (lane == s->f.lane_count) {
      size_t at = s->f.lane_count;
      if (at && s->lanes[at - 1].gpu) --at; /* (the GPU's stays last) */
      lane = slot_insert_lane(s, at, thread, false);
    }
    slot_push_event(s, lane, e);
    return;
  }
}

void bui_prof_frame_end(void) {
  if (!bui_prof_enabled()) return;
  init();
  const uint64_t now = bui_prof_now_ns();
  frame_slot* rec = &R.building;
  slot_clear(rec);
  rec->f.index = R.frame++;
  rec->f.start = R.frame_start;
  rec->f.end = now;
  rec->f.gpu_ms = -1.0;
  rec->f.lanes = rec->lanes;
  mtx_lock(&R.mutex);
  for (size_t bi = 0; bi < R.buf_count; ++bi) {
    thread_buf* b = R.bufs[bi];
    if (!b->ring) continue;
    const uint64_t w = atomic_load_explicit(&b->written, memory_order_acquire);
    if (w - b->read > RING) {
      R.dropped += w - b->read - RING;
      b->read = w - RING;
    }
    size_t lane = (size_t)-1;
    for (uint64_t i = b->read; i < w; ++i) {
      const bui_prof_event e = b->ring[i % RING];
      if (e.start < rec->f.start && R.count) {
        file_late(b->name, e);
        continue;
      }
      if (lane == (size_t)-1) lane = slot_insert_lane(rec, rec->f.lane_count, b->name, false);
      slot_push_event(rec, lane, e);
    }
    b->read = w;
    if (lane != (size_t)-1)
      qsort((void*)rec->lanes[lane].events, rec->lanes[lane].event_count, sizeof(bui_prof_event), by_start);
  }
  mtx_unlock(&R.mutex);
  /* The main thread's lane first (the one whose tasks span the frame). */
  for (size_t i = 1; i < rec->f.lane_count; ++i)
    if (strcmp(rec->lanes[i].thread, "Main") == 0) {
      const bui_prof_lane l = rec->lanes[i];
      const size_t c = rec->event_caps[i];
      memmove(&rec->lanes[1], &rec->lanes[0], i * sizeof *rec->lanes);
      memmove(&rec->event_caps[1], &rec->event_caps[0], i * sizeof *rec->event_caps);
      rec->lanes[0] = l, rec->event_caps[0] = c;
      break;
    }
  R.frame_start = now;
  if (R.frozen) return;
  /* Kept: swapped into the history (the oldest's arrays reused next). */
  frame_slot* to;
  if (R.count < BUI_PROF_HISTORY) {
    to = slot_at(R.count++);
  } else {
    to = slot_at(0);
    R.head = (R.head + 1) % BUI_PROF_HISTORY;
  }
  const frame_slot t = *to;
  *to = *rec;
  *rec = t;
}

void bui_prof_add_gpu(uint64_t frame, uint64_t start_ns, const bui_prof_gpu_span* spans, size_t count) {
  for (size_t k = 0; k < R.count; ++k) {
    frame_slot* s = slot_at(k);
    if (s->f.index != frame) continue;
    if (!count) return;
    const size_t lane = slot_insert_lane(s, s->f.lane_count, "GPU", true);
    double first = spans[0].start_ms, last = 0.0;
    for (size_t i = 0; i < count; ++i) {
      bui_prof_event e;
      e.name = spans[i].name;
      e.start = start_ns + (uint64_t)(spans[i].start_ms * 1e6);
      e.end = start_ns + (uint64_t)(spans[i].end_ms * 1e6);
      e.depth = spans[i].depth;
      slot_push_event(s, lane, e);
      if (spans[i].end_ms > last) last = spans[i].end_ms;
      if (spans[i].start_ms < first) first = spans[i].start_ms;
    }
    s->f.gpu_ms = last - first;
    return;
  }
}

size_t bui_prof_frame_count(void) { return R.count; }
const bui_prof_frame* bui_prof_frame_at(size_t i) { return i < R.count ? &slot_at(i)->f : NULL; }
void bui_prof_set_frozen(bool frozen) { R.frozen = frozen; }
bool bui_prof_frozen(void) { return R.frozen; }
uint64_t bui_prof_dropped(void) { return R.dropped; }

/* -------------------------------------------------------------- the view */

void bui_prof_step_view(bui_prof_step step) {
  if (step == BUI_PROF_LIVE) {
    R.view_live = true;
    R.frozen = false;
    return;
  }
  if (!R.count) return;
  /* Stepping freezes the capture (the frames hold still to look through). */
  if (R.view_live) R.view_sel = slot_at(R.count - 1)->f.index;
  R.view_live = false;
  R.frozen = true;
  /* The shown frame's place in the history. */
  size_t at = R.count - 1;
  for (size_t i = 0; i < R.count; ++i)
    if (slot_at(i)->f.index == R.view_sel) at = i;
  const size_t n = step == BUI_PROF_PREV_10 || step == BUI_PROF_NEXT_10 ? 10 : 1;
  if (step == BUI_PROF_PREV || step == BUI_PROF_PREV_10) at = at >= n ? at - n : 0;
  if (step == BUI_PROF_NEXT || step == BUI_PROF_NEXT_10) at = at + n < R.count ? at + n : R.count - 1;
  if (step == BUI_PROF_SLOWEST_OLDER || step == BUI_PROF_SLOWEST_NEWER) {
    size_t best = at;
    double worst = -1.0;
    const size_t lo = step == BUI_PROF_SLOWEST_OLDER ? 0 : at + 1;
    const size_t hi = step == BUI_PROF_SLOWEST_OLDER ? at : R.count;
    for (size_t i = lo; i < hi; ++i) {
      const double ms = bui_prof_frame_cpu_ms(&slot_at(i)->f);
      if (ms > worst) worst = ms, best = i;
    }
    at = best;
  }
  R.view_sel = slot_at(at)->f.index;
}

const bui_prof_frame* bui_prof_view_frame(void) {
  if (!R.count) return NULL;
  if (!R.view_live)
    for (size_t i = 0; i < R.count; ++i)
      if (slot_at(i)->f.index == R.view_sel) return &slot_at(i)->f;
  return &slot_at(R.count - 1)->f;
}

bool bui_prof_view_live(void) { return R.view_live; }
