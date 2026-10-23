#include "vfs/FileStream.h"

#include <algorithm>

using namespace std;

namespace vfs {
    FileStream::FileStream(const string& fileName, size_t start, size_t size) : start(start), size(size), position(0)
    {
        this->file.open(fileName.c_str(), ios::in | ios::binary);
    }

    FileStream::FileStream(const string& fileName) : start(0), size(0), position(0)
    {
        this->file.open(fileName.c_str(), ios::in | ios::binary | ios::ate);
        if (this->file.is_open())
            this->size = static_cast<size_t>(this->file.tellg());
    }

    size_t FileStream::Read(char* buffer, size_t count)
    {
        count = min(count, this->size - this->position);
        this->file.clear();
        this->file.seekg(static_cast<streamoff>(this->start + this->position));
        this->file.read(buffer, static_cast<streamsize>(count));
        size_t read = static_cast<size_t>(this->file.gcount());
        this->position += read;
        return read;
    }

    bool FileStream::Seek(size_t newPosition)
    {
        if (newPosition > this->size)
            return false;
        this->position = newPosition;
        return true;
    }
}
