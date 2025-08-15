
#pragma once
#include "containers/dynamic_array.h"
#include "platform/file_system.h"
#include "renderer/mesh.h"
#include "renderer/vertex.h"
#include "resource_manager.h"
#include "systems/system_manager.h"
#include "time/clock.h"
#include "time/scoped_timer.h"

namespace C3D
{
    constexpr i32 FACE_INDEX_NOT_POPULATED = -1;

    class File;

    enum class MeshFileType
    {
        NotFound,
        Obj,
    };

    namespace
    {
        /** @brief Enum describing what the format of the f (face) lines looks like in the OBJ file.
         * NOTE: This assumes we don't mix formats (is that even allowed?)
         */
        enum class FaceFormat : u8
        {
            Unknown,
            V_vt_vn,
            V_vt,
            V_vn,
            V
        };
    }  // namespace

    template <>
    class C3D_API ResourceManager<Mesh> final : public IResourceManager
    {
    public:
        ResourceManager();

        bool Read(const String& name, Mesh& resource);
        void Cleanup(Mesh& resource) const;

    private:
        bool ImportObjFile(Mesh& resource);
        void CountLinesAndReserveSpace();
        void ObjParseVertexLine();
        void ObjParseFaceLine();

        DynamicArray<vec3> m_vs;
        DynamicArray<vec3> m_vns;
        DynamicArray<vec2> m_vts;
        DynamicArray<i32> m_fs;

        i32 m_fI = 0;

        FaceFormat m_faceFormat;

        String m_line;
    };
}  // namespace C3D
