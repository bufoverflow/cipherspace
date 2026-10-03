# Cipherspace — Chapter 1

A native Game Boy Color game for the ModRetro Chromatic. Version 0.3.0 rebuilds the opening around visible actions and a first encounter with the stranded alien.

## Play

Use the D-pad to select a letter in **YOUR NAME**. **A** adds, **B** erases, and **Start** finishes. The pilot wears your chosen name on her spacesuit. All action prompts separate a button badge from the action text.

| Button | Action |
| --- | --- |
| D-pad | Choose, steer, or walk |
| A | Perform the displayed action |
| B | Undo a puzzle choice |
| Select | Free word and puzzle help |
| Start | Finish name entry, or pause during play |

In a puzzle, Left/Right selects a letter or symbol. Up/Down moves the linked symbol-and-answer cursor. A enters your choice; repeated symbols fill together. B undoes. Blue connects the active pair; amber marks the choice you will enter. A wrong answer stays selected and editable. Solving a puzzle triggers the related action in the scene.

After the ten-second countdown and powered ascent, steer toward the Moon. Line up over the south-pole landing light, then press A when LAND appears. Walk toward the alien and press A when MEET appears. Try greeting her, notice her response, and use the shared symbol key to send a greeting she understands.

The pause menu controls sound and returns to the title. New Trip requires holding A+B for two seconds. Progress saves at choices and milestones. This redesign uses version 3 saves and deliberately ignores older saves: it starts fresh from name entry. Its own saved trips can resume across play sessions.

## A chapter built around actions

1. **Suit up:** meet a confident girl pilot wearing the chosen name.
2. **Discover:** inspect the backyard ship and zoom into its encoded door note.
3. **Open and repair:** decode OPEN, watch the hatch open, fit the loose crystal into its socket, and wake the ship.
4. **Find a reason to fly:** see the empty seat and play a cockpit recording. The owner visited the Moon’s south pole, escaped a malfunction, and lost her ship to Earth. The next note encodes MOON.
5. **Launch:** climb into the seat, activate the console, count down from 10, and rise from the backyard through clouds into space.
6. **Rescue:** steer, align with a safe landing spot, and walk toward a pulsing signal.
7. **Make contact:** English greetings leave the alien puzzled. Her response points to the shared symbols; encoding HI earns a warm response and a new friend.

The Mars-to-distant-home journey and earned transmission hints remain future chapters. Chapter 1’s guidance is free and never fills the puzzle automatically.

## Learning basis

The exercises introduce substitution keys, repeated symbols, decoding, and encoding. They adapt the concepts in the CIA's [Codes and Code Breaking lesson](https://www.cia.gov/spy-kids/static/c2afecbc689adbc329e427cf2e8e2dd9/Lesson-plan-codes-and-code-breaking.pdf) and [Code Cracking briefing](https://www.cia.gov/spy-kids/static/823c6f835b4f45e8e971063fb7686681/Briefing-code-cracking.pdf) into original space puzzles. The story uses a shared symbol code as a communication aid; substitution encryption and translating an unknown language are different tasks. The picture key and illustrations are original game assets. NSA's [National Cryptologic Museum education programs](https://www.nsa.gov/History/National-Cryptologic-Museum/Museum-Tours/School-Field-Trips/) are an additional curriculum reference; the legacy CryptoKids pages were unavailable during research.

The lunar setting uses low sunlight, long shadows, and a safe lit landing area, informed by NASA’s [Moon south-pole explanation](https://science.nasa.gov/resource/shadows-near-the-moons-south-pole/). Launch art shows a powered rise followed by a turn toward space, following NASA’s [flight-to-orbit description](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/flight-to-orbit/). Travel time is compressed for play; the alien spacecraft is fictional.

## Build and test

Run these commands from the repository root:

```sh
make -C game
make -C game test
game/.venv/bin/python -m pyboy game/build/cipherspace.gbc
```

The output is `game/build/cipherspace.gbc`. The build uses GBDK 4.5.0 at `game/.tools/gbdk`; override it with `make -C game GBDK=/absolute/path/to/gbdk`. It also runs the ROM header check. `make test` runs `game/tests/playthrough.py`; the local requirements pin PyBoy 2.6.1. This release is tested with the shared toolchain's PyBoy 2.7.0; the report records the version used.

`make -C game release` runs the tests and creates `game/dist/cipherspace-chapter1-v0.3.0.zip` and its unpacked folder, including controls, flashing instructions, checksums, test evidence, and an emulator preview. Packaging checks that the ROM is the exact file that passed the playthrough. The v0.1.0 and v0.2.0 releases are retained. Final releases in `game/dist` can be committed; build output, toolchains, virtual environments, and generated previews are ignored.

To build and test separately while a live preview uses the default ROM, choose another output directory:

```sh
make -C game BUILD_DIR=build/immersive-review test
make -C game BUILD_DIR=build/immersive-review release
```

The ROM, objects, report, and screenshots all use that directory. Packaging receives it through `CIPHERSPACE_BUILD_DIR`. Avoid rebuilding the default ROM during an active live preview.

For a fresh checkout, download the appropriate macOS archive from the [official GBDK 4.5.0 release](https://github.com/gbdk-2020/gbdk-2020/releases/tag/4.5.0), verify its published checksum, and extract its `gbdk` directory into `game/.tools`. Create the Python environment and install the pinned test dependencies:

```sh
python3 -m venv game/.venv
game/.venv/bin/python -m pip install -r game/requirements-test.txt
```

### Use the shared Chromatic tools

After the ModRetro Chromatic plugin has prepared its build and emulator dependencies, pass their paths to Make. For the default macOS setup root:

```sh
cipherspace_tools="$HOME/Library/Application Support/modretro-chromatic/toolchain/.local"
make -C game -B test \
  GBDK="$cipherspace_tools/gbdk" \
  PYTHON="$cipherspace_tools/pyboy-venv/bin/python"
```

Use the paths reported by the plugin's toolchain doctor if your setup root differs. `-B` rebuilds every object with the selected compiler before running the tests. The report records the PyBoy version actually used. The project-local defaults remain available when these overrides are omitted; keep the project's pinned requirements in its local environment so they do not downgrade the shared emulator.

Emulator tests check the ROM's behavior. Cartridge boot, physical controls, speaker output, and saves surviving power-off still require a hardware check.

Test evidence is written to `game/build/playthrough-report.json`, with actual emulator frames in `game/build/screenshots`. The report includes the tested ROM's SHA-256 digest.

The release report records the exact ROM, check results, continuous frame audit, and motion samples. Tests cover the complete new route, linked puzzle selection, countdown/ascent timing, controlled landing, first contact, mandatory HI, fresh starts from old saves, and version 3 save recovery.

The renderer loads each illustration into a hidden graphics buffer before switching the image and palettes together. A scanline split keeps the caption font readable below the artwork. The hatch regression checks every transition frame for complete images, alongside the continuous blank-frame audit.

Generated graphics are included, so rebuilding the ROM does not need Node.js. To regenerate graphics, use Node.js with `sharp` installed (or set `SHARP_PATH` to the module directory):

```sh
node game/tools/make-ui.cjs
node game/tools/convert-scenes.cjs
node game/tools/make-flight.cjs
node game/tools/make-cinema.cjs
```

The font source is `game/assets/ui-font.json`. Original scene art is in `design-assets/opening-scenes-v2.png`; new cinematic source art is in `design-assets/cinema`. `make-cinema.cjs` converts its source art to native tiles and palettes. Each illustrated frame is 160×96 pixels with 240 tiles and seven four-color palettes; palette 7 belongs to the interface. The ROM uses 16 banks (256 KiB).

## Flash the DevDay cartridge

Install the [official Chromatic CLI](https://www.npmjs.com/package/%40modretro/chromatic-cli) if it is not already available:

```sh
npm install --global @modretro/chromatic-cli@1.2.1
```

The npm launcher requires Node.js 18 or newer. macOS needs no additional USB drivers. The helper looks for the executable in `CHROMATIC_CLI`, then `game/.tools/chromatic-cli`, then `PATH`. Set `CHROMATIC_CLI` to one executable path, without extra arguments.

1. If developer mode is not configured yet, follow the [official DevDay setup guide](https://support.modretro.com/en_us/chromatic-devday-edition-quickstart-guid-By1iOlcMg) and complete activation yourself in the official MR Updater. Keep activation codes out of source files, terminal history, and chat.
2. Insert the DevDay cartridge, turn on the console, and connect it with a USB data cable. Connect exactly one Chromatic.
3. Run the helper and review the detected cartridge before accepting the official CLI's write confirmation:

```sh
bash game/tools/flash.sh
# Or select a different original homebrew ROM:
bash game/tools/flash.sh /absolute/path/to/game.gbc
```

Writing replaces the game currently on the cartridge. The helper checks the file size, detects the connected device and cartridge, computes the ROM's SHA-256 digest, and passes it to `write-homebrew --expect-sha256`. The CLI checks developer activation and flash capacity, writes the ROM, and verifies the write. The helper keeps its interactive confirmation and performs no automatic installation, activation, or firmware update.

## Hardware acceptance check

After flashing completes, disconnect USB and power-cycle the console. Check name entry, the title screen, full Chapter 1 route, help, pause, undo, and sound. Confirm that the chosen name and progress survive turning the console off and on. For audio, test the console speaker as well as any emulator audio used during development.

The CLI's optional `live-demo` runs an emulator on the computer and streams it to Chromatic. It is useful for checking the display and controls, but the final acceptance check must boot the written cartridge directly.

See [ModRetro's updater instructions](https://support.modretro.com/en_us/chromatic-firmware-updater-ryhoYnzCx) and [GBDK's cartridge/save documentation](https://gbdk.org/docs/api/docs_rombanking_mbcs.html) for hardware details.
