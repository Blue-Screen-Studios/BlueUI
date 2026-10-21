/* Feedback for the frame's UI event: a sound from the built-in sound packs
 * (made in code at the context's sample rate, when it's created), handed
 * to the game as a WAV, and a rumble from the built-in rumble packs, handed
 * to the game as timings in the state. */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

static const char* const kNames[BUI_EVENT_COUNT] = {"move", "press", "back", "change", "on", "off", "tab", "screen", "denied"};

/* Which of two events wins a frame: the more specific. */
static int rank(int sound) {
  static const int kRank[BUI_EVENT_COUNT] = {1, 3, 7, 2, 6, 6, 5, 4, 8};
  return sound < 0 || sound >= BUI_EVENT_COUNT ? 0 : kRank[sound];
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
      case BUI_EVENT_MOVE: note(&c, 0, 35, 880, 880, SINE, 0.25f); break;
      case BUI_EVENT_PRESS: note(&c, 0, 70, 660, 700, SINE, 0.5f); break;
      case BUI_EVENT_BACK: note(&c, 0, 90, 620, 420, SINE, 0.45f); break;
      case BUI_EVENT_CHANGE: note(&c, 0, 30, 1040, 1040, SINE, 0.3f); break;
      case BUI_EVENT_ON: note(&c, note(&c, 0, 60, 523, 523, SINE, 0.45f), 80, 784, 784, SINE, 0.45f); break;
      case BUI_EVENT_OFF: note(&c, note(&c, 0, 60, 784, 784, SINE, 0.45f), 80, 523, 523, SINE, 0.45f); break;
      case BUI_EVENT_TAB: note(&c, 0, 80, 740, 990, SINE, 0.4f); break;
      case BUI_EVENT_SCREEN: note(&c, 0, 180, 440, 440, SINE, 0.3f), note(&c, 0, 180, 660, 660, SINE, 0.2f); break;
      case BUI_EVENT_DENIED: note(&c, 0, 140, 200, 170, SINE, 0.6f); break;
    }
  } else if (pack == BUI_SOUNDS_CRISP) {  /* clicks and square-wave ticks */
    switch (sound) {
      case BUI_EVENT_MOVE: note(&c, 0, 8, 0, 0, NOISE, 0.3f); break;
      case BUI_EVENT_PRESS: note(&c, note(&c, 0, 6, 0, 0, NOISE, 0.4f), 30, 1400, 1400, SQUARE, 0.25f); break;
      case BUI_EVENT_BACK: note(&c, 0, 50, 1000, 600, SQUARE, 0.25f); break;
      case BUI_EVENT_CHANGE: note(&c, 0, 12, 2000, 2000, SQUARE, 0.15f); break;
      case BUI_EVENT_ON: note(&c, 0, 50, 900, 1500, SQUARE, 0.25f); break;
      case BUI_EVENT_OFF: note(&c, 0, 50, 1500, 900, SQUARE, 0.25f); break;
      case BUI_EVENT_TAB: note(&c, note(&c, 0, 20, 1200, 1200, SQUARE, 0.2f), 25, 1600, 1600, SQUARE, 0.2f); break;
      case BUI_EVENT_SCREEN: note(&c, note(&c, 0, 15, 0, 0, NOISE, 0.3f), 90, 800, 1200, SQUARE, 0.2f); break;
      case BUI_EVENT_DENIED: note(&c, 0, 120, 150, 150, SQUARE, 0.35f); break;
    }
  }
  return c;
}

void bui__make_packs(bui_context* ui) {
  const int rate = bui__voice_rate(ui->voice);
  for (int p = 1; p < BUI_SOUND_PACK_COUNT; ++p)
    for (int s = 0; s < BUI_EVENT_COUNT; ++s) {
      const clip c = make(p, s, rate);
      ui->packs[p].pcm[s] = c.pcm, ui->packs[p].count[s] = c.count;
    }
}

void bui__free_packs(bui_context* ui) {
  for (int p = 0; p < BUI_SOUND_PACK_COUNT; ++p)
    for (int s = 0; s < BUI_EVENT_COUNT; ++s) free(ui->packs[p].pcm[s]), ui->packs[p].pcm[s] = NULL;
}

/* ---------------------------------------------------------- rumble packs */

/* Each event's pattern: {low motor, high motor, milliseconds} steps. */
static const bui_rumble kRumble[BUI_RUMBLE_PACK_COUNT][BUI_EVENT_COUNT] = {
    [BUI_RUMBLE_SOFT] =
        {
            /* Gentle, rounded pulses on the heavy motor. */
            [BUI_EVENT_MOVE] = {{{0.12f, 0.00f, 40}}, 1},
            [BUI_EVENT_PRESS] = {{{0.30f, 0.10f, 70}}, 1},
            [BUI_EVENT_BACK] = {{{0.25f, 0.00f, 60}, {0.12f, 0.00f, 50}}, 2},
            [BUI_EVENT_CHANGE] = {{{0.08f, 0.00f, 30}}, 1},
            [BUI_EVENT_ON] = {{{0.15f, 0.05f, 50}, {0.30f, 0.10f, 60}}, 2},
            [BUI_EVENT_OFF] = {{{0.30f, 0.10f, 60}, {0.15f, 0.05f, 50}}, 2},
            [BUI_EVENT_TAB] = {{{0.20f, 0.05f, 60}}, 1},
            [BUI_EVENT_SCREEN] = {{{0.20f, 0.05f, 60}, {0.35f, 0.10f, 90}}, 2},
            [BUI_EVENT_DENIED] = {{{0.45f, 0.00f, 90}, {0, 0, 60}, {0.45f, 0.00f, 90}}, 3},
        },
    [BUI_RUMBLE_CRISP] =
        {
            /* Short, sharp ticks on the light motor. */
            [BUI_EVENT_MOVE] = {{{0.00f, 0.20f, 15}}, 1},
            [BUI_EVENT_PRESS] = {{{0.10f, 0.50f, 25}}, 1},
            [BUI_EVENT_BACK] = {{{0.20f, 0.30f, 30}}, 1},
            [BUI_EVENT_CHANGE] = {{{0.00f, 0.15f, 10}}, 1},
            [BUI_EVENT_ON] = {{{0.00f, 0.40f, 20}, {0, 0, 30}, {0.00f, 0.40f, 20}}, 3},
            [BUI_EVENT_OFF] = {{{0.10f, 0.30f, 25}}, 1},
            [BUI_EVENT_TAB] = {{{0.00f, 0.35f, 20}, {0, 0, 20}, {0.00f, 0.35f, 20}}, 3},
            [BUI_EVENT_SCREEN] = {{{0.20f, 0.45f, 40}}, 1},
            [BUI_EVENT_DENIED] = {{{0.40f, 0.40f, 30}, {0, 0, 30}, {0.40f, 0.40f, 30}, {0, 0, 30}}, 4},
        },
};

/* -------------------------------------------------------------- playing */

void bui__auto_event(bui_context* ui, bui_event sound) {
  if (!ui->event_requested && rank((int)sound) > rank(ui->pending_event)) ui->pending_event = (int)sound;
}

void bui_trigger(bui_context* ui, bui_event sound) {
  if (sound < 0 || sound >= BUI_EVENT_COUNT) return;
  ui->pending_event = (int)sound, ui->event_requested = true;
}

/* The frame's rumble: the event's pattern in the theme's pack, scaled by
 * its strength (a controller in use only). */
static void rumble(bui_context* ui, int event) {
  const int pack = (int)ui->theme.rumble_pack;
  const float strength = ui->theme.rumble_strength > 1.0f ? 1.0f : ui->theme.rumble_strength;
  if (ui->in.device != BUI_DEVICE_GAMEPAD || pack <= BUI_RUMBLE_NONE || pack >= BUI_RUMBLE_PACK_COUNT || strength <= 0.0f) return;
  const bui_rumble* r = &kRumble[pack][event];
  if (r->step_count <= 0) return;
  bui_rumble out = *r;
  if (out.step_count > BUI_RUMBLE_MAX_STEPS) out.step_count = BUI_RUMBLE_MAX_STEPS;
  for (int k = 0; k < out.step_count; ++k) {
    bui_rumble_step* s = &out.steps[k];
    s->low = fminf(1.0f, fmaxf(0.0f, s->low * strength)), s->high = fminf(1.0f, fmaxf(0.0f, s->high * strength));
    s->ms = fmaxf(0.0f, s->ms);
  }
  ui->frame_rumble = out, ui->has_frame_rumble = true;
}

void bui__emit_event(bui_context* ui) {
  const int sound = ui->pending_event, pack = (int)ui->theme.sound_pack;
  ui->pending_event = -1, ui->event_requested = false;
  ui->has_frame_event = sound >= 0, ui->has_frame_rumble = false;
  if (sound < 0) return;
  ui->frame_event = (bui_event)sound;
  rumble(ui, sound);
  if (pack <= BUI_SOUNDS_NONE || pack >= BUI_SOUND_PACK_COUNT) return;
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
