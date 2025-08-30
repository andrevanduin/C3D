
#include "shader_manager.h"

#include "logger/logger.h"
#include "platform/file_system.h"
#include "string/string_utils.h"
#include "system/system_manager.h"
#include "time/scoped_timer.h"

namespace C3D
{
    constexpr const char* INCLUDE_DIRECTIVE = "#include";

    ShaderManager::ShaderManager() : IAssetManager(MemoryType::Array, AssetType::ShaderSource, "shaders") {}

    bool ShaderManager::Read(const String& name, ShaderAsset& asset)
    {
        ScopedTimer timer(String::FromFormat("Loading ShaderModule: '{}'.", name));

        const char* EXTENSIONS[] = { "glsl" };

        if (name.Empty())
        {
            ERROR_LOG("No valid name was provided.")
            return false;
        }

        // Try a few different extensions
        String fullPath;
        const char* correctExtension = nullptr;
        for (auto extension : EXTENSIONS)
        {
            fullPath = String::FromFormat("{}/{}/{}.{}", m_assetPath, m_subFolder, name, extension);
            if (File::Exists(fullPath))
            {
                correctExtension = extension;
                break;
            }
        }

        if (!correctExtension)
        {
            ERROR_LOG("Unable to open find file: '{}' with any known shader source extension ({}).", name,
                      StringUtils::Join(EXTENSIONS, ARRAY_SIZE(EXTENSIONS), ','));
            return false;
        }

        ShaderAsset import;
        if (!LoadShaderSource(name, fullPath, import))
        {
            return false;
        }

        // Copy over all the required data
        asset.name   = import.name;
        asset.path   = import.path;
        asset.size   = import.size;
        asset.source = Memory.Allocate<char>(MemoryType::Shader, asset.size);
        std::memcpy(asset.source, import.source, import.size);

        Cleanup(import);

        return true;
    }

    void ShaderManager::Cleanup(ShaderAsset& asset)
    {
        if (asset.source)
        {
            Memory.Free(asset.source);
            asset.source = nullptr;
        }

        asset.name.Destroy();
        asset.path.Destroy();
    }

    bool ShaderManager::LoadShaderSource(const String& name, const String& path, ShaderAsset& asset)
    {
        File file;
        if (!file.Open(path, FileModeRead | FileModeBinary))
        {
            ERROR_LOG("Found file: '{}' but was unable to open it for reading.", path);
            return false;
        }

        asset.path = path;

        u64 fileSize = 0;
        if (!file.Size(&fileSize))
        {
            ERROR_LOG("Unable to get size of file: '{}'.", path);
            file.Close();
            return false;
        }

        asset.source = Memory.Allocate<char>(MemoryType::Array, fileSize);
        asset.name   = name;

        if (!file.ReadAll(asset.source, &asset.size))
        {
            ERROR_LOG("Unable to read data from file: '{}'.", path);
            file.Close();
            return false;
        }

        // Find and resolve all #include statements
        FindAndResolveIncludes(asset);

        file.Close();

        return true;
    }

    void ShaderManager::FindAndResolveIncludes(ShaderAsset& asset)
    {
        const char* s = asset.source;
        while (s)
        {
            // Find the next #include
            const char* inc = std::strstr(s, INCLUDE_DIRECTIVE);
            if (inc == nullptr)
            {
                // No more #include
                break;
            }
            // Skip ahead to the "#include"
            s = inc;
            // Skip the '#include "'
            s += 10;
            // Find the closing '"'
            const char* end = static_cast<const char*>(std::memchr(s, '"', s - asset.source));
            // Keep track of the name of the file we need to include and the position of the #include
            ShaderInclude include;
            include.asset.name = String(s, end - s);
            include.start      = inc - asset.source;
            include.end        = end - asset.source;
            // Resolve this include before proceeding
            u64 afterIncludePos = ResolveInclude(asset, include);
            // Set s to point to right after the include
            s = asset.source + afterIncludePos;
        }
    }

    u64 ShaderManager::ResolveInclude(ShaderAsset& asset, ShaderInclude& include)
    {
        LoadShaderSource(include.asset.name, String::FromFormat("{}/{}/{}", m_assetPath, m_subFolder, include.asset.name), include.asset);

        // Length of the #include statement
        u64 includeLength = include.end - include.start;
        // Extra length will be include_length - (#include statement length)
        u64 extraLength = include.asset.size - includeLength - 1;
        // The new size will be the old size + extraLength
        u64 newSize = asset.size + extraLength;

        // Allocate enough space for the asset + the extra stuff from the include
        char* newSource = Memory.Allocate<char>(MemoryType::Array, asset.size + extraLength);
        // Copy everything before #include into the new source
        std::memcpy(newSource, asset.source, include.start);
        // Copy the include
        std::memcpy(newSource + include.start, include.asset.source, include.asset.size);
        // Copy everything after the #include
        std::memcpy(newSource + include.start + include.asset.size, asset.source + include.end + 1, asset.size - include.end - 1);

        Memory.Free(asset.source);
        asset.source = newSource;
        asset.size   = newSize;

        // Cleanup the asset for loading the include
        Cleanup(include.asset);

        // Finally we return the point where the parser should continue
        return include.asset.size + include.start;
    }

}  // namespace C3D
