# Cipherspace — Chapter 1

A native Game Boy Color game for the ModRetro Chromatic DevDay Edition cartridge. The opening covers the backyard, OPEN code, crystal repair, empty seat, MOON destination, assisted flight, beacon help, optional HI message, and lunar landing. Chapter 1 ends before the alien encounter.

## Play

Choose the player's name during setup: use the **D-pad** to move around the alphabet grid, **A** to add a letter, and **B** to delete. Enter **1–8 letters, A–Z**, then press **Start** or select **DONE** and press **A**. The name is saved and appears on the title screen, in the backyard, and when paused.

Fresh games and New Trip ask for a name. Existing version 1 saves keep their progress and ask for a name before continuing.

| Button | Action |
| --- | --- |
| D-pad | Choose or steer |
| A | Confirm |
| B | Undo or choose Later |
| Select | Help |
| Start | Pause |

In a code puzzle, **Left/Right** select a letter or symbol, **Up/Down** select a message position, and **A** fills it. Matching symbols fill together. **B** undoes the last choice and returns to that position. On the optional HI puzzle, **B** chooses Later when there is nothing to undo. Completed puzzles use **A** to continue.

The pause menu can turn sound off and return to the title. New Trip requires holding **A+B** for two seconds after opening its confirmation. Progress saves after puzzle choices and milestones, and when pausing. Continue selects the next unfinished symbol.

The game includes sound and cartridge saving. The ROM targets Game Boy Color with MBC5, 128 KiB ROM, and 8 KiB battery-backed SRAM. Its cartridge header title is `CIPHERSPACE`. Persistent saves require compatible cartridge storage.

## What is in this release

- Rich color backgrounds for the damaged ship, restored ship, empty cockpit, and Moon.
- Three original substitution-cipher exercises: decode OPEN, decode MOON, and optionally encode HI.
- A consistent key, repeated-symbol discovery, free hints, and simple word definitions.
- Gentle steering, automatic Moon arrival, short sound cues, and two checked save records for recovery from an interrupted write.

This release is the playable opening chapter. The alien encounter, earned transmission hints, and the Mars-to-distant-home journey remain future chapters.

## Learning basis

The exercises introduce substitution keys, repeated symbols, decoding, and encoding. They adapt the concepts in the CIA's [Codes and Code Breaking lesson](https://www.cia.gov/spy-kids/static/c2afecbc689adbc329e427cf2e8e2dd9/Lesson-plan-codes-and-code-breaking.pdf) and [Code Cracking briefing](https://www.cia.gov/spy-kids/static/823c6f835b4f45e8e971063fb7686681/Briefing-code-cracking.pdf) into original space puzzles. The picture key and illustrations are original game assets. NSA's [National Cryptologic Museum education programs](https://www.nsa.gov/History/National-Cryptologic-Museum/Museum-Tours/School-Field-Trips/) are an additional curriculum reference; the legacy CryptoKids pages were unavailable during research.

## Build and test

Run these commands from the repository root:

```sh
make -C game
make -C game test
game/.venv/bin/python -m pyboy game/build/cipherspace.gbc
```

The output is `game/build/cipherspace.gbc`. The build uses GBDK 4.5.0 at `game/.tools/gbdk`; override it with `make -C game GBDK=/absolute/path/to/gbdk`. It also runs the ROM header check. `make test` runs `game/tests/playthrough.py` using PyBoy 2.6.1.

`make -C game release` runs the tests and creates `game/dist/cipherspace-chapter1-v0.1.0.zip` and its unpacked folder, including controls, flashing instructions, checksums, test evidence, and an emulator preview. Packaging checks that the ROM is the exact file that passed the playthrough. Final releases in `game/dist` can be committed; build output, toolchains, virtual environments, and generated previews are ignored.

For a fresh checkout, download the appropriate macOS archive from the [official GBDK 4.5.0 release](https://github.com/gbdk-2020/gbdk-2020/releases/tag/4.5.0), verify its published checksum, and extract its `gbdk` directory into `game/.tools`. Create the Python environment and install the pinned test dependencies:

```sh
python3 -m venv game/.venv
game/.venv/bin/python -m pip install -r game/requirements-test.txt
```

Emulator tests check the ROM's behavior. Cartridge boot, physical controls, speaker output, and saves surviving power-off still require a hardware check.

Test evidence is written to `game/build/playthrough-report.json`, with actual emulator frames in `game/build/screenshots`. The report includes the tested ROM's SHA-256 digest.

Generated graphics are included, so rebuilding the ROM does not need Node.js. To regenerate graphics, use Node.js with `sharp` installed (or set `SHARP_PATH` to the module directory):

```sh
node game/tools/make-ui.cjs
node game/tools/convert-scenes.cjs
node game/tools/make-flight.cjs
```

The font source is `game/assets/ui-font.json`; the illustrated scene source is `design-assets/opening-scenes-v2.png`.

## Flash the DevDay cartridge

Install the [official Chromatic CLI](https://www.npmjs.com/package/%40modretro/chromatic-cli) if it is not already available:

```sh
npm install --global @modretro/chromatic-cli@1.2.1
```

The npm launcher requires Node.js 18 or newer. macOS needs no additional USB drivers. The helper looks for the executable in `CHROMATIC_CLI`, then `game/.tools/chromatic-cli`, then `PATH`. Set `CHROMATIC_CLI` to one executable path, without extra arguments.

1. If developer mode is not configured yet, follow the [official DevDay setup guide](https://support.modretro.com/en_us/chromatic-devday-edition-quickstart-guid-By1iOlcMg) to update the console and activate it with the included code. MR Updater opens the activation dialog with **Cmd-I** on macOS; the CLI also provides `chromatic-cli activate <CODE>`.
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
