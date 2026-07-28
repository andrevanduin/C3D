
#pragma once
#include "assets/types/dds_types.h"
#include "assets/types/texture_types.h"
#include "string/string.h"

namespace C3D
{
    class DDSDeserializer
    {
    public:
        bool Deserialize(const String& path, TextureAsset& asset) const;

    private:
        TextureFormat ConvertFormat(const DDSHeader& header, const DDSHeaderDXT10& headerDxt10) const;
        u32 GetBlockSize(TextureFormat format) const;
        u32 GetImageSizeBC(const TextureAsset& asset) const;
    };
}  // namespace C3D