#ifndef VFS_ILOGGER_H
#define VFS_ILOGGER_H

#include <string>

namespace vfs {
    // told of a file that is there but cannot be read, such as one that fails its CRC-32 check
    class ILogger {
    public:
        virtual ~ILogger() {}

        virtual void Error(const std::string& message) = 0;
    };
}

#endif
