# Speech and audio

BlueUI decides *what* to say and produces the audio. The game decides *how* to play it. Speech and GUI sounds reach the game as one queue of audio events, collected with `bui_next_audio`.

## The voice

When built with `BLUEUI_WITH_FLITE` (the default), each context loads CMU Flite's US English "slt" voice. The voice is compiled into the library, so no files or system services are needed at run time. Synthesis runs on a background thread owned by the context, so speaking never blocks a frame.

Before synthesis, text is lightly prepared for the voice. It is lower-cased, because Flite spells out unknown upper-case words. "%" is read as "percent", and the separators `<`, `>` and `|` become pauses. The text in the audio event is the original line.

Each line is resampled to `bui_config.sample_rate` and normalised so its peak is 0.7 of full scale. Its first and last 5 ms are faded to avoid clicks. `bui_config.speech_rate` sets the speaking speed: 0 means the default of 1.15, and 1.0 is the voice's natural speed.

`bui_has_voice` reports whether the voice loaded.

## Audio events

```c
bui_audio a;
while (bui_next_audio(ui, &a)) {
  /* handle a */
  bui_audio_free(&a);
}
```

Call this every frame, after `bui_end_frame`. Lines finish synthesising in the background, so events can arrive a frame or two after the line was requested. Each event has an operation:

| `op` | Meaning |
|---|---|
| `BUI_AUDIO_PLAY` | Stop whatever speech is playing, then play this line. |
| `BUI_AUDIO_QUEUE` | Play this line after the speech already playing or queued. |
| `BUI_AUDIO_STOP` | Stop all speech now. Has no text or audio. |
| `BUI_AUDIO_SOUND` | A [GUI sound](#gui-sounds). Play it alongside anything else; it never stops speech. |

Every event except `STOP` has `text`, the line as written. With a voice, it also has:

| Field | Contents |
|---|---|
| `wav`, `wav_size` | A complete WAV file in memory: RIFF, PCM, 16-bit, mono. Pass it to any loader that reads WAV from memory. |
| `samples`, `sample_count` | The 16-bit samples inside that buffer. No copy is needed to read them. |
| `sample_rate` | The rate requested in `bui_config`. |

The event belongs to the caller, so keep it as long as needed and free it with `bui_audio_free`. To keep only the samples, copy them before freeing.

### Newest line wins

When a new interrupting line is requested before an older one has been collected, the older one is dropped. Only the newest line arrives. A player moving quickly through a menu hears where they are now, not a backlog. Queued lines are also dropped when an interrupting line or a stop arrives.

### Telling BlueUI what is playing

Set `bui_input.speech_playing` each frame to whether the game is still playing a speech line. BlueUI uses it to know when a held line has finished (see below). If the game cannot tell, leave it false. Held lines then end on the player's first input or after 15 seconds.

## Priorities

`bui_announce(ui, text, priority)` speaks a game event. Nothing is spoken while speech is off.

| Priority | Behaviour |
|---|---|
| `BUI_SAY_NORMAL` | Added to the end of this frame's line, after whatever the screen says. Use it for results of the player's actions, such as "Saved" or "Not enough money". |
| `BUI_SAY_INTERRUPT` | Spoken immediately, cutting off current speech. Use it for events outside the menu, such as "Controller disconnected". |
| `BUI_SAY_QUEUE` | Spoken after current speech finishes. Use it for prompts that should not cut anything off. |
| `BUI_SAY_HOLD` | Spoken immediately, then the screen's own speech waits. The wait ends when the line has finished playing, the player presses a key or clicks, speech is turned off, or 15 seconds pass. Use it for an opening line. |

After an opening line that already names the screen, call `bui_mark_arrived` so the screen's name and hint are not spoken again.

`bui_say(ui, text, queued)` speaks regardless of whether speech is enabled. Use it sparingly, for example to tell a player who has speech off how to turn it on. `bui_stop_speech` stops everything and sends a `STOP` event.

## Text-only mode

With `bui_config.no_voice = true`, or a build with `BLUEUI_WITH_FLITE=OFF`, nothing is synthesized. Lines arrive immediately as events with `text` set and `wav` and `samples` set to `NULL`. Use this mode to:

- forward lines to a platform screen reader or another speech engine;
- show captions;
- run tests, where it is deterministic and fast.

## Synthesizing directly

`bui_synthesize(ui, text, &out)` produces a line's WAV synchronously on the calling thread, using the same processing as spoken lines. It returns false when there is no voice. Free the result with `bui_audio_free`. It is useful for pre-rendering lines or measuring them in tests.

## GUI sounds

Besides speech, BlueUI can play short sounds that mark what happens on screen. The theme chooses a built-in *sound pack*:

| `bui_sound_pack` | Sound |
|---|---|
| `BUI_SOUNDS_NONE` | No GUI sounds. The default theme's choice. |
| `BUI_SOUNDS_SOFT` | Rounded sine tones. |
| `BUI_SOUNDS_CRISP` | Clicks and square-wave ticks. |

```c
bui_theme theme = bui_default_theme();
theme.sound_pack = BUI_SOUNDS_SOFT;
theme.sound_volume = 0.8f;      /* 0..1 */
```

The packs are generated in code when a context is created, at the context's sample rate. BlueUI ships and loads no sound files.

### Events

| Event | Played when |
|---|---|
| `BUI_SOUND_MOVE` | The focus moves, or the mouse enters an item. |
| `BUI_SOUND_PRESS` | An item is pressed. |
| `BUI_SOUND_BACK` | A back item is pressed, by Escape, B or a click. |
| `BUI_SOUND_CHANGE` | A value changes: a slider, a choice, or a custom item's `value`. While dragging, at most every 70 ms. |
| `BUI_SOUND_ON`, `BUI_SOUND_OFF` | A toggle switches on or off; a foldout opens or closes. |
| `BUI_SOUND_TAB` | The tab or page changes. |
| `BUI_SOUND_SCREEN` | A new screen is shown. |
| `BUI_SOUND_DENIED` | A disabled item is pressed or stepped. |

At most one sound plays per frame. When several events happen together, the most specific wins. For example, a toggle being clicked plays *on*, not *press*. The order of precedence is: denied, back, on/off, tab, screen, press, change, move.

Custom widgets get these sounds automatically through `bui_declare` and value-change detection. To choose a sound explicitly, call `bui_play_sound(ui, BUI_SOUND_ON)`. A requested sound replaces the one BlueUI chose for that frame.

### Playing them

GUI sounds arrive from `bui_next_audio` as `BUI_AUDIO_SOUND` events. Their `text` is the event's name, such as `"press"`. Their `wav` and `samples` hold the clip at the context's sample rate, with the theme's `sound_volume` applied. Play them on a channel separate from speech, so neither cuts the other off. GUI sounds play whether or not speech is enabled.

## Customising BlueUI's own words

The few phrases BlueUI generates itself can be replaced, for example to translate them:

```c
static const bui_strings french = {
  "Élément {i} sur {n}",
  "Narrateur activé",
  "Narrateur désactivé. Appuyez sur N pour le réactiver",
  "onglet",
  "Annulé",
  {"bouton", "onglet", "interrupteur", "curseur", "sélecteur", "texte", ""},
};
bui_set_strings(ui, &french);
```

`item_of` may use `{i}` and `{n}`. The pointers are kept, not copied, so the strings must outlive the context or the next call to `bui_set_strings`. The default `speech_off` line mentions the N key, so change it if the game binds speech differently.

The bundled Flite voice speaks English only. Translated phrases need a speech engine for that language, fed from text-only mode.
