#ifndef VFS_MEMORY_STREAM_H
#define VFS_MEMORY_STREAM_H

#include <algorithm>
#include <cstring>
#include <string>

#include "vfs/IStream.h"

namespace vfs {
    // a file already read whole, which the stream keeps
    class MemoryStream : public IStream {
    public:
        explicit MemoryStream(std::string bytes) : bytes(std::move(bytes)), position(0) {}

        size_t Read(char* buffer, size_t count) override
        {
            count = std::min(count, this->bytes.size() - this->position);
            std::memcpy(buffer, this->bytes.data() + this->position, count);
            this->position += count;
            return count;
        }

        bool Seek(size_t newPosition) override
        {
            if (newPosition > this->bytes.size())
                return false;
            this->position = newPosition;
            return true;
        }

        size_t GetPosition() override { return this->position; }
        size_t GetSize() override { return this->bytes.size(); }

    private:
        std::string bytes;
        size_t position;
    };
}

#endif
