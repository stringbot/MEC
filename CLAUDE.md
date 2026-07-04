# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

MEC (Micro Expression Control) is a lightweight C++11 environment for expressive MIDI/OSC controllers (Eigenharp, Soundplane, Push 2, etc.), designed to run on small SoCs (Raspberry Pi, BeagleBone/Bela, Organelle) without a host computer. The core is `mec-api`, a shared library; several "integration apps" are built on top of it. It is the controller backbone for related projects like Orac.

## Build

```bash
git submodule update --init --recursive   # required first; deps live in external/ as submodules

mkdir build && cd build
cmake ..              # Linux / generic
cmake .. -GXcode      # macOS (then: xcodebuild -project mec.xcodeproj -target ALL_BUILD -configuration MinSizeRel)
cmake --build .
```

Build outputs go to `build/release/lib` (shared libs) and `build/release/bin` (executables, incl. tests).

System deps: CMake, libusb (`libusb-1.0-0-dev` / `brew install libusb`), and on Linux `libasound2-dev` + `libcairo2-dev` (cairo is also needed for Push 2). On macOS `brew install cairo`.

`mec-max` (Max/MSP external) and `mec-vst` (VST/AU) are **not** built by the top-level CMake — build them separately via their own projects. For VST, after building, copy dylibs from `build/release/lib` into `MEC.vst/Frameworks`.

### Build flags (passed as `-DFLAG=1` to cmake)

Device support is toggled at configure time. Default-on devices disabled with `DISABLE_*`:
`DISABLE_SOUNDPLANELITE`, `DISABLE_EIGENHARP`, `DISABLE_PUSH2`, `DISABLE_OSCDISPLAY`, `DISABLE_LIBUSB`.
Default-off devices enabled with: `NUI`, `TTUI`, `ZERORAC`, `PUSH2`.
Platform/board flags: `SALT`, `PEPPER` (both imply `BELA`), which add Xenomai/Cobalt real-time linkage. These `DISABLE_*`/define macros are also injected into the compile so code is `#ifdef`-guarded accordingly.

Windows is experimental (MinGW-w64 + pthreads); libusb and most USB devices are force-disabled there.

## Tests

Tests are small standalone executables (no framework), built as part of the normal cmake build:

```bash
./build/release/bin/t_scale     # mec-api: t_scale, t_voice, t_surface
./build/release/bin/t_kontrol   # mec-kontrol
```

Source: `mec-api/tests/`, `mec-kontrol/tests/`. `mec-app/tests/` holds `.pd`/`.maxpat` patches for manual integration testing, not unit tests.

## Run

```bash
./build/release/bin/microexpression [config.json]   # Linux; "MicroExpression" on macOS
```

Configuration is a single JSON file (default `mec.json` in cwd, or first CLI arg). `resources/mec.json` is the documented sample — most keys are optional and just show defaults. JSON has no comments: by convention a key is "commented out" by prefixing it with `_`. Watch trailing commas.

Linux needs realtime/udev setup: add `@audio` rtprio/memlock limits, add user to `audio` group, and `sudo cp resources/*.rules /etc/udev/rules.d/` then reload udev (see `docs/RUNNING.md`).

### macOS kernel-panic caveat (important)

Abruptly killing MEC on macOS corrupts the USB driver and triggers a **kernel panic**. Always shut down gracefully: send OSC `oscsend localhost 9000 /t3d/command s shutdown`, or press Ctrl-C **once** (a handler does clean shutdown — do not hammer it). This matters even when debugging in Xcode.

## Architecture

Data flows: **input device → surface translation → mec-api callbacks → app → output (MIDI/OSC)**.

- **`mec-api/`** — the core shared library. `MecApi` (`mec_api.h`) loads devices from JSON config, and an app registers an `ICallback`/`Callback` then calls `process()`. Callbacks fire **synchronously** inside `process()`, which is expected to run on the audio thread, so callback code must be non-blocking (no allocation/locks). Two callback styles exist: the original touch/control callbacks (`touchOn/Continue/Off`, `control`, `mec_control`) and a newer experimental `Surface`/`Touch` API.
  - `devices/` — one class per input/output device (`mec_eigenharp`, `mec_soundplane`, `mec_push2`, `mec_mididevice`, `mec_osct3d`, `mec_kontroldevice`, `mec_nui`, `mec_oscdisplay`). All implement the `mec_device.h` interface. Push 2 and NUI have their own subdirs for mode/param/menu UI logic.
  - `processors/` — output/transform stages (`mec_midi_processor`, `mec_mpe_processor`) that map touches to MIDI (incl. MPE).
  - `mec_surface` / `mec_surfacemapper` — translate device coordinates into surface `Touch`es (x/y/z plus r/c "string + position along string"); enables splits where one physical device looks like several to the app.
  - `mec_scaler`, `mec_voice`, `mec_msg_queue` — note/scale mapping, voice allocation, lock-free message passing (moodycamel queue).

- **`mec-app/`** — the primary standalone console app (`microexpression`). `mecapi_cmd.cpp` is the entry point; wires `MecApi` to MIDI/OSC output.

- **`mec-kontrol/`** — parameter/patch system (modules, racks, parameters), independent of the touch pipeline. Has its own OSC broadcaster/receiver for remote control and a `pd/` Pure Data external (`kontrol`, `kontrolhost~`) for Organelle. `api/` is target-independent core. Used by Push 2 / NUI for parameter navigation. Being merged in from a former standalone project; still in active development.

- **`mec-utils/`** — shared helpers: `mec_prefs` (JSON config wrapper over cJSON), `mec_log`, general utils.

- **`mec-vst/`**, **`mec-max/`** — JUCE VST/AU and Max/MSP integrations (built separately).

- **`external/`** — dependencies, mostly git submodules: `SoundplaneLite` and `EigenLite` (stripped/extended forks of Madrona Labs / Eigenlabs device code), `NuiLite`, `TTuiLite`, `ArduiPi_OLED`, plus vendored oscpack, portaudio, rtmidi, cJSON, libusb, moodycamel.

### Note on Eigenharp PICO

The PICO decoder is closed-source; the source tree ships a non-functional **dummy** implementation. Prebuilt real decoder libs for various platforms live in `resources/` and must replace the dummy at link/deploy time.

## Conventions

- C++11, formatted via `.clang-format` (4-space indent, based on a customized style — run clang-format before committing).
- Logging via `LOG_0`/etc. macros from `mec_log.h`, not raw `printf`/`cout`.
- New device support should follow the `mec_device.h` interface and be wired into `mec-api/CMakeLists.txt` behind a `DISABLE_*`/enable flag.

## CI

`.github/workflows/build.yml` runs **macOS-only** and only on **tag push** (not every commit, to limit load). Linux/Windows CI are stubbed out (Linux blocked on sourcing libusb). Tagging triggers a release build.
