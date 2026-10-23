// Checks the .VFA packer and its reader: node tools/test/pack.test.js
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const Pack = require('../pack.js');

const FIXTURES = path.resolve(__dirname, '../../tests/fixtures');

let failures = 0;
function check(name, condition) {
  console.log('  ' + name.padEnd(76) + (condition ? 'ok' : 'FAILED'));
  if (!condition) failures++;
}
function throws(f) {
  try { f(); return false; } catch (e) { return true; }
}

function makeFolder() {
  const folder = fs.mkdtempSync(path.join(os.tmpdir(), 'vfs-pack-'));
  const files = {
    'textures/shared/wall.tex': 'The quick brown fox jumps over the lazy dog. '.repeat(400),
    'textures/zeta/Tile.TEX': Buffer.alloc(4096, 5),
    'textures/zeta/source.png': Buffer.alloc(100, 9),
    'levels/b.json': '{"sectors": []}',
    'levels/a.json': '{"sectors": [], "texture_span": 2.0}',
    'sounds/music.ogg': crypto.randomBytes(5000),
    'empty.txt': Buffer.alloc(0),
  };
  for (const [p, bytes] of Object.entries(files)) {
    fs.mkdirSync(path.dirname(path.join(folder, p)), { recursive: true });
    fs.writeFileSync(path.join(folder, p), bytes);
  }
  return folder;
}

console.log('paths');
{
  check('paths are lower case with / between names', Pack.normalise('Textures\\Shared/Floor.TEX') === 'textures/shared/floor.tex');
  check('empty names, . and .. are taken out', Pack.normalise('./levels//x/../a.json') === 'levels/a.json');
  check('only A-Z are lowered, as the C++ reader lowers them', Pack.normalise('É.json') === 'É.json');
}

console.log('packing a folder');
const folder = makeFolder();
const packed = Pack.pack(folder, ['.png']);
const archive = Pack.read(packed.archive);
{
  const paths = archive.entries.map((e) => e.path);
  check('files with a left out extension are left out', !paths.includes('textures/zeta/source.png'));
  check('every other file is packed', paths.length === 6);
  check('paths are stored within the folder, in lower case', paths.includes('textures/zeta/tile.tex'));
  check('the table is sorted by path', paths.join('\n') === paths.slice().sort().join('\n'));
  check('the files are written in path order', archive.entries.every((e, i) => i === 0 || e.offset >= archive.entries[i - 1].offset));
  check('the header names version 1 and 2,048-byte alignment', archive.version === 1 && archive.alignment === 2048);
  check('every file starts on a 2,048-byte boundary', archive.entries.every((e) => e.offset % 2048 === 0));

  let same = true;
  for (const f of Pack.collect(folder, ['.png'])) {
    if (!archive.read(f.path).equals(fs.readFileSync(f.file))) same = false;
  }
  check('every file reads back byte for byte', same);
  check('a lookup is normalised', archive.read('Levels\\A.json') !== null);
  check('a file not in the archive reads as null', archive.read('levels/c.json') === null);

  const byPath = new Map(archive.entries.map((e) => [e.path, e]));
  check('a repetitive file is deflated', byPath.get('textures/shared/wall.tex').method === Pack.METHOD_DEFLATE);
  check('random bytes, which deflate cannot shrink by a tenth, are stored',
    byPath.get('sounds/music.ogg').method === Pack.METHOD_STORED && byPath.get('sounds/music.ogg').storedSize === 5000);
  check('an empty file is stored, and reads back empty', byPath.get('empty.txt').method === Pack.METHOD_STORED && archive.read('empty.txt').length === 0);
}

console.log('order');
{
  const written = Pack.write([
    { path: 'z.txt', bytes: Buffer.from('z') }, { path: 'a.txt', bytes: Buffer.from('a') }, { path: 'm.txt', bytes: Buffer.from('m') },
  ]);
  const entries = Pack.read(written.archive).entries;
  check('the table is sorted by path whatever order the files are given in', entries.map((e) => e.path).join() === 'a.txt,m.txt,z.txt');
  check('the files are written in the order given',
    entries.slice().sort((a, b) => a.offset - b.offset).map((e) => e.path).join() === 'z.txt,a.txt,m.txt');
  check('an archive of no files has a header and nothing else', Pack.write([]).archive.length === 32 && Pack.read(Pack.write([]).archive).entries.length === 0);
}

console.log('integrity');
{
  check('the same folder packs to the same archive, byte for byte', Pack.pack(folder, ['.png']).archive.equals(packed.archive));
  check('tests/fixtures/archive.vfa is tests/fixtures/folder packed',
    Pack.pack(path.join(FIXTURES, 'folder')).archive.equals(fs.readFileSync(path.join(FIXTURES, 'archive.vfa'))));
  const wall = archive.entries.find((e) => e.path === 'textures/shared/wall.tex');
  const damaged = Buffer.from(packed.archive);
  damaged[wall.offset + Math.floor(wall.storedSize / 2)] ^= 0x01;
  check('a changed byte in a deflated file fails', throws(() => Pack.read(damaged).read('textures/shared/wall.tex')));
  const music = archive.entries.find((e) => e.path === 'sounds/music.ogg');
  const damagedStored = Buffer.from(packed.archive);
  damagedStored[music.offset + 100] ^= 0x80;
  check('a changed byte in a stored file fails its CRC-32', throws(() => Pack.read(damagedStored).read('sounds/music.ogg')));
  check('two paths the same once in lower case are refused',
    throws(() => Pack.write([{ path: 'a/B.json', bytes: Buffer.alloc(1) }, { path: 'a/b.json', bytes: Buffer.alloc(1) }])));
  check('an archive with another magic is refused', throws(() => Pack.read(Buffer.alloc(64))));
}

fs.rmSync(folder, { recursive: true, force: true });
console.log(failures === 0 ? 'all passed' : failures + ' FAILED');
process.exitCode = failures === 0 ? 0 : 1;
