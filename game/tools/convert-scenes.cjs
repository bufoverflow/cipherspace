#!/usr/bin/env node
'use strict';

// Run from any directory: node game/tools/convert-scenes.cjs
// Dependency: sharp (normal Node resolution, SHARP_PATH, or the Codex runtime).
// All input/output paths are resolved relative to game/, not the current shell.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

function loadSharp() {
  for (const candidate of [
    process.env.SHARP_PATH,
    'sharp',
    path.join(os.homedir(), '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/sharp'),
  ].filter(Boolean)) {
    try { return require(candidate); } catch (error) {
      if (error.code !== 'MODULE_NOT_FOUND') throw error;
    }
  }
  throw new Error('Install sharp or set SHARP_PATH to its module directory.');
}

const sharp = loadSharp();
const gameDir = path.resolve(__dirname, '..');
const sourcePath = path.resolve(gameDir, '../design-assets/opening-scenes-v2.png');
const outputDir = path.join(gameDir, 'generated');
const previewDir = path.join(gameDir, 'art-preview');
const WIDTH = 160;
const HEIGHT = 96;
const TILE_SIZE = 8;
const TILE_COUNT = WIDTH * HEIGHT / (TILE_SIZE * TILE_SIZE);
const PALETTE_COUNT = 7; // BG palette 7 belongs to the text interface.
const COLORS_PER_PALETTE = 4;
const NAMES = ['crashed-yard', 'restored-yard', 'empty-cockpit', 'lunar-landing'];

function rgb555(r, g, b) {
  return Math.round(r * 31 / 255) |
    (Math.round(g * 31 / 255) << 5) |
    (Math.round(b * 31 / 255) << 10);
}

function unpack555(word) {
  return [word & 31, (word >>> 5) & 31, (word >>> 10) & 31]
    .map(channel => Math.round(channel * 255 / 31));
}

function linear(channel) {
  channel /= 255;
  return channel <= 0.04045 ? channel / 12.92 : ((channel + 0.055) / 1.055) ** 2.4;
}

function srgb(channel) {
  const value = channel <= 0.0031308 ? channel * 12.92 : 1.055 * channel ** (1 / 2.4) - 0.055;
  return Math.max(0, Math.min(255, value * 255));
}

function toLab(rgb) {
  const [r, g, b] = rgb.map(linear);
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return [
    0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
    1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
    0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s,
  ];
}

function fromLab([l, a, b]) {
  const ll = (l + 0.3963377774 * a + 0.2158037573 * b) ** 3;
  const mm = (l - 0.1055613458 * a - 0.0638541728 * b) ** 3;
  const ss = (l - 0.0894841775 * a - 1.2914855480 * b) ** 3;
  return [
    srgb(4.0767416621 * ll - 3.3077115913 * mm + 0.2309699292 * ss),
    srgb(-1.2684380046 * ll + 2.6097574011 * mm - 0.3413193965 * ss),
    srgb(-0.0041960863 * ll - 0.7034186147 * mm + 1.7076147010 * ss),
  ];
}

const colorCache = new Map();
function color(word) {
  if (!colorCache.has(word)) {
    const rgb = unpack555(word);
    const [r, g, b] = rgb;
    const saturation = (Math.max(...rgb) - Math.min(...rgb)) / 255;
    // Give the story's mint technology and amber lights modest extra weight.
    // This changes quantization error weights, never the source pixels.
    const mint = g > r * 1.25 && b > r * 1.15 && g > 90;
    const amber = r > b * 1.5 && g > b * 1.25 && r > 110;
    colorCache.set(word, {
      word,
      rgb,
      lab: toLab(rgb),
      importance: 1 + 0.25 * saturation + (mint ? 0.8 : 0) + (amber ? 0.5 : 0),
    });
  }
  return colorCache.get(word);
}

function distance(a, b) {
  const dl = a[0] - b[0];
  const da = a[1] - b[1];
  const db = a[2] - b[2];
  return dl * dl + da * da + db * db;
}

function nearest(point, palette) {
  let index = 0;
  let error = Infinity;
  for (let i = 0; i < palette.length; i++) {
    const candidate = distance(point.lab, palette[i].lab);
    if (candidate < error) { error = candidate; index = i; }
  }
  return { index, error };
}

function histogram(words) {
  const counts = new Map();
  for (const word of words) counts.set(word, (counts.get(word) || 0) + 1);
  return [...counts.entries()].sort((a, b) => a[0] - b[0])
    .map(([word, count]) => ({ ...color(word), count, weight: count * color(word).importance }));
}

function centroid(points) {
  const total = points.reduce((sum, point) => sum + point.weight, 0);
  const lab = [0, 0, 0];
  for (const point of points) {
    for (let i = 0; i < 3; i++) lab[i] += point.lab[i] * point.weight / total;
  }
  return color(rgb555(...fromLab(lab)));
}

function fitPalette(points, seed) {
  assert(points.length > 0);
  let palette = seed ? seed.slice() : [centroid(points)];
  while (palette.length < COLORS_PER_PALETTE) {
    let best = points[0];
    let bestScore = -1;
    for (const point of points) {
      const score = nearest(point, palette).error * Math.sqrt(point.weight);
      if (score > bestScore) { bestScore = score; best = point; }
    }
    palette.push(color(best.word));
  }
  for (let iteration = 0; iteration < 24; iteration++) {
    const groups = Array.from({ length: COLORS_PER_PALETTE }, () => []);
    for (const point of points) groups[nearest(point, palette).index].push(point);
    const next = groups.map((group, i) => group.length ? centroid(group) : palette[i]);
    if (next.every((entry, i) => entry.word === palette[i].word)) break;
    palette = next;
  }
  // Stable dark-to-light ordering also makes the emitted data easier to inspect.
  return palette.sort((a, b) => a.lab[0] - b.lab[0] || a.word - b.word);
}

function tileError(tile, palette) {
  let error = 0;
  for (const point of tile.points) error += nearest(point, palette).error * point.weight;
  return error;
}

function choosePalettes(tiles, allPoints, seedTile) {
  const palettes = [fitPalette(seedTile === null ? allPoints : tiles[seedTile].points)];
  // Seed subsequent palettes from the tile reconstructed least accurately.
  while (palettes.length < PALETTE_COUNT) {
    let worstTile = tiles[0];
    let worstError = -1;
    for (const tile of tiles) {
      const error = Math.min(...palettes.map(palette => tileError(tile, palette)));
      if (error > worstError) { worstError = error; worstTile = tile; }
    }
    palettes.push(fitPalette(worstTile.points));
  }

  let lastAssignments;
  let best;
  for (let iteration = 0; iteration < 32; iteration++) {
    const assignments = [];
    let totalError = 0;
    for (const tile of tiles) {
      let bestIndex = 0;
      let bestError = Infinity;
      for (let p = 0; p < PALETTE_COUNT; p++) {
        const error = tileError(tile, palettes[p]);
        if (error < bestError) { bestError = error; bestIndex = p; }
      }
      assignments.push(bestIndex);
      totalError += bestError;
    }
    if (!best || totalError < best.error) {
      best = { palettes: palettes.map(palette => palette.slice()), assignments, error: totalError };
    }
    if (lastAssignments && assignments.every((value, i) => value === lastAssignments[i])) break;
    lastAssignments = assignments;
    for (let p = 0; p < PALETTE_COUNT; p++) {
      const words = [];
      tiles.forEach((tile, i) => { if (assignments[i] === p) words.push(...tile.words); });
      if (words.length) palettes[p] = fitPalette(histogram(words), palettes[p]);
    }
  }
  return best;
}

function makeTiles(data) {
  const tiles = [];
  for (let ty = 0; ty < HEIGHT / TILE_SIZE; ty++) {
    for (let tx = 0; tx < WIDTH / TILE_SIZE; tx++) {
      const words = [];
      for (let y = 0; y < TILE_SIZE; y++) {
        for (let x = 0; x < TILE_SIZE; x++) {
          const offset = ((ty * TILE_SIZE + y) * WIDTH + tx * TILE_SIZE + x) * 3;
          words.push(rgb555(data[offset], data[offset + 1], data[offset + 2]));
        }
      }
      tiles.push({ words, points: histogram(words) });
    }
  }
  return tiles;
}

function packScene(tiles, result) {
  const tileBytes = [];
  tiles.forEach((tile, tileIndex) => {
    const palette = result.palettes[result.assignments[tileIndex]];
    const indices = tile.words.map(word => nearest(color(word), palette).index);
    for (let row = 0; row < TILE_SIZE; row++) {
      let low = 0;
      let high = 0;
      for (let x = 0; x < TILE_SIZE; x++) {
        const index = indices[row * TILE_SIZE + x];
        low |= (index & 1) << (7 - x);
        high |= ((index >>> 1) & 1) << (7 - x);
      }
      tileBytes.push(low, high);
    }
  });
  return {
    tileBytes,
    attrs: result.assignments,
    palettes: result.palettes.flat().map(entry => entry.word),
  };
}

function reconstruct(scene) {
  const pixels = Buffer.alloc(WIDTH * HEIGHT * 3);
  let pixelsChecked = 0;
  for (let tile = 0; tile < TILE_COUNT; tile++) {
    for (let row = 0; row < TILE_SIZE; row++) {
      const low = scene.tileBytes[tile * 16 + row * 2];
      const high = scene.tileBytes[tile * 16 + row * 2 + 1];
      for (let x = 0; x < TILE_SIZE; x++) {
        const index = ((low >>> (7 - x)) & 1) | (((high >>> (7 - x)) & 1) << 1);
        const word = scene.palettes[scene.attrs[tile] * COLORS_PER_PALETTE + index];
        const px = (tile % (WIDTH / TILE_SIZE)) * TILE_SIZE + x;
        const py = Math.floor(tile / (WIDTH / TILE_SIZE)) * TILE_SIZE + row;
        const offset = (py * WIDTH + px) * 3;
        Buffer.from(unpack555(word)).copy(pixels, offset);
        pixelsChecked++;
      }
    }
  }
  assert.equal(pixelsChecked, WIDTH * HEIGHT);
  return pixels;
}

function arrayDefinition(type, name, values, hexDigits, columns) {
  const rows = [];
  for (let i = 0; i < values.length; i += columns) {
    rows.push('    ' + values.slice(i, i + columns)
      .map(value => '0x' + value.toString(16).padStart(hexDigits, '0')).join(', ') + ',');
  }
  return `const ${type} ${name}[${values.length}] = {\n${rows.join('\n')}\n};\n`;
}

function readEmittedArray(text, name) {
  const body = text.match(new RegExp(`${name}\\[\\d+\\] = \\{([^}]+)\\}`));
  assert(body, `Missing emitted array ${name}`);
  return [...body[1].matchAll(/0x([0-9a-f]+)/g)].map(match => parseInt(match[1], 16));
}

async function main() {
  const originalSource = fs.readFileSync(sourcePath);
  const sourceHash = crypto.createHash('sha256').update(originalSource).digest('hex');
  const metadata = await sharp(originalSource).metadata();
  assert.equal(metadata.width % 2, 0, 'Source atlas width must split evenly.');
  assert.equal(metadata.height % 2, 0, 'Source atlas height must split evenly.');
  fs.mkdirSync(outputDir, { recursive: true });
  fs.mkdirSync(previewDir, { recursive: true });
  const reports = [];
  const enlargedPreviews = [];
  for (let index = 0; index < NAMES.length; index++) {
    const crop = {
      left: (index % 2) * metadata.width / 2,
      top: Math.floor(index / 2) * metadata.height / 2,
      width: metadata.width / 2,
      height: metadata.height / 2,
    };
    const { data, info } = await sharp(originalSource).extract(crop)
      .resize(WIDTH, HEIGHT, { fit: 'fill', kernel: 'lanczos3' })
      .removeAlpha().toColourspace('srgb').raw().toBuffer({ resolveWithObject: true });
    assert.equal(info.channels, 3);
    const tiles = makeTiles(data);
    const allPoints = histogram(tiles.flatMap(tile => tile.words));
    // Three deterministic starts reduce sensitivity to the initial palette.
    const starts = [null, 8 * 20 + 11, 8 * 20 + 4];
    const candidates = starts.map(seed => choosePalettes(tiles, allPoints, seed));
    const result = candidates.reduce((best, candidate) => candidate.error < best.error ? candidate : best);
    const scene = packScene(tiles, result);
    assert.equal(scene.tileBytes.length, 3840);
    assert.equal(scene.attrs.length, 240);
    assert.equal(scene.palettes.length, 28);
    assert(scene.tileBytes.every(value => Number.isInteger(value) && value >= 0 && value <= 255));
    assert(scene.attrs.every(value => Number.isInteger(value) && value >= 0 && value <= 6));
    assert(scene.palettes.every(value => Number.isInteger(value) && value >= 0 && value <= 0x7fff));
    const c = `/* Generated by tools/convert-scenes.cjs; do not edit. */\n` +
      `#pragma bank ${index + 2}\n#include <stdint.h>\n\n` +
      arrayDefinition('uint8_t', `scene${index}_tiles`, scene.tileBytes, 2, 16) + '\n' +
      arrayDefinition('uint8_t', `scene${index}_attrs`, scene.attrs, 2, 20) + '\n' +
      arrayDefinition('uint16_t', `scene${index}_palettes`, scene.palettes, 4, 4);
    const cPath = path.join(outputDir, `scene${index}.c`);
    fs.writeFileSync(cPath, c);
    // Verify what is actually on disk, then render those emitted bitplanes.
    const emitted = fs.readFileSync(cPath, 'utf8');
    const diskScene = {
      tileBytes: readEmittedArray(emitted, `scene${index}_tiles`),
      attrs: readEmittedArray(emitted, `scene${index}_attrs`),
      palettes: readEmittedArray(emitted, `scene${index}_palettes`),
    };
    assert.deepEqual(diskScene, scene);
    const preview = reconstruct(diskScene);
    const raw = { width: WIDTH, height: HEIGHT, channels: 3 };
    await sharp(preview, { raw }).png().toFile(path.join(previewDir, `scene${index}-${NAMES[index]}.png`));
    const enlarged = await sharp(preview, { raw }).resize(WIDTH * 4, HEIGHT * 4, { kernel: 'nearest' }).png().toBuffer();
    enlargedPreviews.push({ input: enlarged, left: (index % 2) * WIDTH * 4, top: Math.floor(index / 2) * HEIGHT * 4 });
    let squaredError = 0;
    for (let i = 0; i < data.length; i++) squaredError += (data[i] - preview[i]) ** 2;
    const report = {
      index,
      name: NAMES[index],
      bank: index + 2,
      width: WIDTH,
      height: HEIGHT,
      crop,
      tileCount: TILE_COUNT,
      tileBytes: scene.tileBytes.length,
      attributeBytes: scene.attrs.length,
      paletteCount: PALETTE_COUNT,
      paletteColorCounts: Array(PALETTE_COUNT).fill(COLORS_PER_PALETTE),
      paletteTileCounts: Array.from({ length: PALETTE_COUNT }, (_, p) => scene.attrs.filter(value => value === p).length),
      palettesRGB555: result.palettes.map(palette => palette.map(entry => entry.word)),
      totalAssetBytes: scene.tileBytes.length + scene.attrs.length + scene.palettes.length * 2,
      weightedOklabError: Number(result.error.toFixed(8)),
      rgbRootMeanSquaredError: Number(Math.sqrt(squaredError / data.length).toFixed(3)),
      cSha256: crypto.createHash('sha256').update(c).digest('hex'),
      validation: 'Passed: 240 tiles, 3840 tile bytes, 240 attributes in 0..6, 7 x 4 RGB555 colors, emitted C round trip, 15360 reconstructed pixels.',
    };
    reports.push(report);
    console.log(`scene${index}: ${NAMES[index]}, bank ${index + 2}, 240 tiles, 7 x 4 colors, ${report.totalAssetBytes} bytes, RGB RMSE ${report.rgbRootMeanSquaredError}`);
  }
  const declarations = reports.map(({ index }) =>
    `extern const uint8_t scene${index}_tiles[3840];\n` +
    `extern const uint8_t scene${index}_attrs[240];\n` +
    `extern const uint16_t scene${index}_palettes[28];`).join('\n\n');
  fs.writeFileSync(path.join(outputDir, 'scenes.h'),
    `/* Generated by tools/convert-scenes.cjs; do not edit. */\n` +
    `#ifndef CIPHERSPACE_SCENES_H\n#define CIPHERSPACE_SCENES_H\n\n#include <stdint.h>\n\n` +
    `#define SCENE_WIDTH_TILES 20\n#define SCENE_HEIGHT_TILES 12\n#define SCENE_TILE_COUNT 240\n#define SCENE_PALETTE_COUNT 7\n\n` +
    declarations + '\n\n#endif\n');
  fs.writeFileSync(path.join(outputDir, 'scenes.json'), JSON.stringify({
    source: '../design-assets/opening-scenes-v2.png',
    sourceSha256: sourceHash,
    sourceDimensions: { width: metadata.width, height: metadata.height },
    converter: 'tools/convert-scenes.cjs',
    sharpVersion: sharp.versions.sharp,
    resize: 'Lanczos3, exact 160 x 96 fit',
    quantization: 'RGB555, weighted Oklab, 4 colors per tile palette, 7 palette groups, 3 deterministic starts',
    dither: false,
    reservedUIPalette: 7,
    scenes: reports,
  }, null, 2) + '\n');
  await sharp({ create: { width: WIDTH * 8, height: HEIGHT * 8, channels: 3, background: '#000000' } })
    .composite(enlargedPreviews).png().toFile(path.join(previewDir, 'scenes-contact-sheet.png'));
  assert.equal(crypto.createHash('sha256').update(fs.readFileSync(sourcePath)).digest('hex'), sourceHash,
    'Source atlas must remain unchanged.');
  console.log('Validated all emitted arrays and source integrity; palette 7 remains unused.');
}

main().catch(error => { console.error(error); process.exitCode = 1; });
