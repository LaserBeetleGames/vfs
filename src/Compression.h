#ifndef VFS_COMPRESSION_H
#define VFS_COMPRESSION_H

#include <cstdint>
#include <string>

namespace vfs {
    // the bytes inflated from deflate data; valid only when the data inflated to exactly the size asked for
    struct InflateResult {
        bool valid;
        std::string bytes;
    };

    // zlib's CRC-32 of the bytes
    uint32_t Crc32(const std::string& bytes);

    // inflates raw deflate data, with no zlib or gzip header, which must inflate to exactly size bytes
    InflateResult Inflate(const std::string& deflated, size_t size);
}

#endif
