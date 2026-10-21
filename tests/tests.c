/* BlueUI's tests: screens declared as a game would, driven by keys, clicks
 * and the mouse; what's said read back from the state and the lines handed
 * over (a context with no voice: text alone). */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "blueui/widgets.h"

static int failures = 0;

static void expect(bool ok, const char* what, const char* said) {
  if (said) printf("%s %s (\"%s\")\n", ok ? "  ok  " : "  FAIL", what, said);
  else printf("%s %s\n", ok ? "  ok  " : "  FAIL", what);
  failures += !ok;
}

static bool starts(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }
static bool same(const char* a, const char* b) { return strcmp(a, b) == 0; }

/* ----------------------------------------------------------- a harness */

typedef struct harness harness;
typedef void (*screen_fn)(harness*);

struct harness {
  bui_context* ui;
  double time;
  screen_fn screen;
  size_t lines; /* lines handed over */
  /* The screens' state. */
  int pressed, hit, tab, mode, stop;
  bool full, filter_on, open, back, cancelled;
  float strength;
  int64_t bound;
};

static void harness_init(harness* h, screen_fn screen) {
  memset(h, 0, sizeof(*h));
  bui_config c = {0};
  c.no_voice = true, c.skip_narrator_support_modal = true;
  h->ui = bui_create(&c);
  h->screen = screen;
  h->pressed = h->hit = -1;
  h->stop = 2;
  h->strength = 1.0f;
}

static void harness_free(harness* h) { bui_destroy(h->ui); }

static void frame_with(harness* h, const bui_key* keys, int n, bui_input in) {
  for (int k = 0; k < n; ++k) bui_input_key(&in, keys[k]);
  in.time = h->time += 1.0 / 60.0;
  bui_begin_frame(h->ui, &in);
  h->screen(h);
  bui_end_frame(h->ui);
  bui_audio a;
  while (bui_next_audio(h->ui, &a)) {
    if (a.op != BUI_AUDIO_STOP) ++h->lines;
    bui_audio_free(&a);
  }
}

static bui_input none(void) {
  bui_input in;
  bui_input_clear(&in);
  return in;
}

static void frame(harness* h) { frame_with(h, NULL, 0, none()); }
static void key(harness* h, bui_key k) { frame_with(h, &k, 1, none()); }

static void click(harness* h, bui_vec2 at) {
  bui_input in = none();
  in.mouse = at, in.clicked = true, in.click_at = at;
  frame_with(h, NULL, 0, in);
}

static const char* said(const harness* h) { return bui_get_state(h->ui)->last_line; }
static const char* focused(const harness* h) { return bui_focused_id(h->ui); }

static bui_rect row_at(int k) { return bui_rect_make(10, 50.0f + 20.0f * (float)k, 400, 68.0f + 20.0f * (float)k); }

/* ------------------------------------------------------------ the menu */

static void menu_screen(harness* h) {
  const bui_hints hints = {"Up and down to move, Enter to choose", "The D-pad to move"};
  bui_screen(h->ui, "menu", "Main menu", &hints);
  const char* names[] = {"PLAY", "CHARACTER", "QUIT"};
  for (int k = 0; k < 3; ++k) {
    const bui_button_opts o = {k == 0 ? "Play" : NULL, false, false, 0.0f};
    if (bui_button(h->ui, names[k], names[k], row_at(k), &o)) h->pressed = k;
  }
}

static void other_screen(harness* h) { bui_screen(h->ui, "other", "Other", NULL); }

static void menu_again(harness* h) {
  const bui_hints hints = {"Up and down to move, Enter to choose", NULL};
  bui_screen(h->ui, "menu", "Main menu", &hints);
  bui_button(h->ui, "PLAY", "PLAY", row_at(0), NULL);
}

static void two_buttons(harness* h) {
  bui_screen(h->ui, "menu", "Main menu", NULL);
  for (int k = 0; k < 2; ++k)
    if (bui_button(h->ui, k ? "B" : "A", k ? "SECOND" : "FIRST", row_at(k), NULL)) h->hit = k;
}

static void menu_tests(void) {
  printf("The main menu\n");
  harness h;
  harness_init(&h, menu_screen);
  frame(&h);
  expect(same(said(&h), "Main menu. Up and down to move, Enter to choose"), "come to: its name, its keys", said(&h));
  key(&h, BUI_KEY_DOWN);
  expect(bui_navigating(h.ui) && same(focused(&h), "PLAY") && same(said(&h), "Play. Item 1 of 3, button"),
         "the first key: where the focus is, said", said(&h));
  key(&h, BUI_KEY_DOWN);
  expect(same(said(&h), "CHARACTER. Item 2 of 3, button"), "down: the next", said(&h));
  key(&h, BUI_KEY_UP), key(&h, BUI_KEY_UP);
  expect(same(focused(&h), "QUIT"), "up past the first: round to the last", NULL);
  const size_t lines = h.lines;
  frame(&h), frame(&h);
  expect(h.lines == lines, "nothing said again while it stays", NULL);
  key(&h, BUI_KEY_ACTIVATE);
  expect(h.pressed == 2, "Enter presses the focused one", NULL);
  const bui_state* s = bui_get_state(h.ui);
  expect(s->item_count == 3 && same(s->items[2].id, "QUIT") && s->items[2].focused && s->items[2].index == 2 &&
             same(s->screen, "menu") && same(s->focused, "QUIT"),
         "the state: its items, the focus", NULL);
  size_t count = 0;
  bui_get_geometry(h.ui, &count);
  expect(count > 0, "the geometry: its rectangles", NULL);
  /* Back to it later: its name, not its keys again. */
  h.screen = other_screen;
  frame(&h);
  h.screen = menu_again;
  frame(&h);
  expect(starts(said(&h), "Main menu. PLAY. Item 1 of 1"), "its keys said once", said(&h));
  harness_free(&h);

  /* The mouse over an item: said; clicked: pressed. */
  harness m;
  harness_init(&m, two_buttons);
  frame(&m);
  bui_input over = none();
  over.mouse = bui_rect_center(row_at(1));
  frame_with(&m, NULL, 0, over);
  expect(same(said(&m), "SECOND. Item 2 of 2, button"), "the mouse over one: it's said", said(&m));
  click(&m, bui_rect_center(row_at(0)));
  expect(m.hit == 0, "clicked: pressed", NULL);
  harness_free(&m);
}

/* ------------------------------------------------------- the settings */

static void percent(float v, char* out, size_t size, void* user) {
  (void)user;
  snprintf(out, size, "%d%%", (int)(v * 100 + 0.5f));
}

static void settings_screen(harness* h) {
  bui_context* ui = h->ui;
  const bui_hints hints = {"Tab for the next tab", NULL};
  bui_screen(ui, "settings", "Settings", &hints);
  const char* tabs[] = {"Audio", "Visual"};
  bui_tab_bar(ui, "tabs", bui_rect_make(10, 20, 600, 40), tabs, 2, &h->tab, 0.0f);
  if (h->tab == 0) {
    const bui_field filter = {"Filter", "Colors adjusted", NULL, false};
    bui_toggle_row(ui, "filter", row_at(0), &filter, &h->filter_on, NULL, NULL);
    const bui_field strength = {"Filter strength", "Pick a filter first", NULL, !h->filter_on};
    bui_slider_row(ui, "strength", row_at(1), &strength, &h->strength, 0, 1, 0.05f, percent, NULL, NULL);
  } else {
    const bui_field full = {"Fullscreen", "The whole screen", NULL, false}, shadows = {"Shadows", NULL, NULL, false};
    bui_toggle_row(ui, "full", row_at(0), &full, &h->full, NULL, NULL);
    bui_foldout_row(ui, "shadows", row_at(1), &shadows, &h->open);
    if (h->open) {
      const bui_field mode = {"Sample mode", NULL, NULL, false}, range = {"Range", NULL, NULL, false};
      const bui_option modes[] = {{"SINGLE", "1 ray", NULL}, {"4 CORNERS", "4 rays", NULL}};
      const bui_option stops[] = {{"3.2 M", NULL, NULL}, {"6.4 M", NULL, NULL}, {"12.8 M", NULL, NULL}, {"25.6 M", NULL, NULL}};
      bui_choice_row(ui, "mode", row_at(2), &mode, &h->mode, modes, 2);
      bui_stops_row(ui, "range", row_at(3), &range, &h->stop, stops, 4);
    }
  }
  const bui_button_opts back = {NULL, false, true, 0.0f};
  if (bui_button(ui, "back", "BACK", row_at(6), &back)) h->back = true;
}

static void settings_tests(void) {
  printf("A settings screen\n");
  harness h;
  harness_init(&h, settings_screen);
  frame(&h);
  expect(same(said(&h), "Settings, Audio tab. Tab for the next tab"), "come to: its name and tab, its keys", said(&h));
  key(&h, BUI_KEY_DOWN), key(&h, BUI_KEY_DOWN);
  expect(same(said(&h), "Filter strength, 100%. Pick a filter first. Item 2 of 3, slider"), "a slider that's off: why, when it's come to",
         said(&h));
  key(&h, BUI_KEY_LEFT);
  expect(h.strength == 1.0f, "off: not changed", NULL);
  key(&h, BUI_KEY_UP), key(&h, BUI_KEY_ACTIVATE);
  expect(h.filter_on && same(said(&h), "Filter, on"), "a toggle pressed: its new state said", said(&h));
  key(&h, BUI_KEY_DOWN), key(&h, BUI_KEY_LEFT);
  expect(fabsf(h.strength - 0.95f) < 1e-4f && same(said(&h), "Filter strength, 95%"), "a slider nudged", said(&h));
  const bui_item_state* f = bui_find(h.ui, "strength");
  expect(f && same(f->value, "95%") && f->kind == BUI_SLIDER && f->enabled, "the state: its value", NULL);
  /* Tab: the next tab, its first item. */
  key(&h, BUI_KEY_NEXT);
  expect(h.tab == 1 && same(said(&h), "Settings, Visual tab. Fullscreen, off. Item 1 of 3, toggle"),
         "Tab: the next tab, said with its first item", said(&h));
  key(&h, BUI_KEY_DOWN), key(&h, BUI_KEY_ACTIVATE);
  expect(h.open && same(said(&h), "Shadows, expanded"), "a foldout opened", said(&h));
  key(&h, BUI_KEY_DOWN), key(&h, BUI_KEY_RIGHT);
  expect(h.mode == 1 && starts(said(&h), "Sample mode, 4 CORNERS"), "a choice turned", said(&h));
  key(&h, BUI_KEY_DOWN), key(&h, BUI_KEY_RIGHT), key(&h, BUI_KEY_RIGHT);
  expect(h.stop == 3 && same(said(&h), "Range, 25.6 M"), "a slider's stops: stepped, kept to its last", said(&h));
  key(&h, BUI_KEY_BACK);
  expect(h.back, "Back presses its back button", NULL);
  /* The bumpers, a tab's number. */
  key(&h, BUI_KEY_TAB_PREV);
  expect(h.tab == 0, "a bumper: the tab before", NULL);
  bui_input jump = none();
  jump.tab_hotkey = 2;
  frame_with(&h, NULL, 0, jump);
  expect(h.tab == 1, "its number: that tab", NULL);
  harness_free(&h);
}

/* --------------------------------------------------------- scrolling */

static void list_screen(harness* h) {
  bui_context* ui = h->ui;
  bui_screen(ui, "list", "List", NULL);
  const float off = bui_begin_scroll(ui, "rows", bui_rect_make(0, 0, 400, 100), 20.0f * 30);
  for (int k = 0; k < 30; ++k) {
    bui_item it = {0};
    it.rect = bui_rect_make(0, 20.0f * (float)k - off, 400, 20.0f * (float)(k + 1) - off);
    it.spoken = bui_format(ui, "Row %d", k + 1);
    bui_declare(ui, bui_format(ui, "r%d", k), &it);
  }
  bui_end_scroll(ui);
}

static void scroll_tests(void) {
  printf("A scrolled list\n");
  harness h;
  harness_init(&h, list_screen);
  frame(&h);
  key(&h, BUI_KEY_DOWN);
  for (int k = 0; k < 9; ++k) key(&h, BUI_KEY_DOWN);
  frame(&h);
  const bui_item_state* f = bui_find(h.ui, "r9");
  expect(same(focused(&h), "r9") && f && f->rect.y1 <= 100.5f && f->shown && bui_scroll_of(h.ui, "rows") > 0.0f,
         "the focus moved past the view: scrolled to it", NULL);
  key(&h, BUI_KEY_UP);
  expect(same(said(&h), "Row 9. Item 9 of 30, button"), "said as it's come to", said(&h));
  harness_free(&h);
}

/* --------------------------------------------------------- rebinding */

static void controls_screen(harness* h) {
  bui_screen(h->ui, "controls", "Controls", NULL);
  bui_item it = {0};
  it.rect = row_at(0), it.spoken = "Jump, Space";
  const bui_captured c = bui_capture(h->ui, "jump", &it, "Press a key for jump. Escape to cancel", 8.0);
  if (c.has_code) h->bound = c.code;
  if (c.cancelled) h->cancelled = true;
}

static void capture_tests(void) {
  printf("Rebinding\n");
  harness h;
  harness_init(&h, controls_screen);
  frame(&h), key(&h, BUI_KEY_DOWN);
  key(&h, BUI_KEY_ACTIVATE);
  expect(bui_capturing(h.ui, "jump") && strstr(said(&h), "Press a key for jump") && same(bui_get_state(h.ui)->capturing, "jump"),
         "pressed: waiting, said so", said(&h));
  bui_input press = none();
  bui_input_pressed(&press, 42);
  const bui_key down = BUI_KEY_DOWN;
  frame_with(&h, &down, 1, press);
  expect(h.bound == 42 && !bui_capturing(h.ui, NULL) && same(focused(&h), "jump"), "the next key taken (not moving the focus)", NULL);
  key(&h, BUI_KEY_ACTIVATE);
  key(&h, BUI_KEY_CANCEL_CAPTURE);
  expect(h.cancelled && same(said(&h), "Cancelled"), "cancelled", said(&h));
  h.cancelled = false;
  key(&h, BUI_KEY_ACTIVATE);
  for (int k = 0; k < 9 * 60; ++k) frame(&h);
  expect(h.cancelled, "8 seconds and nothing: given up", NULL);
  harness_free(&h);
}

/* ------------------------------------------------------------- speech */

static void play_screen(harness* h) {
  bui_screen(h->ui, "menu", "Main menu", NULL);
  bui_button(h->ui, "play", "PLAY", row_at(0), NULL);
}

static void speech_tests(void) {
  printf("Speech\n");
  harness h;
  harness_init(&h, play_screen);
  bui_announce(h.ui, "Welcome", BUI_SAY_HOLD);
  frame(&h);
  expect(same(said(&h), "Welcome"), "held: the screen waits", said(&h));
  key(&h, BUI_KEY_DOWN);
  expect(starts(said(&h), "Main menu"), "a key: the screen said after all", said(&h));
  bui_announce(h.ui, "Saved", BUI_SAY_NORMAL);
  frame(&h);
  expect(same(said(&h), "Saved"), "an announcement with the frame's line", said(&h));
  bui_set_speech_enabled(h.ui, false, true);
  frame(&h);
  const bui_state* s = bui_get_state(h.ui);
  expect(!s->speech_enabled && s->speech_toggled && same(said(&h), "Narrator off"), "switched off (announced): said", said(&h));
  const size_t lines = h.lines;
  bui_announce(h.ui, "Unheard", BUI_SAY_NORMAL);
  frame(&h);
  expect(h.lines == lines, "off: nothing said", NULL);
  bui_set_speech_enabled(h.ui, true, true);
  frame(&h);
  expect(starts(said(&h), "Narrator on. Main menu"), "on: where it is", said(&h));
  key(&h, BUI_KEY_REPEAT);
  expect(h.lines == lines + 2, "the last line again", NULL);
  /* The lines handed over: their text, a stop. */
  bui_stop_speech(h.ui);
  bui_say(h.ui, "Hello", true);
  bui_audio a, b;
  const bool got = bui_next_audio(h.ui, &a) && bui_next_audio(h.ui, &b);
  expect(got && a.op == BUI_AUDIO_STOP && b.op == BUI_AUDIO_QUEUE && same(b.text, "Hello") && !b.wav, "handed over: a stop, a line queued",
         NULL);
  if (got) bui_audio_free(&a), bui_audio_free(&b);
  harness_free(&h);
}

/* -------------------------------------------------------------- theme */

static bool has_color(const bui_context* ui, bui_color c) {
  size_t n = 0;
  const bui_draw_cmd* d = bui_get_geometry(ui, &n);
  for (size_t i = 0; i < n; ++i)
    if (d[i].r == c.r && d[i].g == c.g && d[i].b == c.b && d[i].a == c.a) return true;
  return false;
}

static void theme_tests(void) {
  printf("Themes\n");
  bui_theme mine = bui_default_theme();
  mine.button = bui_rgb(0.1f, 0.2f, 0.3f), mine.focus = bui_rgb(0.9f, 0.1f, 0.5f);
  harness h;
  harness_init(&h, play_screen);
  bui_destroy(h.ui);
  bui_config c = {0};
  c.no_voice = true, c.theme = &mine, c.skip_narrator_support_modal = true;
  h.ui = bui_create(&c);
  frame(&h);
  expect(has_color(h.ui, mine.button) && bui_get_theme(h.ui)->button.b == 0.3f, "the config's theme: the widgets drawn with it", NULL);
  key(&h, BUI_KEY_DOWN);
  expect(has_color(h.ui, mine.focus), "the focus outline in the theme's colour", NULL);
  bui_set_theme(h.ui, NULL);
  frame(&h);
  const bui_theme d = bui_default_theme();
  expect(!has_color(h.ui, mine.button) && has_color(h.ui, d.button_hot) && has_color(h.ui, d.focus), "bui_set_theme(NULL): the default again", NULL);
  harness_free(&h);
}

/* ----------------------------------------------------------- GUI sounds */

/* The sounds handed over in the last frame: their event names, joined. */
static char heard[256];

static void sound_frame_with(harness* h, const bui_key* keys, int n, bui_input in) {
  for (int k = 0; k < n; ++k) bui_input_key(&in, keys[k]);
  in.time = h->time += 1.0 / 60.0;
  bui_begin_frame(h->ui, &in);
  h->screen(h);
  bui_end_frame(h->ui);
  heard[0] = 0;
  bui_audio a;
  while (bui_next_audio(h->ui, &a)) {
    if (a.op == BUI_AUDIO_SOUND) {
      if (heard[0]) strcat(heard, " ");
      strncat(heard, a.text, sizeof(heard) - strlen(heard) - 2);
    }
    bui_audio_free(&a);
  }
}
static void sound_key(harness* h, bui_key k) { sound_frame_with(h, &k, 1, none()); }

static void sounds_screen(harness* h) {
  bui_context* ui = h->ui;
  bui_screen(ui, "sounds", "Sounds", NULL);
  bui_button(ui, "a", "A", row_at(0), NULL);
  const bui_field toggle = {"Toggle", NULL, NULL, false}, slider = {"Slider", NULL, NULL, false};
  bui_toggle_row(ui, "toggle", row_at(1), &toggle, &h->full, NULL, NULL);
  bui_slider_row(ui, "slider", row_at(2), &slider, &h->strength, 0, 1, 0.1f, percent, NULL, NULL);
  const bui_button_opts off = {NULL, true, false, 0.0f};
  bui_button(ui, "off", "OFF", row_at(3), &off);
}

static void sound_tests(void) {
  printf("GUI sounds\n");
  harness h;
  harness_init(&h, sounds_screen);
  h.strength = 0.5f;
  sound_frame_with(&h, NULL, 0, none());
  expect(heard[0] == 0, "the default theme: no sounds", NULL);
  bui_theme t = bui_default_theme();
  t.sound_pack = BUI_SOUNDS_SOFT;
  bui_set_theme(h.ui, &t);
  h.screen = other_screen, sound_frame_with(&h, NULL, 0, none());
  h.screen = sounds_screen, sound_frame_with(&h, NULL, 0, none());
  expect(strcmp(heard, "screen") == 0, "a new screen", heard);
  sound_key(&h, BUI_KEY_DOWN);
  expect(strcmp(heard, "move") == 0, "the focus shown: move", heard);
  sound_key(&h, BUI_KEY_ACTIVATE);
  expect(strcmp(heard, "press") == 0, "a button pressed: press", heard);
  sound_key(&h, BUI_KEY_DOWN), sound_key(&h, BUI_KEY_ACTIVATE);
  expect(strcmp(heard, "on") == 0 && h.full, "a toggle switched on: on, not press", heard);
  sound_key(&h, BUI_KEY_ACTIVATE);
  expect(strcmp(heard, "off") == 0, "and off: off", heard);
  sound_key(&h, BUI_KEY_DOWN), sound_key(&h, BUI_KEY_RIGHT);
  expect(strcmp(heard, "change") == 0, "a slider stepped: change", heard);
  sound_key(&h, BUI_KEY_DOWN), sound_key(&h, BUI_KEY_ACTIVATE);
  expect(strcmp(heard, "denied") == 0, "a disabled button pressed: denied", heard);
  /* A sound asked for wins over BlueUI's own choice; each pack's clips differ. */
  bui_audio a = {0};
  bui_trigger(h.ui, BUI_EVENT_MOVE);
  sound_key(&h, BUI_KEY_ACTIVATE);
  expect(strcmp(heard, "move") == 0, "bui_trigger: in place of BlueUI's choice", heard);
  size_t lengths[BUI_SOUND_PACK_COUNT] = {0};
  for (int p = BUI_SOUNDS_SOFT; p < BUI_SOUND_PACK_COUNT; ++p) {
    t.sound_pack = (bui_sound_pack)p;
    bui_set_theme(h.ui, &t);
    bui_trigger(h.ui, BUI_EVENT_PRESS);
    bui_begin_frame(h.ui, NULL), h.screen(&h), bui_end_frame(h.ui);
    while (bui_next_audio(h.ui, &a)) {
      if (a.op == BUI_AUDIO_SOUND && a.sample_rate == 48000 && memcmp(a.wav, "RIFF", 4) == 0) lengths[p] = a.sample_count;
      bui_audio_free(&a);
    }
  }
  expect(lengths[BUI_SOUNDS_SOFT] > 0 && lengths[BUI_SOUNDS_CRISP] > 0 && lengths[BUI_SOUNDS_SOFT] != lengths[BUI_SOUNDS_CRISP],
         "Soft and Crisp: built in, their own sounds, as WAVs at the context's rate", NULL);
  t.sound_volume = 0.0f;
  bui_set_theme(h.ui, &t);
  sound_key(&h, BUI_KEY_UP);
  expect(heard[0] == 0, "sound_volume 0: silent", NULL);
  t.sound_volume = 0.8f, t.sound_pack = BUI_SOUNDS_SOFT;
  bui_set_theme(h.ui, &t);
  /* Sounds don't cut speech off, and play with the narrator off. */
  bui_say(h.ui, "Speech", false);
  bui_trigger(h.ui, BUI_EVENT_MOVE);
  bui_begin_frame(h.ui, NULL), h.screen(&h), bui_end_frame(h.ui);
  int speech = 0, sounds = 0;
  while (bui_next_audio(h.ui, &a)) speech += a.op == BUI_AUDIO_PLAY, sounds += a.op == BUI_AUDIO_SOUND, bui_audio_free(&a);
  bui_set_speech_enabled(h.ui, false, false);
  sound_key(&h, BUI_KEY_UP);
  expect(speech == 1 && sounds == 1 && strcmp(heard, "move") == 0, "alongside speech; and with the narrator off", heard);
  harness_free(&h);
}

/* --------------------------------------------------------------- rumble */

static void pad_key(harness* h, bui_key k) {
  bui_input in = none();
  in.device = BUI_DEVICE_GAMEPAD;
  sound_frame_with(h, &k, 1, in);
}

static void rumble_tests(void) {
  printf("Rumble\n");
  harness h;
  harness_init(&h, sounds_screen);
  sound_frame_with(&h, NULL, 0, none());  /* (arrived: past the screen's event) */
  pad_key(&h, BUI_KEY_DOWN);
  const bui_state* s = bui_get_state(h.ui);
  expect(s->has_event && s->event == BUI_EVENT_MOVE && !s->has_rumble, "the default theme: the event, no rumble", NULL);
  bui_theme t = bui_default_theme();
  t.rumble_pack = BUI_RUMBLE_SOFT;
  bui_set_theme(h.ui, &t);
  pad_key(&h, BUI_KEY_ACTIVATE);
  s = bui_get_state(h.ui);
  const bui_rumble soft = s->rumble;
  expect(s->has_rumble && s->event == BUI_EVENT_PRESS && soft.step_count >= 1 && soft.steps[0].ms > 0 && soft.steps[0].low > 0,
         "Soft, a press on a controller: its pattern in the state", NULL);
  t.rumble_pack = BUI_RUMBLE_CRISP;
  bui_set_theme(h.ui, &t);
  pad_key(&h, BUI_KEY_ACTIVATE);
  s = bui_get_state(h.ui);
  expect(s->has_rumble && memcmp(&s->rumble, &soft, sizeof(soft)) != 0, "Crisp: a pattern of its own", NULL);
  const bui_rumble crisp = s->rumble;
  t.rumble_strength = 0.5f;
  bui_set_theme(h.ui, &t);
  pad_key(&h, BUI_KEY_ACTIVATE);
  s = bui_get_state(h.ui);
  expect(s->has_rumble && fabsf(s->rumble.steps[0].high - 0.5f * crisp.steps[0].high) < 1e-5f && s->rumble.steps[0].ms == crisp.steps[0].ms,
         "rumble_strength: the motors scaled, the timings kept", NULL);
  sound_key(&h, BUI_KEY_ACTIVATE);
  s = bui_get_state(h.ui);
  expect(s->has_event && !s->has_rumble, "the keyboard: no rumble", NULL);
  sound_frame_with(&h, NULL, 0, none());
  expect(!bui_get_state(h.ui)->has_event, "nothing happened: no event", NULL);
  harness_free(&h);
}

/* --------------------------------------------------------------- modals */

static void modals_tests(void) {
  printf("Modals\n");
  /* The narrator's offer: shown first unless skipped, heard with speech off. */
  harness h;
  harness_init(&h, menu_screen);
  bui_destroy(h.ui);
  bui_config c = {0};
  c.no_voice = true;
  h.ui = bui_create(&c);
  bui_set_speech_enabled(h.ui, false, false);
  frame(&h);
  const bui_state* s = bui_get_state(h.ui);
  expect(s->modal && strcmp(s->modal, BUI_MODAL_NARRATOR) == 0 && starts(said(&h), "Narration available. This game can read") &&
             strstr(said(&h), "Enable narrator. Item 1 of 2, button") && same(focused(&h), "bui.modal.0"),
         "the narrator's offer: shown, spoken (speech off), Enable focused", said(&h));
  key(&h, BUI_KEY_DOWN);
  expect(h.pressed == -1 && same(focused(&h), "bui.modal.1"), "the screen under it: no input; down: Disable", NULL);
  key(&h, BUI_KEY_UP), key(&h, BUI_KEY_ACTIVATE);
  int button = -1;
  const bool answered = bui_take_modal_result(h.ui, BUI_MODAL_NARRATOR, &button) && button == 0;
  frame(&h);
  s = bui_get_state(h.ui);
  expect(answered && !s->modal && s->speech_enabled && s->speech_toggled && starts(said(&h), "Narrator on. Main menu"),
         "Enable: speech on (speech_toggled), the screen said again", said(&h));
  harness_free(&h);

  /* A controller unplugged: its modal; the focus back after. */
  harness_init(&h, menu_screen);
  bui_input in = none();
  in.gamepad_connected = true;
  const bui_key down = BUI_KEY_DOWN;
  frame_with(&h, NULL, 0, in);
  frame_with(&h, &down, 1, in), frame_with(&h, &down, 1, in);
  expect(same(focused(&h), "CHARACTER"), "(the focus on the second item)", NULL);
  frame(&h);  /* (connected: false now) */
  s = bui_get_state(h.ui);
  expect(s->modal && strcmp(s->modal, BUI_MODAL_CONTROLLER) == 0 && starts(said(&h), "Controller disconnected"),
         "connected, then not: \"Controller disconnected\"", said(&h));
  key(&h, BUI_KEY_BACK);
  frame(&h);
  expect(!bui_get_state(h.ui)->modal && same(focused(&h), "CHARACTER") && h.back == false, "Back: OK; the focus back on its item", NULL);
  in = none();
  in.gamepad_event = BUI_GAMEPAD_DISCONNECTED;
  frame_with(&h, NULL, 0, in);
  in = none();
  in.gamepad_event = BUI_GAMEPAD_CONNECTED;
  frame_with(&h, NULL, 0, in);
  expect(!bui_get_state(h.ui)->modal, "reported events; plugged back in: the modal closes itself", NULL);
  bui_set_controller_modals(h.ui, BUI_CONTROLLER_MODALS_ALL);
  frame_with(&h, NULL, 0, in);
  expect(bui_modal_open(h.ui, BUI_MODAL_CONTROLLER) && starts(said(&h), "Controller connected"), "ALL: \"Controller connected\" too", said(&h));
  bui_close_modal(h.ui, BUI_MODAL_CONTROLLER);
  bui_set_controller_modals(h.ui, BUI_CONTROLLER_MODALS_NONE);
  in.gamepad_event = BUI_GAMEPAD_DISCONNECTED;
  frame_with(&h, NULL, 0, in);
  expect(!bui_modal_open(h.ui, NULL), "NONE: no modal", NULL);

  /* A game's own: its buttons, clicked; its answer once. */
  const char* buttons[] = {"Save", "Discard", "Cancel"};
  bui_open_modal(h.ui, "quit", "Quit?", "You have unsaved changes.", buttons, 3);
  frame(&h);
  const bui_item_state* discard = bui_find(h.ui, "bui.modal.1");
  click(&h, discard ? bui_rect_center(discard->rect) : bui_rect_center(row_at(0)));
  button = -1;
  expect(discard && bui_take_modal_result(h.ui, "quit", &button) && button == 1 && !bui_take_modal_result(h.ui, "quit", &button) &&
             h.pressed == -1,
         "a game's modal: clicked Discard (1), taken once; nothing under it pressed", NULL);
  harness_free(&h);
}

/* -------------------------------------------------------------- voice */

static void voice_tests(void) {
  printf("The voice\n");
  bui_context* ui = bui_create(NULL);
  if (!bui_has_voice(ui)) {
    printf("  (no voice built in)\n");
    bui_destroy(ui);
    return;
  }
  bui_audio a;
  const bool made = bui_synthesize(ui, "Volume, 80%", &a);
  bool wav = made && a.wav_size == 44 + 2 * a.sample_count && memcmp(a.wav, "RIFF", 4) == 0 && memcmp(a.wav + 8, "WAVE", 4) == 0 &&
             a.sample_rate == 48000 && a.sample_count > 48000 / 2;
  int peak = 0;
  for (size_t i = 0; made && i < a.sample_count; ++i) peak = abs(a.samples[i]) > peak ? abs(a.samples[i]) : peak;
  expect(wav && fabs(peak / 32767.0 - 0.7) < 0.01, "a line spoken: a WAV at 48 kHz, its loudest 0.7", NULL);
  if (made) bui_audio_free(&a);
  /* Said: made on its thread, handed over. */
  bui_say(ui, "Main menu", false);
  bool got = false;
  for (int k = 0; k < 500 && !got; ++k) {
    got = bui_next_audio(ui, &a);
    if (!got) {
      struct timespec t = {0, 10 * 1000 * 1000};
      thrd_sleep(&t, NULL);
    }
  }
  expect(got && a.op == BUI_AUDIO_PLAY && same(a.text, "Main menu") && a.wav, "said: on its thread, its WAV handed over", NULL);
  if (got) bui_audio_free(&a);
  bui_destroy(ui);
}

int main(void) {
  menu_tests();
  settings_tests();
  scroll_tests();
  capture_tests();
  speech_tests();
  theme_tests();
  sound_tests();
  rumble_tests();
  modals_tests();
  voice_tests();
  printf(failures ? "%d failed\n" : "all passed\n", failures);
  return failures ? 1 : 0;
}
