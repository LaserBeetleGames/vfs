#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "vfs/ArchiveFileSystem.h"
#include "vfs/Exception.h"
#include "vfs/FileStream.h"
#include "vfs/FolderFileSystem.h"
#include "vfs/MemoryStream.h"
#include "vfs/Paths.h"
#include "vfs/VirtualFileSystem.h"

// tests/fixtures/archive.vfa is tests/fixtures/folder packed by tools/pack.js, which tools/test/pack.test.js checks
const std::string FIXTURES = VFS_FIXTURES;
const std::string FOLDER = FIXTURES + "/folder";
const std::string ARCHIVE = FIXTURES + "/archive.vfa";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

struct CountingLogger : vfs::ILogger {
    int errors = 0;
    std::string last;

    void Error(const std::string& message) override
    {
        errors++;
        last = message;
    }
};

static std::string ReadFile(const std::string& fileName)
{
    std::ifstream file(fileName.c_str(), std::ios::in | std::ios::binary);
    std::ostringstream bytes;
    bytes << file.rdbuf();
    return bytes.str();
}

static std::string ReadAll(vfs::IStream& stream, size_t pieceSize)
{
    std::string bytes;
    std::vector<char> piece(pieceSize);
    size_t read;
    while ((read = stream.Read(piece.data(), piece.size())) > 0)
        bytes.append(piece.data(), read);
    return bytes;
}

// a copy of the fixture archive, changed, in a file of its own that is deleted with it
struct ChangedArchive {
    std::string fileName;
    std::string bytes;

    explicit ChangedArchive(const std::string& name)
        : fileName((std::filesystem::temp_directory_path() / ("vfs-test-" + name + ".vfa")).string()), bytes(ReadFile(ARCHIVE)) {}

    ~ChangedArchive() { std::filesystem::remove(fileName); }

    // the offset of an entry's first field in the table
    size_t FindEntry(const std::string& path)
    {
        return bytes.find(path) - vfs::ARCHIVE_ENTRY_SIZE;
    }

    uint32_t ReadUint32(size_t at)
    {
        uint32_t value = 0;
        for (int i = 3; i >= 0; i--)
            value = value << 8 | static_cast<uint8_t>(bytes[at + static_cast<size_t>(i)]);
        return value;
    }

    const std::string& Write()
    {
        std::ofstream file(fileName.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return fileName;
    }
};

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

TEST_CASE("paths are normalised") {
    CHECK(vfs::NormalisePath("textures\\shared/floor.tex") == "textures/shared/floor.tex");
    CHECK(vfs::NormalisePath("./levels//x/../a.json") == "levels/a.json");
    CHECK(vfs::NormalisePath("/a/") == "a");
    CHECK(vfs::NormalisePath("../../a") == "a");
    CHECK(vfs::NormalisePath("") == "");
}

TEST_CASE("only A to Z are lowered") {
    CHECK(vfs::LowerCase("Textures/FLOOR.Tex") == "textures/floor.tex");
    CHECK(vfs::LowerCase("\xC3\x89.json") == "\xC3\x89.json");
}

// ---------------------------------------------------------------------------
// Streams
// ---------------------------------------------------------------------------

TEST_CASE("a memory stream reads, seeks and stops at its end") {
    vfs::MemoryStream stream("abcdef");
    char buffer[4];
    CHECK(stream.GetSize() == 6);
    CHECK(stream.Read(buffer, 4) == 4);
    CHECK(std::string(buffer, 4) == "abcd");
    CHECK(stream.Read(buffer, 4) == 2);
    CHECK(std::string(buffer, 2) == "ef");
    CHECK(stream.Read(buffer, 4) == 0);
    CHECK(stream.Seek(1));
    CHECK(stream.GetPosition() == 1);
    CHECK_FALSE(stream.Seek(7));
    CHECK(stream.GetPosition() == 1);
}

TEST_CASE("a file stream reads its stretch of a file") {
    std::string whole = ReadFile(FOLDER + "/Readme.TXT");

    vfs::FileStream stream(FOLDER + "/Readme.TXT", 4, 11);
    REQUIRE(stream.IsOpen());
    CHECK(stream.GetSize() == 11);
    CHECK(ReadAll(stream, 3) == whole.substr(4, 11));
    CHECK(stream.Seek(6));
    char buffer[16];
    CHECK(stream.Read(buffer, sizeof(buffer)) == 5);
    CHECK(std::string(buffer, 5) == whole.substr(10, 5));
    CHECK_FALSE(stream.Seek(12));

    vfs::FileStream all(FOLDER + "/Readme.TXT");
    CHECK(all.GetSize() == whole.size());
    CHECK(ReadAll(all, 100) == whole);

    CHECK_FALSE(vfs::FileStream(FOLDER + "/missing.txt").IsOpen());
}

// ---------------------------------------------------------------------------
// FolderFileSystem
// ---------------------------------------------------------------------------

TEST_CASE("a folder reads its files") {
    vfs::FolderFileSystem folder(FOLDER);
    CHECK(folder.Read("levels/Level.json") == ReadFile(FOLDER + "/levels/Level.json"));
    CHECK(folder.Read("empty.txt").empty());
    CHECK(folder.Read("missing.txt").empty());

    auto stream = folder.Open("sounds/noise.bin");
    REQUIRE(stream != nullptr);
    CHECK(stream->GetSize() == 5000);
    CHECK(ReadAll(*stream, 1024) == ReadFile(FOLDER + "/sounds/noise.bin"));
    CHECK(folder.Open("missing.txt") == nullptr);

    CHECK(vfs::FolderFileSystem(FIXTURES + "/no such folder").Read("readme.txt").empty());
}

// ---------------------------------------------------------------------------
// ArchiveFileSystem
// ---------------------------------------------------------------------------

TEST_CASE("an archive reads every file byte for byte") {
    CountingLogger logger;
    vfs::ArchiveFileSystem archive(ARCHIVE, &logger);
    CHECK(archive.GetFileCount() == 6);

    int files = 0;
    for (const auto& item : std::filesystem::recursive_directory_iterator(FOLDER)) {
        if (!item.is_regular_file())
            continue;
        std::string path = std::filesystem::relative(item.path(), FOLDER).generic_string();
        CHECK_MESSAGE(archive.Read(path) == ReadFile(item.path().string()), path);
        files++;
    }
    CHECK(files == 6);
    CHECK(logger.errors == 0);
}

TEST_CASE("an archive looks paths up normalised and in any case") {
    vfs::ArchiveFileSystem archive(ARCHIVE);
    std::string readme = ReadFile(FOLDER + "/Readme.TXT");
    CHECK(archive.Read("readme.txt") == readme);
    CHECK(archive.Read("README.TXT") == readme);
    CHECK(archive.Read("levels\\..\\Readme.txt") == readme);
    CHECK(archive.Read("missing.txt").empty());
    CHECK(archive.Open("missing.txt") == nullptr);
}

TEST_CASE("an archive streams stored and deflated files") {
    vfs::ArchiveFileSystem archive(ARCHIVE);

    auto stored = archive.Open("sounds/noise.bin");
    REQUIRE(stored != nullptr);
    CHECK(dynamic_cast<vfs::FileStream*>(stored.get()) != nullptr);
    CHECK(stored->GetSize() == 5000);
    CHECK(ReadAll(*stored, 333) == ReadFile(FOLDER + "/sounds/noise.bin"));

    auto deflated = archive.Open("textures/wall.tex");
    REQUIRE(deflated != nullptr);
    CHECK(dynamic_cast<vfs::MemoryStream*>(deflated.get()) != nullptr);
    CHECK(ReadAll(*deflated, 4096) == ReadFile(FOLDER + "/textures/wall.tex"));

    auto empty = archive.Open("empty.txt");
    REQUIRE(empty != nullptr);
    CHECK(empty->GetSize() == 0);
}

TEST_CASE("a damaged file reads empty and is logged") {
    SUBCASE("deflated") {
        ChangedArchive changed("deflated");
        size_t entry = changed.FindEntry("readme.txt");
        size_t offset = changed.ReadUint32(entry);
        size_t storedSize = changed.ReadUint32(entry + 4);
        changed.bytes[offset + storedSize / 2] ^= 0x01;

        CountingLogger logger;
        vfs::ArchiveFileSystem archive(changed.Write(), &logger);
        CHECK(archive.Read("readme.txt").empty());
        CHECK(archive.Open("readme.txt") == nullptr);
        CHECK(logger.errors == 2);
        CHECK(archive.Read("levels/level.json") == ReadFile(FOLDER + "/levels/Level.json"));
    }
    SUBCASE("stored") {
        ChangedArchive changed("stored");
        size_t offset = changed.ReadUint32(changed.FindEntry("sounds/noise.bin"));
        changed.bytes[offset + 100] ^= 0x80;

        CountingLogger logger;
        vfs::ArchiveFileSystem archive(changed.Write(), &logger);
        CHECK(archive.Read("sounds/noise.bin").empty());
        CHECK(logger.errors == 1);
        CHECK(logger.last.find("CRC-32") != std::string::npos);
        CHECK(archive.Open("sounds/noise.bin") != nullptr);
    }
    SUBCASE("cut short") {
        ChangedArchive changed("short");
        size_t entry = changed.FindEntry("textures/wall.tex");
        changed.bytes.resize(changed.ReadUint32(entry) + 1);

        CountingLogger logger;
        vfs::ArchiveFileSystem archive(changed.Write(), &logger);
        CHECK(archive.Read("textures/wall.tex").empty());
        CHECK(logger.errors == 1);
    }
}

TEST_CASE("an archive that cannot be read is refused") {
    CHECK_THROWS_AS(vfs::ArchiveFileSystem(FIXTURES + "/missing.vfa"), vfs::Exception);
    CHECK_THROWS_AS(vfs::ArchiveFileSystem(FOLDER + "/Readme.TXT"), vfs::Exception);

    SUBCASE("another version") {
        ChangedArchive changed("version");
        changed.bytes[4] = 2;
        CHECK_THROWS_AS(vfs::ArchiveFileSystem(changed.Write()), vfs::Exception);
    }
    SUBCASE("ends inside its table") {
        ChangedArchive changed("table");
        changed.bytes.resize(40);
        CHECK_THROWS_AS(vfs::ArchiveFileSystem(changed.Write()), vfs::Exception);
    }
    SUBCASE("more entries than its table holds") {
        ChangedArchive changed("count");
        changed.bytes[8] = 7;
        CHECK_THROWS_AS(vfs::ArchiveFileSystem(changed.Write()), vfs::Exception);
    }
    SUBCASE("an unknown method") {
        ChangedArchive changed("method");
        changed.bytes[changed.FindEntry("readme.txt") + 16] = 9;
        CHECK_THROWS_AS(vfs::ArchiveFileSystem(changed.Write()), vfs::Exception);
    }
    SUBCASE("a stored file whose sizes differ") {
        ChangedArchive changed("sizes");
        changed.bytes[changed.FindEntry("sounds/noise.bin") + 8] ^= 0x01;
        CHECK_THROWS_AS(vfs::ArchiveFileSystem(changed.Write()), vfs::Exception);
    }
}

// ---------------------------------------------------------------------------
// VirtualFileSystem
// ---------------------------------------------------------------------------

TEST_CASE("a path's first name chooses its mount") {
    vfs::VirtualFileSystem files;
    files.Mount("assets", std::make_shared<vfs::ArchiveFileSystem>(ARCHIVE));
    files.Mount("loose", std::make_shared<vfs::FolderFileSystem>(FOLDER));
    std::string readme = ReadFile(FOLDER + "/Readme.TXT");

    CHECK(files.Read("assets/readme.txt") == readme);
    CHECK(files.Read("ASSETS\\Levels/../readme.txt") == readme);
    CHECK(files.Read("loose/Readme.TXT") == readme);
    CHECK(files.Read("other/readme.txt").empty());
    CHECK(files.Read("readme.txt").empty());

    auto stream = files.Open("assets/sounds/noise.bin");
    REQUIRE(stream != nullptr);
    CHECK(stream->GetSize() == 5000);
    CHECK(files.Open("other/sounds/noise.bin") == nullptr);
}

TEST_CASE("the empty name mounts every path without a mount of its own") {
    vfs::VirtualFileSystem files;
    files.Mount("", std::make_shared<vfs::FolderFileSystem>(FOLDER));
    files.Mount("assets", std::make_shared<vfs::ArchiveFileSystem>(ARCHIVE));

    CHECK(files.Read("Readme.TXT") == ReadFile(FOLDER + "/Readme.TXT"));
    CHECK(files.Read("sounds/noise.bin") == ReadFile(FOLDER + "/sounds/noise.bin"));
    CHECK(files.Open("levels/Level.json") != nullptr);
    CHECK(files.Read("assets/missing.txt").empty());
}

TEST_CASE("one file system mounts at several names, and mounts are removed") {
    vfs::VirtualFileSystem files;
    auto archive = std::make_shared<vfs::ArchiveFileSystem>(ARCHIVE);
    files.Mount("assets", archive);
    files.Mount("Patch", archive);
    CHECK(files.Read("patch/readme.txt") == files.Read("assets/readme.txt"));

    files.Unmount("ASSETS");
    CHECK(files.Read("assets/readme.txt").empty());
    CHECK_FALSE(files.Read("patch/readme.txt").empty());
}

// ---------------------------------------------------------------------------
// Layers
// ---------------------------------------------------------------------------

// a folder of its own, deleted with it, holding the files given
struct TemporaryFolder {
    std::string folder;

    TemporaryFolder(const std::string& name, const std::vector<std::pair<std::string, std::string>>& files)
        : folder((std::filesystem::temp_directory_path() / ("vfs-test-" + name)).string())
    {
        std::filesystem::remove_all(folder);
        for (const auto& [path, bytes] : files) {
            std::filesystem::path fileName = std::filesystem::path(folder) / path;
            std::filesystem::create_directories(fileName.parent_path());
            std::ofstream file(fileName.string().c_str(), std::ios::out | std::ios::binary);
            file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }
    }

    ~TemporaryFolder() { std::filesystem::remove_all(folder); }
};

// a file system that counts the questions it is asked
struct CountingFileSystem : vfs::IFileSystem {
    int exists = 0;

    bool Exists(const std::string&) override { exists++; return true; }
    std::string Read(const std::string&) override { return "counted"; }
    std::unique_ptr<vfs::IStream> Open(const std::string&) override { return nullptr; }
};

TEST_CASE("without layering, a name takes one file system") {
    vfs::VirtualFileSystem files;
    auto archive = std::make_shared<vfs::ArchiveFileSystem>(ARCHIVE);
    files.Mount("assets", archive);
    CHECK_THROWS_AS(files.Mount("Assets", std::make_shared<vfs::FolderFileSystem>(FOLDER)), vfs::Exception);
    CHECK(files.Read("assets/readme.txt") == ReadFile(FOLDER + "/Readme.TXT"));
    files.Mount("other", archive);

    files.Unmount("assets");
    files.Mount("assets", std::make_shared<vfs::FolderFileSystem>(FOLDER + "/levels"));
    CHECK(files.Read("assets/Level.json") == ReadFile(FOLDER + "/levels/Level.json"));
}

TEST_CASE("files and archives say which files they have") {
    vfs::ArchiveFileSystem archive(ARCHIVE);
    CHECK(archive.Exists("Readme.TXT"));
    CHECK(archive.Exists("empty.txt"));
    CHECK_FALSE(archive.Exists("missing.txt"));

    vfs::FolderFileSystem folder(FOLDER);
    CHECK(folder.Exists("levels/Level.json"));
    CHECK(folder.Exists("empty.txt"));
    CHECK_FALSE(folder.Exists("missing.txt"));
    CHECK_FALSE(folder.Exists("levels"));
    CHECK(folder.Read("levels").empty());
    CHECK(folder.Open("levels") == nullptr);
}

TEST_CASE("a file is read from the last file system mounted that has it") {
    TemporaryFolder patch("patch", { { "readme.txt", "patched" }, { "new.txt", "new" }, { "levels/level.json", "" } });
    vfs::VirtualFileSystem files({ .layering = true });
    files.Mount("assets", std::make_shared<vfs::ArchiveFileSystem>(ARCHIVE));
    files.Mount("assets", std::make_shared<vfs::FolderFileSystem>(patch.folder));

    CHECK(files.Read("assets/readme.txt") == "patched");
    CHECK(files.Read("assets/new.txt") == "new");
    CHECK(files.Read("assets/sounds/noise.bin") == ReadFile(FOLDER + "/sounds/noise.bin"));
    CHECK(files.Read("assets/levels/level.json").empty());
    CHECK(files.Exists("assets/levels/level.json"));

    auto stream = files.Open("assets/readme.txt");
    REQUIRE(stream != nullptr);
    CHECK(ReadAll(*stream, 64) == "patched");
    REQUIRE(files.Open("assets/sounds/noise.bin") != nullptr);
    CHECK(files.Open("assets/sounds/noise.bin")->GetSize() == 5000);

    CHECK(files.Read("assets/missing.txt").empty());
    CHECK(files.Open("assets/missing.txt") == nullptr);
    CHECK_FALSE(files.Exists("assets/missing.txt"));
}

TEST_CASE("a damaged file in a later layer does not fall through to an earlier one") {
    ChangedArchive changed("layer");
    size_t offset = changed.ReadUint32(changed.FindEntry("sounds/noise.bin"));
    changed.bytes[offset + 100] ^= 0x80;

    CountingLogger logger;
    vfs::VirtualFileSystem files({ .layering = true });
    files.Mount("assets", std::make_shared<vfs::FolderFileSystem>(FOLDER));
    files.Mount("assets", std::make_shared<vfs::ArchiveFileSystem>(changed.Write(), &logger));
    CHECK(files.Read("assets/sounds/noise.bin").empty());
    CHECK(logger.errors == 1);
    CHECK(files.Read("assets/readme.txt") == ReadFile(FOLDER + "/Readme.TXT"));
}

TEST_CASE("removing one layer leaves the others") {
    TemporaryFolder patch("unmount", { { "readme.txt", "patched" } });
    auto folder = std::make_shared<vfs::FolderFileSystem>(patch.folder);
    vfs::VirtualFileSystem files({ .layering = true });
    files.Mount("assets", std::make_shared<vfs::ArchiveFileSystem>(ARCHIVE));
    files.Mount("assets", folder);
    CHECK(files.Read("assets/readme.txt") == "patched");

    files.Unmount("assets", folder);
    CHECK(files.Read("assets/readme.txt") == ReadFile(FOLDER + "/Readme.TXT"));
    files.Unmount("assets", folder);
    CHECK_FALSE(files.Read("assets/readme.txt").empty());
}

TEST_CASE("the empty name takes layers too") {
    TemporaryFolder patch("root", { { "Readme.TXT", "patched" } });
    vfs::VirtualFileSystem files({ .layering = true });
    files.Mount("", std::make_shared<vfs::FolderFileSystem>(FOLDER));
    files.Mount("", std::make_shared<vfs::FolderFileSystem>(patch.folder));
    CHECK(files.Read("Readme.TXT") == "patched");
    CHECK(files.Read("levels/Level.json") == ReadFile(FOLDER + "/levels/Level.json"));
}

TEST_CASE("a mount with one file system is read without asking whether it has the file") {
    auto counting = std::make_shared<CountingFileSystem>();
    vfs::VirtualFileSystem files({ .layering = true });
    files.Mount("assets", counting);
    CHECK(files.Read("assets/anything") == "counted");
    CHECK(counting->exists == 0);

    files.Mount("assets", std::make_shared<vfs::FolderFileSystem>(FOLDER));
    CHECK(files.Read("assets/anything") == "counted");
    CHECK(counting->exists == 1);
}
