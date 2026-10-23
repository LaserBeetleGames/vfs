// Writes and reads .VFA archives: a 32-byte header, a table of every file by path, then the files, each
// starting on a 2,048-byte boundary and each stored as it is or deflated. The format is in docs/FORMAT.md.
// Node only, with no packages: Node's own zlib deflates.
//
//   node tools/pack.js [--leave-out .png,.glb] <folder> <archive>
//
// From the command line it packs every file under the folder, in path order. As a module, write() takes
// the files in any order, so a game can put the files it reads together next to each other.
'use strict';

const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const MAGIC = 'VFSA';
const VERSION = 1;
const HEADER_SIZE = 32;
const ENTRY_SIZE = 20;            // a table entry's fields before its path
const ALIGNMENT = 2048;           // the sector size of a CD and a DVD
const METHOD_STORED = 0;
const METHOD_DEFLATE = 1;         // raw deflate, with no zlib or gzip header
const DEFLATE_LEVEL = 9;
const MIN_SAVING = 0.1;           // deflated is kept only when it saves at least a tenth

// ---- paths ------------------------------------------------------------------------------------------

// the path as an archive stores it and as it is looked up: / between names, no empty names, . or ..,
// and A-Z in lower case
function normalise(p) {
  const names = [];
  for (const name of p.split(/[\\/]/)) {
    if (name === '..') names.pop();
    else if (name !== '' && name !== '.') names.push(name);
  }
  return names.join('/').replace(/[A-Z]/g, (c) => c.toLowerCase());
}

function compareBytes(a, b) {
  return Buffer.compare(Buffer.from(a, 'utf8'), Buffer.from(b, 'utf8'));
}

// every file under the folder, as { path, file }, sorted by path, less those whose extension is in
// leaveOut (such as ['.png']); throws when two files are the same path once in lower case
function collect(folder, leaveOut = []) {
  const left = leaveOut.map((extension) => extension.toLowerCase());
  const files = [];
  const seen = new Map();
  (function walk(dir) {
    for (const item of fs.readdirSync(dir, { withFileTypes: true })) {
      const file = path.join(dir, item.name);
      if (item.isDirectory()) { walk(file); continue; }
      if (left.includes(path.extname(item.name).toLowerCase())) continue;
      const p = normalise(path.relative(folder, file));
      if (seen.has(p)) throw new Error(`${seen.get(p)} and ${file} are the same path once in lower case`);
      seen.set(p, file);
      files.push({ path: p, file: file });
    }
  })(folder);
  return files.sort((a, b) => compareBytes(a.path, b.path));
}

// ---- writing ------------------------------------------------------------------------------------------

function alignUp(value, alignment) {
  return Math.ceil(value / alignment) * alignment;
}

// each file as it is stored: deflated when that saves at least a tenth, otherwise as it is
function store(bytes) {
  const deflated = zlib.deflateRawSync(bytes, { level: DEFLATE_LEVEL });
  if (bytes.length > 0 && deflated.length <= bytes.length * (1 - MIN_SAVING))
    return { method: METHOD_DEFLATE, data: deflated };
  return { method: METHOD_STORED, data: bytes };
}

// the archive holding the files, [{ path, bytes }], written in the order given; the table is sorted by path.
// The same files in the same order give the same archive, byte for byte
function write(files) {
  const entries = files.map((f) => {
    const stored = store(f.bytes);
    return {
      path: normalise(f.path), pathBytes: Buffer.from(normalise(f.path), 'utf8'),
      size: f.bytes.length, crc: zlib.crc32(f.bytes), method: stored.method, data: stored.data,
    };
  });
  const paths = new Set();
  for (const entry of entries) {
    if (paths.has(entry.path)) throw new Error(`${entry.path} is in the archive twice`);
    paths.add(entry.path);
  }

  const tableSize = entries.reduce((size, e) => size + alignUp(ENTRY_SIZE + e.pathBytes.length, 4), 0);
  let offset = alignUp(HEADER_SIZE + tableSize, ALIGNMENT);
  for (const entry of entries) {
    entry.offset = offset;
    offset = alignUp(offset + entry.data.length, ALIGNMENT);
  }
  const last = entries[entries.length - 1];
  const archive = Buffer.alloc(last ? last.offset + last.data.length : HEADER_SIZE + tableSize);

  archive.write(MAGIC, 0, 'ascii');
  archive.writeUInt16LE(VERSION, 4);
  archive.writeUInt16LE(0, 6);
  archive.writeUInt32LE(entries.length, 8);
  archive.writeUInt32LE(tableSize, 12);
  archive.writeUInt32LE(ALIGNMENT, 16);

  let position = HEADER_SIZE;
  for (const entry of entries.slice().sort((a, b) => Buffer.compare(a.pathBytes, b.pathBytes))) {
    archive.writeUInt32LE(entry.offset, position);
    archive.writeUInt32LE(entry.data.length, position + 4);
    archive.writeUInt32LE(entry.size, position + 8);
    archive.writeUInt32LE(entry.crc, position + 12);
    archive.writeUInt8(entry.method, position + 16);
    archive.writeUInt16LE(entry.pathBytes.length, position + 18);
    entry.pathBytes.copy(archive, position + ENTRY_SIZE);
    position += alignUp(ENTRY_SIZE + entry.pathBytes.length, 4);
  }
  for (const entry of entries) entry.data.copy(archive, entry.offset);

  return {
    archive: archive,
    entries: entries.map((e) => ({ path: e.path, size: e.size, storedSize: e.data.length, method: e.method })),
  };
}

// packs every file under the folder, less those whose extension is in leaveOut, in path order:
// { archive, entries }
function pack(folder, leaveOut = []) {
  return write(collect(folder, leaveOut).map((f) => ({ path: f.path, bytes: fs.readFileSync(f.file) })));
}

// ---- reading --------------------------------------------------------------------------------------------

// the archive's header and table, and read(path), which gives a file's bytes, null when it is not in the
// archive, or throws when it fails its CRC-32
function read(archive) {
  if (archive.length < HEADER_SIZE || archive.toString('ascii', 0, 4) !== MAGIC) throw new Error('not a .VFA archive');
  const version = archive.readUInt16LE(4);
  if (version !== VERSION) throw new Error(`version ${version}, only version ${VERSION} can be read`);
  const count = archive.readUInt32LE(8);
  const tableSize = archive.readUInt32LE(12);
  const alignment = archive.readUInt32LE(16);

  const entries = [];
  let position = HEADER_SIZE;
  for (let i = 0; i < count; i++) {
    const pathLength = archive.readUInt16LE(position + 18);
    entries.push({
      offset: archive.readUInt32LE(position),
      storedSize: archive.readUInt32LE(position + 4),
      size: archive.readUInt32LE(position + 8),
      crc: archive.readUInt32LE(position + 12),
      method: archive.readUInt8(position + 16),
      path: archive.toString('utf8', position + ENTRY_SIZE, position + ENTRY_SIZE + pathLength),
    });
    position += alignUp(ENTRY_SIZE + pathLength, 4);
  }
  if (position !== HEADER_SIZE + tableSize) throw new Error('the table size does not match its entries');

  const byPath = new Map(entries.map((e) => [e.path, e]));
  function readFile(p) {
    const entry = byPath.get(normalise(p));
    if (!entry) return null;
    const data = archive.subarray(entry.offset, entry.offset + entry.storedSize);
    const bytes = entry.method === METHOD_DEFLATE ? zlib.inflateRawSync(data) : Buffer.from(data);
    if (bytes.length !== entry.size || zlib.crc32(bytes) !== entry.crc) throw new Error(`${entry.path} fails its CRC-32 check`);
    return bytes;
  }
  return { version: version, alignment: alignment, tableSize: tableSize, entries: entries, read: readFile };
}

// ---- the command line -------------------------------------------------------------------------------------

function kilobytes(bytes) {
  return Math.ceil(bytes / 1024).toLocaleString('en-GB') + ' K';
}

// prints each file's size before and after, and the totals
function report(written, archiveFile) {
  let size = 0;
  let storedSize = 0;
  for (const entry of written.entries) {
    const how = entry.method === METHOD_DEFLATE ? 'deflated' : 'stored';
    console.log(`  ${entry.path.padEnd(56)} ${kilobytes(entry.size).padStart(9)} ${kilobytes(entry.storedSize).padStart(9)}  ${how}`);
    size += entry.size;
    storedSize += entry.storedSize;
  }
  console.log(`${written.entries.length} files, ${kilobytes(size)} packed into ${kilobytes(storedSize)}; ` +
    `${archiveFile} is ${kilobytes(written.archive.length)}`);
}

function main(args) {
  let leaveOut = [];
  if (args[0] === '--leave-out') {
    leaveOut = (args[1] || '').split(',').filter((extension) => extension !== '');
    args = args.slice(2);
  }
  if (args.length !== 2) {
    console.log('usage: node tools/pack.js [--leave-out .png,.glb] <folder> <archive>');
    return 1;
  }
  const [folder, archiveFile] = args;
  const written = pack(folder, leaveOut);
  fs.writeFileSync(archiveFile, written.archive);
  report(written, archiveFile);
  return 0;
}

module.exports = { normalise, collect, write, pack, read, report, ALIGNMENT, METHOD_STORED, METHOD_DEFLATE };

if (require.main === module) process.exitCode = main(process.argv.slice(2));
