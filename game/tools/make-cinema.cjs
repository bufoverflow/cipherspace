#!/usr/bin/env node
'use strict';

// Run from any directory: node game/tools/make-cinema.cjs
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
const outputDir = path.join(gameDir, 'generated');
const previewDir = path.join(gameDir, 'art-preview');
const WIDTH = 160;
const HEIGHT = 96;
const TILE_SIZE = 8;
const TILE_COUNT = WIDTH * HEIGHT / (TILE_SIZE * TILE_SIZE);
const PALETTE_COUNT = 7; // BG palette 7 belongs to the text interface.
const COLORS_PER_PALETTE = 4;
const NAMES = ['PILOT','DOOR_WIDE','DOOR_CLOSE','HATCH_HALF','HATCH_OPEN','CRYSTAL_OUT','CRYSTAL_IN','SHIP_DIM','SHIP_BRIGHT','EMPTY_COCKPIT','MOON_LOG','MOON_NOTE','BOARDING','SEATED','LAUNCH_GROUND','LAUNCH_RISE','LAUNCH_CLOUD','LAUNCH_SPACE','MOON_APPROACH','MOON_SURFACE','ALIEN_WAIT','ALIEN_CONFUSED','ALIEN_CODE','ALIEN_HAPPY'];

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

const artDir = path.resolve(gameDir, '../design-assets/cinema');
const SOURCES = [
  ['hero-repair-cockpit.png',0],['hatch-repair.png',0],['hatch-repair.png',0],
  ['hatch-repair.png',1],['hero-repair-cockpit.png',1],['hatch-repair.png',2],
  ['hatch-repair.png',3],['../opening-scenes-v2.png',1],['../opening-scenes-v2.png',1],
  ['../opening-scenes-v2.png',2],['accident-launch.png',0],['hero-repair-cockpit.png',3],
  ['hero-repair-cockpit.png',1],['hero-repair-cockpit.png',3],['accident-launch.png',1],
  ['accident-launch.png',2],['accident-launch.png',3],['space-moon-alien.png',0],
  ['space-moon-alien.png',1],['space-moon-alien.png',2],['space-moon-alien.png',3],
  ['meeting.png',0],['meeting.png',1],['meeting.png',2],
];
const rawOptions = {width:WIDTH,height:HEIGHT,channels:3};
const atlasCache = new Map();

function paint(data,x,y,rgb) {
  if(x<0||x>=WIDTH||y<0||y>=HEIGHT)return;
  data.set(rgb,(y*WIDTH+x)*3);
}
function rect(data,x,y,w,h,rgb){
  for(let yy=y;yy<y+h;yy++)for(let xx=x;xx<x+w;xx++)paint(data,xx,yy,rgb);
}
function plaque(data,x,y,w,h){
  const gold=[180,139,74],navy=[13,31,45],ivory=[229,214,170];
  rect(data,x+2,y,w-4,h,gold);rect(data,x,y+2,w,h-4,gold);
  rect(data,x+2,y+2,w-4,h-4,navy);
  for(const dx of [3,w-4])for(const dy of [3,h-4])paint(data,x+dx,y+dy,ivory);
}
async function quadrant(filename,q){
  const file=path.resolve(artDir,filename);
  if(!atlasCache.has(file))atlasCache.set(file,fs.readFileSync(file));
  const bytes=atlasCache.get(file),meta=await sharp(bytes).metadata();
  // Some generated atlases have an odd dimension. Drop the single center
  // column/row, keeping all four crops exactly equal rather than stretching one.
  const width=Math.floor(meta.width/2),height=Math.floor(meta.height/2);
  return sharp(bytes).extract({left:q%2?meta.width-width:0,top:q>=2?meta.height-height:0,width,height})
    .resize(WIDTH,HEIGHT,{fit:'fill',kernel:'lanczos3'}).removeAlpha().toColourspace('srgb').raw().toBuffer();
}
async function zoom(data,left,top,width,height){
  return sharp(data,{raw:rawOptions}).extract({left,top,width,height}).resize(WIDTH,HEIGHT,{fit:'fill',kernel:'nearest'}).raw().toBuffer();
}
function shift(data,dx,dy){
  const out=Buffer.alloc(data.length);
  for(let y=0;y<HEIGHT;y++)for(let x=0;x<WIDTH;x++){
    const sx=x-dx,sy=y-dy;
    if(sy<0||sx<0){paint(out,x,y,[5,12,28]);if((x*83+y*47)%151===0)paint(out,x,y,[89,109,155]);}
    else{
      // Mirror the narrow exposed landscape edge rather than stretching its
      // last column into horizontal bands.
      const rx=sx>=WIDTH?2*WIDTH-2-sx:sx;
      const offset=(Math.min(HEIGHT-1,sy)*WIDTH+Math.max(0,rx))*3;
      paint(out,x,y,[data[offset],data[offset+1],data[offset+2]]);
    }
  }
  return out;
}
async function framePixels(index){
  let data=await quadrant(...SOURCES[index]);
  switch(index){
    case 0:plaque(data,46,64,68,18);break; // Name: tiles x6..13,row9; covers original small badge.
    case 2:data=await zoom(data,59,10,96,78);plaque(data,24,24,112,48);break;
    case 7:
      for(let i=0;i<data.length;i+=3){
        const mint=data[i+1]>data[i]*1.25&&data[i+2]>data[i]*1.15;
        const scale=mint?0.55:0.8;
        for(let c=0;c<3;c++)data[i+c]=Math.round(data[i+c]*scale);
      }break;
    case 11:data=await zoom(data,79,34,77,60);plaque(data,24,24,112,48);break;
    case 12:data=await zoom(data,42,7,118,88);break;
    case 18:data=shift(data,40,0);break; // Actual drawn south-pole marker becomes (118,75).
    case 19:data=shift(data,-12,16);break; // Beacon becomes (132,65); walking lane stays clear.
    case 22:plaque(data,32,64,96,28);break; // HI: big symbols at (7,9),(10,9).
  }
  return data;
}

const ACTORS = [
  {
    name:'girl',palette:[[0,0,0],[52,34,49],[248,221,173],[80,172,152]],
    rows:[
      '0000011111100000','0001122222211000','0012221112222100','0122111111122210',
      '0121111111112210','0121122222212210','0121221221222210','0121222222222210',
      '0012222112222100','0001233333211000','0001332222331000','0012322222232100',
      '0122323332232210','0122323332232210','0122322222232210','0012322222232100',
      '0001323332310000','0001232223210000','0001222112210000','0001221002210000',
      '0001331003310000','0012221002221000','0011111001111000','0000000000000000',
    ],
  },
  {
    name:'alien',palette:[[0,0,0],[19,43,57],[109,203,170],[242,189,91]],
    rows:[
      '0000000000110000','0000000001221000','0000000012221000','0000111122210000',
      '0011222222110000','0122222222221100','1222332223322211','1223132231322221',
      '1223132231322221','1222332223322210','0122222222221100','0012221122211000',
      '0001333333100000','0001333333310000','0012222222131000','0121222222121000',
      '0121222222121000','0011222222110000','0001222222100000','0001222222100000',
      '0001221122100000','0012221122210000','0011110011110000','0000000000000000',
    ],
  },
];
function actorData(){
  const tiles=[],palettes=[];
  for(const actor of ACTORS){
    assert.equal(actor.rows.length,24);
    assert(actor.rows.every(row=>/^[0-3]{16}$/.test(row)));
    palettes.push(...actor.palette.map(rgb=>rgb555(...rgb)));
    for(let ty=0;ty<3;ty++)for(let tx=0;tx<2;tx++)for(let y=0;y<8;y++){
      let lo=0,hi=0;
      for(let x=0;x<8;x++){
        const c=Number(actor.rows[ty*8+y][tx*8+x]);
        lo|=(c&1)<<(7-x);hi|=((c>>>1)&1)<<(7-x);
      }tiles.push(lo,hi);
    }
  }
  assert.equal(tiles.length,192);assert.equal(palettes.length,8);
  return {tiles,palettes};
}

async function main(){
  fs.mkdirSync(outputDir,{recursive:true});fs.mkdirSync(previewDir,{recursive:true});
  const reports=[],previews=[];
  for(let index=0;index<NAMES.length;index++){
    const data=await framePixels(index),tiles=makeTiles(data);
    const points=histogram(tiles.flatMap(tile=>tile.words));
    const candidates=[null,171,164].map(seed=>choosePalettes(tiles,points,seed));
    const result=candidates.reduce((best,item)=>item.error<best.error?item:best);
    const scene=packScene(tiles,result),bank=7+Math.floor(index/3);
    assert.equal(scene.tileBytes.length,3840);assert.equal(scene.attrs.length,240);assert.equal(scene.palettes.length,28);
    assert(scene.attrs.every(p=>p>=0&&p<7));assert(scene.palettes.every(c=>c>=0&&c<32768));
    const c=`/* Generated by tools/make-cinema.cjs: ${NAMES[index]}. */\n#pragma bank ${bank}\n#include <stdint.h>\n\n`+
      arrayDefinition('uint8_t',`cinema${index}_tiles`,scene.tileBytes,2,16)+'\n'+
      arrayDefinition('uint8_t',`cinema${index}_attrs`,scene.attrs,2,20)+'\n'+
      arrayDefinition('uint16_t',`cinema${index}_palettes`,scene.palettes,4,4);
    const file=path.join(outputDir,`cinema${index}.c`);fs.writeFileSync(file,c);
    const written=fs.readFileSync(file,'utf8');
    assert.deepEqual(readEmittedArray(written,`cinema${index}_tiles`),scene.tileBytes);
    assert.deepEqual(readEmittedArray(written,`cinema${index}_attrs`),scene.attrs);
    assert.deepEqual(readEmittedArray(written,`cinema${index}_palettes`),scene.palettes);
    const preview=reconstruct(scene);
    await sharp(preview,{raw:rawOptions}).png().toFile(path.join(previewDir,`cinema${index}-${NAMES[index].toLowerCase()}.png`));
    const large=await sharp(preview,{raw:rawOptions}).resize(WIDTH*2,HEIGHT*2,{kernel:'nearest'}).png().toBuffer();
    previews.push({input:large,left:(index%4)*WIDTH*2,top:Math.floor(index/4)*HEIGHT*2});
    reports.push({index,name:NAMES[index],bank,source:SOURCES[index],tileCount:240,tileBytes:3840,attributeBytes:240,paletteCount:7,colorsPerPalette:4,totalBytes:4136,weightedError:Number(result.error.toFixed(8)),sha256:crypto.createHash('sha256').update(c).digest('hex')});
    console.log(`CIN_${NAMES[index]}=${index}: bank${bank},240tiles,7x4palettes,4136bytes`);
  }
  const actors=actorData();
  fs.writeFileSync(path.join(outputDir,'cinema_actors.c'),'/* Generated original native 16x24 character sprites. */\n#pragma bank 15\n#include <stdint.h>\n\n'+
    arrayDefinition('uint8_t','cinema_actor_tiles',actors.tiles,2,16)+'\n'+arrayDefinition('uint16_t','cinema_actor_palettes',actors.palettes,4,4));
  const declarations=NAMES.map((name,index)=>`extern const uint8_t cinema${index}_tiles[3840];\nextern const uint8_t cinema${index}_attrs[240];\nextern const uint16_t cinema${index}_palettes[28];`).join('\n');
  fs.writeFileSync(path.join(outputDir,'cinema.h'),'/* Generated by tools/make-cinema.cjs. */\n#ifndef CIPHERSPACE_CINEMA_H\n#define CIPHERSPACE_CINEMA_H\n#include <stdint.h>\n\nenum {\n'+
    NAMES.map((name,index)=>`    CIN_${name} = ${index},`).join('\n')+'\n    CINEMA_FRAME_COUNT = 24\n};\n\n'+declarations+
    '\n\nextern const uint8_t cinema_actor_tiles[192];\nextern const uint16_t cinema_actor_palettes[8];\n\n#endif\n');
  fs.writeFileSync(path.join(artDir,'actors.json'),JSON.stringify({frameWidth:16,frameHeight:24,tileOrder:'row-major,2columnsx3rows',transparentIndex:0,actors:ACTORS},null,2)+'\n');
  fs.writeFileSync(path.join(outputDir,'cinema.json'),JSON.stringify({width:WIDTH,height:HEIGHT,reservedUIPalette:7,frames:reports,actorBytes:208,actorBank:15,sourceHashes:[...atlasCache].map(([file,bytes])=>({file:path.relative(gameDir,file),sha256:crypto.createHash('sha256').update(bytes).digest('hex')})),overlays:{pilotName:{x:48,y:72,w:64,h:8},doorNote:{x:32,y:32,w:96,h:32},moonNote:{x:32,y:32,w:96,h:32},sharedCode:{x:40,y:72,w:80,h:16},crystalStart:{x:68,y:38},crystalSocket:{x:103,y:38},southPoleTarget:{x:118,y:75},surfaceBeacon:{x:132,y:65}}},null,2)+'\n');
  await sharp({create:{width:WIDTH*8,height:HEIGHT*12,channels:3,background:'#09162a'}}).composite(previews).png().toFile(path.join(previewDir,'cinema-contact-sheet.png'));
  const actorPixels=Buffer.alloc(32*24*4);
  ACTORS.forEach((actor,n)=>actor.rows.forEach((row,y)=>[...row].forEach((v,x)=>{
    const i=Number(v),offset=(y*32+n*16+x)*4;actorPixels.set([...actor.palette[i],i?255:0],offset);
  })));
  await sharp(actorPixels,{raw:{width:32,height:24,channels:4}}).resize(256,192,{kernel:'nearest'}).png().toFile(path.join(previewDir,'cinema-actors.png'));
  console.log('PASS:24frames,99264backgroundbytes,12408bytesperbank7..14;208actorbytesbank15;allCarraysroundtripped.');
}
main().catch(error=>{console.error(error);process.exitCode=1;});
