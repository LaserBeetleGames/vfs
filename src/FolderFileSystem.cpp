#include "vfs/FolderFileSystem.h"

#include <algorithm>
#include <sstream>
#include <cstdio>
#if !defined(NXDK)
#include <sys/stat.h>
#endif

#include "vfs/FileStream.h"

using namespace std;

namespace vfs {
    // true when the name is a file, not a folder or anything else; a folder opens as a file on some platforms.
    // On the PlayStation 2 and with the Xbox's nxdk, which has no stat, a name is a file when it opens
    static bool IsFile(const string& fileName)
    {
#if defined(_EE) || defined(PS2) || defined(__PS2__) || defined(NXDK)
        FILE* file = fopen(fileName.c_str(), "rb");
        if (file == nullptr)
            return false;
        fclose(file);
        return true;
#else
        struct stat status;
        return stat(fileName.c_str(), &status) == 0 && (status.st_mode & S_IFMT) == S_IFREG;
#endif
    }

    bool FolderFileSystem::Exists(const string& path)
    {
        return IsFile(GetFileName(path));
    }

    string FolderFileSystem::Read(const string& path)
    {
        if (!IsFile(GetFileName(path)))
            return "";
        ifstream file(GetFileName(path).c_str(), ios::in | ios::binary);
        if (!file.is_open())
            return "";
        ostringstream bytes;
        bytes << file.rdbuf();
        return bytes.str();
    }

    unique_ptr<IStream> FolderFileSystem::Open(const string& path)
    {
        if (!IsFile(GetFileName(path)))
            return nullptr;
        auto stream = make_unique<FileStream>(GetFileName(path));
        if (!stream->IsOpen())
            return nullptr;
        return stream;
    }

    // with the Xbox's nxdk, whose file calls pass names straight to the kernel, folders are separated by
    // backslashes
    string FolderFileSystem::GetFileName(const string& path)
    {
        string fileName = this->folder.empty() ? path : this->folder + "/" + path;
#if defined(NXDK)
        replace(fileName.begin(), fileName.end(), '/', '\\');
#endif
        return fileName;
    }
}
