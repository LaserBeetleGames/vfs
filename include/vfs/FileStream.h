#ifndef VFS_FILE_STREAM_H
#define VFS_FILE_STREAM_H

#include <fstream>
#include <string>

#include "vfs/IStream.h"

namespace vfs {
    // a stretch of a file, size bytes from start, read through a file handle of its own, so it can be read
    // on another thread from the one that opened it
    class FileStream : public IStream {
    public:
        FileStream(const std::string& fileName, size_t start, size_t size);

        // the whole file
        explicit FileStream(const std::string& fileName);

        bool IsOpen() { return this->file.is_open(); }

        size_t Read(char* buffer, size_t count) override;
        bool Seek(size_t newPosition) override;
        size_t GetPosition() override { return this->position; }
        size_t GetSize() override { return this->size; }

    private:
        std::ifstream file;
        size_t start;
        size_t size;
        size_t position;
    };
}

#endif
