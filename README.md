# TyrQuake on Nokia Series 40

Native Quake for **Nokia 301 RM-839, firmware 02.33**, with a 240×320 display,
keypad controls and stereo sound effects. CD music and networking are disabled.

## Build

Keep this fork in `tyrquake/` beside the [S40 homebrew repository](https://github.com/s40homebrew/s40)
in `main/`. The build finds the SDK at `../main`; for a different layout, set
`S40_SDK_ROOT` to the SDK repository’s absolute path. Follow its
[dependency setup](https://github.com/s40homebrew/s40/blob/main/docs/building.md),
then run from this repository’s root:

```sh
bash s40/build.sh
```

Outputs: `s40/dist/RM839NativeQuake.jar` and its matching `.jad`.
The shared SDK supplies the PCM audio runtime and firmware adapter.
Builds require no firmware dumps. See the [port guide](s40/README.md)
for controls, configuration and build options.

## Play

Put `pak0.pak` and optional registered-game `pak1.pak` in `D:\quake\id1\`,
leave them packed, and install the JAR/JAD. Press 5 or centre to start.
Saves and configuration use the selected `id1/` folder; `quake.log` goes beside it.

D-pad moves/turns; 1/3 strafes; 5 fires; 0 jumps; 2/8 looks up/down; 4 centres
the view; 7/9 changes weapons; * or right softkey opens the menu; left softkey
confirms. # quicksaves and 6 quickloads. The regular save/load menus also work.
Raise **Sound Volume** in Options after upgrading from a silent build.

## Source and licenses

Based on [libretro/TyrQuake](https://github.com/libretro/tyrquake).
S40 code is in `s40/`; edit `s40/src/QuakeLauncher.java.in` for the launcher.
Quake, TyrQuake and the derived port are [GPL-2.0-or-later](LICENSE.txt).
Independent shared S40 code is [MIT](s40/LICENSE); other library notices
are in `s40/licenses/`.
