/* The voice: CMU's Flite and its US English "slt" voice (compiled in:
 * nothing of the system's), lines synthesized on a thread of its own into
 * WAVs the game takes (bui_next_audio). The newest line wins: what's
 * waiting is replaced by it, and what's playing cut off when it's ready;
 * queued lines wait their turn. Without Flite (or with no_voice): the
 * lines handed over as text alone, at once. */

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "internal.h"

#ifdef BLUEUI_WITH_FLITE
#include "flite.h"
cst_voice* register_cmu_us_slt(const char* voxdir);
#endif

struct bui__voice {
  void* flite;  /* (cst_voice: NULL, none) */
  int rate;
  float speed;
  thrd_t thread;
  bool running, quit;
  mtx_t lock;
  mtx_t engine; /* (Flite's not to be run from two threads at once) */
  cnd_t wake;
  /* What's to be said: the line waiting (cutting in), those queued. */
  char* waiting;
  char** queued;
  size_t queued_count, queued_cap;
  bool working;
  unsigned long asked;  /* (bumped by every cut-in: a line made meanwhile is stale) */
  /* What's ready for the game, in order. */
  bui_audio* out;
  size_t out_count, out_cap;
};

static char* dup(const char* s) {
  const size_t n = strlen(s) + 1;
  char* p = malloc(n);
  if (!p) abort();
  memcpy(p, s, n);
  return p;
}

static void push_out(bui__voice* v, bui_audio a) {
  if (v->out_count == v->out_cap) {
    v->out_cap = v->out_cap ? v->out_cap * 2 : 8;
    v->out = realloc(v->out, v->out_cap * sizeof(bui_audio));
    if (!v->out) abort();
  }
  v->out[v->out_count++] = a;
}

/* (Lines not yet played, dropped: a cut-in's.) */
static void drop_pending(bui__voice* v) {
  free(v->waiting);
  v->waiting = NULL;
  for (size_t k = 0; k < v->queued_count; ++k) free(v->queued[k]);
  v->queued_count = 0;
  size_t keep = 0;
  for (size_t k = 0; k < v->out_count; ++k) {
    if (v->out[k].op == BUI_AUDIO_STOP || v->out[k].op == BUI_AUDIO_SOUND) v->out[keep++] = v->out[k];
    else bui_audio_free(&v->out[k]);
  }
  v->out_count = keep;
}

/* A WAV round 16-bit mono samples (the buffer's own: the game's to free). */
bool bui__make_wav(const char* text, const float* pcm, size_t n, int rate, bui_audio* a) {
  memset(a, 0, sizeof(*a));
  const size_t data = n * 2, size = 44 + data;
  uint8_t* w = malloc(size);
  if (!w) return false;
  const uint32_t riff = (uint32_t)(size - 8), fmt = 16, byte_rate = (uint32_t)rate * 2, sr = (uint32_t)rate, len = (uint32_t)data;
  const uint16_t pcm_tag = 1, channels = 1, align = 2, bits = 16;
  uint8_t* p = w;
#define PUT(bytes, n_) (memcpy(p, (bytes), (n_)), p += (n_))
#define PUT32(v_) (p[0] = (uint8_t)(v_), p[1] = (uint8_t)((v_) >> 8), p[2] = (uint8_t)((v_) >> 16), p[3] = (uint8_t)((v_) >> 24), p += 4)
#define PUT16(v_) (p[0] = (uint8_t)(v_), p[1] = (uint8_t)((v_) >> 8), p += 2)
  PUT("RIFF", 4), PUT32(riff), PUT("WAVE", 4);
  PUT("fmt ", 4), PUT32(fmt), PUT16(pcm_tag), PUT16(channels), PUT32(sr), PUT32(byte_rate), PUT16(align), PUT16(bits);
  PUT("data", 4), PUT32(len);
#undef PUT
#undef PUT32
#undef PUT16
  int16_t* s = (int16_t*)(w + 44);
  for (size_t i = 0; i < n; ++i) {
    float f = pcm[i] * 32767.0f;
    f = f > 32767.0f ? 32767.0f : f < -32768.0f ? -32768.0f : f;
    s[i] = (int16_t)lrintf(f);
  }
  a->text = text ? dup(text) : NULL;
  a->wav = w, a->wav_size = size;
  a->samples = s, a->sample_count = n, a->sample_rate = rate;
  return true;
}

#ifdef BLUEUI_WITH_FLITE

/* Text as it should be read: lower case (Flite spells out words in
 * capitals it doesn't know), "%" as "percent", a UI's separators as pauses. */
static char* readable(const char* text) {
  bui__str out = {0};
  bui__str_set(&out, "");
  for (const char* p = text; *p; ++p) {
    char c[2] = {(char)tolower((unsigned char)*p), 0};
    if (*p == '%') bui__str_add(&out, " percent");
    else if (*p == '<' || *p == '>' || *p == '|') bui__str_add(&out, ", ");
    else bui__str_add(&out, c);
  }
  return out.data;
}

/* `text` spoken: at the voice's rate, its loudest 0.7, its ends eased. */
static bool synthesize(bui__voice* v, const char* text, bui_audio* a) {
  if (!v->flite) return false;
  char* said = readable(text);
  cst_wave* w;
  mtx_lock(&v->engine);
  cst_voice* fv = v->flite;
  const float speed = v->speed < 0.5f ? 0.5f : v->speed > 2.0f ? 2.0f : v->speed;
  feat_set_float(fv->features, "duration_stretch", 1.0f / speed);
  w = flite_text_to_wave(said, fv);
  mtx_unlock(&v->engine);
  free(said);
  if (!w) return false;
  /* To the game's rate (Catmull-Rom between the voice's samples). */
  const int n = w->num_samples, from = w->sample_rate > 1 ? w->sample_rate : 1, to = v->rate;
  const double step = (double)from / to;
  const size_t count = n > 0 ? (size_t)((double)n / step) : 0;
  float* out = malloc((count ? count : 1) * sizeof(float));
  if (!out) abort();
  float peak = 0.0f;
#define AT(i_) ((float)w->samples[(i_) < 0 ? 0 : (i_) > n - 1 ? n - 1 : (i_)] / 32768.0f)
  for (size_t i = 0; i < count; ++i) {
    const double t = (double)i * step;
    const int k = (int)t;
    const float f = (float)(t - k), p0 = AT(k - 1), p1 = AT(k), p2 = AT(k + 1), p3 = AT(k + 2);
    out[i] = p1 + 0.5f * f * (p2 - p0 + f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 + f * (3.0f * (p1 - p2) + p3 - p0)));
    if (fabsf(out[i]) > peak) peak = fabsf(out[i]);
  }
#undef AT
  delete_wave(w);
  /* Its loudest at 0.7; its ends eased (5 ms). */
  const float gain = peak > 1e-4f ? 0.7f / peak : 0.0f;
  size_t ease = (size_t)to / 200;
  if (ease > count / 2) ease = count / 2;
  for (size_t i = 0; i < count; ++i) {
    float e = 1.0f;
    if (ease > 0 && i < ease) e = (float)i / (float)ease;
    if (ease > 0 && count - 1 - i < ease) {
      const float end = (float)(count - 1 - i) / (float)ease;
      if (end < e) e = end;
    }
    out[i] *= gain * e;
  }
  const bool ok = bui__make_wav(text, out, count, to, a);
  free(out);
  return ok;
}

static int run(void* arg) {
  bui__voice* v = arg;
  for (;;) {
    mtx_lock(&v->lock);
    while (!v->quit && !v->waiting && v->queued_count == 0) cnd_wait(&v->wake, &v->lock);
    if (v->quit) {
      mtx_unlock(&v->lock);
      return 0;
    }
    char* text;
    bool queued = false;
    if (v->waiting) {
      text = v->waiting, v->waiting = NULL;
    } else {
      text = v->queued[0], queued = true;
      memmove(v->queued, v->queued + 1, (v->queued_count - 1) * sizeof(char*));
      --v->queued_count;
    }
    const unsigned long asked = v->asked;
    v->working = true;
    mtx_unlock(&v->lock);

    bui_audio a;
    const bool made = synthesize(v, text, &a);
    free(text);
    mtx_lock(&v->lock);
    v->working = false;
    if (made) {
      /* (Something newer asked meanwhile, cutting in: this one's stale.) */
      if (asked != v->asked || v->quit) bui_audio_free(&a);
      else a.op = queued ? BUI_AUDIO_QUEUE : BUI_AUDIO_PLAY, push_out(v, a);
    }
    mtx_unlock(&v->lock);
  }
}

#endif

bui__voice* bui__voice_create(const bui_config* c) {
  bui__voice* v = calloc(1, sizeof(bui__voice));
  if (!v) abort();
  v->rate = c->sample_rate > 0 ? c->sample_rate : 48000;
  v->speed = c->speech_rate > 0.0f ? c->speech_rate : 1.15f;
  mtx_init(&v->lock, mtx_plain);
  mtx_init(&v->engine, mtx_plain);
  cnd_init(&v->wake);
#ifdef BLUEUI_WITH_FLITE
  if (!c->no_voice) {
    flite_init();
    v->flite = register_cmu_us_slt(NULL);
    if (v->flite && thrd_create(&v->thread, run, v) == thrd_success) v->running = true;
    else if (v->flite) delete_voice(v->flite), v->flite = NULL;
  }
#endif
  return v;
}

void bui__voice_destroy(bui__voice* v) {
  if (!v) return;
  mtx_lock(&v->lock);
  v->quit = true;
  cnd_broadcast(&v->wake);
  mtx_unlock(&v->lock);
  if (v->running) thrd_join(v->thread, NULL);
#ifdef BLUEUI_WITH_FLITE
  if (v->flite) delete_voice(v->flite);
#endif
  drop_pending(v);
  for (size_t k = 0; k < v->out_count; ++k) bui_audio_free(&v->out[k]);
  free(v->out), free(v->queued);
  mtx_destroy(&v->lock), mtx_destroy(&v->engine);
  cnd_destroy(&v->wake);
  free(v);
}

bool bui__voice_real(const bui__voice* v) { return v->flite != NULL; }
int bui__voice_rate(const bui__voice* v) { return v->rate; }

void bui__voice_push(bui__voice* v, bui_audio a) {
  mtx_lock(&v->lock);
  push_out(v, a);
  mtx_unlock(&v->lock);
}

void bui__voice_say(bui__voice* v, const char* text, bool queued) {
  if (!text || !*text) return;
  mtx_lock(&v->lock);
  if (!v->flite) {
    /* (Text alone: handed over as it is.) */
    bui_audio a = {0};
    a.op = queued ? BUI_AUDIO_QUEUE : BUI_AUDIO_PLAY;
    a.text = dup(text);
    if (!queued) drop_pending(v);
    push_out(v, a);
  } else if (queued) {
    if (v->queued_count == v->queued_cap) {
      v->queued_cap = v->queued_cap ? v->queued_cap * 2 : 8;
      v->queued = realloc(v->queued, v->queued_cap * sizeof(char*));
      if (!v->queued) abort();
    }
    v->queued[v->queued_count++] = dup(text);
  } else {
    drop_pending(v);
    v->waiting = dup(text);
    ++v->asked;
  }
  cnd_signal(&v->wake);
  mtx_unlock(&v->lock);
}

void bui__voice_stop(bui__voice* v) {
  mtx_lock(&v->lock);
  drop_pending(v);
  ++v->asked;
  bui_audio a = {0};
  a.op = BUI_AUDIO_STOP;
  push_out(v, a);
  mtx_unlock(&v->lock);
}

bool bui__voice_busy(bui__voice* v) {
  mtx_lock(&v->lock);
  bool busy = v->waiting || v->working || v->queued_count > 0;
  for (size_t k = 0; k < v->out_count; ++k) busy = busy || v->out[k].op == BUI_AUDIO_PLAY || v->out[k].op == BUI_AUDIO_QUEUE;
  mtx_unlock(&v->lock);
  return busy;
}

bool bui__voice_next(bui__voice* v, bui_audio* out) {
  mtx_lock(&v->lock);
  const bool any = v->out_count > 0;
  if (any) {
    *out = v->out[0];
    memmove(v->out, v->out + 1, (v->out_count - 1) * sizeof(bui_audio));
    --v->out_count;
  }
  mtx_unlock(&v->lock);
  return any;
}

bool bui__voice_synthesize(bui__voice* v, const char* text, bui_audio* out) {
#ifdef BLUEUI_WITH_FLITE
  if (synthesize(v, bui__s(text), out)) {
    out->op = BUI_AUDIO_PLAY;
    return true;
  }
#else
  (void)v, (void)text;
#endif
  memset(out, 0, sizeof(*out));
  return false;
}
