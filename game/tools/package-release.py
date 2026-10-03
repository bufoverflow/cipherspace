"""Package only a ROM whose digest matches its completed emulator playthrough."""
import hashlib
import json
import os
import shutil
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
build = Path(os.environ.get('CIPHERSPACE_BUILD_DIR', 'build'))
if not build.is_absolute():
    build = root / build
build = build.resolve()
rom = build / 'cipherspace.gbc'
report = build / 'playthrough-report.json'
preview = build / 'screenshots/ux-contact-sheet.png'
digest = hashlib.sha256(rom.read_bytes()).hexdigest()
evidence = json.loads(report.read_text())
assert evidence['rom_sha256'] == digest, 'Run make test for this ROM before packaging.'
audit = evidence['frame_audit']
assert audit['frames_observed'] > 0 and audit['lcd_disabled_frames'] == audit['uniform_frames'] == 0
assert preview.is_file(), 'The tested build must include its UX preview.'
release = root / 'dist/cipherspace-chapter1-v0.3.1'
release.mkdir(parents=True, exist_ok=True)
shutil.copy2(rom, release / rom.name)
shutil.copy2(report, release / report.name)
shutil.copy2(preview, release / 'preview.png')
shutil.copy2(build / 'screenshots/cinema-contact-sheet.png', release / 'cinema-preview.png')
shutil.copy2(build / 'screenshots/reading-contact-sheet.png', release / 'reading-preview.png')
shutil.copy2(build / 'screenshots/19-powered-ascent.gif', release / 'launch.gif')
shutil.copy2(build / 'screenshots/12-crystal-repair.gif', release / 'repair.gif')
shutil.copy2(build / 'screenshots/25-first-contact-motion.gif', release / 'first-contact.gif')
shutil.copy2(build / 'screenshots/15-turn-forward-1.gif', release / 'log-book.gif')
(release / 'SHA256SUMS').write_text(f'{digest}  {rom.name}\n')
(release / 'PLAY-AND-FLASH.txt').write_text(f'''CIPHERSPACE
Chapter 1: The Empty Ship | v0.3.1

Choose your name, suit up, discover a ship, and rescue its pilot on the Moon.

WHAT CHANGED
Round A/B icons resemble the console buttons, and story actions have more space.
The pilot wears a smaller name patch. Scrolling instructions pause at each end
and move slowly. The illustrated log book lets the reader turn pages and look
back at their own pace. The pilot's appearance and suit stay consistent across
scenes, and steering to the Moon uses the same illustrated style as the story.

CONTROLS
Name: D-pad selects A-Z; A adds; B erases; Start finishes.
A and B appear as round buttons beside each action. Start and Select use pills.
Puzzles: Left/Right chooses; Up/Down moves the linked symbol-and-answer cursor.
A applies a choice; B undoes. Select offers free help. Start pauses.
The quiet blue cursor connects the symbol to its answer. Amber marks your choice.
Wrong answers stay editable. Solving a note triggers a visible action.
Flight: arrows steer, release to stop. Approach the Moon, then steer to the
south-pole landing light and press A when LAND appears. On the surface, walk
left/right toward the light; press A when MEET appears.
The countdown and ascent play through before steering begins.
Log book: A turns a page; B looks back. Pages stay open until the reader turns them.

SAVES
This overhaul starts fresh: v0.1/v0.2 saves are not loaded or migrated.
New v0.3 adventures save their own name and progress. New Trip asks for a
two-second A+B hold. Restarting during countdown/ascent returns to Lift Off.

FLASH YOUR DEVDAY CARTRIDGE
Complete Developer Mode activation yourself in the official ModRetro Updater:
https://support.modretro.com/en_us/chromatic-devday-edition-quickstart-guid-By1iOlcMg

Install the official CLI if needed (Node.js 18 or newer):
  npm install --global @modretro/chromatic-cli@1.2.1

Insert the DevDay cartridge, power on the Chromatic, and connect USB.
From this extracted folder, run:
  chromatic-cli list-devices
  chromatic-cli detect-cart --all
  chromatic-cli write-homebrew ./cipherspace.gbc --expect-sha256 {digest}

Writing replaces the current cartridge game and may erase its saves.
Review the selected cartridge before accepting the CLI confirmation.
After a verified write, disconnect USB and test boot, controls, sound and saves.

VALIDATION
Native Game Boy Color ROM: 256 KiB, MBC5, 8 KiB battery RAM.
{evidence['checks_passed']} emulator checks passed against the included ROM digest.
{audit['frames_observed']} gameplay frames audited: no blank or LCD-off frames.
The preview contains real emulator frames. Native cartridge validation is pending.
''')

archive = release.parent / (release.name + '.zip')
with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as z:
    for file in sorted(release.iterdir()):
        info = zipfile.ZipInfo(release.name + '/' + file.name)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        z.writestr(info, file.read_bytes())
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert hashlib.sha256(z.read(release.name + '/' + rom.name)).hexdigest() == digest
print(f'Release: {archive}\nROM: {release / rom.name}\nSHA-256: {digest}')
