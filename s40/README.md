# Native TyrQuake for Nokia 301

Quake port for **RM-839 firmware 02.33**. The engine, software
renderer, QuakeC VM, files, input and timing run as native ARM code; the Java ME
JAR supplies the launcher and bootstrap. Version **0.3.1** caps audio mix-ahead
at 120 ms and mixes new sounds before rendering.

Sound effects use Quake's original mixer at 11025 Hz, 16-bit stereo through
the shared SDK's native PCM backend. CD/external music is disabled. When
upgrading from a silent build, raise **Sound Volume** in Options: the old
config may contain `volume "0"`. Existing saves and settings are preserved;
large saved `_snd_mixahead` values are capped automatically. `quake.log`
reports `Native PCM` or `Audio unavailable; continuing silently`.

Based on the [libretro fork of TyrQuake 0.62](https://github.com/libretro/tyrquake/tree/9dcc3ff4ccf96d2f7aa029b738f2c90685b9257f).
Its system, video, input and file services are replaced by the S40 backend;
RetroArch is not required.

## Install

Copy `dist/RM839NativeQuake.jar` and its `.jad` together to the phone. Place
the game data on the memory card in this layout:

```
D:\quake\id1\pak0.pak
D:\quake\id1\pak1.pak
```

`pak0.pak` is required. `pak1.pak` is used for the registered game and is
optional for shareware. Leave both PAKs compressed/packed: there is nothing
to extract from them. This release contains no game data or data ZIP.

Data root search order: `D:\quake\`, `C:\predefgallery\quake\`, `C:\quake\`,
`E:\quake\`. The first root with a `PACK`-signature `id1/pak0.pak` wins.
Uppercase names work on the handset's case-insensitive filesystem.

The launcher displays native startup errors after returning. Runtime messages
go to `quake.log` alongside `id1`. Saves and `config.cfg` go inside `id1`.

## Controls

| Key | Action |
| --- | --- |
| D-pad | Forward, back, turn left/right; navigate menus |
| 5 / centre | Fire; confirm in menus |
| 1 / 3 | Strafe left/right |
| 0 | Jump |
| 2 / 8 | Look up/down; navigate up/down in menus |
| 4 | Centre view |
| 7 / 9 | Previous/next weapon |
| * / right softkey | Menu/back |
| Left softkey | Confirm |
| # | Save `id1/s40quick.sav` during play |
| 6 | Load `id1/s40quick.sav` during play |

The regular Single Player save/load slots use `s0.sav` through `s11.sav`.
For Quit, select Quit from the main menu and press 5 to confirm. `*` cancels.
Losing UI focus ends the native session and releases its tracked resources.

## Rendering and resource limits

- ARMv6 with software floating point. Integer world texture/depth arithmetic
  falls back to the original floating-point path for unsupported inputs.
  Sky, water and model drawers retain their existing arithmetic.
- 240×320 portrait display: a 240×272 world viewport above a 48-pixel HUD.
  The original HUD/menu artwork and handlers are retained, with native-size
  small text. `viewsize` stays at 100 and initial horizontal FOV is 75 degrees.
- 8 MiB engine heap, falling back to 6 MiB, and a 768 KiB surface cache.
  Stock Quake entity/model/sound limits apply; large mods are unsupported.
- A 30 FPS ceiling and visible FPS counter. The engine retains its 100 ms
  simulation-step clamp.
- Audio mix-ahead is capped at 120 ms; the firmware source queue is capped at
  about 160 ms.
- External networking, directory completion and free-text input are disabled;
  the local game server remains active.

## Build

From the repository root:

```sh
bash s40/build.sh
```

The engine and its S40 adaptations are in `common/`.

Keep this checkout beside the S40 SDK in `../main`, or set `S40_SDK_ROOT` to
that repository. Follow the SDK's [build setup](https://github.com/s40homebrew/s40/blob/main/docs/building.md).
The build needs LLVM/LLD, Arm GNU Toolchain soft-float newlib/libm/libgcc,
Python, Java 8, the legacy CLDC compiler, S40 API classes and ProGuard.
Tool paths can be overridden through `LLVM`, `CC`, `LD`, `ARM_GCC`,
`JAVA8_HOME`, `LEGACY_JAVAC`, `SDK_CLASSES` and `PROGUARD_JAR`.
Normal builds do not require firmware or game assets.
`S40_OPT_LEVEL=2` and `S40_RENDER_LTO=0` disable the stronger compiler options
independently for comparisons; defaults are 3 and 1 respectively.
`S40_FIXED_SPANS=0` disables the integer texture/depth paths for comparison.

The build generates firmware veneers and launcher fingerprints from `docs/abi.json`,
packs relative ARM relocations, preverifies the Java classes and checks the package.

## Source and firmware bindings

`native/quake_s40.c` supplies system/display/input entry points; `retro_s40.c`
provides buffered file operations; `ui_s40.c` adapts the HUD and menu drawing;
`span_s40.c` and `span_math_s40.h` implement integer world texture/depth math;
`audio_s40.c` connects the original mixer to the shared PCM stream.

Core firmware bindings are in `docs/abi.json`; audio bindings are in the SDK's
`homebrew/src/native/audio/devices/rm839_0233_abi.json`. See the
[Doom firmware notes](https://github.com/s40homebrew/doom/blob/s40/s40/rm839/docs/firmware.md)
for the common bootstrap and ABI contracts, and the SDK's
[audio guide](https://github.com/s40homebrew/s40/blob/main/homebrew/docs/audio.md)
for PCM ownership. License notices are included in the JAR and `licenses/`.
