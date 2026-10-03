# Cipherspace

A gentle cryptography adventure for Game Boy Color and the ModRetro Chromatic.

## Chapter 1: The Empty Ship

Choose your pilot’s name, suit up, and discover a spacecraft in your backyard. Decode the note on its door, repair the ship, and follow its owner’s recorded message to the Moon’s south pole. Land, walk toward a signal, and meet an alien whose words you do not understand. Find a shared way to say hello through the ship’s symbol code.

- A named girl pilot, illustrated close-ups, and animated actions.
- Button badges, a padded alphabet cursor, and linked cipher selections.
- Original OPEN, MOON, and HI substitution puzzles with free hints and undo.
- A ten-second countdown, powered ascent, gentle steering, and a landing you control.
- A fresh start for this redesign; new adventures save their own progress.

## Play or build

Get [Chapter 1 v0.3.0](game/dist/cipherspace-chapter1-v0.3.0/) or its [ZIP](game/dist/cipherspace-chapter1-v0.3.0.zip). The package contains the ROM, controls, flashing guide, emulator screenshots, checksums, and test report. See the [development guide](game/README.md) for setup and learning references.

```sh
make -C game
make -C game test
make -C game release
```

The game uses GBDK 4.5.0 and targets a 256 KiB Game Boy Color cartridge with MBC5 and 8 KiB save RAM. Emulator checks use PyBoy; native cartridge validation of this build remains pending.
