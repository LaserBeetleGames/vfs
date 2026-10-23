#include "vfs/VirtualFileSystem.h"

#include <algorithm>
#include <format>

#include "vfs/Exception.h"
#include "vfs/Paths.h"

using namespace std;

namespace vfs {
    void VirtualFileSystem::Mount(const string& name, shared_ptr<IFileSystem> fileSystem)
    {
        vector<shared_ptr<IFileSystem>>& layers = this->mounts[LowerCase(NormalisePath(name))];
        if (!layers.empty() && !this->features.layering)
            throw Exception(format("\"{}\" is mounted already, and layering is off", name));
        layers.push_back(std::move(fileSystem));
    }

    void VirtualFileSystem::Unmount(const string& name)
    {
        this->mounts.erase(LowerCase(NormalisePath(name)));
    }

    void VirtualFileSystem::Unmount(const string& name, const shared_ptr<IFileSystem>& fileSystem)
    {
        auto it = this->mounts.find(LowerCase(NormalisePath(name)));
        if (it == this->mounts.end())
            return;

        vector<shared_ptr<IFileSystem>>& layers = it->second;
        layers.erase(remove(layers.begin(), layers.end(), fileSystem), layers.end());
        if (layers.empty())
            this->mounts.erase(it);
    }

    bool VirtualFileSystem::Exists(const string& path)
    {
        MountedPath mounted = FindMount(path);
        return mounted.fileSystem != nullptr && mounted.fileSystem->Exists(mounted.path);
    }

    string VirtualFileSystem::Read(const string& path)
    {
        MountedPath mounted = FindMount(path);
        if (mounted.fileSystem == nullptr)
            return "";
        return mounted.fileSystem->Read(mounted.path);
    }

    unique_ptr<IStream> VirtualFileSystem::Open(const string& path)
    {
        MountedPath mounted = FindMount(path);
        if (mounted.fileSystem == nullptr)
            return nullptr;
        return mounted.fileSystem->Open(mounted.path);
    }

    VirtualFileSystem::MountedPath VirtualFileSystem::FindMount(const string& path)
    {
        string normalised = NormalisePath(path);
        string within = normalised;

        size_t slash = normalised.find('/');
        auto it = this->mounts.end();
        if (slash != string::npos) {
            it = this->mounts.find(LowerCase(normalised.substr(0, slash)));
            if (it != this->mounts.end())
                within = normalised.substr(slash + 1);
        }
        if (it == this->mounts.end())
            it = this->mounts.find("");
        if (it == this->mounts.end())
            return { nullptr, normalised };

        // one file system is read without asking whether it has the file
        const vector<shared_ptr<IFileSystem>>& layers = it->second;
        if (layers.size() == 1)
            return { layers.front().get(), within };

        for (auto layer = layers.rbegin(); layer != layers.rend(); ++layer) {
            if ((*layer)->Exists(within))
                return { layer->get(), within };
        }
        return { nullptr, within };
    }
}
