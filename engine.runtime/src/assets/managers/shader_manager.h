
#pragma once
#include "asset_manager.h"

namespace C3D
{
    struct ShaderAsset final : IAsset
    {
        ShaderAsset() : IAsset(AssetType::ShaderSource) {}

        char* source = nullptr;
        u64 size     = 0;
    };

    struct ShaderInclude
    {
        ShaderAsset asset;
        u64 start = 0;
        u64 end   = 0;
    };

    class C3D_API ShaderManager final : public IAssetManager
    {
    public:
        ShaderManager();

        bool Read(const String& name, ShaderAsset& asset);
        static void Cleanup(ShaderAsset& asset);

    private:
        bool LoadShaderSource(const String& path, const String& name, ShaderAsset& import);

        /** @brief Finds all #include statements in the source.
         * Then it fills the m_includes array with those #include filenames.
         */
        void FindAndResolveIncludes(ShaderAsset& import);

        /**
         * @brief Resolves the provided include for the asset.
         *
         * @param asset The asset which needs it's #include resolved
         * @param include The information about the #include
         * @return The size of the include source in bytes (used to skip ahead before parsing the rest of the asset)
         */
        u64 ResolveInclude(ShaderAsset& asset, ShaderInclude& include);
    };
}  // namespace C3D