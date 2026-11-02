![js8](docs/screenshot.png)

# JS8

A small JS8 receiver for Omarchy: audio from a radio interface, a full-screen
vector console drawn with raylib, and JS8 Normal decodes from
[gfsk8-modem](../gfsk8-modem). Qt owns the window and the Omarchy lifecycle;
the console's vector status strip, spectrum trace, waterfall, ticker, and help
are drawn by raylib and follow the Omarchy accent live.

This is a bootstrap, not a JS8Call replacement. It receives and decodes; it
does not transmit, key a radio, or implement CAT control. Normal (15 s) is the
only submode wired up so far.

## What it does

- Captures the default (or selected) audio input at 48 kHz and decimates it to
  the modem's 12 kHz, using the same FIR as gfsk8-modem's `js8rx` example.
- Draws a 0–3 kHz waterfall with a 4096-point FFT, about 44 seconds of history.
- Draws a vector status strip, live spectrum trace, waterfall frame/grid, and
  a three-line decode ticker with the Hershey Roman Simplex stroke font.
- Decodes JS8 Normal at each 15-second UTC boundary on a worker thread, so the
  waterfall keeps moving while the modem searches.
- Shows the decoded text, SNR, audio offset, and UTC time, and appends each
  decode to a log.
- Includes a synthesized demo signal so you can see the waterfall and decoder
  work without a radio. The status strip says SIM and every demo decode is tagged SIM.
- Remembers the input device, callsign, grid, dial frequency, and whether the
  receiver was running.
- Rebuilds the palette and primary vector colour when the Omarchy accent
  changes. The window title and C++ status line follow the selected language;
  the console labels are short English/numeric abbreviations. The app opens
  fullscreen.

## Keyboard

| Key | Action |
| --- | --- |
| `Space` | Start or stop the receiver |
| `D` | Toggle the demo signal |
| `I` | Cycle the audio input device |
| `R` | Reload audio input devices |
| `?` | Show or hide the shortcut list |
| `Esc` | Close the shortcut list |
| `Q` | Quit |
| `Ctrl+Q` | Quit |

The console has no text fields; callsign, grid, and the input device are
stored in `~/.config/js8/settings.json`, and `I` cycles the available inputs.

## Requirements

- Omarchy with `qmake6`, `make`, a C++ compiler, and Qt 6.
- `portaudio` (installed on Omarchy).
- A [gfsk8-modem](../gfsk8-modem) checkout beside this project (or set
  `GFSK8_MODEM_DIR`).
- raylib is fetched and built statically by `bin/build` (raylib 6.2). There is
  no raylib package dependency at runtime.
- Link-time X11 libraries: `libx11`, `libxrandr`, `libxinerama`, `libxcursor`,
  `libxi`. raylib's static archive includes its GLFW backend, which references
  them even though this app never calls `InitWindow`.

## Build and run

```sh
./bin/build
./build/js8 --demo
```

If the modem checkout is somewhere else:

```sh
GFSK8_MODEM_DIR=/path/to/gfsk8-modem ./bin/build
```

Run the tests (theme, language, settings, FFT, and a full modem
pack → modulate → decode round trip):

```sh
./bin/test
```

## Install

The Omarchy way, as a local package:

```sh
./bin/install
```

`bin/install` builds the package with `makepkg -fsi`, so it needs your sudo
approval.

If you would rather not install a package, `bin/install-user` puts the same
binary, desktop entry, and icon under your home directory, with no root:

```sh
./bin/install-user
```

Either way, JS8 appears in the Omarchy launcher (`Super + Space`).

## Data

| Path | Contents |
| --- | --- |
| `~/.config/js8/settings.json` | Callsign, grid, input device, dial frequency, last monitor state |
| `~/.local/share/js8/decodes.log` | One tab-separated line per decode |

Both are written atomically where it matters; the log is append-only. Demo
decodes are logged with the same format, so remove the log if you only want
real traffic.

## How it is put together

| File | Role |
| --- | --- |
| `src/main.cpp` | App setup, command line, screenshot harness |
| `src/backend.*` | State, timers, settings, spectrum rows, decode log |
| `src/audio.*` | PortAudio capture, 4:1 FIR decimator, 60 s ring, demo source |
| `src/decoderworker.*` | Worker thread that runs the gfsk8 decoder once per period |
| `src/spectrum.*` | 4096-point radix-2 FFT to 1024 bins over 0–3 kHz |
| `src/strokefont.*` | Parser for the Hershey `.jhf` single-stroke vector font |
| `src/tacticaldisplay.*` | Full-screen `QQuickFramebufferObject` vector console rendered by raylib's rlgl |
| `src/Main.qml` | Window, app shortcuts, and the single `TacticalDisplay` item |
| `src/theme.*`, `src/translator.*` | Live Omarchy accent/rounding and language |

### Why raylib lives inside Qt

raylib has no supported API for drawing into somebody else's window, so the
app does not try to run a second GLFW window beside Qt. `TacticalDisplay` is a
`QQuickFramebufferObject`; its renderer calls raylib's own renderer (`rlgl`)
with Qt's current OpenGL context and draws the entire console: vector lines,
the Hershey text, and the waterfall texture. raylib does the visible work, and
Qt keeps the window, theme, translations, and launcher integration.

The modem runs on its own `QThread` because a full 60-second search can take
long enough to stutter the waterfall on the GUI thread. `gfsk8::Decoder` is
not thread-safe, so there is exactly one instance and one decode thread.

## Demo signal

The demo is built with `gfsk8::pack()` + `gfsk8::modulate()` at 800, 1500, and
2300 Hz, mixed with a noise floor. It starts at each 15-second period and is
decoded by the real decoder, so it exercises the same path as a radio signal.
It never pretends to be a received station: the status strip says `SIM` and
each demo decode is tagged `SIM`.

## Typography

The tactical console font is **Hershey Roman Simplex**
(`src/fonts/hershey/rowmans.jhf`), parsed by `src/strokefont.*` and drawn as
single-stroke polylines rather than filled glyphs. The original Hershey glyphs
are public domain; the `.jhf` distribution is permissively licensed with the
acknowledgments reproduced in `src/fonts/hershey/NOTICE` and `LICENSE`.

A font sample:

![Hershey Roman Simplex preview](docs/font-preview.png)

The font covers ASCII 32–126. The console uses short English/numeric labels
(`FREQ`, `MODE`, `RX`, `SNR`, `UTC`); a translated UI can live in the settings
overlay or use a separate text font if full Unicode is needed.

## Screenshot

`docs/screenshot.png` was captured with the fallback accent (`#FFD60A`) and
made-up demo content, so it does not depend on the current theme. It is the
fullscreen console after a full waterfall period:

```sh
HOME=$(mktemp -d) ./build/js8 --demo --screenshot docs/screenshot.png --wait 46
```

## Scope and next steps

The physical layer, waterfall, settings, and receive UI are in place. The
obvious next steps are the other four submodes, the full JS8Call message layer,
CAT/dial control, and transmit with an explicit confirmation. None of those
are faked here.

## License

GPL-3.0-only. `gfsk8-modem` is GPL-3.0 (derived from JS8Call-improved, which
is derived from JS8Call by Jordan Sherer KN4CRD and WSJT-X by K1JT). The theme
and translator patterns are adapted from Omacut and Monologue (MIT, David
Heinemeier Hansson); the MIT notice is reproduced in `LICENSE`.
