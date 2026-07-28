
#pragma once
#include "defines.h"
#include "string/string.h"

namespace C3D
{
    namespace Path
    {
        /**
         * @brief Checks if the provided path exists on the system.
         *
         * @param path The path you want to check
         * @return True if the path exists; False otherwise
         */
        C3D_API bool Exists(const String& path);

        /**
         * @brief Splits the provided path and returns only the filename part.
         *
         * @param path The path you want to get a filename from
         * @param includeExtension A boolean indicating if the extension (e.g. .png) should be included (defaults to false)
         * @return The filename part of the provided path
         */
        C3D_API String FileNameFromPath(const String& path, bool includeExtension = false);
    }  // namespace Path
}  // namespace C3D