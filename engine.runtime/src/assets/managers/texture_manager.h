
#pragma once

#include "asset_manager.h"
#include "assets/types/texture_types.h"
#include "defines.h"

namespace C3D
{
    class C3D_API TextureManager : IAssetManager
    {
    public:
        TextureManager();

        bool Read(const String& path, TextureAsset& asset);
        void Cleanup(TextureAsset& asset);
    };
}  // namespace C3D