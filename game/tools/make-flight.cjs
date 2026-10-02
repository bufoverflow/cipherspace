#!/usr/bin/env node
'use strict';

// Original procedural Game Boy Color art. Run from any directory:
// node game/tools/make-flight.cjs
// No source image or player sprite is used. Palette 7 is reserved for the UI.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const assert = require('node:assert/strict');

const gameDir = path.resolve(__dirname, '..');
const generatedDir = path.join(gameDir, 'generated');
const previewDir = path.join(gameDir, 'art-preview');
const W = 160;
const H = 96;
const BG = '#071027';
const DIM = '#182b49';
const STAR = '#6e90ad';
const WHITE = '#e1e9e5';
const SEA_DARK = '#12335c';
const SEA = '#246896';
const LAND = '#4fa6a0';
const CLOUD = '#cadfdf';
const ATMOSPHERE = '#76bfd5';
const MOON_DARK = '#3d4865';
const MOON_SHADE = '#6c7892';
const MOON_MID = '#a1adc1';
const MOON_LIGHT = '#d6dbe3';
const GOLD_DARK = '#74614b';
const GOLD = '#ddb464';
const GOLD_LIGHT = '#fff0b0';
const ROUTE = '#55889b';

function rgb(hex) {
  return [1, 3, 5].map(i => parseInt(hex.slice(i, i + 2), 16));
}

function to555(value) {
  const [r, g, b] = typeof value === 'string' ? rgb(value) : value;
  return Math.round(r * 31 / 255) | (Math.round(g * 31 / 255) << 5) | (Math.round(b * 31 / 255) << 10);
}

function from555(word) {
  return [word & 31, (word >>> 5) & 31, (word >>> 10) & 31].map(v => Math.round(v * 255 / 31));
}

function pixelNoise(x, y, salt = 0) {
  let n = Math.imul(x + 17, 374761393) + Math.imul(y + 31, 668265263) + Math.imul(salt + 5, 1274126177);
  n = Math.imul(n ^ (n >>> 13), 1274126177);
  return ((n ^ (n >>> 16)) >>> 0) / 4294967296;
}

function canvas() {
  return Array(W * H).fill(to555(BG));
}

function put(pixels, x, y, value) {
  x = Math.round(x);
  y = Math.round(y);
  if (x >= 0 && x < W && y >= 0 && y < H) pixels[y * W + x] = typeof value === 'number' ? value : to555(value);
}

function stars(pixels, salt) {
  // Sparse, deterministic pinpoints; only two large stars per scene.
  for (let y = 2; y < H - 2; y++) {
    for (let x = 2; x < W - 2; x++) {
      const value = pixelNoise(x, y, salt);
      if (value > 0.9968) put(pixels, x, y, STAR);
      else if (value > 0.991) put(pixels, x, y, DIM);
    }
  }
  for (const [x, y] of salt === 1 ? [[24, 19], [90, 11]] : [[24, 15], [137, 26]]) {
    put(pixels, x, y, WHITE);
    put(pixels, x - 1, y, STAR);
    put(pixels, x + 1, y, STAR);
    put(pixels, x, y - 1, STAR);
    put(pixels, x, y + 1, STAR);
  }
}

function earth(pixels) {
  const cx = 18;
  const cy = 94;
  const radius = 30;
  for (let y = cy - radius; y <= cy + radius; y++) {
    for (let x = cx - radius; x <= cx + radius; x++) {
      const dx = (x - cx) / radius;
      const dy = (y - cy) / radius;
      const rr = dx * dx + dy * dy;
      if (rr > 1) continue;
      const z = Math.sqrt(1 - rr);
      const light = Math.max(0, -0.45 * dx - 0.52 * dy + 0.73 * z);
      let value = light > 0.62 ? SEA : SEA_DARK;
      // Abstract land shapes: deliberately no literal geographic outlines.
      const land = Math.sin(dx * 7 + dy * 3) + Math.cos(dy * 9 - dx * 2) + Math.sin(dx * 12 - dy * 8) * 0.4;
      if (land > 0.9 && rr < 0.93 && light > 0.35) value = LAND;
      const cloudBand = Math.sin(dx * 5.2 + 0.4) * 0.1 - 0.48;
      const cloudBand2 = Math.sin(dx * 6.5 - 1.2) * 0.085 - 0.08;
      if ((Math.abs(dy - cloudBand) < 0.035 && dx > -0.8 && dx < 0.66) ||
          (Math.abs(dy - cloudBand2) < 0.035 && dx > -0.45 && dx < 0.78)) value = CLOUD;
      if (rr > 0.91 && dy < 0.16 && dx < 0.7) value = ATMOSPHERE;
      put(pixels, x, y, value);
    }
  }
}

function moon(pixels, cx, cy, radius) {
  const shades = [MOON_DARK, MOON_SHADE, MOON_MID, MOON_LIGHT];
  for (let y = cy - radius; y <= cy + radius; y++) {
    for (let x = cx - radius; x <= cx + radius; x++) {
      const dx = (x - cx) / radius;
      const dy = (y - cy) / radius;
      const rr = dx * dx + dy * dy;
      if (rr > 1) continue;
      const z = Math.sqrt(1 - rr);
      const light = Math.max(0, -0.42 * dx - 0.5 * dy + 0.76 * z);
      let shade = light > 0.84 ? 3 : light > 0.47 ? 2 : light > 0.13 ? 1 : 0;
      if (pixelNoise(x, y, 7) > 0.94 && shade > 0) shade--;
      put(pixels, x, y, shades[shade]);
    }
  }
  const craters = [
    [-0.29, -0.33, 0.23], [0.34, -0.12, 0.17], [-0.06, 0.42, 0.16],
    [-0.57, 0.15, 0.115], [0.31, 0.47, 0.08], [0.12, -0.61, 0.075],
  ];
  for (const [nx, ny, size] of craters) {
    const px = Math.round(cx + nx * radius);
    const py = Math.round(cy + ny * radius);
    const r = Math.max(1, Math.round(radius * size));
    for (let y = -r; y <= r; y++) {
      for (let x = -r; x <= r; x++) {
        const d = x * x + y * y;
        if (d > r * r || ((px + x - cx) ** 2 + (py + y - cy) ** 2) > radius * radius) continue;
        const outer = d >= (r - 1) ** 2;
        put(pixels, px + x, py + y, outer ? (x + y < 0 ? MOON_LIGHT : MOON_DARK) : MOON_SHADE);
      }
    }
  }
}

function route(pixels) {
  // A dotted quadratic path; the final dot stops short of the Moon's edge.
  for (let i = 0; i <= 17; i++) {
    const t = i / 17;
    const x = (1 - t) ** 2 * 40 + 2 * (1 - t) * t * 71 + t * t * 112;
    const y = (1 - t) ** 2 * 80 + 2 * (1 - t) * t * 46 + t * t * 38;
    put(pixels, x, y, ROUTE);
  }
}

function signal(pixels) {
  // A small surface transmitter centered on (84, 79), with three radio arcs.
  for (let radius of [12, 21, 30]) {
    for (let angle = -0.48; angle <= 0.28; angle += 0.035) {
      const x = 84 + Math.cos(angle) * radius;
      const y = 77 + Math.sin(angle) * radius;
      put(pixels, x, y, radius === 30 ? GOLD_DARK : GOLD);
    }
  }
  for (let y = 76; y <= 81; y++) put(pixels, 84, y, GOLD);
  for (let x = 81; x <= 87; x++) put(pixels, x, 82, GOLD_DARK);
  for (let x = 82; x <= 86; x++) put(pixels, x, 81, GOLD);
  put(pixels, 84, 74, GOLD_LIGHT);
  put(pixels, 83, 75, GOLD);
  put(pixels, 84, 75, GOLD_LIGHT);
  put(pixels, 85, 75, GOLD);
  put(pixels, 84, 76, GOLD_LIGHT);
}

function distance(a, b) {
  const dr = a[0] - b[0];
  const dg = a[1] - b[1];
  const db = a[2] - b[2];
  return 2 * dr * dr + 3 * dg * dg + 2 * db * db;
}

function encode(pixels, hexPalettes) {
  const palettes = hexPalettes.map(p => p.map(to555));
  assert.equal(palettes.length, 7);
  palettes.forEach(p => { assert.equal(p.length, 4); assert.equal(new Set(p).size, 4); });
  const colors = palettes.map(p => p.map(from555));
  const mapped = new Map();
  for (const word of new Set(pixels)) {
    mapped.set(word, colors.map(palette => {
      let index = 0;
      let error = Infinity;
      palette.forEach((color, i) => {
        const candidate = distance(from555(word), color);
        if (candidate < error) { error = candidate; index = i; }
      });
      return { index, error };
    }));
  }
  const tiles = [];
  const attrs = [];
  for (let ty = 0; ty < 12; ty++) {
    for (let tx = 0; tx < 20; tx++) {
      const tile = [];
      const error = Array(7).fill(0);
      for (let y = 0; y < 8; y++) {
        for (let x = 0; x < 8; x++) {
          const word = pixels[(ty * 8 + y) * W + tx * 8 + x];
          tile.push(word);
          mapped.get(word).forEach((match, p) => { error[p] += match.error; });
        }
      }
      const palette = error.indexOf(Math.min(...error));
      attrs.push(palette);
      for (let y = 0; y < 8; y++) {
        let low = 0;
        let high = 0;
        for (let x = 0; x < 8; x++) {
          const index = mapped.get(tile[y * 8 + x])[palette].index;
          low |= (index & 1) << (7 - x);
          high |= ((index >>> 1) & 1) << (7 - x);
        }
        tiles.push(low, high);
      }
    }
  }
  assert.equal(tiles.length, 3840);
  assert.equal(attrs.length, 240);
  assert(attrs.every(value => value >= 0 && value <= 6));
  return { tiles, attrs, palettes: palettes.flat() };
}

function reconstruct({ tiles, attrs, palettes }) {
  const pixels = Buffer.alloc(W * H * 3);
  for (let tile = 0; tile < 240; tile++) {
    for (let y = 0; y < 8; y++) {
      for (let x = 0; x < 8; x++) {
        const low = tiles[tile * 16 + y * 2];
        const high = tiles[tile * 16 + y * 2 + 1];
        const index = ((low >>> (7 - x)) & 1) | (((high >>> (7 - x)) & 1) << 1);
        const offset = ((Math.floor(tile / 20) * 8 + y) * W + (tile % 20) * 8 + x) * 3;
        pixels.set(from555(palettes[attrs[tile] * 4 + index]), offset);
      }
    }
  }
  return pixels;
}

function array(type, name, values, width, columns) {
  const lines = [];
  for (let i = 0; i < values.length; i += columns) {
    lines.push('    ' + values.slice(i, i + columns).map(v => '0x' + v.toString(16).padStart(width, '0')).join(', ') + ',');
  }
  return `const ${type} ${name}[${values.length}] = {\n${lines.join('\n')}\n};\n`;
}

function makeScenes() {
  const flight = canvas();
  stars(flight, 1);
  earth(flight);
  moon(flight, 128, 28, 17);
  route(flight);
  const beacon = canvas();
  stars(beacon, 2);
  moon(beacon, 72, 50, 33);
  signal(beacon);
  return {
    flight: encode(flight, [
      [BG, DIM, STAR, WHITE],
      [BG, SEA, LAND, CLOUD],
      [SEA_DARK, SEA, LAND, CLOUD],
      [BG, SEA_DARK, ATMOSPHERE, CLOUD],
      [BG, MOON_DARK, MOON_MID, MOON_LIGHT],
      [MOON_DARK, MOON_SHADE, MOON_MID, MOON_LIGHT],
      [BG, MOON_DARK, MOON_SHADE, ROUTE],
    ]),
    beacon: encode(beacon, [
      [BG, DIM, STAR, WHITE],
      [BG, MOON_DARK, MOON_MID, MOON_LIGHT],
      [MOON_DARK, MOON_SHADE, MOON_MID, MOON_LIGHT],
      [BG, MOON_DARK, MOON_SHADE, MOON_MID],
      [MOON_SHADE, MOON_MID, GOLD, GOLD_LIGHT],
      [BG, GOLD_DARK, GOLD, GOLD_LIGHT],
      [BG, MOON_DARK, MOON_MID, GOLD],
    ]),
  };
}

async function main() {
  fs.mkdirSync(generatedDir, { recursive: true });
  fs.mkdirSync(previewDir, { recursive: true });
  const scenes = makeScenes();
  let c = '/* Original procedural art generated by tools/make-flight.cjs. */\n#pragma bank 6\n#include <stdint.h>\n\n';
  let declarations = '';
  for (const [name, scene] of Object.entries(scenes)) {
    c += array('uint8_t', `${name}_tiles`, scene.tiles, 2, 16) + '\n';
    c += array('uint8_t', `${name}_attrs`, scene.attrs, 2, 20) + '\n';
    c += array('uint16_t', `${name}_palettes`, scene.palettes, 4, 4) + '\n';
    declarations += `extern const uint8_t ${name}_tiles[3840];\nextern const uint8_t ${name}_attrs[240];\nextern const uint16_t ${name}_palettes[28];\n\n`;
    console.log(`${name}: 160x96, 240 tiles, 3840 tile bytes, 240 attrs, 7 x 4 RGB555 colors, 4136 bytes`);
  }
  fs.writeFileSync(path.join(generatedDir, 'flight.c'), c.trimEnd() + '\n');
  fs.writeFileSync(path.join(generatedDir, 'flight.h'),
    '/* Generated by tools/make-flight.cjs. */\n#ifndef CIPHERSPACE_FLIGHT_H\n#define CIPHERSPACE_FLIGHT_H\n\n#include <stdint.h>\n\n' + declarations + '#endif\n');
  const written = fs.readFileSync(path.join(generatedDir, 'flight.c'), 'utf8');
  for (const [name, scene] of Object.entries(scenes)) {
    for (const key of ['tiles', 'attrs', 'palettes']) {
      const body = written.match(new RegExp(`${name}_${key}\\[\\d+\\] = \\{([^}]+)\\}`))[1];
      const recovered = [...body.matchAll(/0x([0-9a-f]+)/g)].map(m => parseInt(m[1], 16));
      assert.deepEqual(recovered, scene[key]);
    }
  }
  let sharp;
  for (const candidate of [process.env.SHARP_PATH, 'sharp', path.join(os.homedir(), '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp')].filter(Boolean)) {
    try { sharp = require(candidate); break; } catch (error) { if (error.code !== 'MODULE_NOT_FOUND') throw error; }
  }
  if (sharp) {
    const previews = [];
    for (const [name, scene] of Object.entries(scenes)) {
      const pixels = reconstruct(scene);
      const raw = { width: W, height: H, channels: 3 };
      await sharp(pixels, { raw }).png().toFile(path.join(previewDir, `${name}.png`));
      const large = await sharp(pixels, { raw }).resize(W * 4, H * 4, { kernel: 'nearest' }).png().toBuffer();
      previews.push({ input: large, left: previews.length * W * 4, top: 0 });
    }
    await sharp({ create: { width: W * 8, height: H * 4, channels: 3, background: BG } })
      .composite(previews).png().toFile(path.join(previewDir, 'flight-contact-sheet.png'));
  }
  console.log('Validated emitted C round trip; 8272 total asset bytes in ROM bank 6; palette 7 unused.');
}

main().catch(error => { console.error(error); process.exitCode = 1; });
