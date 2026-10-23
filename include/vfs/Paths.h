#ifndef VFS_PATHS_H
#define VFS_PATHS_H

#include <string>
#include <vector>

namespace vfs {
    // the path with / between its names, and no empty names, . or ..; a .. removes the name before it.
    // \ is read as /
    inline std::string NormalisePath(const std::string& path)
    {
        std::vector<std::string> names;
        std::string name;
        for (char c : path + "/") {
            if (c != '/' && c != '\\') {
                name += c;
                continue;
            }
            if (name == "..") {
                if (!names.empty())
                    names.pop_back();
            }
            else if (!name.empty() && name != ".") {
                names.push_back(name);
            }
            name.clear();
        }

        std::string normalised;
        for (const std::string& n : names) {
            if (!normalised.empty())
                normalised += '/';
            normalised += n;
        }
        return normalised;
    }

    // the text with A to Z in lower case; every other byte is left as it is
    inline std::string LowerCase(const std::string& text)
    {
        std::string lower = text;
        for (char& c : lower) {
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
        }
        return lower;
    }
}

#endif
