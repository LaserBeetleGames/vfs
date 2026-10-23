#ifndef VFS_VIRTUAL_FILE_SYSTEM_H
#define VFS_VIRTUAL_FILE_SYSTEM_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "vfs/FeatureConfig.h"
#include "vfs/IFileSystem.h"

namespace vfs {
    // reads files by path from the file systems mounted in it. A path's first name is a mount when one has
    // that name, and the rest of the path is read from the file systems mounted there; any other path is
    // read whole from the file systems mounted at the empty name, when there are any. Paths are normalised
    // before they are looked up, and mount names match in any case.
    //
    // With layering, a name can have several file systems mounted at it, in layers: a file is read from
    // the last mounted that has it, so a folder mounted over an archive replaces the archive's copies of
    // the files the folder holds. A file is in a layer when the layer has it, readable or not, so a
    // damaged file reads empty rather than falling through to an earlier layer's copy. A file missing
    // from every layer of its mount is missing, whatever is mounted at other names
    class VirtualFileSystem {
    public:
        explicit VirtualFileSystem(const FeatureConfig& features = FeatureConfig()) : features(features) {}

        // mounts the file system at the name; one file system can be mounted at several names. The empty
        // name mounts it for paths with no mount of their own. With layering, it is mounted over anything
        // mounted at the name already; without, a name already mounted throws vfs::Exception
        void Mount(const std::string& name, std::shared_ptr<IFileSystem> fileSystem);

        // removes every file system mounted at the name
        void Unmount(const std::string& name);

        // removes the file system from the name, leaving any others mounted there
        void Unmount(const std::string& name, const std::shared_ptr<IFileSystem>& fileSystem);

        // true when a file system mounted for the path has the file
        bool Exists(const std::string& path);

        // the file's whole contents; empty when there is no such file or it cannot be read
        std::string Read(const std::string& path);

        // the file as a stream, read as it is used; null when there is no such file or it cannot be opened
        std::unique_ptr<IStream> Open(const std::string& path);

    private:
        // the file system a path reads from, the last mounted that has the file, with the path within it;
        // no file system when none has it
        struct MountedPath {
            IFileSystem* fileSystem;
            std::string path;
        };

        FeatureConfig features;
        std::map<std::string, std::vector<std::shared_ptr<IFileSystem>>> mounts;    // by name, in lower case, in the order mounted

        MountedPath FindMount(const std::string& path);
    };
}

#endif
