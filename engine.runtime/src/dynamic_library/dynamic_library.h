
#pragma once
#include "memory/allocators/malloc_allocator.h"
#include "platform/platform.h"
#include "string/string.h"

namespace C3D
{
    class C3D_API DynamicLibrary
    {
    public:
        ~DynamicLibrary();

        bool Load(const String& name);
        bool Unload();

        template <typename Signature>
        Signature LoadFunction(const String& name)
        {
            return reinterpret_cast<Signature>(Platform::LoadDynamicLibraryFunction(name.Data(), m_data));
        }

        template <typename Plugin, typename... Args>
        Plugin* CreatePlugin(Args&&... args)
        {
            const auto createPluginFunc = LoadFunction<Plugin* (*)(Args...)>("CreatePlugin");
            if (!createPluginFunc)
            {
                ERROR_LOG("Failed to load create function.");
                return nullptr;
            }
            return createPluginFunc(args...);
        }

        template <typename Plugin>
        void DeletePlugin(Plugin* plugin)
        {
            const auto deletePluginFunc = LoadFunction<void (*)(Plugin*)>("DeletePlugin");
            if (!deletePluginFunc)
            {
                ERROR_LOG("Failed to load delete function.");
            }
            deletePluginFunc(plugin);
        }

    protected:
        String m_name;

        u64 m_dataSize = 0;
        void* m_data   = nullptr;
    };
}  // namespace C3D
