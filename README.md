# Classic to Pro Controller

An [Aroma](https://aroma.foryour.cafe/)/[WUPS](https://github.com/wiiu-env/WiiUPluginSystem) plugin
for the Wii U that makes a **Wii Classic Controller** (or **Classic Controller Pro**), connected via
a Wii Remote, report itself to games as a **Wii U Pro Controller**. This lets you play games that
added Pro Controller support but never added Classic Controller support.

## How it works

Games talk to non-GamePad controllers through `padscore.rpl`'s `KPAD`/`WPAD` API. Every sample the
system hands back is a `KPADStatus` struct with an `extensionType` byte telling the game which
controller is attached, plus a union holding the actual button/stick data for that controller type.

It turns out the Classic Controller and Pro Controller members of that union are laid out
identically, and use identical bit values for every button they share (`A`/`B`/`X`/`Y`/`L`/`R`/
`ZL`/`ZR`/`+`/`-`/`HOME`/D-pad, plus both analog sticks). A Classic Controller sample is therefore
already, byte-for-byte, a valid Pro Controller sample — the only real difference is the
`extensionType` label itself and 8 trailing bytes that mean "analog trigger position" for Classic
but "charging/wired flags" for Pro.

So this plugin doesn't do any button-mapping guesswork. It hooks four functions in `padscore.rpl`
(`KPADRead`, `KPADReadEx`, `WPADProbe`, `WPADSetExtensionCallback`) and, whenever a sample or query
reports `WPAD_EXT_CLASSIC` / `WPAD_EXT_MPLUS_CLASSIC`, relabels it as `WPAD_EXT_PRO_CONTROLLER`
(clearing the two fields that don't carry over). Because it's a relabel rather than a translation,
there's very little that can go wrong — no per-game button tables, no drift between "what the
controller sent" and "what the game sees".

**Scope / limitations:**
- This only helps games that check for the Pro Controller via the standard `KPAD`/`pro` struct
  (the vast majority do, since that's the only documented way). A game that reads only the
  Wii-Remote-style "core" button bits at the top of `KPADStatus` — which the Pro Controller doesn't
  meaningfully populate either — won't be affected either way.
- It cannot invent rumble/motion features the Classic Controller doesn't have.
- It does not touch the GamePad (`VPAD`) at all.

## Building

You'll need [devkitPro](https://devkitpro.org/) with the `wut`, `wums`, and `wups` packages, or
just use the provided `Dockerfile` (recommended — it pins known-good toolchain/library versions):

```bash
docker build -t classic-to-pro-builder .
docker run --rm -v "$PWD":/project classic-to-pro-builder make
```

This produces `classic_to_pro.wps` in the project root.

If you'd rather build natively, install devkitPro's `wut`, `wums-tools`/`libmappedmemory`,
`libnotifications`, and `wups`, `export DEVKITPRO=/opt/devkitpro` (or wherever it's installed),
then run:

```bash
make
```

## Installing

1. Copy `classic_to_pro.wps` to `sd:/wiiu/environments/aroma/plugins/` on your console's SD card
   (create the folder if it doesn't exist).
2. Boot into Aroma as usual. The plugin loads automatically for every title.
3. Pair your Wii Remote + Classic Controller as normal (via the Wii U's Bluetooth sync, same as
   pairing a Pro Controller/Wii Remote).

## Using it

Open the Aroma plugin config menu in-game (default combo: hold **L + Down + Select** on the
GamePad, or check your Aroma build's configured combo) to toggle the plugin on/off. It's enabled by
default. A small on-screen notification confirms when a Classic Controller has been detected and is
being reported as a Pro Controller for the current title.

## Files

- `src/input_patches.cpp` — the actual hooks and the classic→pro relabeling logic.
- `src/config.cpp` / `src/config.h` — the in-game config menu (enable/disable toggle).
- `src/main.cpp` — plugin metadata and lifecycle hooks.
- `src/globals.cpp` / `src/globals.h` — shared plugin state.
