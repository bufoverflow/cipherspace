# Cipherspace

A gentle cryptography adventure for Game Boy Color and the ModRetro Chromatic.

## Chapter 1: The Empty Ship

Choose your pilot's name, discover a spacecraft in your backyard, decode its notes, repair it, and steer to the Moon. The ship's owner is waiting to be found in a later chapter.

- Enter a name of up to eight letters using the D-pad.
- Decode OPEN and MOON with a consistent symbol key; optionally encode HI.
- Explore word definitions, ask for free hints, and undo puzzle choices.
- Enjoy illustrated color backgrounds, gentle flight, and saved progress.

## Play or build

The cartridge ROM, controls, flashing guide, emulator preview, and test report are in [the Chapter 1 release](game/dist/cipherspace-chapter1-v0.1.0/). See [the development guide](game/README.md) for build instructions and learning references.

```sh
make -C game
make -C game test
make -C game release
```

The game uses GBDK 4.5.0 and PyBoy 2.6.1 for emulator checks. The release targets a 128 KiB Game Boy Color cartridge with MBC5 and 8 KiB save RAM. Physical cartridge validation remains pending.
