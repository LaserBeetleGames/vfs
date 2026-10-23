#ifndef VFS_ARCHIVE_FILE_SYSTEM_H
#define VFS_ARCHIVE_FILE_SYSTEM_H

#include <cstdint>
#include <fstream>
#include <map>
#include <string>

#include "vfs/IFileSystem.h"
#include "vfs/ILogger.h"

namespace vfs {
    constexpr const char* ARCHIVE_MAGIC = "VFSA";
    constexpr uint16_t ARCHIVE_VERSION = 1;
    constexpr size_t ARCHIVE_HEADER_SIZE = 32;
    constexpr size_t ARCHIVE_ENTRY_SIZE = 20;       // a table entry's fields before its path
    constexpr uint8_t ARCHIVE_METHOD_STORED = 0;
    constexpr uint8_t ARCHIVE_METHOD_DEFLATE = 1;   // raw deflate, with no zlib or gzip header

    // one file in an archive
    struct ArchiveEntry {
        uint32_t offset;        // of its first byte, from the start of the archive
        uint32_t storedSize;    // in the archive
        uint32_t size;          // once read
        uint32_t crc;           // CRC-32 of the file once read
        uint8_t method;
    };

    // a .VFA archive: a header, a table of its files by path, then the files, each stored or deflated
    // (docs/FORMAT.md). The file stays open while the file system exists. Paths are looked up normalised
    // and in lower case. Read shares one file handle, so it is called from one thread at a time
    class ArchiveFileSystem : public IFileSystem {
    public:
        // opens the archive and reads its table; throws vfs::Exception when it cannot be opened, is not a
        // version 1 archive, or names a method it cannot read. The logger, when there is one, is told of
        // each file that cannot be read
        explicit ArchiveFileSystem(const std::string& fileName, ILogger* logger = nullptr);

        bool Exists(const std::string& path) override;

        // the file, inflated when it is deflated; empty, with an error logged, when it fails its CRC-32
        std::string Read(const std::string& path) override;

        // a stored file is read from the archive as the stream is used, through a file handle of its own, and
        // its CRC-32 is not checked; a deflated file is read whole, as Read reads it, and streamed from memory
        std::unique_ptr<IStream> Open(const std::string& path) override;

        size_t GetFileCount() const { return this->entries.size(); }

    private:
        std::string fileName;
        std::ifstream file;
        std::map<std::string, ArchiveEntry> entries;
        ILogger* logger;

        // up to size bytes from the offset; fewer when the archive ends first
        std::string ReadBytes(uint32_t offset, uint32_t size);

        void LogError(const std::string& message);
    };
}

#endif
