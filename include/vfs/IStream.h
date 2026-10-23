#ifndef VFS_ISTREAM_H
#define VFS_ISTREAM_H

#include <cstddef>

namespace vfs {
    // a file read a piece at a time, from a position that moves past what is read
    class IStream {
    public:
        virtual ~IStream() {}

        // up to count bytes into the buffer; the number read, fewer at the end of the file
        virtual size_t Read(char* buffer, size_t count) = 0;

        // false, leaving the position where it was, when the new position is past the end of the file
        virtual bool Seek(size_t newPosition) = 0;

        virtual size_t GetPosition() = 0;
        virtual size_t GetSize() = 0;
    };
}

#endif
