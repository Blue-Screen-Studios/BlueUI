/* GUI sounds: the built-in packs (made in code at the context's sample
 * rate, when it's created) and the frame's one sound, handed to the game as
 * a WAV. */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

static const char* const kNames[BUI_SOUND_COUNT] = {"move", "press", "back", "change", "on", "off", "tab", "screen", "denied"};

/* Which of two events wins a frame: the more specific. */
static int rank(int sound) {
  static const int kRank[BUI_SOUND_COUNT] = {1, 3, 7, 2, 6, 6, 5, 4, 8};
  return sound < 0 || sound >= BUI_SOUND_COUNT ? 0 : kRank[sound];
}

/* ---------------------------------------------------------- the packs */

typedef enum wave { SINE, SQUARE, NOISE } wave;

/* A clip being made: its samples, grown as notes are added. */
typedef struct clip {
  float* pcm;
  size_t count;
  int rate;
  unsigned seed;
} clip;

/* A note mixed into the clip from `at` seconds: `ms` long, its pitch
 * gliding from f0 to f1 Hz, a 2 ms attack, an exponential fade. Returns
 * where it ends, in seconds. */
static float note(clip* c, float at, float ms, float f0, float f1, wave w, float gain) {
  const size_t start = (size_t)(at * (float)c->rate), n = (size_t)(ms * (float)c->rate / 1000.0f);
  if (start + n > c->count) {
    float* grown = realloc(c->pcm, (start + n) * sizeof(float));
    if (!grown) return at;
    memset(grown + c->count, 0, (start + n - c->count) * sizeof(float));
    c->pcm = grown, c->count = start + n;
  }
  float phase = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    const float t = (float)i / (float)n, f = f0 + (f1 - f0) * t;
    phase += 6.2831853f * f / (float)c->rate;
    float s;
    if (w == SINE) s = sinf(phase);
    else if (w == SQUARE) s = sinf(phase) > 0.0f ? 0.6f : -0.6f;
    else c->seed = c->seed * 1103515245u + 12345u, s = (float)(c->seed >> 16 & 0x7fff) / 16384.0f - 1.0f;
    const float attack = fminf(1.0f, (float)i / (0.002f * (float)c->rate)), decay = expf(-5.0f * t);
    c->pcm[start + i] += s * gain * attack * decay;
  }
  return at + ms / 1000.0f;
}

/* One event's clip in one pack. */
static clip make(int pack, int sound, int rate) {
  clip c = {NULL, 0, rate, 1};
  if (pack == BUI_SOUNDS_SOFT) {  /* rounded sine blips */
    switch (sound) {
      case BUI_SOUND_MOVE: note(&c, 0, 35, 880, 880, SINE, 0.25f); break;
      case BUI_SOUND_PRESS: note(&c, 0, 70, 660, 700, SINE, 0.5f); break;
      case BUI_SOUND_BACK: note(&c, 0, 90, 620, 420, SINE, 0.45f); break;
      case BUI_SOUND_CHANGE: note(&c, 0, 30, 1040, 1040, SINE, 0.3f); break;
      case BUI_SOUND_ON: note(&c, note(&c, 0, 60, 523, 523, SINE, 0.45f), 80, 784, 784, SINE, 0.45f); break;
      case BUI_SOUND_OFF: note(&c, note(&c, 0, 60, 784, 784, SINE, 0.45f), 80, 523, 523, SINE, 0.45f); break;
      case BUI_SOUND_TAB: note(&c, 0, 80, 740, 990, SINE, 0.4f); break;
      case BUI_SOUND_SCREEN: note(&c, 0, 180, 440, 440, SINE, 0.3f), note(&c, 0, 180, 660, 660, SINE, 0.2f); break;
      case BUI_SOUND_DENIED: note(&c, 0, 140, 200, 170, SINE, 0.6f); break;
    }
  } else if (pack == BUI_SOUNDS_CRISP) {  /* clicks and square-wave ticks */
    switch (sound) {
      case BUI_SOUND_MOVE: note(&c, 0, 8, 0, 0, NOISE, 0.3f); break;
      case BUI_SOUND_PRESS: note(&c, note(&c, 0, 6, 0, 0, NOISE, 0.4f), 30, 1400, 1400, SQUARE, 0.25f); break;
      case BUI_SOUND_BACK: note(&c, 0, 50, 1000, 600, SQUARE, 0.25f); break;
      case BUI_SOUND_CHANGE: note(&c, 0, 12, 2000, 2000, SQUARE, 0.15f); break;
      case BUI_SOUND_ON: note(&c, 0, 50, 900, 1500, SQUARE, 0.25f); break;
      case BUI_SOUND_OFF: note(&c, 0, 50, 1500, 900, SQUARE, 0.25f); break;
      case BUI_SOUND_TAB: note(&c, note(&c, 0, 20, 1200, 1200, SQUARE, 0.2f), 25, 1600, 1600, SQUARE, 0.2f); break;
      case BUI_SOUND_SCREEN: note(&c, note(&c, 0, 15, 0, 0, NOISE, 0.3f), 90, 800, 1200, SQUARE, 0.2f); break;
      case BUI_SOUND_DENIED: note(&c, 0, 120, 150, 150, SQUARE, 0.35f); break;
    }
  }
  return c;
}

void bui__make_packs(bui_context* ui) {
  const int rate = bui__voice_rate(ui->voice);
  for (int p = 1; p < BUI_SOUND_PACK_COUNT; ++p)
    for (int s = 0; s < BUI_SOUND_COUNT; ++s) {
      const clip c = make(p, s, rate);
      ui->packs[p].pcm[s] = c.pcm, ui->packs[p].count[s] = c.count;
    }
}

void bui__free_packs(bui_context* ui) {
  for (int p = 0; p < BUI_SOUND_PACK_COUNT; ++p)
    for (int s = 0; s < BUI_SOUND_COUNT; ++s) free(ui->packs[p].pcm[s]), ui->packs[p].pcm[s] = NULL;
}

/* -------------------------------------------------------------- playing */

void bui__auto_sound(bui_context* ui, bui_sound sound) {
  if (!ui->sound_requested && rank((int)sound) > rank(ui->pending_sound)) ui->pending_sound = (int)sound;
}

void bui_play_sound(bui_context* ui, bui_sound sound) {
  if (sound < 0 || sound >= BUI_SOUND_COUNT) return;
  ui->pending_sound = (int)sound, ui->sound_requested = true;
}

void bui__emit_sound(bui_context* ui) {
  const int sound = ui->pending_sound, pack = (int)ui->theme.sound_pack;
  ui->pending_sound = -1, ui->sound_requested = false;
  if (sound < 0 || pack <= BUI_SOUNDS_NONE || pack >= BUI_SOUND_PACK_COUNT) return;
  const bui__pack* p = &ui->packs[pack];
  if (!p->pcm[sound] || !p->count[sound]) return;
  const float volume = ui->theme.sound_volume < 0.0f ? 0.0f : ui->theme.sound_volume > 1.0f ? 1.0f : ui->theme.sound_volume;
  if (volume <= 0.0f) return;
  float* pcm = malloc(p->count[sound] * sizeof(float));
  if (!pcm) return;
  for (size_t i = 0; i < p->count[sound]; ++i) pcm[i] = p->pcm[sound][i] * volume;
  bui_audio a;
  if (bui__make_wav(kNames[sound], pcm, p->count[sound], bui__voice_rate(ui->voice), &a)) {
    a.op = BUI_AUDIO_SOUND;
    bui__voice_push(ui->voice, a);
  }
  free(pcm);
}
