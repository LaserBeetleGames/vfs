#ifndef VFS_IFILE_SYSTEM_H
#define VFS_IFILE_SYSTEM_H

#include <memory>
#include <string>

#include "vfs/IStream.h"

namespace vfs {
    // a source of files, each read by its path within the file system: a backend a VirtualFileSystem mounts
    class IFileSystem {
    public:
        virtual ~IFileSystem() {}

        // true when the file system has the file, whether or not it can be read
        virtual bool Exists(const std::string& path) = 0;

        // the file's whole contents; empty when there is no such file or it cannot be read
        virtual std::string Read(const std::string& path) = 0;

        // the file as a stream, read as it is used; null when there is no such file or it cannot be opened
        virtual std::unique_ptr<IStream> Open(const std::string& path) = 0;
    };
}

#endif
