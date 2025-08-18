
#pragma once
#include "defines.h"
#include "system/system.h"

namespace C3D
{
    class C3D_API ConfigSystem final : public SystemWithConfig
    {
    public:
        bool OnInit(const CSONObject& config) override;
        void OnShutdown() override;

        bool GetProperty(const String& name, u64& value);
        bool GetProperty(const String& name, String& value);

    private:
        CSONObject m_config;
    };
}  // namespace C3D