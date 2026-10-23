#ifndef VFS_EXCEPTION_H
#define VFS_EXCEPTION_H

#include <stdexcept>
#include <string>

namespace vfs {
    // thrown when a file system cannot be opened, such as an archive that is missing or damaged
    class Exception : public std::runtime_error {
    public:
        explicit Exception(const std::string& message) : std::runtime_error(message) {}
    };
}

#endif
