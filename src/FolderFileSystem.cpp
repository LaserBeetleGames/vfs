#include "vfs/FolderFileSystem.h"

#include <sstream>
#include <sys/stat.h>

#include "vfs/FileStream.h"

using namespace std;

namespace vfs {
    // true when the name is a file, not a folder or anything else; a folder opens as a file on some platforms
    static bool IsFile(const string& fileName)
    {
        struct stat status;
        return stat(fileName.c_str(), &status) == 0 && (status.st_mode & S_IFMT) == S_IFREG;
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

    string FolderFileSystem::GetFileName(const string& path)
    {
        if (this->folder.empty())
            return path;
        return this->folder + "/" + path;
    }
}
