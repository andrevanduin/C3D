
#pragma once
#include "assets/types.h"
#include "defines.h"

namespace C3D
{
    /** @brief An enum containing all possible formats for a texture. */
    enum class TextureFormat
    {
        UNDEFINED,
        BC1_RGBA_UNORM_BLOCK,
        BC2_UNORM_BLOCK,
        BC3_UNORM_BLOCK,
        BC4_UNORM_BLOCK,
        BC4_SNORM_BLOCK,
        BC5_UNORM_BLOCK,
        BC5_SNORM_BLOCK,
        BC6H_UFLOAT_BLOCK,
        BC6H_SFLOAT_BLOCK,
        BC7_UNORM_BLOCK
    };

    struct TextureAsset final : IAsset
    {
        TextureAsset() : IAsset(AssetType::Texture) {}

        /** @brief The width of the texture. */
        u32 width = 0;
        /** @brief The height of the texture. */
        u32 height = 0;
        /** @brief The number of mip levels in the texture. */
        u32 mipLevelCount = 0;
        /** @brief The number of bytes per block. */
        u32 blockSize = 0;
        /** @brief The actual size of the texture. */
        u32 size = 0;
        /** @brief A pointer a buffer that contains the actual texture data. For performance reasons this should point to a scratch/staging buffer. */
        u8* buffer = nullptr;
        /** @brief The size of the underlying buffer that holds the texture data. NOTE: This should be >= the actual texture (data) size. */
        u32 bufferSize = 0;
        /** @brief The format of the texture. */
        TextureFormat format = TextureFormat::UNDEFINED;
    };
}  // namespace C3D