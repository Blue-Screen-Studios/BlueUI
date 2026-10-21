/* BlueUI demo.
 *
 * One page per feature. Each page opens with a short explanation (an item
 * of its own, so the narrator reads it) followed by examples to try. Pick
 * a page from the list on the right, or with Tab, Shift+Tab, the bumpers
 * or the number keys.
 *
 * SDL3 belongs to the demo alone: it turns SDL's events into a bui_input,
 * draws BlueUI's geometry with SDL's renderer, and plays BlueUI's WAVs
 * through an SDL audio stream. */

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "blueui/widgets.h"

#define RATE 48000

/* ---------------------------------------------------------------- input */

typedef struct dpad_repeat {
  bui_key key;
  bool held;
  double next;
} dpad_repeat;

static bui_input input;
static bool click;
static dpad_repeat dpad[4];

/* Raw codes reported for rebinding: keyboard keys as SDL keycodes, mouse
 * and controller buttons offset into ranges of their own. */
#define MOUSE_CODE 0x40000000LL
#define PAD_CODE 0x50000000LL

static void key_down(SDL_Keycode k, bool shift) {
  switch (k) {
    case SDLK_UP: bui_input_key(&input, BUI_KEY_UP); break;
    case SDLK_DOWN: bui_input_key(&input, BUI_KEY_DOWN); break;
    case SDLK_LEFT: bui_input_key(&input, BUI_KEY_LEFT); break;
    case SDLK_RIGHT: bui_input_key(&input, BUI_KEY_RIGHT); break;
    case SDLK_TAB: bui_input_key(&input, shift ? BUI_KEY_PREV : BUI_KEY_NEXT); break;
    case SDLK_RETURN:
    case SDLK_SPACE: bui_input_key(&input, BUI_KEY_ACTIVATE); break;
    case SDLK_ESCAPE:
      bui_input_key(&input, BUI_KEY_BACK);
      bui_input_key(&input, BUI_KEY_CANCEL_CAPTURE);
      break;
    case SDLK_N: bui_input_key(&input, BUI_KEY_TOGGLE_SPEECH); break;
    case SDLK_R: bui_input_key(&input, BUI_KEY_REPEAT); break;
    default:
      if (k >= SDLK_1 && k <= SDLK_9) input.tab_hotkey = (int)(k - SDLK_1) + 1;
  }
  bui_input_pressed(&input, (int64_t)k);
}

static void pad_button(Uint8 b, bool down, double now) {
  static const bui_key dirs[4] = {BUI_KEY_UP, BUI_KEY_DOWN, BUI_KEY_LEFT, BUI_KEY_RIGHT};
  static const Uint8 dir_buttons[4] = {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
                                       SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
  input.device = BUI_DEVICE_GAMEPAD;
  for (int k = 0; k < 4; ++k)
    if (b == dir_buttons[k]) {
      dpad[k].key = dirs[k], dpad[k].held = down, dpad[k].next = now + 0.4;
      if (down) bui_input_key(&input, dirs[k]);
      return;
    }
  if (!down) return;
  switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: bui_input_key(&input, BUI_KEY_ACTIVATE); break;
    case SDL_GAMEPAD_BUTTON_EAST: bui_input_key(&input, BUI_KEY_BACK); break;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: bui_input_key(&input, BUI_KEY_TAB_PREV); break;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: bui_input_key(&input, BUI_KEY_TAB_NEXT); break;
    case SDL_GAMEPAD_BUTTON_BACK: bui_input_key(&input, BUI_KEY_TOGGLE_SPEECH); break;
    case SDL_GAMEPAD_BUTTON_START: bui_input_key(&input, BUI_KEY_CANCEL_CAPTURE); break;
    default: break;
  }
  bui_input_pressed(&input, PAD_CODE + b);
}

static const char* code_name(bui_context* ui, int64_t code) {
  if (code == 0) return "None";
  if (code >= PAD_CODE) return bui_format(ui, "Pad %s", SDL_GetGamepadStringForButton((SDL_GamepadButton)(code - PAD_CODE)));
  if (code >= MOUSE_CODE) return bui_format(ui, "Mouse %d", (int)(code - MOUSE_CODE));
  return bui_format(ui, "%s", SDL_GetKeyName((SDL_Keycode)code));
}

/* --------------------------------------------------------------- output */

static Uint8 byte(float v, bool srgb) {
  v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
  return (Uint8)lroundf((srgb ? powf(v, 1.0f / 2.2f) : v) * 255.0f);
}

static void render(SDL_Renderer* r, const bui_draw_cmd* cmds, size_t n) {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (size_t i = 0; i < n; ++i) {
    const bui_draw_cmd* c = &cmds[i];
    SDL_SetRenderDrawColor(r, byte(c->r, true), byte(c->g, true), byte(c->b, true), byte(c->a, false));
    const SDL_FRect f = {c->x0, c->y0, c->x1 - c->x0, c->y1 - c->y0};
    SDL_RenderFillRect(r, &f);
  }
}

/* BlueUI's audio events: speech into one stream (PLAY and STOP clear what's
 * queued), GUI sounds into another, so they never cut speech off. */
static void play(SDL_AudioStream* speech, SDL_AudioStream* sounds, bui_context* ui) {
  bui_audio a;
  while (bui_next_audio(ui, &a)) {
    if (a.op == BUI_AUDIO_SOUND) {
      if (sounds) SDL_ClearAudioStream(sounds), SDL_PutAudioStreamData(sounds, a.samples, (int)(a.sample_count * sizeof(int16_t)));
    } else {
      if (speech && (a.op == BUI_AUDIO_STOP || a.op == BUI_AUDIO_PLAY)) SDL_ClearAudioStream(speech);
      if (speech && a.samples) SDL_PutAudioStreamData(speech, a.samples, (int)(a.sample_count * sizeof(int16_t)));
      if (a.text && !a.samples) printf("(no voice) %s\n", a.text);
    }
    bui_audio_free(&a);
  }
}

/* --------------------------------------------------------------- themes */

static bui_theme green_theme(void) {
  bui_theme t = bui_default_theme();
  t.text = bui_rgb(0.92f, 0.94f, 0.88f), t.dim = bui_rgb(0.6f, 0.72f, 0.6f), t.blurb = bui_rgb(0.55f, 0.64f, 0.55f);
  t.value = bui_rgb(0.92f, 0.85f, 0.4f), t.off = bui_rgb(0.4f, 0.46f, 0.4f);
  t.hover = bui_rgba(0.3f, 0.6f, 0.3f, 0.12f), t.rule = bui_rgb(0.16f, 0.4f, 0.14f);
  t.edge = bui_rgb(0.38f, 0.5f, 0.3f), t.panel = bui_rgba(0.025f, 0.06f, 0.035f, 0.97f), t.title_bar = bui_rgb(0.09f, 0.26f, 0.1f);
  t.title_right = bui_rgb(0.6f, 0.9f, 0.45f);
  t.tab_on = bui_rgb(0.16f, 0.4f, 0.14f), t.tab_off = bui_rgb(0.06f, 0.12f, 0.06f), t.tab_hot = bui_rgb(0.11f, 0.2f, 0.1f);
  t.tab_text_off = bui_rgb(0.62f, 0.75f, 0.6f);
  t.toggle_on = bui_rgb(0.12f, 0.42f, 0.15f), t.toggle_on_hot = bui_rgb(0.18f, 0.56f, 0.2f);
  t.well = bui_rgb(0.04f, 0.09f, 0.04f), t.arrow = bui_rgb(0.12f, 0.36f, 0.13f), t.arrow_hot = bui_rgb(0.18f, 0.5f, 0.18f);
  t.choice_text = bui_rgb(1.0f, 0.95f, 0.8f);
  t.track = bui_rgb(0.1f, 0.14f, 0.1f), t.fill = bui_rgb(0.3f, 0.7f, 0.22f), t.knob = bui_rgb(0.92f, 0.92f, 0.7f), t.tick = bui_rgb(0.45f, 0.6f, 0.42f);
  t.button_edge = bui_rgb(0.55f, 0.48f, 0.12f), t.button = bui_rgba(0.06f, 0.14f, 0.06f, 0.95f);
  t.button_hot = bui_rgba(0.16f, 0.3f, 0.12f, 0.95f), t.button_text = bui_rgb(1.0f, 0.95f, 0.8f);
  return t;
}

static bui_theme red_theme(void) {
  bui_theme t = bui_default_theme();
  t.text = bui_rgb(0.98f, 0.92f, 0.86f), t.dim = bui_rgb(0.80f, 0.62f, 0.50f), t.blurb = bui_rgb(0.72f, 0.56f, 0.48f);
  t.value = bui_rgb(1.0f, 0.70f, 0.30f), t.off = bui_rgb(0.48f, 0.40f, 0.36f);
  t.hover = bui_rgba(0.9f, 0.4f, 0.2f, 0.14f), t.rule = bui_rgb(0.55f, 0.20f, 0.08f);
  t.edge = bui_rgb(0.75f, 0.35f, 0.15f), t.panel = bui_rgba(0.08f, 0.03f, 0.02f, 0.97f), t.title_bar = bui_rgb(0.40f, 0.10f, 0.04f);
  t.title_right = bui_rgb(1.0f, 0.70f, 0.30f);
  t.tab_on = bui_rgb(0.60f, 0.18f, 0.06f), t.tab_off = bui_rgb(0.14f, 0.06f, 0.04f), t.tab_hot = bui_rgb(0.28f, 0.10f, 0.06f);
  t.tab_text_off = bui_rgb(0.85f, 0.65f, 0.55f);
  t.toggle_on = bui_rgb(0.70f, 0.25f, 0.08f), t.toggle_on_hot = bui_rgb(0.85f, 0.35f, 0.12f);
  t.well = bui_rgb(0.12f, 0.05f, 0.03f), t.arrow = bui_rgb(0.55f, 0.18f, 0.06f), t.arrow_hot = bui_rgb(0.75f, 0.28f, 0.10f);
  t.choice_text = bui_rgb(1.0f, 0.90f, 0.80f);
  t.track = bui_rgb(0.18f, 0.09f, 0.06f), t.fill = bui_rgb(1.0f, 0.45f, 0.15f), t.knob = bui_rgb(1.0f, 0.92f, 0.80f), t.tick = bui_rgb(0.70f, 0.45f, 0.35f);
  t.button_edge = bui_rgb(1.0f, 0.55f, 0.20f), t.button = bui_rgba(0.20f, 0.07f, 0.04f, 0.95f);
  t.button_hot = bui_rgba(0.42f, 0.14f, 0.06f, 0.95f), t.button_text = bui_rgb(1.0f, 0.95f, 0.88f);
  t.focus = bui_rgb(0.4f, 0.9f, 1.0f);
  return t;
}

static bui_theme contrast_theme(void) {
  bui_theme t = bui_default_theme();
  const bui_color black = bui_rgb(0, 0, 0), white = bui_rgb(1, 1, 1), yellow = bui_rgb(1.0f, 0.9f, 0.0f);
  t.text = white, t.dim = white, t.blurb = bui_rgb(0.85f, 0.85f, 0.85f), t.value = yellow, t.off = bui_rgb(0.55f, 0.55f, 0.55f);
  t.stripe = bui_rgba(1, 1, 1, 0.08f), t.hover = bui_rgba(1, 1, 1, 0.2f), t.rule = white;
  t.edge = white, t.panel = bui_rgba(0, 0, 0, 1), t.title_bar = black, t.title = yellow, t.title_right = white;
  t.tab_on = white, t.tab_off = black, t.tab_hot = bui_rgb(0.3f, 0.3f, 0.3f), t.tab_text = black, t.tab_text_off = white;
  t.toggle_on = yellow, t.toggle_on_hot = bui_rgb(1, 1, 0.5f), t.toggle_off = bui_rgb(0.25f, 0.25f, 0.25f);
  t.toggle_off_hot = bui_rgb(0.4f, 0.4f, 0.4f), t.toggle_text_on = black, t.toggle_text_off = white;
  t.well = black, t.arrow = bui_rgb(0.3f, 0.3f, 0.3f), t.arrow_hot = bui_rgb(0.5f, 0.5f, 0.5f), t.choice_text = yellow;
  t.track = bui_rgb(0.3f, 0.3f, 0.3f), t.fill = yellow, t.knob = white, t.knob_hot = yellow, t.tick = white;
  t.button_edge = white, t.button = black, t.button_hot = bui_rgb(0.3f, 0.3f, 0.3f), t.button_text = white;
  t.focus = bui_rgb(0.0f, 1.0f, 1.0f), t.focus_width = 2.0f;
  return t;
}

/* ---------------------------------------------------------------- state */

enum {
  PAGE_BUTTONS,
  PAGE_TOGGLES,
  PAGE_CHOICES,
  PAGE_SLIDERS,
  PAGE_TABS,
  PAGE_FOLDOUTS,
  PAGE_SCROLLING,
  PAGE_CUSTOM,
  PAGE_REBINDING,
  PAGE_SPEECH,
  PAGE_SOUNDS,
  PAGE_THEMES,
  PAGE_COUNT
};
static const char* const kPages[PAGE_COUNT] = {"Buttons", "Toggles", "Choices", "Sliders", "Tabs", "Foldouts",
                                               "Scrolling", "Custom items", "Rebinding", "Speech", "Sounds", "Themes"};

typedef struct demo {
  bool running;
  int page;
  int presses;
  bool toggle, labelled, dependent;
  int choice, choice_b;
  float slider, spoken_slider;
  int stops;
  int example_tab;
  bool fold_a, fold_b, inner_a, inner_b;
  int swatch, stepper;
  int64_t binding[3];
  int theme, size;
  int sounds;  /* the theme's sound pack: a bui_sound_pack */
} demo;

/* Rows down a page, from (x0, y) to x1, in font pixels times `p`. */
typedef struct layout {
  float x0, x1, y, p;
} layout;

static bui_rect row(layout* l, float h) {
  const bui_rect r = bui_rect_make(l->x0, l->y, l->x1, l->y + (h - 2.0f) * l->p);
  l->y += h * l->p;
  return r;
}

static void gap(layout* l, float h) { l->y += h * l->p; }

/* A heading over a group of examples (drawn only: not an item). */
static void heading(bui_context* ui, layout* l, const char* text) {
  gap(l, 6.0f);
  if (bui_visible(ui, bui_rect_make(l->x0, l->y, l->x1, l->y + 9.0f * l->p)))
    bui_label(ui, text, l->x0, l->y, bui_get_theme(ui)->dim, 0.0f, 0.0f);
  gap(l, 12.0f);
}

/* A page's explanation: wrapped to the page's width and declared as a text
 * item, so it is reachable with the keys and read aloud. */
static void explain(bui_context* ui, layout* l, const char* id, const char* text) {
  const bui_theme* t = bui_get_theme(ui);
  const float p = l->p, line_h = BUI_LINE_ADVANCE * p, width = l->x1 - l->x0 - 6.0f * p;
  const float top = l->y;
  float y = top + 3.0f * p;
  const char* line = text;
  while (*line) {
    /* The longest run of whole words that fits. */
    const char* end = line;
    const char* fit = line;
    while (*end) {
      const char* word = end;
      while (*word == ' ') ++word;
      const char* next = word;
      while (*next && *next != ' ') ++next;
      if (bui_text_width(line, (size_t)(next - line), p) > width && fit != line) break;
      fit = end = next;
    }
    if (fit == line) fit = line + strlen(line);
    if (bui_visible(ui, bui_rect_make(l->x0, y, l->x1, y + line_h)))
      bui_label(ui, bui_format(ui, "%.*s", (int)(fit - line), line), l->x0 + 3.0f * p, y, t->blurb, 0.0f, 0.0f);
    y += line_h;
    line = fit;
    while (*line == ' ') ++line;
  }
  bui_item it = {0};
  it.kind = BUI_TEXT, it.rect = bui_rect_make(l->x0, top, l->x1, y + 2.0f * p), it.spoken = text;
  bui_declare(ui, id, &it);
  l->y = y + 6.0f * p;
}

/* A small note under an example, drawn only. */
static void note(bui_context* ui, layout* l, const char* text) {
  if (bui_visible(ui, bui_rect_make(l->x0, l->y, l->x1, l->y + 9.0f * l->p)))
    bui_label(ui, text, l->x0 + 3.0f * l->p, l->y, bui_get_theme(ui)->value, 0.0f, 0.0f);
  gap(l, 12.0f);
}

static void percent(float v, char* out, size_t size, void* user) {
  (void)user;
  snprintf(out, size, "%d%%", (int)lroundf(v * 100.0f));
}
static void times(float v, char* out, size_t size, void* user) {
  (void)user;
  snprintf(out, size, "%.1fX", v);
}
static void times_spoken(float v, char* out, size_t size, void* user) {
  (void)user;
  snprintf(out, size, "Example slider with its own words, %.1f times", v);
}

/* ---------------------------------------------------------------- pages */

static void page_buttons(bui_context* ui, demo* d, layout* l) {
  explain(ui, l, "about",
          "Buttons run an action. Click one, or move the focus to it with the arrow keys or D-pad and press Enter, Space or A. "
          "A disabled button can still be reached and heard, but not pressed. A back button is also pressed by Escape or B.");
  const float p = l->p, w = 110.0f * p;
  heading(ui, l, "EXAMPLES");
  const bui_rect r = row(l, 18.0f);
  if (bui_button(ui, "try", "TRY ME!", bui_rect_make(r.x0, r.y0, r.x0 + w, r.y1), NULL)) {
    ++d->presses;
    bui_announce(ui, bui_format(ui, "Pressed %d %s", d->presses, d->presses == 1 ? "time" : "times"), BUI_SAY_NORMAL);
  }
  bui_button_opts off = {0};
  off.disabled = true;
  off.spoken = "Disabled example. It can be reached and heard, but not pressed";
  bui_button(ui, "disabled", "DISABLED", bui_rect_make(r.x0 + w + 8 * p, r.y0, r.x0 + 2 * w + 8 * p, r.y1), &off);
  bui_button_opts back = {0};
  back.back = true;
  back.spoken = "Back button example. Escape or B presses it too";
  if (bui_button(ui, "back", "BACK (ESC)", bui_rect_make(r.x0 + 2 * w + 16 * p, r.y0, r.x0 + 3 * w + 16 * p, r.y1), &back))
    bui_announce(ui, "Back pressed", BUI_SAY_NORMAL);
  gap(l, 4.0f);
  note(ui, l, bui_format(ui, "TRY ME! PRESSED %d %s", d->presses, d->presses == 1 ? "TIME" : "TIMES"));
}

static void page_toggles(bui_context* ui, demo* d, layout* l) {
  explain(ui, l, "about",
          "A toggle switches a setting on or off. Press it, or use left and right. Each state can have its own label and "
          "description. A row can be disabled while another setting is off; it is still read out, with the reason.");
  heading(ui, l, "EXAMPLES");
  const bui_field plain = {"Example toggle", "A plain on and off switch"};
  bui_toggle_row(ui, "toggle", row(l, 18.0f), &plain, &d->toggle, NULL, NULL);
  const bui_field labelled = {"Example labelled toggle"};
  const bui_option left = {"LEFT", "The left state's own description"}, right = {"RIGHT", "The right state's own description"};
  bui_toggle_row_ex(ui, "labelled", row(l, 18.0f), &labelled, &d->labelled, &left, &right);
  const bui_field dependent = {"Example dependent toggle", d->toggle ? "Enabled by the first toggle" : "Turn on the first toggle first", NULL,
                               !d->toggle};
  bui_toggle_row(ui, "dependent", row(l, 18.0f), &dependent, &d->dependent, NULL, NULL);
}

static void page_choices(bui_context* ui, demo* d, layout* l) {
  explain(ui, l, "about",
          "A choice picks one option from a list. Use left and right, click the arrows, or press it to go to the next option. "
          "The chosen option's description is shown beside it and read out.");
  heading(ui, l, "EXAMPLES");
  const bui_field f = {"Example choice"};
  const bui_option options[] = {{"OPTION A", "The first option"}, {"OPTION B", "The second option"}, {"OPTION C", "The third option"}};
  bui_choice_row(ui, "choice", row(l, 18.0f), &f, &d->choice, options, 3);
  const bui_field g = {"Example with spoken text"};
  const bui_option spoken[] = {{"SHORT", "Short label", "Example with spoken text: short. Options can say more than they show"},
                               {"LONG", "Long label", "Example with spoken text: long. This one has its own words too"}};
  bui_choice_row(ui, "choice_b", row(l, 18.0f), &g, &d->choice_b, spoken, 2);
}

static void page_sliders(bui_context* ui, demo* d, layout* l) {
  explain(ui, l, "about",
          "A slider sets a value in a range. Drag it, or use left and right to step. The value is shown and read out in your "
          "own format. A stops slider has a fixed number of marked positions.");
  heading(ui, l, "EXAMPLES");
  const bui_field a = {"Example slider", "Steps of 5%"};
  bui_slider_row(ui, "slider", row(l, 18.0f), &a, &d->slider, 0.0f, 1.0f, 0.05f, percent, NULL, NULL);
  const bui_field b = {"Example slider with its own words", "Read out in a sentence"};
  bui_slider_row(ui, "spoken_slider", row(l, 18.0f), &b, &d->spoken_slider, 0.5f, 3.0f, 0.1f, times, times_spoken, NULL);
  const bui_field c = {"Example stops", "Four marked positions"};
  const bui_option stops[] = {{"LOW"}, {"MEDIUM"}, {"HIGH"}, {"MAX"}};
  bui_stops_row(ui, "stops", row(l, 18.0f), &c, &d->stops, stops, 4);
}

static void page_tabs(bui_context* ui, demo* d, layout* l) {
  explain(ui, l, "about",
          "A tab bar splits a screen into pages. Click a tab, or use Tab and Shift+Tab, the bumpers, or the number keys. "
          "On this page those keys switch the example tabs below; use the page list to leave.");
  heading(ui, l, "EXAMPLE");
  static const char* const tabs[] = {"First", "Second", "Third"};
  const bui_rect bar = row(l, 16.0f);
  if (bui_tab_bar(ui, "example_tabs", bar, tabs, 3, &d->example_tab, 0.0f))
    bui_announce(ui, bui_format(ui, "%s tab", tabs[d->example_tab]), BUI_SAY_NORMAL);
  gap(l, 4.0f);
  explain(ui, l, bui_format(ui, "tab.%d", d->example_tab),
          bui_format(ui, "This is the %s tab's content. Each tab shows its own items here.", tabs[d->example_tab]));
}

static void page_foldouts(bui_context* ui, demo* d, layout* l) {
  explain(ui, l, "about",
          "A foldout is a heading that shows or hides a group of rows. Press it to open or close it. Hidden rows are skipped "
          "by the keys and not read out.");
  heading(ui, l, "EXAMPLES");
  const bui_field a = {"Example foldout", "Two rows inside"};
  bui_foldout_row(ui, "fold_a", row(l, 18.0f), &a, &d->fold_a);
  if (d->fold_a) {
    const bui_field x = {"Inside the foldout", "A toggle in the group"};
    bui_toggle_row(ui, "inner_a", row(l, 18.0f), &x, &d->inner_a, NULL, NULL);
    const bui_field y = {"Also inside", "Another one"};
    bui_toggle_row(ui, "inner_b", row(l, 18.0f), &y, &d->inner_b, NULL, NULL);
  }
  gap(l, 4.0f);
  const bui_field b = {"Second example foldout", "Nothing inside"};
  bui_foldout_row(ui, "fold_b", row(l, 18.0f), &b, &d->fold_b);
  if (d->fold_b) note(ui, l, "(EMPTY)");
}

static void page_scrolling(bui_context* ui, layout* l, float bottom) {
  explain(ui, l, "about",
          "A scroll view shows part of a longer list. Scroll with the mouse wheel; moving the focus with the keys scrolls the view "
          "to keep the focused row in sight. Rows outside the view cannot be clicked.");
  heading(ui, l, "EXAMPLE");
  const bui_theme* t = bui_get_theme(ui);
  const float p = l->p, row_h = 14.0f * p;
  const bui_rect view = bui_rect_make(l->x0, l->y, l->x1, bottom);
  bui_fill(ui, view, t->well);
  const int count = 30;
  const float offset = bui_begin_scroll(ui, "list", view, (float)count * row_h);
  for (int k = 0; k < count; ++k) {
    const bui_rect r = bui_rect_make(view.x0, view.y0 + (float)k * row_h - offset, view.x1 - 6 * p, view.y0 + (float)(k + 1) * row_h - offset);
    bui_item it = {0};
    it.rect = r, it.spoken = bui_format(ui, "Example row %d", k + 1);
    const bui_result res = bui_declare(ui, bui_format(ui, "row.%d", k), &it);
    if (res.activated) bui_announce(ui, bui_format(ui, "Row %d pressed", k + 1), BUI_SAY_NORMAL);
    const bool even = bui_row_stripe(ui) % 2 == 0;
    if (!bui_visible(ui, r)) continue;
    if (even) bui_fill(ui, r, t->stripe);
    if (res.hovered) bui_fill(ui, r, t->hover);
    bui_label(ui, bui_format(ui, "EXAMPLE ROW %d", k + 1), r.x0 + 4 * p, roundf(0.5f * (r.y0 + r.y1 - BUI_GLYPH_H * p)), t->text, 0.0f, 0.0f);
  }
  bui_end_scroll(ui);
  const float content = (float)count * row_h, h = bui_rect_h(view), at = bui_scroll_of(ui, "list") / content;
  bui_fill(ui, bui_rect_make(view.x1 - 3 * p, view.y0, view.x1 - p, view.y1), t->track);
  bui_fill(ui, bui_rect_make(view.x1 - 3 * p, view.y0 + at * h, view.x1 - p, view.y0 + (at + h / content) * h), t->fill);
}

static void page_custom(bui_context* ui, demo* d, layout* l) {
  explain(ui, l, "about",
          "Any rectangle can become an item with bui_declare. The game draws it however it likes, and BlueUI handles focus, "
          "clicks, stepping and speech. Below: a row of swatches with a role of their own, and a stepper with arrow buttons.");
  const bui_theme* t = bui_get_theme(ui);
  const float p = l->p;
  heading(ui, l, "EXAMPLE SWATCHES");
  static const char* const names[] = {"Red", "Yellow", "Green", "Blue", "Purple"};
  static const float colors[][3] = {{0.8f, 0.1f, 0.1f}, {0.95f, 0.85f, 0.1f}, {0.15f, 0.7f, 0.2f}, {0.15f, 0.35f, 0.9f}, {0.5f, 0.2f, 0.8f}};
  const bui_rect strip = row(l, 26.0f);
  const float s = 22.0f * p;
  for (int k = 0; k < 5; ++k) {
    const bui_rect sw = bui_rect_make(strip.x0 + (float)k * (s + 8 * p), strip.y0, strip.x0 + (float)k * (s + 8 * p) + s, strip.y0 + s);
    bui_item it = {0};
    it.kind = BUI_CUSTOM, it.role = "swatch", it.rect = sw;
    it.spoken = bui_format(ui, "%s%s", names[k], d->swatch == k ? ", selected" : "");
    if (bui_declare(ui, bui_format(ui, "swatch.%d", k), &it).activated) {
      d->swatch = k;
      bui_announce(ui, bui_format(ui, "%s selected", names[k]), BUI_SAY_NORMAL);
    }
    if (d->swatch == k) bui_fill(ui, bui_rect_grow(sw, 2 * p), t->text);
    bui_fill(ui, sw, bui_rgb(colors[k][0], colors[k][1], colors[k][2]));
  }
  heading(ui, l, "EXAMPLE STEPPER");
  const bui_rect r = row(l, 18.0f);
  const float aw = 14 * p, w = 120 * p;
  const bui_rect prev = bui_rect_make(r.x0, r.y0, r.x0 + aw, r.y1), next = bui_rect_make(r.x0 + w - aw, r.y0, r.x0 + w, r.y1);
  bui_item it = {0};
  it.kind = BUI_CHOICE, it.rect = bui_rect_make(r.x0, r.y0, r.x0 + w, r.y1), it.prev = prev, it.next = next;
  it.value = bui_format(ui, "%d", d->stepper);
  it.spoken = bui_format(ui, "Example stepper, %d of 10. Left and right change it", d->stepper);
  it.changed = bui_format(ui, "%d", d->stepper);
  const bui_result res = bui_declare(ui, "stepper", &it);
  const int step = res.step ? res.step : res.activated ? 1 : 0;
  if (step) {
    d->stepper = d->stepper + step < 1 ? 10 : d->stepper + step > 10 ? 1 : d->stepper + step;
    bui_revise(ui, bui_format(ui, "%d", d->stepper), bui_format(ui, "%d", d->stepper));
  }
  const bui_vec2 mouse = bui_get_input(ui)->mouse;
  bui_fill(ui, it.rect, t->well);
  bui_fill(ui, prev, bui_rect_contains(prev, mouse) ? t->arrow_hot : t->arrow);
  bui_fill(ui, next, bui_rect_contains(next, mouse) ? t->arrow_hot : t->arrow);
  bui_centered(ui, prev, "-", t->choice_text, 0.0f);
  bui_centered(ui, next, "+", t->choice_text, 0.0f);
  bui_centered(ui, bui_rect_make(prev.x1, r.y0, next.x0, r.y1), bui_format(ui, "%d", d->stepper), t->choice_text, 0.0f);
}

static void page_rebinding(bui_context* ui, demo* d, layout* l) {
  explain(ui, l, "about",
          "A capture slot waits for the next key, mouse button or controller button and reports it to the game. Press a slot, "
          "then press anything. Escape or Start cancels, and it gives up after 8 seconds.");
  const bui_theme* t = bui_get_theme(ui);
  const float p = l->p;
  heading(ui, l, "EXAMPLES");
  for (int k = 0; k < 3; ++k) {
    const bui_rect r = row(l, 16.0f);
    const bui_rect slot = bui_rect_make(r.x1 - 130 * p, r.y0 + p, r.x1 - 4 * p, r.y1 - p);
    const char* bound = code_name(ui, d->binding[k]);
    bui_item it = {0};
    it.rect = slot, it.value = bound, it.spoken = bui_format(ui, "Example action %d, %s", k + 1, bound);
    const bui_captured c =
        bui_capture(ui, bui_format(ui, "bind.%d", k), &it, bui_format(ui, "Press anything for example action %d. Escape to cancel", k + 1), 8.0);
    if (c.has_code) {
      d->binding[k] = c.code;
      bui_announce(ui, bui_format(ui, "Example action %d: %s", k + 1, code_name(ui, c.code)), BUI_SAY_NORMAL);
    }
    const bool even = bui_row_stripe(ui) % 2 == 0;
    if (even) bui_fill(ui, r, t->stripe);
    bui_label(ui, bui_format(ui, "EXAMPLE ACTION %d", k + 1), r.x0 + 3 * p, roundf(0.5f * (r.y0 + r.y1 - BUI_GLYPH_H * p)), t->text, 0.0f, 0.0f);
    const bool hot = !bui_navigating(ui) && bui_rect_contains(slot, bui_get_input(ui)->mouse);
    bui_fill(ui, slot, c.waiting ? t->toggle_on : hot ? t->button_hot : t->button);
    bui_centered(ui, slot, c.waiting ? "PRESS ANYTHING" : bound, t->button_text, 0.0f);
  }
}

static void page_speech(bui_context* ui, layout* l) {
  explain(ui, l, "about",
          "Everything is read out automatically. A game can also announce its own lines with a priority: normal joins the "
          "current line, interrupt cuts in, queue waits its turn, and hold makes the screen wait until it finishes. "
          "N or the View button switches the narrator, and R repeats the last line.");
  const float p = l->p, w = 96.0f * p;
  heading(ui, l, "EXAMPLE ANNOUNCEMENTS");
  static const char* const labels[] = {"NORMAL", "INTERRUPT", "QUEUE", "HOLD"};
  static const char* const lines[] = {"A normal announcement, after what the screen says", "An interrupting announcement",
                                      "A queued announcement, after the current line", "A held announcement. The screen waits for it"};
  static const bui_priority priorities[] = {BUI_SAY_NORMAL, BUI_SAY_INTERRUPT, BUI_SAY_QUEUE, BUI_SAY_HOLD};
  const bui_rect r = row(l, 18.0f);
  for (int k = 0; k < 4; ++k) {
    bui_button_opts o = {0};
    o.spoken = bui_format(ui, "Try a %s announcement", labels[k]);
    const bui_rect b = bui_rect_make(r.x0 + (float)k * (w + 8 * p), r.y0, r.x0 + (float)k * (w + 8 * p) + w, r.y1);
    if (bui_button(ui, bui_format(ui, "say.%d", k), labels[k], b, &o)) bui_announce(ui, lines[k], priorities[k]);
  }
  heading(ui, l, "NARRATOR");
  bool on = bui_speech_enabled(ui);
  const bui_field narrator = {"Narrator", "Reads everything out"};
  if (bui_toggle_row(ui, "narrator", row(l, 18.0f), &narrator, &on, NULL, NULL)) bui_set_speech_enabled(ui, on, true);
  const bui_field repeat = {"Repeat", "Say the last line again"};
  if (bui_button_row(ui, "repeat", row(l, 18.0f), &repeat, "REPEAT")) bui_announce(ui, bui_get_state(ui)->last_line, BUI_SAY_INTERRUPT);
  const bui_field stop = {"Stop", "Stop speaking now"};
  if (bui_button_row(ui, "stop", row(l, 18.0f), &stop, "STOP")) bui_stop_speech(ui);
  const bui_field always = {"Always speak", "Spoken even with the narrator off"};
  if (bui_button_row(ui, "always", row(l, 18.0f), &always, "SAY")) bui_say(ui, "This line is spoken even with the narrator off", false);
}

static void page_sounds(bui_context* ui, demo* d, layout* l) {
  explain(ui, l, "about",
          "GUI sounds mark what happens: the focus moving, a press, a toggle switching, a value changing, a new tab or screen, "
          "and pressing something disabled. A game picks a sound pack in its theme: none, soft or crisp. The packs are built "
          "into BlueUI and made in code, so no sound files are needed.");
  heading(ui, l, "EXAMPLE PACKS");
  const bui_field pack = {"Example sound pack"};
  const bui_option packs[] = {{"NONE", "No GUI sounds"}, {"SOFT", "Rounded tones"}, {"CRISP", "Clicks and ticks"}};
  bui_choice_row(ui, "pack", row(l, 18.0f), &pack, &d->sounds, packs, 3);
  heading(ui, l, "TRY EACH SOUND");
  static const char* const names[] = {"MOVE", "PRESS", "BACK", "CHANGE", "ON", "OFF", "TAB", "SCREEN", "DENIED"};
  const float p = l->p, w = 64.0f * p;
  for (int s = 0; s < BUI_SOUND_COUNT; ++s) {
    if (s % 5 == 0) row(l, 18.0f);
    const float x = l->x0 + (float)(s % 5) * (w + 6 * p), y = l->y - 18.0f * p;
    bui_button_opts o = {0};
    o.spoken = bui_format(ui, "Play the %s sound", names[s]);
    if (bui_button(ui, bui_format(ui, "sound.%d", s), names[s], bui_rect_make(x, y, x + w, y + 16.0f * p), &o)) bui_play_sound(ui, (bui_sound)s);
  }
}

static void page_themes(bui_context* ui, demo* d, layout* l) {
  explain(ui, l, "about",
          "A theme sets every colour and size BlueUI draws, including the focus outline. A game passes its own theme once and "
          "every widget follows it. Try the examples below; blue is BlueUI's default.");
  heading(ui, l, "EXAMPLES");
  const bui_field theme = {"Example theme"};
  const bui_option themes[] = {{"BLUE", "The default"}, {"GREEN", "Greens and gold"}, {"RED", "Warm reds"},
                               {"HIGH CONTRAST", "Black, white and yellow"}};
  bui_choice_row(ui, "theme", row(l, 18.0f), &theme, &d->theme, themes, 4);
  const bui_field size = {"Example size", "The theme's pixel scale"};
  const bui_option sizes[] = {{"SMALL"}, {"MEDIUM"}, {"LARGE"}};
  bui_stops_row(ui, "size", row(l, 18.0f), &size, &d->size, sizes, 3);
}

/* ---------------------------------------------------------------- frame */

static void declare(bui_context* ui, demo* d, int w, int h) {
  const bui_theme* t = bui_get_theme(ui);
  const float p = t->pixel;
  const bui_hints hints = {"Up and down to move, left and right to change, Tab for the next page, N for the narrator, Escape to quit",
                           "The D-pad to move and change, the bumpers for the next page, A to choose, View for the narrator, B to quit"};
  bui_screen(ui, "demo", "BlueUI demo", &hints);
  bui_fill(ui, bui_rect_make(0, 0, (float)w, (float)h), bui_rgb(0.0f, 0.0f, 0.0f));
  const float m = fmaxf(4 * p, roundf(0.02f * (float)(w < h ? w : h)));
  const bui_rect win = bui_rect_make(m, m, (float)w - m, (float)h - m);
  bui_window(ui, win, "BLUEUI DEMO", bui_format(ui, "%d / %d  %s", d->page + 1, PAGE_COUNT, kPages[d->page]));

  /* The page list: on the right, declared after the page so the focus
   * starts on the page's explanation. */
  const float list_w = 92.0f * p, footer = 30.0f * p;
  const bui_rect list = bui_rect_make(win.x1 - 6 * p - list_w, win.y0 + 20 * p, win.x1 - 6 * p, win.y1 - footer);
  const bui_rect content = bui_rect_make(win.x0 + 6 * p, win.y0 + 20 * p, list.x0 - 10 * p, win.y1 - footer);

  /* The page. */
  layout l = {content.x0, content.x1, content.y0, p};
  bui_label(ui, bui_format(ui, "%s", kPages[d->page]), l.x0, l.y, t->title, 0.7f, 2 * p);
  gap(&l, 20.0f);
  switch (d->page) {
    case PAGE_BUTTONS: page_buttons(ui, d, &l); break;
    case PAGE_TOGGLES: page_toggles(ui, d, &l); break;
    case PAGE_CHOICES: page_choices(ui, d, &l); break;
    case PAGE_SLIDERS: page_sliders(ui, d, &l); break;
    case PAGE_TABS: page_tabs(ui, d, &l); break;
    case PAGE_FOLDOUTS: page_foldouts(ui, d, &l); break;
    case PAGE_SCROLLING: page_scrolling(ui, &l, content.y1 - 4 * p); break;
    case PAGE_CUSTOM: page_custom(ui, d, &l); break;
    case PAGE_REBINDING: page_rebinding(ui, d, &l); break;
    case PAGE_SPEECH: page_speech(ui, &l); break;
    case PAGE_SOUNDS: page_sounds(ui, d, &l); break;
    case PAGE_THEMES: page_themes(ui, d, &l); break;
  }
  bui_page(ui, kPages[d->page]);  /* (after the page: the Tabs page's example tab bar names a page too) */

  /* The page list: Tab, the bumpers and the number keys switch pages (the
   * Tabs page's example takes them first). */
  bui_fill(ui, bui_rect_make(list.x0 - 4 * p, list.y0, list.x0 - 3 * p, list.y1), t->rule);
  int page = d->page;
  const int step = bui_take_tab_step(ui), jump = bui_take_tab_jump(ui);
  if (step) page = ((page + step) % PAGE_COUNT + PAGE_COUNT) % PAGE_COUNT;
  if (jump >= 0 && jump < PAGE_COUNT) page = jump;
  const float item_h = 15.0f * p;
  for (int k = 0; k < PAGE_COUNT; ++k) {
    const bui_rect r = bui_rect_make(list.x0, list.y0 + (float)k * item_h, list.x1, list.y0 + (float)(k + 1) * item_h - 2 * p);
    bui_item it = {0};
    it.kind = BUI_TAB, it.rect = r, it.spoken = bui_format(ui, "%s page%s", kPages[k], k == d->page ? ", shown" : "");
    const bui_result res = bui_declare(ui, bui_format(ui, "page.%d", k), &it);
    if (res.activated) page = k;
    bui_fill(ui, r, k == d->page ? t->tab_on : res.hovered ? t->tab_hot : t->tab_off);
    bui_label(ui, bui_format(ui, "%d %s", k + 1, kPages[k]), r.x0 + 4 * p, roundf(0.5f * (r.y0 + r.y1 - BUI_GLYPH_H * p)),
              k == d->page ? t->tab_text : t->tab_text_off, 0.0f, 0.0f);
  }
  d->page = page;

  /* The footer: BlueUI's state output, and Quit (also pressed by Back). */
  const bui_state* st = bui_get_state(ui);
  const float sp = fmaxf(1.0f, p - 1.0f);
  bui_label(ui,
            bui_format(ui, "STATE  FOCUS: %s   MODE: %s   DEVICE: %s", *st->focused ? st->focused : "-", st->navigating ? "KEYS" : "MOUSE",
                       st->device == BUI_DEVICE_GAMEPAD ? "CONTROLLER" : "KEYBOARD"),
            win.x0 + 6 * p, win.y1 - footer + 6 * p, t->dim, 0.0f, sp);
  char said[160];
  snprintf(said, sizeof(said), "LAST LINE: %s", *st->last_line ? st->last_line : "-");
  bui_label(ui, said, win.x0 + 6 * p, win.y1 - footer + 16 * p, t->blurb, 0.0f, sp);
  const bui_button_opts quit = {"Quit the demo", false, true, 0.0f};
  if (bui_button(ui, "quit", "QUIT", bui_rect_make(win.x1 - 50 * p, win.y1 - 24 * p, win.x1 - 6 * p, win.y1 - 8 * p), &quit)) d->running = false;
}

int main(void) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) return 1;
  SDL_Window* win = NULL;
  SDL_Renderer* ren = NULL;
  if (!SDL_CreateWindowAndRenderer("BlueUI demo", 1280, 720, SDL_WINDOW_RESIZABLE, &win, &ren)) return 1;
  const SDL_AudioSpec spec = {SDL_AUDIO_S16, 1, RATE};
  SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
  SDL_AudioStream* sounds = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
  if (stream) SDL_ResumeAudioStreamDevice(stream);
  if (sounds) SDL_ResumeAudioStreamDevice(sounds);

  const bui_theme themes[4] = {bui_default_theme(), green_theme(), red_theme(), contrast_theme()};
  bui_config config = {0};
  config.sample_rate = RATE;
  config.theme = &themes[0];
  bui_context* ui = bui_create(&config);
  /* The page list's pages are pages, not tabs: "BlueUI demo, Buttons page". */
  bui_strings words = *bui_get_strings(ui);
  words.tab = "page";
  bui_set_strings(ui, &words);
  bui_announce(ui, "Welcome to the BlueUI demo. Press N to switch the narrator off.", BUI_SAY_HOLD);

  demo d = {0};
  d.running = true, d.slider = 0.5f, d.spoken_slider = 1.0f, d.stops = 1, d.stepper = 5, d.size = 1, d.sounds = BUI_SOUNDS_SOFT;
  bui_input_clear(&input);

  while (d.running) {
    const double now = (double)SDL_GetTicksNS() * 1e-9;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_EVENT_QUIT: d.running = false; break;
        case SDL_EVENT_GAMEPAD_ADDED: SDL_OpenGamepad(e.gdevice.which); break;
        case SDL_EVENT_KEY_DOWN:
          input.device = BUI_DEVICE_KEYBOARD;
          key_down(e.key.key, (e.key.mod & SDL_KMOD_SHIFT) != 0);
          break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
          input.device = BUI_DEVICE_KEYBOARD;
          if (e.button.button == SDL_BUTTON_LEFT) click = true, input.click_at.x = e.button.x, input.click_at.y = e.button.y;
          bui_input_pressed(&input, MOUSE_CODE + e.button.button);
          break;
        case SDL_EVENT_MOUSE_WHEEL: input.wheel += e.wheel.y; break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: pad_button(e.gbutton.button, e.gbutton.down, now); break;
        default: break;
      }
    }
    for (int k = 0; k < 4; ++k)  /* a held D-pad repeats */
      if (dpad[k].held && now >= dpad[k].next) bui_input_key(&input, dpad[k].key), dpad[k].next = now + 0.12;
    float mx, my;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mx, &my);
    input.mouse.x = mx, input.mouse.y = my, input.mouse_down = (buttons & SDL_BUTTON_LMASK) != 0, input.clicked = click;
    input.time = now;
    input.speech_playing = stream && SDL_GetAudioStreamQueued(stream) > 0;

    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren, &w, &h);
    bui_theme theme = themes[d.theme];
    theme.pixel = fmaxf(1.0f, roundf(fminf((float)h / 360.0f, (float)w / 640.0f)) + (float)(d.size - 1));
    theme.sound_pack = (bui_sound_pack)d.sounds;
    bui_set_theme(ui, &theme);

    bui_begin_frame(ui, &input);
    const bui_device device = input.device;
    bui_input_clear(&input), input.device = device, click = false;
    declare(ui, &d, w, h);
    bui_end_frame(ui);
    play(stream, sounds, ui);

    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    size_t n = 0;
    const bui_draw_cmd* cmds = bui_get_geometry(ui, &n);
    render(ren, cmds, n);
    SDL_RenderPresent(ren);
  }
  bui_destroy(ui);
  if (stream) SDL_DestroyAudioStream(stream);
  if (sounds) SDL_DestroyAudioStream(sounds);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
