/* BlueUI's demo: a main menu and a settings screen (every widget). SDL3 is
 * the demo's alone: its events made into BlueUI's input, BlueUI's geometry
 * drawn with its renderer, BlueUI's WAVs played through its audio. Mouse,
 * keyboard or controller; N switches the narrator. */

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>

#include "blueui/widgets.h"

#define RATE 48000

/* ----------------------------------------------- SDL's events as input */

typedef struct pad_repeat {
  bui_key key;
  bool held;
  double next;
} pad_repeat;

static bui_input input;
static bool click;
static pad_repeat dpad[4];

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
  const bui_key dirs[4] = {BUI_KEY_UP, BUI_KEY_DOWN, BUI_KEY_LEFT, BUI_KEY_RIGHT};
  const Uint8 dir_buttons[4] = {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
                                SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
  for (int k = 0; k < 4; ++k)
    if (b == dir_buttons[k]) {
      dpad[k].key = dirs[k], dpad[k].held = down, dpad[k].next = now + 0.4;
      if (down) bui_input_key(&input, dirs[k]);
      return;
    }
  if (!down) return;
  input.device = BUI_DEVICE_GAMEPAD;
  switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: bui_input_key(&input, BUI_KEY_ACTIVATE); break;
    case SDL_GAMEPAD_BUTTON_EAST: bui_input_key(&input, BUI_KEY_BACK); break;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: bui_input_key(&input, BUI_KEY_TAB_PREV); break;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: bui_input_key(&input, BUI_KEY_TAB_NEXT); break;
    case SDL_GAMEPAD_BUTTON_BACK: bui_input_key(&input, BUI_KEY_TOGGLE_SPEECH); break;
    case SDL_GAMEPAD_BUTTON_START: bui_input_key(&input, BUI_KEY_CANCEL_CAPTURE); break;
    default: break;
  }
  bui_input_pressed(&input, 0x10000 + b);
}

/* ------------------------------------------------- drawing, the voice */

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

/* What BlueUI said, played (at once: what's playing cleared first). */
static void play(SDL_AudioStream* stream, bui_context* ui) {
  bui_audio a;
  while (bui_next_audio(ui, &a)) {
    if (stream && (a.op == BUI_AUDIO_STOP || a.op == BUI_AUDIO_PLAY)) SDL_ClearAudioStream(stream);
    if (stream && a.samples) SDL_PutAudioStreamData(stream, a.samples, (int)(a.sample_count * sizeof(int16_t)));
    if (a.text && !a.samples) printf("(said) %s\n", a.text);
    bui_audio_free(&a);
  }
}

static void percent(float v, char* out, size_t size, void* user) {
  (void)user;
  snprintf(out, size, "%d%%", (int)lroundf(v * 100.0f));
}

int main(void) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) return 1;
  SDL_Window* win = NULL;
  SDL_Renderer* ren = NULL;
  if (!SDL_CreateWindowAndRenderer("BlueUI demo", 1280, 720, SDL_WINDOW_RESIZABLE, &win, &ren)) return 1;
  const SDL_AudioSpec spec = {SDL_AUDIO_S16, 1, RATE};
  SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
  if (stream) SDL_ResumeAudioStreamDevice(stream);

  const bui_config config = {RATE, 0.0f, false};
  bui_context* ui = bui_create(&config);
  bui_theme theme = bui_default_theme();
  bui_announce(ui, "BlueUI demo. Press N to switch the narrator off.", BUI_SAY_HOLD);

  enum { MENU, SETTINGS } screen = MENU;
  int tab = 0, mode = 0, stop = 2, voice = 0;
  bool fullscreen = false, colored = false, open = true, running = true;
  float volume = 0.8f;
  bui_input_clear(&input);

  while (running) {
    const double now = (double)SDL_GetTicksNS() * 1e-9;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_EVENT_QUIT: running = false; break;
        case SDL_EVENT_GAMEPAD_ADDED: SDL_OpenGamepad(e.gdevice.which); break;
        case SDL_EVENT_KEY_DOWN:
          input.device = BUI_DEVICE_KEYBOARD;
          key_down(e.key.key, (e.key.mod & SDL_KMOD_SHIFT) != 0);
          break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
          input.device = BUI_DEVICE_KEYBOARD;
          if (e.button.button == SDL_BUTTON_LEFT) click = true, input.click_at.x = e.button.x, input.click_at.y = e.button.y;
          bui_input_pressed(&input, 0x20000 + e.button.button);
          break;
        case SDL_EVENT_MOUSE_WHEEL: input.wheel += e.wheel.y; break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: pad_button(e.gbutton.button, e.gbutton.down, now); break;
        default: break;
      }
    }
    /* (A D-pad held: repeated.) */
    for (int k = 0; k < 4; ++k)
      if (dpad[k].held && now >= dpad[k].next) bui_input_key(&input, dpad[k].key), dpad[k].next = now + 0.12;
    float mx, my;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mx, &my);
    input.mouse.x = mx, input.mouse.y = my, input.mouse_down = (buttons & SDL_BUTTON_LMASK) != 0, input.clicked = click;
    input.time = now;
    input.speech_playing = stream && SDL_GetAudioStreamQueued(stream) > 0;

    int w = 0, h = 0;
    SDL_GetRenderOutputSize(ren, &w, &h);
    theme.pixel = fmaxf(2.0f, roundf((float)h / 300.0f));
    const float p = theme.pixel;
    bui_set_focus_style(ui, bui_rgb(1.0f, 0.86f, 0.2f), p, p);

    bui_begin_frame(ui, &input);
    const bui_device device = input.device;
    bui_input_clear(&input), input.device = device, click = false;
    if (screen == MENU) {
      const bui_hints hints = {"Up and down to move, Enter to choose, N to switch the narrator off", "The D-pad to move, A to choose"};
      bui_screen(ui, "menu", "Main menu", &hints);
      bui_fill(ui, bui_rect_make(0, 0, (float)w, (float)h), bui_rgb(0.02f, 0.04f, 0.03f));
      bui_label(ui, &theme, "BLUEUI", roundf(0.5f * ((float)w - bui_text_width("BLUEUI", BUI_NUL, 3 * p))), roundf((float)h * 0.2f),
                bui_rgb(0.45f, 0.85f, 0.3f), 0.7f, 3 * p);
      const float bw = 64 * p, bh = 13 * p, x0 = roundf(0.5f * ((float)w - bw));
      float y = roundf((float)h * 0.42f);
      const char* ids[] = {"play", "settings", "quit"};
      const char* names[] = {"PLAY", "SETTINGS", "QUIT"};
      for (int k = 0; k < 3; ++k, y += bh + 5 * p) {
        if (!bui_button(ui, &theme, ids[k], names[k], bui_rect_make(x0, y, x0 + bw, y + bh), NULL)) continue;
        if (k == 0) bui_announce(ui, "There's no game here: it's a demo", BUI_SAY_NORMAL);
        if (k == 1) screen = SETTINGS;
        if (k == 2) running = false;
      }
    } else {
      const bui_hints hints = {"Up and down to move, left and right to change a setting, Tab for the next tab, Escape to go back",
                               "The D-pad to move and change, the bumpers for its tabs, B to go back"};
      bui_screen(ui, "settings", "Settings", &hints);
      const float m = fmaxf(4 * p, roundf(0.03f * (float)(w < h ? w : h)));
      const bui_rect win_r = bui_rect_make(m, m, (float)w - m, (float)h - m);
      bui_window(ui, &theme, win_r, "SETTINGS", "BLUEUI DEMO");
      const char* tabs[] = {"Audio", "Visual"};
      bui_tab_bar(ui, &theme, "tabs", bui_rect_make(win_r.x0 + 6 * p, win_r.y0 + 18 * p, win_r.x1 - 6 * p, win_r.y0 + 32 * p), tabs, 2, &tab,
                  0.0f);
      const float row_h = 18 * p;
      float y = win_r.y0 + 52 * p;
#define ROW() (y += row_h, bui_rect_make(win_r.x0 + 6 * p, y - row_h, win_r.x1 - 6 * p, y - 2 * p))
      if (tab == 0) {
        const bui_field vol = {"Volume", "Everything the game plays", NULL, false}, voice_f = {"Voice", NULL, NULL, false};
        const bui_option voices[] = {{"SLT", "Flite's US English voice", NULL}};
        bui_slider_row(ui, &theme, "volume", ROW(), &vol, &volume, 0, 1, 0.05f, percent, NULL, NULL);
        bui_choice_row(ui, &theme, "voice", ROW(), &voice_f, &voice, voices, 1);
      } else {
        const bui_field full = {"Fullscreen", "The whole screen, or a window", NULL, false};
        const bui_field shadows = {"Shadows", "How the sun's shadows are worked out", NULL, false};
        bui_toggle_row(ui, &theme, "full", ROW(), &full, &fullscreen, NULL, NULL);
        y += 6 * p;
        bui_foldout_row(ui, &theme, "shadows", ROW(), &shadows, &open);
        if (open) {
          const bui_field mode_f = {"Occlusion sample mode", NULL, NULL, false};
          const bui_option modes[] = {{"SINGLE", "1 ray a face: the fastest", NULL}, {"4 CORNERS", "4 rays a face: cleaner edges", NULL}};
          const bui_field range = {"Shadow detail range", "Then 4, 16, 64 faces a check", NULL, false};
          const bui_option stops[] = {{"3.2 M", NULL, NULL}, {"6.4 M", NULL, NULL}, {"12.8 M", NULL, NULL}, {"25.6 M", NULL, NULL}};
          const bui_field col = {"Colored shadows", colored ? "Glass tints the light" : "Light passes glass plainly", NULL, false};
          bui_choice_row(ui, &theme, "mode", ROW(), &mode_f, &mode, modes, 2);
          bui_stops_row(ui, &theme, "range", ROW(), &range, &stop, stops, 4);
          bui_toggle_row(ui, &theme, "colored", ROW(), &col, &colored, NULL, NULL);
        }
      }
#undef ROW
      const bui_button_opts back = {"Back", false, true, 0.0f};
      if (bui_button(ui, &theme, "back", "BACK", bui_rect_make(win_r.x1 - 50 * p, win_r.y1 - 18 * p, win_r.x1 - 6 * p, win_r.y1 - 6 * p), &back) ||
          bui_back_pressed(ui))
        screen = MENU;
    }
    bui_end_frame(ui);
    play(stream, ui);

    SDL_SetRenderDrawColor(ren, 10, 20, 14, 255);
    SDL_RenderClear(ren);
    size_t n = 0;
    const bui_draw_cmd* cmds = bui_get_geometry(ui, &n);
    render(ren, cmds, n);
    SDL_RenderPresent(ren);
  }
  bui_destroy(ui);
  if (stream) SDL_DestroyAudioStream(stream);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
