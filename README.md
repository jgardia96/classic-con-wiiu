# Classic to Pro Controller

Disclaimer: This was built with Claude Sonnet 5 for those who are against the development of Homebrew with AI

An [Aroma](https://aroma.foryour.cafe/)/[WUPS](https://github.com/wiiu-env/WiiUPluginSystem) plugin
for the Wii U that makes a **Wii Classic Controller** (connected via a Wii Remote) report itself to
games as a **Wii U Pro Controller** — for games that added Pro Controller support but never added
Classic Controller support.

## How it works

Hooks every `padscore.rpl` function games use to read controller state
(`KPADRead`/`KPADReadEx`, `KPADGetUnifiedWpadStatus`, `WPADProbe`, `WPADSetExtensionCallback`,
`WPADSetDataFormat`/`WPADGetDataFormat`, `WPADRead`) and relabels Classic Controller samples as Pro
Controller. Buttons and analog sticks in the calibrated `KPADStatus` struct are bit-identical
between the two, so those are a straight relabel. The raw hardware report formats
(`KPADGetUnifiedWpadStatus`, `WPADRead`) use different layouts and a different stick range
(Classic `[-512,511]` vs Pro `[-2048,2047]`, exactly 4x), so those get an actual translation instead
of a relabel.

**Limitations:** only helps games that read controllers through the documented KPAD/WPAD API
(nearly all do). Can't add rumble/motion features the Classic Controller doesn't have. Doesn't
touch the GamePad (`VPAD`).

## Building

Needs [devkitPro](https://devkitpro.org/) (`wut`, `wums`, `wups`), or just use the provided
`Dockerfile`:

```bash
docker build -t classic-to-pro-builder .
docker run --rm -v "$PWD":/project classic-to-pro-builder make
```

Produces `classic_to_pro.wps` in the project root. For a native build, install the packages above,
set `DEVKITPRO`, and run `make`.

## Installing (Assuming WiiU has Aroma Already)

1. Copy `classic_to_pro.wps` to `sd:/wiiu/environments/aroma/plugins/`.
2. Boot Aroma — it loads automatically for every title.
3. Pair your Wii Remote + Classic Controller like you would a Pro Controller.

## Using it

Toggle it on/off from the Aroma plugin config menu in-game (enabled by default). A notification
confirms when a Classic Controller is detected and being reported as Pro for the current title.

## Files

- `src/input_patches.cpp` — the hooks and classic→pro translation logic
- `src/config.cpp` / `src/config.h` — in-game config menu
- `src/main.cpp` — plugin metadata and lifecycle hooks
- `src/globals.cpp` / `src/globals.h` — shared plugin state
