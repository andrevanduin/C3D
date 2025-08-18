
#pragma once
#include "asset_manager.h"

namespace C3D
{
    struct BinaryAsset final : IAsset
    {
        BinaryAsset() : IAsset(AssetType::Binary) {}

        char* data = nullptr;
        u64 size   = 0;
    };

    class C3D_API BinaryManager final : public IAssetManager
    {
    public:
        BinaryManager();

        bool Read(const String& name, BinaryAsset& resource);
        static void Cleanup(BinaryAsset& resource);
    };
}  // namespace C3D