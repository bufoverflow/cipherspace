"""Play the release ROM with real joypad input; inspect state without changing it.

Only save-recovery and older-version fixtures change battery RAM, before a fresh
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
(TITLE, YARD, HATCH, REPAIR, EMPTY_SEAT, DESTINATION, LAUNCH, FLIGHT,
 BEACON, HELLO, LANDING, ENDING, NAME_SETUP, PILOT_CARD, DOOR_NOTE,
 HATCH_OPEN, CRYSTAL_REPAIR, SHIP_WAKE, OWNER_LOG, MOON_NOTE, BOARDING,
 ASCENT, MOON_APPROACH, MOON_WALK, FIRST_CONTACT, ALIEN_REPLY,
 FRIENDSHIP) = range(27)
SHOTS = OUTPUT / 'screenshots'
SHOTS.mkdir(parents=True, exist_ok=True)
SYMS = {name: int(value, 16) for name, value in re.findall(
    r'^DEF (\w+) (0x[0-9A-Fa-f]+)$', ROM.with_suffix('.noi').read_text(), re.M)}
checks = []
frame_audit = {'frames_observed': 0, 'lcd_disabled_frames': 0,
               'uniform_frames': 0, 'actions': {}}
animation_audit = {}


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
        if self.state()[0] != expected:
            self.p.screen.image.save(SHOTS / f'failed-scene-{expected}.png')
            (OUTPUT / f'failed-scene-{expected}.json').write_text(json.dumps({
                'rom_sha256': hashlib.sha256(ROM.read_bytes()).hexdigest(),
                'expected_scene': expected, 'actual_scene': self.state()[0],
                'card': self.state()[1], 'ui_mode': self.value('ui_mode'),
                'frame': self.frames, 'recent_actions': self.recent_actions,
            }, indent=2) + '\n')
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

    def name_focus(self, index):
        """Reach one of the 26 letters, including the short final row."""
        assert 0 <= index < 26
        column = index % 7
        # Columns F and G do not exist in the last row. Start from a full row
        # when necessary, then choose the column before moving vertically.
        for _ in range(4):
            current = self.value('name_choice')
            if current // 7 < 3 or column < 5:
                break
            self.press('up')
        for _ in range(7):
            current = self.value('name_choice')
            if current % 7 == column:
                break
            self.press('right')
        for _ in range(4):
            if self.value('name_choice') == index:
                return
            self.press('down')
        raise AssertionError(f'Cannot reach alphabet letter {index}')

    def name_pick(self, index):
        self.name_focus(index)
        self.press('a')

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

    def await_scene(self, expected, limit=1800):
        self.record_action(f'wait for scene {expected}')
        for _ in range(limit):
            if self.state()[0] == expected:
                self.scene(expected)
                return
            self.tick(1)
        raise AssertionError(f'Scene {expected} was not reached in {limit} frames; '
                             f'current scene is {self.state()[0]}')

    def motion_evidence(self, label, images):
        if not images:
            return
        for suffix, index in [('early', 0), ('middle', len(images) // 2),
                              ('late', len(images) - 1)]:
            images[index].save(SHOTS / f'{label}-{suffix}.png')
        native = [img.convert('RGB').quantize(colors=256, dither=Image.Dither.NONE)
                  for img in images]
        enlarged = [img.resize((480, 432), Image.Resampling.NEAREST) for img in native]
        for frames, suffix in [(native, ''), (enlarged, '-3x')]:
            frames[0].save(SHOTS / f'{label}{suffix}.gif', save_all=True,
                           append_images=frames[1:], duration=100, loop=0,
                           disposal=2, optimize=False)
        animation_audit.setdefault(label, {}).update({
            'sample_frames': len(images), 'sample_interval_frames': 6,
            'gif_frame_duration_ms': 100,
            'native_gif': f'{label}.gif', 'enlarged_gif': f'{label}-3x.gif'})

    def observe_motion(self, label, frames=120):
        """A waiting story scene can keep moving without advancing its dialogue."""
        before = self.state()
        images = []
        self.record_action(f'observe {label}')
        for elapsed in range(frames):
            self.tick(1)
            if elapsed % 6 == 0:
                images.append(self.p.screen.image.copy())
        distinct = len({img.crop((0, 0, 160, 96)).tobytes() for img in images})
        animation_audit[label] = {'observed_frames': frames,
                                  'distinct_scene_images': distinct}
        self.motion_evidence(label, images)
        check(self.state() == before, f'{label} leaves the story under player control')
        check(distinct >= 2, f'{label} remains visibly animated while waiting')

    def animation(self, scene, label, destination=None, limit=1200):
        """Observe authored motion using actual frames and normal button presses."""
        self.scene(scene)
        self.press('a')
        check(self.state()[0] == scene and self.state()[1] == 0,
              f'{label} cannot be skipped by an early A press')
        images = []
        hatch_frames = {}
        for elapsed in range(limit):
            state = self.state()
            if destination is not None and state[0] != scene:
                break
            if destination is None and state[0] == scene and state[1] == 1:
                break
            self.tick(1)
            if scene == HATCH_OPEN and elapsed >= 18:
                # A hidden tilemap alone cannot prevent visible artwork from
                # tearing when its tile data is overwritten. The hatch has two
                # complete poses and no moving overlays during this interval;
                # inspect every frame so a short mixed-pose frame cannot hide
                # between the animation GIF's six-frame samples.
                image = self.p.screen.image.copy()
                pixels = image.crop((0, 0, 160, 96)).tobytes()
                record = hatch_frames.setdefault(pixels, {
                    'first_elapsed_frame': elapsed, 'last_elapsed_frame': elapsed,
                    'frames_observed': 0, 'image': image})
                record['last_elapsed_frame'] = elapsed
                record['frames_observed'] += 1
            if elapsed >= 18 and elapsed % 6 == 0:
                images.append(self.p.screen.image.copy())
        else:
            self.motion_evidence(label, images)
            raise AssertionError(f'{label} did not finish in {limit} frames')
        distinct = len({img.crop((0, 0, 160, 96)).tobytes() for img in images})
        animation_audit[label] = {'observed_frames': elapsed,
                                  'distinct_scene_images': distinct}
        self.motion_evidence(label, images)
        if scene == HATCH_OPEN:
            evidence = []
            for index, record in enumerate(hatch_frames.values()):
                path = f'{label}-consecutive-pose-{index + 1}.png'
                record['image'].save(SHOTS / path)
                evidence.append({key: value for key, value in record.items()
                                 if key != 'image'} | {'screenshot': path})
            audit = {
                'sample_interval_frames': 1,
                'frames_observed': sum(x['frames_observed'] for x in evidence),
                'distinct_scene_images': len(hatch_frames), 'poses': evidence}
            animation_audit[label]['consecutive_frame_audit'] = audit
            (OUTPUT / 'hatch-transition-audit.json').write_text(json.dumps({
                'rom_sha256': hashlib.sha256(ROM.read_bytes()).hexdigest(),
                **audit}, indent=2) + '\n')
            check(len(hatch_frames) == 2,
                  f'{label} shows exactly two complete hatch poses without torn frames')
        check(elapsed >= 24, f'{label} stays visible long enough to observe')
        check(distinct >= 2, f'{label} contains visible animated movement')
        if destination is not None:
            self.scene(destination)
            if destination in (LAUNCH, FLIGHT):
                self.tick(18)
        else:
            check(self.state()[0] == scene and self.state()[1] == 1,
                  f'{label} waits for the player after the animation')
            # The state changes before the hidden tilemap is committed.
            # Let the ready caption reach the screen before the next snapshot.
            self.tick(12)

    def launch(self):
        """Repeated A presses must still show the whole launch countdown."""
        self.scene(LAUNCH)
        check(self.state()[1] >= 2, 'Lift off opens a dedicated countdown screen')
        observed, durations, pictures = [], {}, {}
        self.record_action('press a repeatedly during countdown')
        for elapsed in range(1500):
            if self.state()[0] != LAUNCH:
                break
            count = self.value('launch_count')
            if not observed or observed[-1] != count:
                observed.append(count)
            durations[count] = durations.get(count, 0) + 1
            if durations[count] == 20:
                label = str(count) if count else 'go'
                self.shot('countdown-' + label)
                pictures[count] = self.p.screen.image.crop((40, 24, 120, 112)).tobytes()
            # Fresh presses, not just a held button, test accidental skipping.
            if elapsed % 12 == 0:
                self.p.button_press('a')
            elif elapsed % 12 == 6:
                self.p.button_release('a')
            self.tick(1)
        self.p.button_release('a')
        self.tick(6)
        check(observed == list(range(10, -1, -1)),
              'launch shows every number from 10 to 1, then GO in order')
        check(all(durations.get(count, 0) >= 40 for count in range(11)),
              'repeated A presses cannot skip any countdown step')
        check(sum(durations.get(count, 0) for count in range(1, 11)) >= 560,
              'the numbered countdown lasts about ten seconds of emulated time')
        check(len(pictures) == 11 and len(set(pictures.values())) == 11,
              'each countdown step visibly changes the center of the screen')
        self.scene(ASCENT)
        animation_audit['countdown'] = {'values': observed,
                                         'frames_per_value': durations}

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
s.scene(TITLE)
s.shot('01-title')
check(not s.value('has_save'), 'blank SRAM offers a new adventure')
s.press('a')
s.scene(NAME_SETUP)
check(s.value('name_length') == 0, 'a new adventure starts with an empty player name')
s.press('left')
check(s.value('name_choice') == 6, 'Left wraps from A to G within the first row')
s.press('right')
check(s.value('name_choice') == 0, 'Right wraps from G to A')
s.press('up')
check(s.value('name_choice') == 21, 'Up wraps from A to V')
s.press('down')
check(s.value('name_choice') == 0, 'Down wraps from V to A')
s.name_focus(25)
s.press('right')
check(s.value('name_choice') == 21, 'the short last row wraps from Z to V')
s.press('left')
check(s.value('name_choice') == 25, 'the short last row wraps from V to Z')
s.name_focus(20)
s.press('down')
check(s.value('name_choice') == 6, 'the G column skips missing cells below U')
s.press('up')
check(s.value('name_choice') == 20, 'the G column wraps upward to U')
visited = set()
for index in range(26):
    s.name_focus(index)
    visited.add(s.value('name_choice'))
check(visited == set(range(26)), 'all 26 alphabet cells are reachable without action cells')
s.press('start')
check(s.state()[0] == NAME_SETUP and s.value('name_error'),
      'Start rejects an empty name without beginning the adventure')
s.press('b')
check(s.value('name_length') == 0, 'erasing an empty name is harmless')
s.type_name('NOVAX')
s.press('b')
check(s.name() == 'NOVA', 'B erases the last letter')
s.type_name('Y')
s.press('b')
check(s.name() == 'NOVA', 'letters remain editable before Start')
s.shot('02-name-setup')
s.press('start')
s.scene(PILOT_CARD)
check(s.name() == 'NOVA', 'Start confirms the chosen name before showing the pilot')
s.tick(18)
s.shot('03-pilot')
check(s.shows('NOVA'), 'the pilot introduction displays the chosen name')
s.press('a')
s.scene(YARD)
s.tick(18)
s.shot('04-backyard')
s.press('a')
s.animation(DOOR_NOTE, '05-door-zoom')
s.shot('05-door-note')
s.press('a')
s.scene(HATCH)
s.tick(18)
s.shot('06-open-code')

# Both halves of the active cipher column must visibly move together.
before_answers = s.state()[4:11]
before_focus = s.p.screen.image.copy()
s.press('down')
s.tick(6)
check(s.value('slot') == 1 and s.state()[4:11] == before_answers,
      'Down moves the cipher focus without entering an answer')
after_focus = s.p.screen.image.copy()
s.shot('06-cipher-focus')
check(before_focus.crop((16, 48, 48, 64)).tobytes()
      != after_focus.crop((16, 48, 48, 64)).tobytes(),
      'moving the cursor visibly changes the secret-symbol highlight')
check(before_focus.crop((16, 72, 48, 88)).tobytes()
      != after_focus.crop((16, 72, 48, 88)).tobytes(),
      'moving the cursor also changes its answer highlight')
s.press('up')
check(s.value('slot') == 0, 'Up returns the cipher focus to the first pair')

# The free guide never fills the answer for the player.
before = s.state()
s.press('select')
check(s.value('ui_mode') == 1, 'Select opens help')
s.press('a')
check(s.value('ui_mode') == 2, 'word help opens')
s.press('right')
s.press('right')
check(s.value('word_index') == 2, 'the beacon definition remains available')
s.shot('07-word-help')
for _ in range(3):
    s.press('right')
check(s.value('word_index') == 5 and s.shows('SOUTH POLE'),
      'word help explains the south pole before the landing')
s.shot('07-south-pole-help')
s.press('b')
s.press('down')
s.press('a')
check(s.value('ui_mode') == 3, 'puzzle help opens')
s.press('a')
s.press('a')
s.shot('07-puzzle-help')
s.press('a')
check(s.value('ui_mode') == 0 and s.state() == before,
      'all three hint steps return without entering answers')

# An incorrect letter is explained immediately and can be corrected or undone.
s.press('a')
check(s.state()[4] == 2 and s.value('wrong') and s.value('slot') == 0,
      'an incorrect letter remains selected and shows immediate feedback')
s.shot('08-wrong-answer')
check(s.shows("THAT DOESN'T MATCH"), 'incorrect answers have explicit guidance')
s.press('b')
check(s.state()[4] == 255 and not s.value('wrong'), 'B undoes the incorrect attempt')
s.choose(2)
check(s.state()[4] == 0 and s.value('slot') == 1, 'O solves the first pair and moves focus')
partial = s.power_off()
s = Session(partial)
s.scene(TITLE)
check(s.value('has_save') and s.name() == 'NOVA', 'a v3 save retains the player name')
s.press('a')
s.scene(HATCH)
check(s.state()[4] == 0 and s.state()[5] == 255 and s.value('slot') == 1,
      'a partial puzzle resumes at its next unfinished pair')
s.choose(3)
s.choose(0)
s.choose(0)
check(s.value('wrong') and not (s.state()[3] & 1),
      'an incorrect final pair does not open the hatch')
s.press('b')
check(not s.value('wrong') and s.state()[7] == 255, 'undo restores the last blank')
s.choose(1)
check(s.state()[3] & 1, 'OPEN solves the hatch message')
s.shot('09-open-solved')
s.await_scene(HATCH_OPEN, 180)
s.animation(HATCH_OPEN, '10-hatch-opening')
s.shot('10-hatch-open')
s.press('a')
s.scene(REPAIR)
s.tick(18)
s.shot('11-crystal-before')
s.press('a')
s.animation(CRYSTAL_REPAIR, '12-crystal-repair', destination=SHIP_WAKE)
check(s.state()[2] == 1, 'replacing the crystal repairs the ship')
s.animation(SHIP_WAKE, '13-ship-waking')
s.shot('13-ship-awake')
s.press('a')
s.scene(EMPTY_SEAT)
s.tick(18)
s.shot('14-empty-cockpit')
s.press('a')
check(s.state()[0] == EMPTY_SEAT and s.state()[1] == 1,
      'the empty cockpit gives the player a second story beat')
s.press('a')
s.animation(OWNER_LOG, '15-owner-log')
s.shot('15-owner-log')
s.press('a')
s.animation(MOON_NOTE, '16-moon-zoom')
s.shot('16-moon-note')
s.press('a')
s.scene(DESTINATION)
s.choose(1)
s.choose(2)
check(s.value('slot') == 3 and s.state()[11] == 0,
      'one O mapping fills both circles in MOON')
s.shot('17-moon-repeats')
s.choose(0)
check(s.state()[3] & 2, 'MOON uses the same symbol key as OPEN')
s.await_scene(LAUNCH, 180)
s.tick(18)
check(s.state()[1] == 0, 'the repaired ship waits for the player to board')
s.press('a')
s.animation(BOARDING, '18-boarding', destination=LAUNCH)
check(s.state()[1] == 1, 'boarding finishes at the lift-off prompt')
s.press('a')
check(s.state()[0] == LAUNCH and s.state()[1] >= 2,
      'lift off starts a full-screen countdown')
launch_save = s.power_off()
s = Session(launch_save)
s.press('a')
s.scene(LAUNCH)
check(s.state()[1] == 1, 'a restart during countdown returns to the lift-off prompt')
s.press('a')
s.launch()
s.animation(ASCENT, '19-powered-ascent', destination=FLIGHT)
check(animation_audit['19-powered-ascent']['observed_frames'] >= 420,
      'powered ascent plays before manual space flight begins')
s.tick(18)
s.shot('20-flight')
x, y = s.state()[25:27]
s.hold('right', 32)
check(s.state()[25] > x and s.state()[26] == y, 'Right steers only horizontally')
stopped = s.state()[25:27]
s.tick(60)
check(s.state()[25:27] == stopped, 'releasing the controls stops the ship')
s.hold('left', 180)
check(s.state()[25] == 8, 'the left flight boundary holds')
s.hold('down', 160)
check(s.state()[26] == 87, 'the lower flight boundary holds')
s.press('start')
check(s.value('ui_mode') == 4, 'Start pauses flight')
s.hold('right', 40)
check(s.state()[25:27] == bytes([8, 87]), 'paused flight ignores steering')
s.press('b')
check(s.state()[27] == 0, 'the sound setting can be switched off')
flight_save = s.power_off()
s = Session(flight_save)
s.press('a')
s.scene(FLIGHT)
check(s.state()[25:28] == bytes([8, 87, 0]), 'v3 saves retain position and sound')
s.press('start')
s.press('b')
s.press('a')
check(s.value('ui_mode') == 0 and s.state()[27] == 1, 'pause resumes with sound restored')
s.hold('up', 150)
s.p.button_press('right')
s.record_action('steer toward the Moon')
for _ in range(400):
    if s.state()[0] != FLIGHT:
        break
    s.tick(1)
s.p.button_release('right')
s.tick(6)
s.scene(MOON_APPROACH)
s.tick(18)
s.shot('21-south-pole-approach')
check(not s.value('nav_ready'), 'the ship approaches before lining up to land')
s.press('a')
check(s.state()[0] == MOON_APPROACH, 'A cannot land before the ship is lined up')


def steer_coordinate(session, offset, target, negative, positive):
    key = positive if session.state()[offset] < target else negative
    session.record_action(f'align with {key}')
    session.p.button_press(key)
    for _ in range(500):
        if abs(session.state()[offset] - target) <= 1:
            break
        session.tick(1)
    session.p.button_release(key)
    session.tick(6)
    check(abs(session.state()[offset] - target) <= 2,
          f'{key} steers toward the landing target')


steer_coordinate(s, 25, 118, 'left', 'right')
steer_coordinate(s, 26, 75, 'up', 'down')
check(s.value('nav_ready'), 'aligning above the safe lunar spot makes landing available')
s.tick(60)
check(s.state()[0] == MOON_APPROACH, 'aligned landing waits for an A press')
s.shot('22-landing-ready')
s.press('a')
s.scene(MOON_WALK)
s.tick(18)
s.shot('23-moon-walk')
walk_start = s.state()[25]
s.press('a')
check(s.state()[0] == MOON_WALK, 'the pilot must walk closer before meeting the alien')
s.hold('right', 32)
check(s.state()[25] > walk_start, 'Right moves the pilot across the lunar surface')
s.p.button_press('right')
s.record_action('walk toward the alien')
for _ in range(400):
    if s.value('nav_ready'):
        break
    s.tick(1)
s.p.button_release('right')
s.tick(6)
check(s.state()[0] == MOON_WALK and s.value('nav_ready'),
      'approaching the alien makes first contact available')
s.shot('24-near-alien')
s.press('a')
s.scene(FIRST_CONTACT)
s.tick(18)
s.shot('25-first-contact')
s.observe_motion('25-first-contact-motion')
s.press('a')
check(s.state()[0] == FIRST_CONTACT and s.state()[1] == 1,
      'the first greeting gives the alien a chance to respond')
s.tick(18)
s.shot('25a-hello-confused')
s.press('a')
check(s.state()[0] == FIRST_CONTACT and s.state()[1] == 2,
      'a second greeting leads the pilot to inspect the shared symbols')
s.tick(18)
s.shot('25b-hi-symbols')
s.press('a')
s.scene(ALIEN_REPLY)
s.tick(18)
s.shot('26-alien-reply')
s.press('a')
s.scene(HELLO)
s.tick(18)
s.shot('27-send-hello')
hello_save = s.power_off()
s = Session(hello_save)
s.press('a')
s.scene(HELLO)
s.press('b')
check(s.state()[0] == HELLO and not (s.state()[3] & 4),
      'B cannot skip the first-contact HI message')
s.choose(0)
check(s.value('wrong') and s.state()[0] == HELLO, 'an incorrect HI symbol stays editable')
s.press('b')
check(not s.value('wrong') and s.state()[0] == HELLO, 'B undoes HI without leaving contact')
s.choose(1)
s.choose(0)
check(s.state()[3] == 7, 'HI encodes the greeting using the shared symbol key')
s.shot('28-hello-solved')
s.await_scene(FRIENDSHIP, 180)
s.animation(FRIENDSHIP, '29-friendship')
s.shot('29-friends')
s.press('a')
s.scene(ENDING)
s.tick(18)
s.shot('30-complete')
complete_save = s.power_off()
s = Session(complete_save)
s.press('a')
s.scene(ENDING)
check(s.state()[3] == 7 and s.name() == 'NOVA', 'v3 saves retain the completed adventure')
s.press('a')
s.scene(TITLE)
s.press('b')
check(s.value('ui_mode') == 5, 'starting over requires deliberate confirmation')
s.press('b')
check(s.value('ui_mode') == 0, 'the new-adventure confirmation can be cancelled')
s.confirm_new_trip(test_short_hold=True)
s.type_name('LU')
pending_name_save = s.power_off()
check(pending_name_save == complete_save, 'editing a new name preserves the completed save')
s = Session(pending_name_save)
s.press('a')
s.scene(ENDING)
check(s.name() == 'NOVA' and s.state()[3] == 7,
      'power loss before confirming a new name preserves the old v3 adventure')
s.press('a')
s.confirm_new_trip()
s.type_name('LUNA')
s.press('start')
s.scene(PILOT_CARD)
check(s.name() == 'LUNA' and s.state()[2:4] == bytes([0, 0])
      and s.state()[4:25] == bytes([255] * 21), 'Start commits fresh named progress')
portrait_save = s.power_off()
s = Session(portrait_save)
s.press('a')
s.scene(YARD)
check(s.name() == 'LUNA', 'restart during the pilot card resumes the durable backyard checkpoint')
s.p.stop(save=False)

# Eight letters fit; a ninth letter cannot overwrite the NUL or finish setup.
s = Session()
s.press('a')
s.type_name('STARLITE')
s.name_pick(25)
check(s.value('name_length') == 8 and s.value('name_error'),
      'a ninth letter is rejected visibly')
check(s.state()[0] == NAME_SETUP, 'pressing A on a letter never finishes name setup')
s.press('start')
s.scene(PILOT_CARD)
check(s.state()[28:37] == b'STARLITE\0', 'the longest name keeps its terminating NUL')
s.tick(18)
s.shot('31-long-name-pilot')
long_name_save = s.power_off()
s = Session(long_name_save)
s.press('a')
s.scene(YARD)
check(s.name() == 'STARLITE', 'all eight name letters survive a restart')
s.p.stop(save=False)

# Recovery uses durable flight checkpoints, avoiding transient animation timing.
records = records_from(flight_save)
check(all(r[0] == 0xa7 and r[1] == 3 and r[3] == GAME_SIZE for r in records),
      'normal play writes committed version-3 records')
latest = latest_slot(records)
older = latest ^ 1
for label, modification in [('interrupted', 'marker'), ('bad CRC', 'payload')]:
    broken = bytearray(flight_save)
    if modification == 'marker':
        broken[latest * 64] = 0
    else:
        broken[latest * 64 + 20] ^= 128
    s = Session(bytes(broken))
    s.press('a')
    check(s.state() == records[older][4:41], f'{label} save falls back to the older v3 slot')
    s.p.stop(save=False)
broken = bytearray(flight_save)
broken[0] = broken[64] = 0
s = Session(bytes(broken))
check(not s.value('has_save'), 'two invalid records safely offer a new adventure')
s.press('a')
s.scene(NAME_SETUP)
s.p.stop(save=False)

for label, bad_name in [('empty', bytes(9)), ('lowercase', b'nova\0\0\0\0\0'),
                        ('nonletter', b'NO1A\0\0\0\0\0'), ('unterminated', b'ABCDEFGHI')]:
    broken = bytearray(flight_save)
    record = bytearray(records[latest])
    record[32:41] = bad_name
    broken[latest * 64:(latest + 1) * 64] = with_crc(record)
    s = Session(bytes(broken))
    check(s.value('has_save'), f'an invalid {label} name leaves the older save available')
    s.press('a')
    check(s.state() == records[older][4:41], f'an invalid {label} name falls back to the intact slot')
    s.p.stop(save=False)

# The user requested a fresh experience for saves from earlier game designs.
for old_version, length in [(1, 28), (2, 37)]:
    old_ram = bytearray(8192)
    old_ram[:64] = fixture_record(records[latest][4:4 + length], old_version, 90)
    s = Session(bytes(old_ram))
    check(not s.value('has_save'), f'a valid v{old_version} save starts a fresh v3 adventure')
    s.press('a')
    s.scene(NAME_SETUP)
    check(s.value('name_length') == 0, f'v{old_version} progress does not carry into v3')
    unchanged = s.power_off()
    check(unchanged == bytes(old_ram), 'unconfirmed setup does not rewrite older save bytes')


def contact_sheet(names, filename, columns=3):
    rows = (len(names) + columns - 1) // columns
    sheet = Image.new('RGB', (columns * 336, rows * 324), '#0a1724')
    draw = ImageDraw.Draw(sheet)
    for index, name in enumerate(names):
        x, y = (index % columns) * 336 + 8, (index // columns) * 324 + 24
        shot = Image.open(SHOTS / (name + '.png')).convert('RGB')
        sheet.paste(shot.resize((320, 288), Image.Resampling.NEAREST), (x, y))
        draw.text((x, y - 18), name.replace('-', ' ').upper(), fill='#e8ebd4')
    sheet.save(SHOTS / filename)


contact_sheet(['03-pilot', '04-backyard', '10-hatch-open', '13-ship-awake',
               '20-flight', '30-complete'], 'chapter1-contact-sheet.png')
contact_sheet(['02-name-setup', '03-pilot', '06-open-code', '06-cipher-focus',
               '08-wrong-answer', '09-open-solved', 'countdown-10', 'countdown-5',
               'countdown-go', '22-landing-ready', '25-first-contact', '29-friends'],
              'ux-contact-sheet.png')
contact_sheet(['05-door-zoom-early', '05-door-zoom-middle', '05-door-zoom-late',
               '10-hatch-opening-early', '10-hatch-opening-middle', '10-hatch-opening-late',
               '12-crystal-repair-early', '12-crystal-repair-middle', '12-crystal-repair-late',
               '19-powered-ascent-early', '19-powered-ascent-middle', '19-powered-ascent-late'],
              'cinema-contact-sheet.png')
check(frame_audit['lcd_disabled_frames'] == 0, 'LCD stays enabled on every observed frame after boot')
check(frame_audit['uniform_frames'] == 0, 'no blank frame appears during inputs, scenes, or animation')
report = {'rom_sha256': hashlib.sha256(ROM.read_bytes()).hexdigest(),
          'emulator': f'PyBoy {version("pyboy")}', 'checks_passed': len(checks),
          'checks': checks, 'frame_audit': frame_audit, 'animation_audit': animation_audit,
          'hardware_tested': False}
(OUTPUT / 'playthrough-report.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'PASS: {len(checks)} checks; alphabet, pilot, animated story, full route, '
      'countdown, ascent, controlled landing, first contact and v3 save recovery. '
      f'{frame_audit["frames_observed"]} consecutive frames checked for blanking.')
