
#pragma once
#include "defines.h"
#include "string/string.h"

namespace C3D
{
    // Pre-defined resource types
    enum class AssetType : u8
    {
        None,
        Text,
        Binary,
        Image,
        Material,
        Mesh,
        Shader,
        BitmapFont,
        SystemFont,
        Scene,
        Terrain,
        AudioFile,
        Custom,
        MaxValue
    };

    using AssetTypeFlag = u8;

    /** @brief The header for our proprietary asset files. */
    struct AssetHeader
    {
        /** @brief A magic number indicating this file is a C3D binary file. */
        u32 magicNumber = INVALID_ID;
        /** @brief The type of this asset, maps to our AssetType enum. */
        AssetType type = AssetType::None;
        /** @brief The format version the resource file uses. */
        u8 version = 0;
        /** @brief Some reserved space for future header data. */
        u16 reserved;
    };

    /** @brief A base-asset. All other assets derive from this. */
    struct IAsset
    {
        IAsset(AssetType type) : type(type) {}

        AssetType type;
        /** @brief The asset version. */
        u8 version = 0;
        /** @brief The name of the asset. */
        String name;
        /** @brief The (full) path to the asset. */
        String path;
    };
}