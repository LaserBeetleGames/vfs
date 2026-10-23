#ifndef VFS_FEATURE_CONFIG_H
#define VFS_FEATURE_CONFIG_H

namespace vfs {
    // the features a VirtualFileSystem is built with, each off unless it is turned on:
    // vfs::VirtualFileSystem files({ .layering = true });
    struct FeatureConfig {
        // several file systems mounted at one name, a file read from the last mounted that has it
        bool layering = false;
    };
}

#endif
