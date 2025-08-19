
#pragma once
#include "asset_manager.h"
#include "containers/dynamic_array.h"
#include "platform/file_system.h"
#include "renderer/mesh.h"
#include "renderer/vertex.h"
#include "system/system_manager.h"
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

        template <typename T>
        struct VArray
        {
            T* data = nullptr;
            u32 size;
            u32 capacity;

            void Create()
            {
                C3D_ASSERT(!data);

                capacity = 64;
                size     = 0;
                data     = Memory.Allocate<T>(MemoryType::AssetManager, capacity);
            }

            void Grow()
            {
                u32 newCapacity = capacity * 1.5;
                T* newData      = Memory.Allocate<T>(MemoryType::AssetManager, newCapacity);

                if (data)
                {
                    std::memcpy(newData, data, capacity * sizeof(T));
                    Memory.Free(data);
                }

                data     = newData;
                capacity = newCapacity;
            }

            void Destroy()
            {
                if (data)
                {
                    Memory.Free(data);
                    data     = nullptr;
                    capacity = 0;
                    size     = 0;
                }
            }
        };

    }  // namespace

    class C3D_API MeshManager final : public IAssetManager
    {
    public:
        MeshManager();

        bool Read(const String& name, Mesh& resource);

        static void Cleanup(Mesh& resource);

    private:
        bool ImportObjFile(Mesh& resource);

        void ObjParseLine(const char* line);

        void ObjParseVertexLine(const char* s);

        /** @brief Fixes a face index to be a correct array index.
         *
         * OBJ files allow multiple ways of specifying indices:
         * 1. Positive indices starting at 1
         * Here we simply subtract 1 to ensure we index into the arrays starting from 0.
         *
         * 2. Negative indices where -1 refers to the last element in the array (-2 to the second last etc.)
         * Here we add the index to the size of the array to get the correct element
         *
         * In both cases after conversion -1 will refer to an empty element.
         */
        i32 FixIndex(i32 index, u32 size);

        const char* ParseFace(const char* s, i32& vi, i32& vti, i32& vni);
        void ObjParseFaceLine(const char* line);

        VArray<f32> m_vs;
        VArray<f32> m_vns;
        VArray<f32> m_vts;

        VArray<i32> m_fs;

        FaceFormat m_faceFormat = FaceFormat::Unknown;
    };
}  // namespace C3D
