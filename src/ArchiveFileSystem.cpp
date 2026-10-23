#include "vfs/ArchiveFileSystem.h"

#include <format>

#include "vfs/Exception.h"
#include "vfs/FileStream.h"
#include "vfs/MemoryStream.h"
#include "vfs/Paths.h"
#include "Compression.h"

using namespace std;

namespace vfs {
    // little-endian values from the bytes at an offset
    static uint16_t ReadUint16(const string& bytes, size_t at)
    {
        return static_cast<uint16_t>(static_cast<uint8_t>(bytes[at])
            | static_cast<uint8_t>(bytes[at + 1]) << 8);
    }

    static uint32_t ReadUint32(const string& bytes, size_t at)
    {
        return static_cast<uint32_t>(static_cast<uint8_t>(bytes[at]))
            | static_cast<uint32_t>(static_cast<uint8_t>(bytes[at + 1])) << 8
            | static_cast<uint32_t>(static_cast<uint8_t>(bytes[at + 2])) << 16
            | static_cast<uint32_t>(static_cast<uint8_t>(bytes[at + 3])) << 24;
    }

    ArchiveFileSystem::ArchiveFileSystem(const string& fileName, ILogger* logger) : fileName(fileName), logger(logger)
    {
        this->file.open(fileName.c_str(), ios::in | ios::binary);
        if (!this->file.is_open())
            throw Exception(format("Archive \"{}\" could not be opened", fileName));

        string header = ReadBytes(0, ARCHIVE_HEADER_SIZE);
        if (header.size() != ARCHIVE_HEADER_SIZE || header.compare(0, 4, ARCHIVE_MAGIC) != 0)
            throw Exception(format("\"{}\" is not a .VFA archive", fileName));

        uint16_t version = ReadUint16(header, 4);
        if (version != ARCHIVE_VERSION)
            throw Exception(format("\"{}\" is version {}, only version {} archives can be read", fileName, version, ARCHIVE_VERSION));

        uint32_t count = ReadUint32(header, 8);
        uint32_t tableSize = ReadUint32(header, 12);
        string table = ReadBytes(ARCHIVE_HEADER_SIZE, tableSize);
        if (table.size() != tableSize)
            throw Exception(format("\"{}\" ends inside its table", fileName));

        // each entry is padded to a multiple of 4 bytes
        size_t position = 0;
        for (uint32_t i = 0; i < count; i++) {
            if (position + ARCHIVE_ENTRY_SIZE > table.size())
                throw Exception(format("\"{}\" has a table shorter than its {} entries", fileName, count));

            ArchiveEntry entry;
            entry.offset = ReadUint32(table, position);
            entry.storedSize = ReadUint32(table, position + 4);
            entry.size = ReadUint32(table, position + 8);
            entry.crc = ReadUint32(table, position + 12);
            entry.method = static_cast<uint8_t>(table[position + 16]);
            uint16_t pathLength = ReadUint16(table, position + 18);
            if (position + ARCHIVE_ENTRY_SIZE + pathLength > table.size())
                throw Exception(format("\"{}\" has a table shorter than its {} entries", fileName, count));
            string path = table.substr(position + ARCHIVE_ENTRY_SIZE, pathLength);

            if (entry.method != ARCHIVE_METHOD_STORED && entry.method != ARCHIVE_METHOD_DEFLATE)
                throw Exception(format("\"{}\" in \"{}\" uses method {}, which cannot be read", path, fileName, entry.method));
            if (entry.method == ARCHIVE_METHOD_STORED && entry.storedSize != entry.size)
                throw Exception(format("\"{}\" in \"{}\" is stored with a size other than its own", path, fileName));

            this->entries[path] = entry;
            position += (ARCHIVE_ENTRY_SIZE + pathLength + 3) / 4 * 4;
        }
    }

    bool ArchiveFileSystem::Exists(const string& path)
    {
        return this->entries.find(LowerCase(NormalisePath(path))) != this->entries.end();
    }

    string ArchiveFileSystem::Read(const string& path)
    {
        auto it = this->entries.find(LowerCase(NormalisePath(path)));
        if (it == this->entries.end())
            return "";
        const ArchiveEntry& entry = it->second;

        string bytes = ReadBytes(entry.offset, entry.storedSize);
        if (bytes.size() != entry.storedSize) {
            LogError(format("\"{}\" in \"{}\" is cut short", path, this->fileName));
            return "";
        }

        if (entry.method == ARCHIVE_METHOD_DEFLATE) {
            InflateResult inflated = Inflate(bytes, entry.size);
            if (!inflated.valid) {
                LogError(format("\"{}\" in \"{}\" could not be inflated", path, this->fileName));
                return "";
            }
            bytes = std::move(inflated.bytes);
        }

        if (Crc32(bytes) != entry.crc) {
            LogError(format("\"{}\" in \"{}\" fails its CRC-32 check", path, this->fileName));
            return "";
        }
        return bytes;
    }

    unique_ptr<IStream> ArchiveFileSystem::Open(const string& path)
    {
        auto it = this->entries.find(LowerCase(NormalisePath(path)));
        if (it == this->entries.end())
            return nullptr;
        const ArchiveEntry& entry = it->second;

        if (entry.method == ARCHIVE_METHOD_DEFLATE) {
            string bytes = Read(path);
            if (bytes.empty() && entry.size != 0)
                return nullptr;
            return make_unique<MemoryStream>(std::move(bytes));
        }

        auto stream = make_unique<FileStream>(this->fileName, entry.offset, entry.size);
        if (!stream->IsOpen())
            return nullptr;
        return stream;
    }

    string ArchiveFileSystem::ReadBytes(uint32_t offset, uint32_t size)
    {
        string bytes(size, '\0');
        this->file.clear();
        this->file.seekg(offset);
        this->file.read(bytes.data(), size);
        bytes.resize(static_cast<size_t>(this->file.gcount()));
        return bytes;
    }

    void ArchiveFileSystem::LogError(const string& message)
    {
        if (this->logger != nullptr)
            this->logger->Error(message);
    }
}
