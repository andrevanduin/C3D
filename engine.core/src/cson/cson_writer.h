
#pragma once
#include "cson_types.h"
#include "defines.h"

namespace C3D
{
    class C3D_API CSONWriter
    {
    public:
        void Write(const CSONObject& object, String& output);
        bool WriteToFile(const CSONObject& object, const String& path);

    private:
        void WriteProperty(const CSONProperty& property, String& output, bool last, bool isInlineArray);
        void WriteArray(const CSONArray& array, String& output);
        void WriteObject(const CSONObject& object, String& output);

        void NextLine(String& output);

        u32 m_indentation = 0;
    };
}  // namespace C3D