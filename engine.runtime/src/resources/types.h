
#pragma once
#include "defines.h"
#include "string/string.h"

namespace C3D
{
    // Pre-defined resource types
    enum class ResourceType : u8
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

    /** @brief The header for our proprietary resource files. */
    struct ResourceHeader
    {
        /** @brief A magic number indicating this file is a C3D binary file. */
        u32 magicNumber;
        /** @brief The type of this resource, maps to our ResourceType enum. */
        u8 resourceType;
        /** @brief The format version the resource file uses. */
        u8 version;
        /** @brief Some reserved space for future header data. */
        u16 reserved;
    };

    /** @brief A base-resource. All other resources derive from this. */
    struct IResource
    {
        IResource(ResourceType type) : resourceType(type) {}

        ResourceType resourceType;
        /** @brief The resource version. */
        u8 version = 0;
        /** @brief The name of the resource. */
        String name;
        /** @brief The full path to the resource. */
        String fullPath;
    };
}