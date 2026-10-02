"""Package only a ROM whose digest matches its completed emulator playthrough."""
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
rom = root / 'build/cipherspace.gbc'
report = root / 'build/playthrough-report.json'
digest = hashlib.sha256(rom.read_bytes()).hexdigest()
evidence = json.loads(report.read_text())
assert evidence['rom_sha256'] == digest, 'Run make test for this ROM before packaging.'
release = root / 'dist/cipherspace-chapter1-v0.1.0'
release.mkdir(parents=True, exist_ok=True)
shutil.copy2(rom, release / rom.name)
shutil.copy2(report, release / report.name)
shutil.copy2(root / 'build/screenshots/chapter1-contact-sheet.png', release / 'preview.png')
(release / 'SHA256SUMS').write_text(f'{digest}  {rom.name}\n')
(release / 'PLAY-AND-FLASH.txt').write_text(f'''CIPHERSPACE
Chapter 1: The Empty Ship | v0.1.0

Discover the ship, read its secret notes, repair it, and fly to the Moon.
This opening chapter ends before meeting the alien.

PLAYER SETUP
Choose 1-8 letters, A-Z. Use the D-pad to move around the alphabet grid.
A adds the selected letter; B deletes. Start or DONE confirms the name.
The saved name appears on the title screen, in the backyard, and in pause.
Fresh games and New Trip ask for a name. Older version 1 saves retain their
progress and ask for a name before continuing.

CONTROLS
Left/Right: pick a letter or symbol. Up/Down: select a message position.
A: choose or continue. B: undo (or Later in HI when there is no undo).
Select: free word and puzzle help. Start: pause and save current progress.
In flight, use the arrows to steer; release to stop. Arrival is automatic.
In pause, B toggles sound. New Trip asks for a two-second A+B hold.

FLASH YOUR DEVDAY CARTRIDGE
Use the official ModRetro setup instructions if developer mode is not enabled:
https://support.modretro.com/en_us/chromatic-devday-edition-quickstart-guid-By1iOlcMg

Install the official CLI, if needed (Node.js 18 or newer):
  npm install --global @modretro/chromatic-cli@1.2.1

Insert the DevDay cartridge, power on the Chromatic, and connect USB.
From this extracted folder, run:
  chromatic-cli list-devices
  chromatic-cli detect-cart --all
  chromatic-cli write-homebrew ./cipherspace.gbc --expect-sha256 {digest}

Writing replaces the current game on the cartridge. Review the detected
cartridge before accepting the CLI's confirmation. The write is verified.
Then disconnect USB, power-cycle, and test controls, sound, saved name and progress.

VALIDATION
Native Game Boy Color ROM: 128 KiB, MBC5, 8 KiB battery RAM.
{evidence['checks_passed']} emulator checks passed. The report records the exact ROM digest.
Hardware validation remains pending; no Chromatic was connected at release.
The preview contains actual emulator frames.
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
