
#pragma once
#include "assets/managers/scene/scene_manager.h"
#include "containers/dynamic_array.h"
#include "defines.h"
#include "dynamic_library/dynamic_library.h"
#include "identifiers/uuid.h"
#include "material.h"
#include "mesh.h"
#include "system/system.h"
#include "types.h"

namespace C3D
{
    struct MeshAsset;
    struct MeshDraw;

    class RendererPlugin;

    class C3D_API RenderSystem final : public SystemWithConfig
    {
    public:
        bool OnInit(const CSONObject& config) override;
        void OnShutdown() override;

        bool OnRun() const;

        bool UploadScene(const SceneAsset& scene);

        bool UploadMeshes(const DynamicArray<C3D::MeshAsset>& meshes);
        bool GenerateDrawCommands() const;

        bool Begin(Window& window) const;
        bool End(Window& window) const;

        bool Submit(Window& window) const;
        bool Present(Window& window) const;

        bool OnCreateWindow(Window& window) const;
        bool OnResizeWindow(Window& window) const;
        void OnDestroyWindow(Window& window) const;

        void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) const;
        void SetScissor(i32 offsetX, i32 offsetY, u32 width, u32 height) const;

        void SetActiveCamera(UUID cameraHandle) const;
        void SetSunDirection(const vec3& sunDirection) const;

    private:
        u32 GenerateMeshlets(const DynamicArray<u32>& indices, const DynamicArray<vec3>& positions, u32 baseVertex);

        /** @brief A pointer to the backend rendering plugin used to actually render things on the screen. */
        RendererPlugin* m_backendPlugin = nullptr;
        /** @brief A dynamic library object to load the rendering plugin. */
        DynamicLibrary m_backendDynamicLibrary;
        /** @brief The configuration for our render system. */
        RenderSystemConfig m_config;
        /** @brief An array containing all materials to be used for rendering. */
        DynamicArray<Material> m_materials;
        /** @brief A structure holding all the geometry ready for rendering. */
        Geometry m_geometry;
    };
}  // namespace C3D