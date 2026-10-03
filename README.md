# Cipherspace

A gentle cryptography adventure for Game Boy Color and the ModRetro Chromatic.

## Chapter 1: The Empty Ship

Choose your pilot's name, discover a spacecraft in your backyard, decode its notes, repair it, and steer to the Moon. The ship's owner is waiting to be found in a later chapter.

- Enter a name of up to eight letters using the D-pad.
- Decode OPEN and MOON with a consistent symbol key; optionally encode HI.
- Explore word definitions, ask for free hints, and undo puzzle choices.
- Enjoy illustrated color backgrounds, gentle flight, and saved progress.
- Read clearer text, follow color-coded cipher panels, and get immediate feedback on a wrong answer.
- Launch with a full-screen 3, 2, 1, GO! countdown and navigate without white screen flashes.

## Play or build

The cartridge ROM, controls, flashing guide, emulator preview, and test report are in [Chapter 1 v0.2.0](game/dist/cipherspace-chapter1-v0.2.0/), also available as a [ZIP](game/dist/cipherspace-chapter1-v0.2.0.zip). See [the development guide](game/README.md) for build instructions and learning references.

```sh
make -C game
make -C game test
make -C game release
```

The game uses GBDK 4.5.0. The local test environment pins PyBoy 2.6.1; this release was verified with the shared Chromatic toolchain's PyBoy 2.7.0. It passed 141 checks, including 4,298 consecutive gameplay frames with no blank screen or LCD-off frame. The release targets a 128 KiB Game Boy Color cartridge with MBC5 and 8 KiB save RAM. Native cartridge validation of this build remains pending.
