
#pragma once
#include "asset_manager.h"

namespace C3D
{
    struct ShaderInclude;

    struct ImportShader
    {
        char* source = nullptr;
        u64 size     = 0;
        String name;
        String path;

        DynamicArray<ShaderInclude> includes;

        void Cleanup();
    };

    struct ShaderInclude
    {
        ImportShader asset;
        u64 start = 0;
        u64 end   = 0;
    };

    struct ShaderAsset final : IAsset
    {
        ShaderAsset() : IAsset(AssetType::ShaderSource) {}

        char* source = nullptr;
        u64 size     = 0;
    };

    class C3D_API ShaderManager final : public IAssetManager
    {
    public:
        ShaderManager();

        bool Read(const String& name, ShaderAsset& resource);
        static void Cleanup(ShaderAsset& resource);

    private:
        bool LoadShaderSource(const String& path, const String& name, ImportShader& import);

        /** @brief Finds all #include statements in the source.
         * Then it fills the m_includes array with those #include filenames.
         */
        void FindIncludes(ImportShader& import);

        /**
         * @brief Resolves all #include statements in current source
         * by replace the #include with the source of that particular file.
         */
        void ResolveIncludes(ImportShader& import);
    };
}  // namespace C3D