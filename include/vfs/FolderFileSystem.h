#ifndef VFS_FOLDER_FILE_SYSTEM_H
#define VFS_FOLDER_FILE_SYSTEM_H

#include <string>

#include "vfs/IFileSystem.h"

namespace vfs {
    // the files in a folder on the platform's own file system, relative to the working folder unless the
    // folder's path is absolute. Paths match files as the platform matches them, in any case on Windows
    // and in their own case on Linux. Only files are read: a folder inside is not a file. A missing folder
    // is not an error: every read from it is empty
    class FolderFileSystem : public IFileSystem {
    public:
        explicit FolderFileSystem(const std::string& folder) : folder(folder) {}

        bool Exists(const std::string& path) override;
        std::string Read(const std::string& path) override;
        std::unique_ptr<IStream> Open(const std::string& path) override;

    private:
        std::string folder;

        std::string GetFileName(const std::string& path);
    };
}

#endif
