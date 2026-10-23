# The .VFA archive format, version 1

A `.VFA` archive is a header, a table of every file by path, then the files. Each file starts on a
2,048-byte boundary, the sector size of a CD and a DVD, and is either stored as it is or deflated.
Every value is little-endian. The header and table come first, so one read at start-up takes both.

## Header, 32 bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic, `VFSA` |
| 4 | 2 | version, 1 |
| 6 | 2 | flags, 0 |
| 8 | 4 | entry count |
| 12 | 4 | table size in bytes, from offset 32 |
| 16 | 4 | alignment of each file's start, 2048 |
| 20 | 12 | reserved, 0 |

## Table

The table starts at offset 32 and holds one entry per file, sorted by the bytes of its path. Each
entry is padded with zeroes to a multiple of 4 bytes.

| Size | Field |
|---|---|
| 4 | offset of the file's first byte, from the start of the archive |
| 4 | stored size, in the archive |
| 4 | size once read |
| 4 | CRC-32 of the file once read, as zlib's `crc32` computes it |
| 1 | method: 0 stored, 1 deflate (raw, with no zlib or gzip header) |
| 1 | reserved, 0 |
| 2 | path length in bytes |
| *n* | the path, UTF-8, without a terminator |

A stored file's stored size and size are the same. A reader refuses an archive with another magic or
version, a table shorter than its entry count says, or a method it does not know, so a method can be
added in a later version without readers misreading it.

## Paths

Paths are stored relative to the archive, normalised: `/` between names, no empty names, `.` or `..`,
and A to Z in lower case (no other character is changed). Every lookup is normalised the same way, so
lookups ignore case on every platform, and `Textures\Wall.TEX` finds `textures/wall.tex`. Two files
whose paths are the same once normalised cannot both be packed.

## Files

The files follow the table, each starting at an offset that is a multiple of the alignment; the
padding before each is zeroes. Files are written in whatever order the packer is given, which need
not be the table's: files read together can sit together, so reading them is one run along a disc.
An empty file takes no space, so its offset can equal the next file's.

## What the packer chooses

`tools/pack.js` deflates each file at level 9 and keeps the result only when it saves at least a
tenth; otherwise the file is stored. A reader takes either method for any file. The same files in the
same order give the same archive, byte for byte: there are no timestamps.

## Reading

`vfs::ArchiveFileSystem` opens the archive, reads the header and table into a map from path to entry,
and keeps the file open. A read seeks to the entry's offset, reads its stored size, inflates it when
it is deflated, and checks its CRC-32; a file that fails is logged and reads empty. A stored file
can also be opened as a stream, read from the archive as it is used, without its CRC-32 being checked.
