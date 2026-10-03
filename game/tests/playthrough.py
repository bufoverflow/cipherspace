"""Play the release ROM with real joypad input; inspect state without changing it.

Only save-recovery and migration fixtures change battery RAM, before a fresh
emulator boots. Gameplay and name entry use joypad input, never memory writes.
"""
import binascii
import hashlib
import io
import json
import os
import re
from importlib.metadata import version
from pathlib import Path

from PIL import Image, ImageDraw
from pyboy import PyBoy

ROOT = Path(__file__).resolve().parents[1]
ROM = Path(os.environ.get('CIPHERSPACE_ROM', ROOT / 'build/cipherspace.gbc')).resolve()
OUTPUT = Path(os.environ.get('CIPHERSPACE_TEST_OUTPUT', ROOT / 'build')).resolve()
GAME_SIZE = 37
NAME_SETUP = 12
SHOTS = OUTPUT / 'screenshots'
SHOTS.mkdir(parents=True, exist_ok=True)
SYMS = {name: int(value, 16) for name, value in re.findall(
    r'^DEF (\w+) (0x[0-9A-Fa-f]+)$', ROM.with_suffix('.noi').read_text(), re.M)}
checks = []
frame_audit = {'frames_observed': 0, 'lcd_disabled_frames': 0,
               'uniform_frames': 0, 'actions': {}}


def check(condition, description):
    assert condition, description
    checks.append(description)


class Session:
    def __init__(self, ram=None):
        self.p = PyBoy(str(ROM), window='null', cgb=True,
                       ram_file=io.BytesIO(ram or bytes(8192)), sound_volume=0)
        self.p.set_emulation_speed(0)
        self.p.tick(180)
        self.frames = 180
        self.action = 'idle'
        self.recent_actions = []

    def tick(self, frames):
        """Inspect every rendered frame, including frames inside screen changes.

        A screenshot after an input can miss the one-frame white flash reported
        on hardware. Begin after the boot ROM, then never skip an emulated frame.
        """
        for _ in range(frames):
            self.p.tick(1, render=True)
            self.frames += 1
            frame_audit['frames_observed'] += 1
            actions = frame_audit['actions']
            actions[self.action] = actions.get(self.action, 0) + 1
            lcd_on = bool(self.p.memory[0xff40] & 0x80)
            extrema = self.p.screen.image.convert('RGB').getextrema()
            uniform = all(low == high for low, high in extrema)
            frame_audit['lcd_disabled_frames'] += int(not lcd_on)
            frame_audit['uniform_frames'] += int(uniform)
            if not lcd_on or uniform:
                self.p.screen.image.save(SHOTS / 'first-blank-frame.png')
                (OUTPUT / 'first-blank-frame.json').write_text(json.dumps({
                    'rom_sha256': hashlib.sha256(ROM.read_bytes()).hexdigest(),
                    'frame': self.frames, 'lcd_on': lcd_on,
                    'rgb_extrema': extrema, 'state': list(self.state()),
                    'action': self.action, 'recent_actions': self.recent_actions,
                }, indent=2) + '\n')
                raise AssertionError(f'Blank/display-off frame {self.frames} '
                                     f'during {self.action}; evidence saved in {SHOTS}')

    def record_action(self, action):
        self.action = action
        self.recent_actions.append({'frame': self.frames, 'action': action,
                                    'scene': self.state()[0]})
        self.recent_actions = self.recent_actions[-32:]

    def state(self):
        addr = SYMS['_game']
        return bytes(self.p.memory[addr:addr + GAME_SIZE])

    def name(self):
        return self.state()[28:37].split(b'\0', 1)[0].decode('ascii')

    def shows(self, text):
        """Read only UI characters, excluding the scene-art tile numbers."""
        tiles = self.p.memory[SYMS['_bgtiles']:SYMS['_bgtiles'] + 360]
        attrs = self.p.memory[SYMS['_bgattrs']:SYMS['_bgattrs'] + 360]
        rows = []
        for row in range(18):
            rows.append(''.join(chr((tiles[i] % 64) + 32)
                                if attrs[i] & 0x08 and tiles[i] < 128 else ' '
                                for i in range(row * 20, (row + 1) * 20)))
        return text in '\n'.join(rows)

    def value(self, name):
        return self.p.memory[SYMS['_' + name]]

    def scene(self, expected):
        check(self.state()[0] == expected, f'scene {expected} reached')

    def press(self, key):
        self.record_action(f'press {key}')
        self.p.button_press(key)
        self.tick(6)
        self.p.button_release(key)
        self.tick(6)

    def hold(self, key, frames):
        self.record_action(f'hold {key}')
        self.p.button_press(key)
        self.tick(frames)
        self.p.button_release(key)
        self.tick(6)

    def choose(self, index):
        for _ in range(5):
            if self.value('choice') == index:
                self.press('a')
                return
            self.press('right')
        raise AssertionError('Cannot select puzzle choice')

    def name_pick(self, index):
        """Reach one of the 28 name-grid cells with real directional input."""
        assert 0 <= index < 28
        for _ in range(4):
            current = self.value('name_choice')
            if current // 7 == index // 7:
                break
            self.press('down')
        for _ in range(7):
            current = self.value('name_choice')
            if current == index:
                self.press('a')
                return
            self.press('right')
        raise AssertionError(f'Cannot select name-grid cell {index}')

    def type_name(self, name):
        for letter in name:
            assert 'A' <= letter <= 'Z'
            self.name_pick(ord(letter) - ord('A'))

    def confirm_new_trip(self, test_short_hold=False):
        self.press('b')
        check(self.value('ui_mode') == 5, 'new trip requires deliberate confirmation')
        self.p.button_press('a')
        self.p.button_press('b')
        self.record_action('hold a+b to reset')
        if test_short_hold:
            self.tick(60)
            check(self.value('ui_mode') == 5, 'short A+B hold does not erase the trip')
            self.tick(75)
        else:
            self.tick(135)
        self.p.button_release('a')
        self.p.button_release('b')
        self.tick(6)
        self.scene(NAME_SETUP)

    def shot(self, name):
        self.p.screen.image.save(SHOTS / (name + '.png'))

    def launch(self):
        """Repeated A presses must still show the whole launch countdown."""
        self.scene(6)
        check(self.state()[1] >= 2, 'Lift off opens a dedicated countdown screen')
        observed, durations, pictures = [], {}, {}
        self.record_action('press a repeatedly during countdown')
        for elapsed in range(600):
            if self.state()[0] != 6:
                break
            count = self.value('launch_count')
            if not observed or observed[-1] != count:
                observed.append(count)
            durations[count] = durations.get(count, 0) + 1
            if durations[count] == 20:
                label = str(count) if count else 'go'
                self.shot('10-countdown-' + label)
                pictures[count] = self.p.screen.image.crop((40, 24, 120, 112)).tobytes()
            # Fresh presses, not just a held button, test accidental skipping.
            if elapsed % 12 == 0:
                self.p.button_press('a')
            elif elapsed % 12 == 6:
                self.p.button_release('a')
            self.tick(1)
        self.p.button_release('a')
        self.tick(6)
        check(observed == [3, 2, 1, 0], 'launch shows 3, 2, 1, then GO in order')
        check(all(durations.get(count, 0) >= 40 for count in (3, 2, 1, 0)),
              'repeated A presses cannot skip any countdown step')
        check(len(pictures) == 4 and len(set(pictures.values())) == 4,
              'each countdown step visibly changes the center of the screen')
        self.scene(7)
        # The scene changes before its staged artwork reaches the screen.
        # Observe the visible transition before testing steering or taking a shot.
        for _ in range(60):
            if (self.shows('STEER TO THE MOON.')
                    and self.p.screen.image.crop((40, 24, 120, 112)).tobytes() != pictures[0]):
                break
            self.tick(1)
        check(self.shows('STEER TO THE MOON.')
              and self.p.screen.image.crop((40, 24, 120, 112)).tobytes() != pictures[0],
              'the visible flight scene replaces GO before steering begins')

    def power_off(self):
        ram = io.BytesIO()
        self.p.stop(ram_file=ram)
        check(len(ram.getvalue()) == 8192, 'save contains 8 KiB SRAM')
        return ram.getvalue()


def records_from(ram):
    return [ram[i * 64:(i + 1) * 64] for i in range(2)]


def latest_slot(records):
    return 1 if ((records[1][2] - records[0][2]) & 255) < 128 else 0


def with_crc(record):
    record = bytearray(record)
    crc = binascii.crc_hqx(record[1:62], 0xffff)
    record[62:64] = crc.to_bytes(2, 'little')
    return bytes(record)


def fixture_record(payload, version, sequence):
    record = bytearray(64)
    record[:4] = bytes([0xa7, version, sequence, len(payload)])
    record[4:4 + len(payload)] = payload
    return with_crc(record)


s = Session()
s.scene(0)
s.shot('01-title')
check(not s.value('has_save'), 'blank SRAM starts without a saved trip')
s.press('a')
s.scene(NAME_SETUP)
check(s.value('name_length') == 0, 'a new trip asks for an empty player name')
s.press('left')
check(s.value('name_choice') == 6, 'name-grid Left wraps inside its row')
s.press('right')
check(s.value('name_choice') == 0, 'name-grid Right wraps inside its row')
s.press('up')
check(s.value('name_choice') == 21, 'name-grid Up wraps to the last row')
s.press('down')
check(s.value('name_choice') == 0, 'name-grid Down wraps to the first row')
s.press('start')
check(s.state()[0] == NAME_SETUP and s.value('name_error'),
      'Start rejects an empty name without beginning the trip')
check(s.shows('ADD A LETTER FIRST'), 'empty-name feedback says how to continue')
s.name_pick(27)
check(s.state()[0] == NAME_SETUP and s.value('name_error'),
      'DONE also rejects an empty name')
s.press('b')
check(s.value('name_length') == 0, 'deleting an empty name is harmless')
s.type_name('NOVAX')
s.press('b')
check(s.value('name_length') == 4, 'B deletes the last name character')
s.type_name('Y')
s.name_pick(26)
check(s.value('name_length') == 4, 'ERASE deletes the last name character')
s.shot('02-name-setup')
s.name_pick(27)
s.scene(1)
check(s.name() == 'NOVA', 'DONE starts the trip using the edited name NOVA')
check(s.shows('NOVA'), 'the backyard displays the chosen player name')
s.shot('02-backyard')
s.press('a')
s.press('a')
s.scene(2)
check(s.shows('YOUR KEY') and s.shows('SECRET NOTE') and s.shows('YOUR WORD')
      and s.shows('LEFT/RIGHT: PICK'),
      'the cipher guide, secret note, answer, and choices have separate labels')
s.shot('03-open-code')

# Free help can be opened and closed without changing a puzzle.
before = s.state()
s.press('select')
check(s.value('ui_mode') == 1, 'Select opens help')
s.press('a')
check(s.value('ui_mode') == 2, 'word guide opens')
s.press('right')
s.press('right')
check(s.value('word_index') == 2, 'beacon definition is accessible')
s.shot('04-word-help')
s.press('b')
s.press('down')
s.press('a')
check(s.value('ui_mode') == 3, 'puzzle help opens')
s.press('a')
s.press('a')
s.shot('05-puzzle-help')
s.press('a')
check(s.value('ui_mode') == 0 and s.state() == before,
      'all three hint steps return without filling answers')

# Incorrect choices remain editable, and B restores the previous answer.
s.press('a')  # E in the circle slot; deliberately wrong.
check(s.state()[4] == 2, 'wrong choice remains editable')
check(s.value('wrong') and s.value('slot') == 0,
      'a wrong letter shows feedback immediately and keeps its position selected')
check(s.shows("THAT DOESN'T MATCH"), 'an incorrect answer has explicit text feedback')
s.shot('05-wrong-answer')
s.press('b')
check(s.state()[4] == 255, 'undo clears a first attempt')
s.choose(2)  # O
check(s.state()[4] == 0 and s.value('slot') == 1, 'O decoded; next blank selected')
partial = s.power_off()
s = Session(partial)
s.scene(0)
check(s.value('has_save'), 'saved trip is offered after a fresh boot')
check(s.name() == 'NOVA' and s.shows('NOVA'), 'the title displays the saved player name')
s.press('a')
s.scene(2)
check(s.name() == 'NOVA', 'Continue resumes a named save without asking again')
check(s.state()[4] == 0 and s.state()[5] == 255,
      'partial puzzle survives power-off')
check(s.value('slot') == 1, 'resumed puzzle focuses the next unfinished symbol')
s.choose(3)  # P
s.choose(0)  # E
s.choose(0)  # Incorrect E where N belongs
check(s.value('wrong') and not s.state()[3], 'incorrect complete word does not unlock ship')
s.shot('06-word-error')
s.press('b')
check(not s.value('wrong') and s.state()[7] == 255, 'undo restores last blank after incorrect word')
s.choose(1)  # N
check(s.state()[3] == 1, 'OPEN unlocks the hatch')
s.shot('06-open-solved')
s.press('a')
s.scene(3)
damaged = s.p.screen.image.crop((0, 8, 160, 72)).tobytes()
s.press('a')
check(s.state()[2] == 1, 'crystal repairs the ship')
check(s.p.screen.image.crop((0, 8, 160, 72)).tobytes() != damaged,
      'repair visibly changes the ship artwork')
s.shot('07-ship-restored')
s.press('a')
s.scene(4)
s.shot('08-empty-cockpit')
for _ in range(3):
    s.press('a')
s.scene(5)
s.choose(1)  # M
s.choose(2)  # O: one mapping fills both circles.
check(s.value('slot') == 3, 'both circles in MOON fill from one O choice')
s.shot('09-moon-repeats')
s.choose(0)  # N
check(s.state()[3] == 3, 'MOON solves using the same symbol key')
s.press('a')
s.scene(6)
s.press('a')
s.press('a')
check(s.state()[0] == 6 and s.state()[1] >= 2,
      'Lift off starts the countdown before flight')
launch_save = s.power_off()
s = Session(launch_save)
s.press('a')
s.scene(6)
check(s.state()[1] == 1,
      'a restart during countdown returns safely to the launch prompt')
s.press('a')
s.launch()
s.scene(7)
s.shot('10-flight')
x, y = s.state()[25:27]
s.hold('right', 32)
check(s.state()[25] > x and s.state()[26] == y, 'held Right moves only horizontally')
stopped = s.state()[25:27]
s.tick(60)
check(s.state()[25:27] == stopped, 'ship stops when the button is released')
s.hold('left', 180)
check(s.state()[25] == 8, 'left flight boundary holds')
s.hold('down', 90)
check(s.state()[26] == 87, 'lower flight boundary holds')
s.press('start')
check(s.value('ui_mode') == 4, 'Start pauses the flight')
check(s.shows('NOVA'), 'the pause screen displays the chosen player name')
s.hold('right', 40)
check(s.state()[25:27] == bytes([8, 87]), 'paused flight ignores movement')
s.press('b')
check(s.state()[27] == 0, 'sound can be turned off')
flight_save = s.power_off()
s = Session(flight_save)
s.press('a')
s.scene(7)
check(s.state()[25:28] == bytes([8, 87, 0]), 'position and sound choice survive restart')
s.press('start')
s.press('b')
s.press('a')
check(s.value('ui_mode') == 0 and s.state()[27] == 1, 'pause resumes with sound restored')
s.hold('up', 100)
s.hold('right', 240)
s.scene(8)
s.shot('11-beacon')
s.press('a')
legacy_progress = s.state()[:28]
check(legacy_progress[1] == 1, 'migration fixture captures a real mid-dialogue checkpoint')
for _ in range(2):
    s.press('a')
s.scene(9)
s.shot('12-send-hello')
hello_save = s.power_off()

# HI is optional: both routes must finish.
s = Session(hello_save)
s.press('a')
s.press('b')
s.scene(10)
check(not (s.state()[3] & 4), 'Later skips the optional HI puzzle without a false solve')
for _ in range(2):
    s.press('a')
s.scene(11)
s.p.stop(save=False)

s = Session(hello_save)
s.press('a')
s.choose(1)  # + encodes H
s.choose(0)  # heart encodes I
check(s.state()[3] == 7, 'HI encodes letters into symbols')
s.shot('13-hello-solved')
s.press('a')
s.scene(10)
s.shot('14-lunar-landing')
s.press('a')
s.press('a')
s.scene(11)
s.shot('15-complete')
complete_save = s.power_off()
s = Session(complete_save)
s.press('a')
s.scene(11)
check(s.state()[3] == 7, 'completed chapter survives restart')
check(s.name() == 'NOVA', 'the player name survives the complete chapter')
s.press('a')
s.scene(0)
s.press('b')
check(s.value('ui_mode') == 5, 'new trip requires deliberate confirmation')
s.press('b')
check(s.value('ui_mode') == 0, 'new-trip confirmation can be cancelled')
s.confirm_new_trip(test_short_hold=True)
check(s.value('name_length') == 0, 'a confirmed new trip asks for a fresh name')
s.type_name('LU')
pending_name_save = s.power_off()
check(pending_name_save == complete_save,
      'a new trip is not committed while its name is still being edited')
s = Session(pending_name_save)
s.press('a')
s.scene(11)
check(s.name() == 'NOVA' and s.state()[3] == 7,
      'power-off during new-name setup preserves the previous completed trip')
s.press('a')
s.confirm_new_trip()
s.type_name('LUNA')
s.press('start')
s.scene(1)
check(s.name() == 'LUNA' and s.state()[2:4] == bytes([0, 0])
      and s.state()[4:25] == bytes([255] * 21),
      'Start accepts the new name and commits fresh progress')
new_trip_save = s.power_off()
s = Session(new_trip_save)
s.press('a')
s.scene(1)
check(s.name() == 'LUNA', 'a newly named trip resumes directly after reboot')
s.p.stop(save=False)

# The longest accepted name remains intact after a rejected ninth character.
s = Session()
s.press('a')
s.scene(NAME_SETUP)
s.type_name('STARLITE')
check(s.value('name_length') == 8, 'eight uppercase letters fit in a player name')
s.name_pick(25)  # A ninth letter, Z, must not overwrite the eighth or its NUL.
check(s.value('name_length') == 8 and s.value('name_error'),
      'a ninth name character is rejected visibly')
check(s.shows('FULL! PRESS START.'), 'full-name feedback gives the next button to press')
s.press('start')
s.scene(1)
check(s.state()[28:37] == b'STARLITE\0',
      'Start accepts all eight original letters with a terminating NUL')
long_name_save = s.power_off()
s = Session(long_name_save)
check(s.name() == 'STARLITE' and s.shows('STARLITE'),
      'the longest saved name is visible on the title')
s.press('a')
s.scene(1)
check(s.name() == 'STARLITE', 'an eight-letter name survives a fresh boot')
s.p.stop(save=False)

# Interrupted/corrupt saves should fall back to an intact slot.
records = records_from(complete_save)
check(all(record[0] == 0xa7 and record[1] == 2 and record[3] == GAME_SIZE
          for record in records), 'normal play writes committed version-2 name records')
latest = latest_slot(records)
older = latest ^ 1
broken = bytearray(complete_save)
broken[latest * 64] = 0  # Interrupted write: no committed marker.
s = Session(bytes(broken))
s.press('a')
check(s.state() == records[older][4:41], 'interrupted newest save falls back to older slot')
s.p.stop(save=False)
broken = bytearray(complete_save)
broken[latest * 64 + 20] ^= 128
s = Session(bytes(broken))
s.press('a')
check(s.state() == records[older][4:41], 'bad CRC falls back to older slot')
s.p.stop(save=False)
broken[older * 64] = 0
s = Session(bytes(broken))
check(not s.value('has_save'), 'two invalid saves offer a new trip safely')
s.press('a')
s.scene(NAME_SETUP)
s.p.stop(save=False)

# A valid checksum does not make an invalid v2 name safe to resume.
for label, bad_name in [
        ('empty', bytes(9)),
        ('lowercase', b'nova\0\0\0\0\0'),
        ('nonletter', b'NO1A\0\0\0\0\0'),
        ('unterminated', b'ABCDEFGHI')]:
    broken = bytearray(complete_save)
    record = bytearray(records[latest])
    record[32:41] = bad_name
    broken[latest * 64:(latest + 1) * 64] = with_crc(record)
    s = Session(bytes(broken))
    check(s.value('has_save'), f'{label} name leaves the older save available')
    s.press('a')
    check(s.state() == records[older][4:41],
          f'a valid-CRC {label} name falls back to the intact older record')
    s.p.stop(save=False)

# A real pre-name checkpoint is migrated only after a name is supplied.
legacy_ram = bytearray(8192)
legacy_ram[:64] = fixture_record(legacy_progress, version=1, sequence=90)
s = Session(bytes(legacy_ram))
check(s.value('has_save'), 'a version-1 save is recognized as an existing trip')
s.press('a')
s.scene(NAME_SETUP)
check(s.value('name_length') == 0, 'legacy Continue requests a player name')
s.type_name('LY')
uncommitted_legacy = s.power_off()
check(uncommitted_legacy == bytes(legacy_ram),
      'an unfinished migration leaves the original legacy save untouched')
s = Session(uncommitted_legacy)
s.press('a')
s.scene(NAME_SETUP)
s.type_name('LYRA')
s.press('start')
s.scene(legacy_progress[0])
check(s.state()[:28] == legacy_progress and s.name() == 'LYRA',
      'migration preserves scene, card, every answer, repairs, position and sound')
migrated_save = s.power_off()
migrated_records = records_from(migrated_save)
migrated_record = migrated_records[latest_slot(migrated_records)]
check(migrated_record[1] == 2 and migrated_record[3] == GAME_SIZE
      and migrated_record[4:32] == legacy_progress
      and migrated_record[32:37] == b'LYRA\0',
      'completed migration writes the preserved progress and name in v2 format')
s = Session(migrated_save)
s.press('a')
s.scene(legacy_progress[0])
check(s.state()[:28] == legacy_progress and s.name() == 'LYRA',
      'a migrated save resumes its original card without repeated name setup')
s.p.stop(save=False)

# Real emulator frames, enlarged with nearest-neighbor sampling for review.
names = ['01-title', '02-name-setup', '02-backyard',
         '07-ship-restored', '10-flight', '14-lunar-landing']
sheet = Image.new('RGB', (3 * 336, 2 * 324), '#0a1724')
draw = ImageDraw.Draw(sheet)
for i, name in enumerate(names):
    x, y = (i % 3) * 336 + 8, (i // 3) * 324 + 24
    shot = Image.open(SHOTS / (name + '.png')).convert('RGB')
    sheet.paste(shot.resize((320, 288), Image.Resampling.NEAREST), (x, y))
    draw.text((x, y - 18), name[3:].replace('-', ' ').upper(), fill='#e8ebd4')
sheet.save(SHOTS / 'chapter1-contact-sheet.png')
ux_names = ['02-name-setup', '02-backyard', '03-open-code',
            '05-wrong-answer', '06-word-error', '06-open-solved',
            '04-word-help', '10-countdown-3', '10-countdown-2',
            '10-countdown-1', '10-countdown-go', '10-flight']
ux_sheet = Image.new('RGB', (3 * 336, 4 * 324), '#0a1724')
draw = ImageDraw.Draw(ux_sheet)
for i, name in enumerate(ux_names):
    x, y = (i % 3) * 336 + 8, (i // 3) * 324 + 24
    shot = Image.open(SHOTS / (name + '.png')).convert('RGB')
    ux_sheet.paste(shot.resize((320, 288), Image.Resampling.NEAREST), (x, y))
    draw.text((x, y - 18), name[3:].replace('-', ' ').upper(), fill='#e8ebd4')
ux_sheet.save(SHOTS / 'ux-contact-sheet.png')
check(frame_audit['lcd_disabled_frames'] == 0,
      'LCD stays enabled on every observed frame after boot')
check(frame_audit['uniform_frames'] == 0,
      'no blank framebuffer appears during inputs, menus, or scene transitions')
report = {'rom_sha256': hashlib.sha256(ROM.read_bytes()).hexdigest(),
          'emulator': f'PyBoy {version("pyboy")}', 'checks_passed': len(checks),
          'checks': checks, 'frame_audit': frame_audit, 'hardware_tested': False}
(OUTPUT / 'playthrough-report.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'PASS: {len(checks)} checks; player names, full route, optional route, '
      'save recovery, migration, input, countdown and screenshots. '
      f'{frame_audit["frames_observed"]} consecutive frames checked for blanking.')
