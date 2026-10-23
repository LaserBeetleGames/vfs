# vfs

A small, read-only virtual file system for C++20 games, written with sixth-generation consoles in
mind: Dreamcast, PS2, Xbox; as well as Windows, Linux, and macOS. Folders and archives are mounted by name, and a
game reads every file by a path whose first name picks the mount. A development build mounts a
folder of loose files; a release build mounts an archive in its place, with no change to the code
that reads them.

The archive format, `.VFA`, is our own: a header, a table of paths, then the files, each starting on
a 2,048-byte sector boundary and each stored or deflated with [zlib](https://zlib.net). Its reader
needs nothing from the platform but opening, seeking and reading one file. The format is in
[docs/FORMAT.md](docs/FORMAT.md).

- **Layers, for development.** With layering turned on, several file systems can be mounted at one
  name, and a file is read from the last mounted that has it: a folder holding a few changed files, mounted over an archive, replaces the
  archive's copies of just those files, so a game can be run without repacking. A mount with one
  file system reads it directly, with no lookups in other places, and a path never reaches a
  different mount's files.
- **Archives are case-blind on every platform.** Paths are normalised (`/` between names, no `.` or
  `..`, A to Z in lower case) before they are looked up in an archive. A folder matches names as its
  platform does: in any case on Windows, in their own case on Linux. A path that reads from a folder
  on Linux therefore reads from the archive everywhere.
- **Disc-friendly.** Files are sector aligned and written in the order you give the packer, so files
  read together can sit together on a disc. The header and table are one read.
- **Checked.** Every file read whole is checked against its CRC-32, and a damaged file is logged and
  reads empty rather than handing a loader bad bytes.
- **Streams.** A stored file can be opened as a stream with its own file handle, such as music that
  an audio thread reads as it plays.

## Building

zlib 1.3.2 and [doctest](https://github.com/doctest/doctest) are submodules:

```bash
git clone --recursive https://github.com/LaserBeetleGames/vfs.git
```

In your project's CMake, add the folder and link the `vfs` static library:

```cmake
add_subdirectory(vfs)
target_link_libraries(my_game PRIVATE vfs)
```

It builds zlib from `src/zlib` as the `zlib` target, unless a `zlib` target exists already (a
platform's own port, say), in which case it uses that. Built on its own, it also builds the tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release
```

### Platforms

vfs is standard C++20 with no platform calls beyond the C runtime's `stat`, and needs a compiler
with `std::format`. GitHub Actions builds it and runs the C++ and Node tests on every push
(`.github/workflows/ci.yml`):

| Platform | Compiler |
|---|---|
| Linux (`ubuntu-latest`) | Ubuntu 24.04 with GCC 13 |
| macOS (`macos-latest`) | macOS 15 (Apple Silicon) with Apple Clang 17 |
| Windows (`windows-latest`) | Microsoft Windows Server 2025 with MSVC 19 |
| Windows 98 and later, 32-bit | GCC 16.2.0 via [MinGW Lite](https://github.com/redpanda-cpp/mingw-lite/releases/tag/16.2.0%2Bemutls-r1)'s 16.2.0+emutls-r1 with `32_686-msvcrt_win98` profile |

Folders on macOS, like Windows, match names in any case by default. The format is little-endian and
read a byte at a time, so the reader does not depend on the machine's byte order.

## Usage

```cpp
#include "vfs/ArchiveFileSystem.h"
#include "vfs/FolderFileSystem.h"
#include "vfs/VirtualFileSystem.h"

vfs::VirtualFileSystem files;

// development: the loose files in assets/
files.Mount("assets", std::make_shared<vfs::FolderFileSystem>("assets"));

// or release: the same paths from ASSETS.VFA
files.Mount("assets", std::make_shared<vfs::ArchiveFileSystem>("ASSETS.VFA"));

// or both, with layering on: the archive, with the files in patch/ read in place of its copies
vfs::VirtualFileSystem layered({ .layering = true });
layered.Mount("assets", std::make_shared<vfs::ArchiveFileSystem>("ASSETS.VFA"));
layered.Mount("assets", std::make_shared<vfs::FolderFileSystem>("patch"));

std::string level = files.Read("assets/levels/underground.json");    // empty when it is not there
std::unique_ptr<vfs::IStream> music = files.Open("assets/sounds/theme.ogg");    // null when it is not there
```

- `VirtualFileSystem(features)` takes a `FeatureConfig`, whose features are all off by default.
- `Mount(name, fileSystem)` mounts a file system at a name, which matches in any case. Without
  layering, a name already mounted throws `vfs::Exception`; with it, the file system is mounted over
  those already there. One file system can be mounted at several names. The empty name mounts it
  for every path whose first name is not a mount, which then reads the whole path.
- With layering, a file is read from the last file system mounted at its name that has it (`Exists`), whether or
  not it can be read: a damaged file reads empty rather than falling back to an earlier copy. A
  name with one file system mounted reads it without asking.
- `Unmount(name)` removes every file system at the name; `Unmount(name, fileSystem)` removes one.
- `Read(path)` gives the file's bytes in a `std::string`, empty when there is no such file or it
  cannot be read.
- `Open(path)` gives an `IStream` with `Read`, `Seek`, `GetPosition` and `GetSize`. A stored file
  in an archive streams from the archive; a deflated one is read whole and streamed from memory.
- `ArchiveFileSystem` throws `vfs::Exception` when the archive cannot be opened or is not a version
  1 `.VFA` archive. Pass it an `ILogger` to be told of files that fail their CRC-32 check. Its
  `Read` shares one file handle, so call it from one thread at a time; streams have their own.

### Backends

A backend is an `IFileSystem`: `Exists(path)`, `Read(path)` and `Open(path)`, with the path inside
the file system.
`FolderFileSystem` (a folder on the platform's own file system) and `ArchiveFileSystem` (a `.VFA`
archive) are the two here. A platform with its own disc API can add one alongside them and mount it
the same way.

## Packing

`tools/pack.js` writes archives. It is Node (20.15, or 22.2 or later, for `zlib.crc32`), with no packages:

```bash
node tools/pack.js assets ASSETS.VFA
node tools/pack.js --leave-out .png,.blend assets ASSETS.VFA
```

From the command line it packs every file under the folder in path order. As a module, `write(files)`
takes `[{ path, bytes }]` in the order they should sit in the archive, so a build script can put the
files a level reads next to each other; `read(archive)` reads one back. `tools/test/pack.test.js`
tests it, and checks that `tests/fixtures/archive.vfa`, which the C++ tests read, is
`tests/fixtures/folder` packed.

## License

MIT; see [LICENSE](LICENSE). zlib and doctest have their own licenses, in their submodules.
