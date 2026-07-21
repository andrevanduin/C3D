
#include "path.h"

#include <filesystem>

namespace C3D
{
    bool Path::Exists(const String& path) { return std::filesystem::exists(path.Data()); }

    String Path::FileNameFromPath(const String& path, bool includeExtension)
    {
        // Split the path based on '/'
        auto parts = path.Split('/');
        // The last part will contain the filename (including extension)
        auto lastPart = parts.Last();

        // If we want to include the extension simply return
        if (includeExtension)
        {
            return lastPart;
        }

        // Otherwise we split on '.' and take the first part (filename only)
        parts = lastPart.Split('.');
        return parts.First();
    }
}  // namespace C3D