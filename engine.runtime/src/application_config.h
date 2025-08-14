
#pragma once
#include "containers/dynamic_array.h"
#include "containers/hash_map.h"
#include "cson/cson_types.h"
#include "defines.h"
#include "platform/platform_types.h"
#include "string/string.h"

namespace C3D
{
    enum ApplicationFlag : u8
    {
        /** @brief No flags set. */
        ApplicationFlagNone = 0x0,
    };

    using ApplicationFlagBits = u8;

    struct ApplicationConfig
    {
        /** @brief The name of the application. */
        String name;
        /** @brief The size of total memory (in MB) that should be allocated to the entire engine/game. */
        u64 memorySize = 1024;
        /** @brief The size that should be allocated for the per-frame allocator. */
        u64 frameAllocatorSize = 0;
        /** @brief Flags that indicate certain properties about this application. */
        ApplicationFlagBits flags = ApplicationFlagNone;
        /** @brief An array of window configs. */
        DynamicArray<WindowConfig> windowConfigs;
        /** @brief A Hashmap containing CSONObjects with the configuration for a system. Indexable by the name of the system. */
        HashMap<String, CSONObject> systemConfigs;
        /** @brief A Hashmap containing CSONObjects with node configuration for a rendergraph. Indexable by the name of the rendergraph. */
        HashMap<String, CSONObject> rendergraphs;
    };

    bool C3D_API ParseArgs(int argc, char** argv, ApplicationConfig& outConfig);
}  // namespace C3D