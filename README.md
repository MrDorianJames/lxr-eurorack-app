# Custom firmware for the Erica Synths LXR Eurorack module

Unofficial custom firmware for the LXR V2 Eurorack drum module (STM32F765). It is based on
[Erica Synths' open-source firmware](https://github.com/erica-synths/lxr-eurorack-app) and stays close
to it: the sound engine is unchanged, and everything new is about saving, loading, mixing and editing
sounds faster while playing.

> **Not official firmware.** This is a modified version, not made, approved or supported by Erica
> Synths. The start-up screen shows **"custom fw r17"** so you can always tell which firmware is
> running. Installing it is at your own risk. The bootloader is never changed, so you can always go
> back to the official firmware with a normal SD card update.
>
> **Licence and trademarks:** the firmware source code is free software under the GNU GPL version 3
> ([LICENSE](LICENSE)). The names **"Erica Synths" and "LXR"** and the **LXR hardware** are the property
> of Erica Synths and are **not** covered by this licence. See [NOTICE.md](NOTICE.md).

## Compatibility

- **Kits** are saved in the stock format and load on both the stock and the custom firmware.
- **Voice presets** are stored in a new `/voices` folder on the SD card, which the stock firmware
  ignores.
- **Global settings** (routing and mixer) are stored after the stock settings in `glo.cfg`. The stock
  firmware reads only its own part and ignores the rest, so you can switch firmware back and forth.
- The DC-coupled outputs stay disabled, as in the stock firmware.

## Features

### Voice presets

Save and load the sound of a single voice, without loading a whole kit.

- Sounds are stored per voice type: drums 1-3 share one list, and the snare, cymbal and hi-hat each
  have their own. A kick saved from drum 1 can be loaded into drum 2 or 3.
- 128 slots per type, each with an 8-character name. Files live in `/voices/drum`, `/voices/snare`,
  `/voices/cymbal` and `/voices/hihat` (for example `07-KICK8.vce`).
- A voice sound is everything that makes up the sound of that voice: oscillator, envelopes,
  modulation, filter and so on. Volume, pan, output routing, the 1V/oct input and the CV mod matrix
  belong to the kit and are not changed by loading a voice.
- **Loading is glitch-free:** a new sound is applied as soon as the voice is quiet, never in the
  middle of a hit (after at most half a second, like a kit load).
- All voice files are read into memory at start-up ("Loading voices"), so loading during a
  performance never waits for the SD card.

### Load/Save menu

Press **Load/Save**. The menu has two pages; turn the encoder past the last column to reach page 2.

```
SV  sv  ld  ld  >        ld  rv  rv      <
VC  kit vc  kit          kvc vc  kit
```

| Column | What it does |
|---|---|
| **sv vc** | Save the selected voice's sound. Knob 1 moves the cursor, knob 2 switches upper/lower case, knob 3 picks the letter, knob 4 adds a number. The encoder picks the slot (starts on the next empty one), push saves (asks before overwriting). |
| **sv kit** | The stock kit save page. If Morph is up, it first asks whether to save the morphed sound. |
| **ld vc** | Scroll the saved sounds for the selected voice with the encoder or knob 3, push to load. |
| **ld kit** | The stock kit load page. |
| **ld kvc** | Load one voice out of any saved kit: scroll the kits with the encoder or knob 1, push loads the selected voice's sound from that kit. |
| **rv vc** | Revert voice: asks first, then the voice goes back to the sound you last loaded or saved for it, or else to how it is in the saved kit. |
| **rv kit** | Revert kit: asks first, then reloads the current kit as it was last saved. |

- The knob under a column opens it directly.
- **Load/Save** steps back one level, and from the overview leaves to the Perform page.
- After loading, "Voice loaded" or "Kit loaded" shows with the slot and name for a second, then the
  module goes to the Perform page.

### Undo

**Shift + Load/Save** undoes the last edit of the selected voice, or of the kit-wide settings such as
FX.

- A short message shows what was undone, for example `Undo  Drm1` / `Osc  Coarse`, or
  "nothing to undo". Any knob, the encoder or a button ends it early.
- Up to 64 steps. One continuous knob turn counts as one step.
- Loading a kit clears the history; loading a voice clears that voice's steps.
- Not undone: the CV mod matrix, morph, and the global mixer and routing.

### Mixer (Perform menu)

Global volume and pan for each voice, kept across kit loads.

Open it from Perform page 1 with knob 4 (under `mix`), by selecting `mix` with the encoder and
pushing, or with **Shift + Perform** from anywhere. **Perform** steps back.

```
VOL dr1 dr2 dr3 >        VOL snr cym hat <
    100  90  80              95 100  70
```

- Knob 1 picks **VOL**, **PAN** or **SRC**. Knobs 2-4 edit the three voices on screen; turn the
  encoder to reach the other three.
- **SRC** decides where a voice's level and pan come from: **kit** (as stored in the kit, the stock
  behaviour) or **glo** (the values set here, the same for every kit). Switching to glo starts from
  the current values, so nothing jumps.
- Changes are saved to the SD card when you leave the mixer.

### Config menu and global routing

Open with **Shift + Edit**. **Edit** steps back from a sub-page, then leaves.

```
scr cv  rtg nfo
 on kit  ▼   ▼
```

| Column | What it does |
|---|---|
| **scr** | Screensaver on/off. |
| **cv** | CV mod routing from the kit or global (stock setting, moved here). |
| **rtg** | Routing page: fix each voice (dr1 dr2 dr3 snr cym hat) and the FX bus output to an output. |
| **nfo** | Shows the custom firmware revision and the stock version it's based on. |

On the routing page, each voice can be set to **kt** (follow the kit, the stock behaviour), a fixed
output (**St1 St2 L1 R1 L2 R2**), or **FX** to send it through the effect. The FX bus itself can be
fixed to an output. A fixed choice always wins over what the kit says, for every kit you load.

On the voice pages, a value replaced by these global settings is marked with `*` (for example the
output of a voice fixed on the routing page, or the volume of a `glo` voice in the mixer).

### Smaller changes

- Changing the FX type clears the effect's memory, so there are no leftover echoes or noise bursts
  from the previous effect.
- Leaving the Load/Save menu, and the stock "kit saved" message, go to the Perform page instead of
  the voice edit page.
- Sub-pages are marked with a filled down triangle (`▼`).

## Installing

1. Build the firmware (see [Building](#building)): you get `build/release/LXRE_V2_V1.51.img`.
2. Copy the `.img` file to the top folder of a FAT32 SD card and delete any older `LXRE*.img` files
   from it.
3. With the module powered off, insert the card, hold down the encoder button and power on. Keep
   holding until "LXR Updater" appears.
4. Wait for "Update OK!". The module restarts, and the start-up screen shows "custom fw r17".

The updater (bootloader) is never changed by an update, so a failed or interrupted update can simply be
repeated. To go back to the official firmware, do the same with Erica Synths' update file. Your kits
work on both.

## Building

You need [CMake](https://cmake.org/download/) 3.21+, [Ninja](https://ninja-build.org/), the
[Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
(`arm-none-eabi-gcc`, tested with 14.3.rel1 and GCC 16) and
[Python 3](https://www.python.org/downloads/), all on `PATH`.

- Arch / CachyOS: `sudo pacman -S cmake ninja python arm-none-eabi-gcc arm-none-eabi-newlib`
- Debian / Ubuntu: `sudo apt install cmake ninja-build python3` plus Arm's toolchain, with its `bin`
  folder on `PATH`.
- macOS: `brew install cmake ninja && brew install --cask gcc-arm-embedded`. Don't use
  `brew install arm-none-eabi-gcc`: it lacks the C library and the build fails with it.
- Windows: `winget install Kitware.CMake Ninja-build.Ninja Python.Python.3.12` plus Arm's installer
  (tick "Add path to environment variable").

Then, in this folder:

```sh
cmake --preset release
cmake --build --preset release
```

The update file is written to `build/release/LXRE_V2_V<version>.img`. The version number is the stock
one it's based on; the custom revision is set in [Core/Inc/customVersion.h](Core/Inc/customVersion.h).

| Option | Effect |
|---|---|
| `cmake --preset debug` | Debug build (`-O3 -g3`, `DEBUG` defined) in `build/debug/` |
| `-DARM_TOOLCHAIN_DIR=<bin folder>` | Use a toolchain that isn't on `PATH` |
| `-DLXR_ITCM_OPT=-O2` | Lower optimisation for code in the 16 KB ITCM, if a change or compiler makes it overflow at `-O3` |
| `-DLXR_GENERATE_LISTING=ON` | Also write a disassembly listing (`.list`) |

To start from scratch (e.g. after switching compilers), delete the `build/` folder.

### Tests

The new menus and the voice preset code are written so they can also be tested on a PC. With a normal
C compiler (`gcc`):

```sh
make -C tests/host
```

This runs seven test programs (voice tables, SD storage, glitch-free loading, the Load/Save menu, the
config menu and routing, the mixer, and undo). Each prints `ALL TESTS PASSED`.

## Programming with a debug probe

Not needed for updates. With an ST-Link and [OpenOCD](https://openocd.org/) on `PATH`:

```sh
cmake --build --preset release --target flash        # firmware only, bootloader untouched
```

For blank devices, configure with the Eurorack bootloader, either from source
(`-DLXR_BOOTLOADER_DIR=path/to/LXR_V2_STM32F765_Bootloader`, built with `LXR_BOOTLOADER_EURORACK`) or
prebuilt (`-DLXR_BOOTLOADER_HEX=path/to/LXR_V2_Eurorack_Bootloader.hex`). The build then also writes
`LXRE_V2_V<version>_full.hex/.bin` (whole flash from 0x08000000) and enables the `flash_full` target.

> **Warning:** `flash_full` overwrites the bootloader and asks you to type `yes` first (or set
> `LXR_CONFIRM_BOOTLOADER_OVERWRITE=yes`). If it is interrupted, the module can only be recovered with a
> debug probe.

## Memory map

| Region | Address | Size | Contents |
|---|---|---|---|
| FLASH | 0x08000000 | 32 KB | Bootloader |
| FLASH | 0x08008000 | 992 KB | This firmware |
| ITCM | 0x00000000 | 16 KB | `IN_ITCM_RAM` DSP code |
| DTCM | 0x20000000 | 128 KB | `INDTCM`/`INDTCMZ` data, DMA buffers, stack |
| RAM | 0x20020000 | 384 KB | `.data`, `.bss`, heap |

## Source layout

New code for the custom firmware:

| Files | Contents |
|---|---|
| `Core/Src/Preset/voicePreset.c` | Which kit parameters make up each voice's sound |
| `Core/Src/Preset/voiceStore.c` | Voice preset files on the SD card and the RAM cache |
| `Core/Src/Preset/voiceLoad*.c` | Glitch-free voice loading |
| `Core/Src/Preset/undo.c` | Undo history |
| `Core/Src/menu/lsMenu*.c` | Load/Save menu |
| `Core/Src/menu/cfgMenu*.c` | Config menu and routing page |
| `Core/Src/menu/mixMenu*.c` | Mixer |
| `Core/Src/globalMix.c` | Global routing and mix, saved in `glo.cfg` |
| `tests/host/` | PC tests for the above |

The `*_glue.c` files connect each module to the rest of the firmware, so the modules themselves can be
tested on a PC. Changes to stock files are small hooks into these modules.

The stock layout:

Originally an STM32CubeIDE project; the CubeMX `.ioc` was removed because regenerating code from it
overwrites application code. Pins, clocks and peripherals live in `Core/Inc/main.h`, `Core/Src/main.c`
and `Core/Src/stm32f7xx_hal_msp.c`. Source files are listed explicitly in the `CMakeLists.txt` files
(`Core/` is split into `lxr_platform`, `lxr_io`, `lxr_ui`, `lxr_preset`, `lxr_dsp`, `lxr_engine` and
`lxr_midi`); add new files there.

## Licence

Copyright (C) 2012-2026 Erica Synths; custom firmware changes by MrDorianJames. Licensed under the [GNU GPL version 3](LICENSE) (GPL-3.0-only)
with the additional permission and terms in [NOTICE.md](NOTICE.md): modified firmware must be marked as
modified and must not be presented as official Erica Synths firmware. The trademarks and the hardware
are not licensed. Third-party components (STMicroelectronics, Arm, ChaN FatFs) keep their own licences,
see [NOTICE.md](NOTICE.md#third-party-components).
