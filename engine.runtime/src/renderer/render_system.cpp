
#include "render_system.h"

#include "cson/cson_types.h"
#include "math/c3d_math.h"
#include "mesh.h"
#include "renderer_plugin.h"
#include "utils/mesh_utils.h"

namespace C3D
{
    bool RenderSystem::OnInit(const CSONObject& config)
    {
        // Parse the user provided config
        for (const auto& prop : config.properties)
        {
            if (prop.name.IEquals("backend"))
            {
                m_config.rendererPlugin = prop.GetString();
            }
            else if (prop.name.IEquals("vsync"))
            {
                if (prop.GetBool()) m_config.flags |= FlagVSync;
            }
            else if (prop.name.IEquals("powersaving"))
            {
                if (prop.GetBool()) m_config.flags |= FlagPowerSaving;
            }
            else if (prop.name.IEquals("validationlayers"))
            {
                if (prop.GetBool()) m_config.flags |= FlagValidationLayers;
            }
            else if (prop.name.IEquals("PCF"))
            {
                if (prop.GetBool()) m_config.flags |= FlagPCF;
            }
        }

        // Load the backend plugin
        m_backendDynamicLibrary.Load(m_config.rendererPlugin);

        m_backendPlugin = m_backendDynamicLibrary.CreatePlugin<RendererPlugin>();
        if (!m_backendPlugin)
        {
            FATAL_LOG("Failed to create valid renderer plugin.");
            return false;
        }

        RendererPluginConfig rendererPluginConfig{};
        rendererPluginConfig.flags = m_config.flags;

        if (!m_backendPlugin->OnInit(rendererPluginConfig))
        {
            FATAL_LOG("Failed to Initialize Renderer Backend.");
            return false;
        }

        INFO_LOG("Initialized successfully.");
        return true;
    }

    void RenderSystem::OnShutdown()
    {
        INFO_LOG("Shutting down.");

        // Shutdown our plugin
        m_backendPlugin->OnShutdown();

        // Delete the plugin
        m_backendDynamicLibrary.DeletePlugin(m_backendPlugin);

        // Unload the library
        if (!m_backendDynamicLibrary.Unload())
        {
            ERROR_LOG("Failed to unload backend plugin dynamic library.");
        }
    }

    bool RenderSystem::CreateResources() const { return m_backendPlugin->CreateResources(); }

    bool RenderSystem::UploadMeshes(const Window& window, const DynamicArray<MeshAsset>& meshAssets)
    {
        for (const auto& asset : meshAssets)
        {
            Mesh mesh = {};

            mesh.vertexOffset = m_geometry.vertices.Size();
            mesh.vertexCount  = asset.vertices.Size();

            m_geometry.vertices.Insert(m_geometry.vertices.end(), asset.vertices.begin(), asset.vertices.end());

            vec3 center = vec3(0);
            for (const auto& v : asset.vertices)
            {
                center += v.pos;
            }
            center /= static_cast<f32>(asset.vertices.Size());

            f32 radius = 0.f;
            for (const auto& v : asset.vertices)
            {
                radius = Max(radius, glm::distance(center, v.pos));
            }

            mesh.center = center;
            mesh.radius = radius;

            DynamicArray<u32> lodIndices = asset.indices;

            bool buildMeshlets = m_backendPlugin->SupportsFeature(RENDERER_SUPPORT_FLAG_MESH_SHADING);

            while (mesh.lodCount < ARRAY_SIZE(mesh.lods))
            {
                MeshLod& lod = mesh.lods[mesh.lodCount++];

                lod.indexOffset = static_cast<u32>(m_geometry.indices.Size());
                lod.indexCount  = static_cast<u32>(lodIndices.Size());

                m_geometry.indices.Insert(m_geometry.indices.end(), lodIndices.begin(), lodIndices.end());

                lod.meshletOffset = static_cast<u32>(m_geometry.meshlets.Size());
                lod.meshletCount  = buildMeshlets ? GenerateMeshlets(lodIndices, asset.vertices) : 0;

                if (mesh.lodCount < ARRAY_SIZE(mesh.lods))
                {
                    u64 nextIndicesSizeTarget = static_cast<u64>(static_cast<f64>(lodIndices.Size() * 0.75));
                    u64 nextIndicesSize       = MeshUtils::Simplify(lodIndices, lodIndices, asset.vertices, nextIndicesSizeTarget, 1e-4f);

                    if (nextIndicesSize == lodIndices.Size())
                    {
                        // We have reached our error bound (so we can't simplify further)
                        break;
                    }

                    lodIndices.Resize(nextIndicesSize);
                    MeshUtils::OptimizeForVertexCache(lodIndices, lodIndices, mesh.vertexCount);
                }
            }

            while (m_geometry.meshlets.Size() % 64)
            {
                m_geometry.meshlets.EmplaceBack();
            }

            m_geometry.meshes.PushBack(mesh);
        }

        return m_backendPlugin->UploadGeometry(window, m_geometry);
    }

    bool RenderSystem::GenerateDrawCommands(const Window& window) const { return m_backendPlugin->GenerateDrawCommands(window, m_geometry); }

    bool RenderSystem::Begin(Window& window) const { return m_backendPlugin->Begin(window); }

    bool RenderSystem::End(Window& window) const { return m_backendPlugin->End(window); }

    bool RenderSystem::Submit(Window& window) const { return m_backendPlugin->Submit(window); }

    bool RenderSystem::Present(Window& window) const { return m_backendPlugin->Present(window); }

    bool RenderSystem::OnCreateWindow(Window& window) const
    {
        // Create the renderer state for this window
        window.rendererState = Memory.New<WindowRendererState>(MemoryType::RenderSystem);

        if (!m_backendPlugin->OnCreateWindow(window))
        {
            ERROR_LOG("The Renderer backend failed to create resources for window: '{}'.", window.name);
            return false;
        }
        return true;
    }

    bool RenderSystem::OnResizeWindow(Window& window) const { return m_backendPlugin->OnResizeWindow(window); }

    void RenderSystem::OnDestroyWindow(Window& window) const
    {
        m_backendPlugin->OnDestroyWindow(window);

        if (window.rendererState)
        {
            Memory.Delete(window.rendererState);
            window.rendererState = nullptr;
        }
    }

    void RenderSystem::SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) const
    {
        m_backendPlugin->SetViewport(x, y, width, height, minDepth, maxDepth);
    }

    void RenderSystem::SetScissor(i32 offsetX, i32 offsetY, u32 width, u32 height) const { m_backendPlugin->SetScissor(offsetX, offsetY, width, height); }

    u32 RenderSystem::GenerateMeshlets(const DynamicArray<u32>& indices, const DynamicArray<Vertex>& vertices)
    {
        // Determine our upper bound of meshlets
        u32 maxMeshlets = MeshUtils::DetermineMaxMeshlets(indices.Size(), MESHLET_MAX_VERTICES, MESHLET_MAX_TRIANGLES);
        // Preallocate enough memory for the maximum number of meshlets
        DynamicArray<MeshUtils::Meshlet> meshlets(maxMeshlets);
        // Generate our meshlets
        u32 numMeshlets = MeshUtils::GenerateMeshlets(indices, vertices.Size(), meshlets, 0.25f);
        //  Resize our meshlet array to the actual number of meshlets
        meshlets.Resize(numMeshlets);

        for (const auto& meshlet : meshlets)
        {
            Meshlet m       = {};
            m.vertexCount   = meshlet.vertexCount;
            m.triangleCount = meshlet.triangleCount;
            m.dataOffset    = m_geometry.meshletData.Size();

            // Populate the vertex indices
            for (u32 i = 0; i < meshlet.vertexCount; ++i)
            {
                m_geometry.meshletData.PushBack(meshlet.vertices[i]);
            }

            // Get a pointer to the triangle data as u32
            const u32* triangleData = reinterpret_cast<const u32*>(meshlet.indices);
            // Count the number of indices (triangles * 3) and add 3 to ensure that rounded down we always have enough room
            u32 packedTriangleCount = (meshlet.triangleCount * 3 + 3) / 4;

            // Populate the triangle indices
            for (u32 i = 0; i < packedTriangleCount; ++i)
            {
                m_geometry.meshletData.PushBack(triangleData[i]);
            }

            MeshletBounds bounds = MeshUtils::GenerateMeshletBounds(meshlet, vertices);

            m.center = bounds.center;
            m.radius = bounds.radius;

            m.coneAxis[0] = bounds.coneAxisS8[0];
            m.coneAxis[1] = bounds.coneAxisS8[1];
            m.coneAxis[2] = bounds.coneAxisS8[2];
            m.coneCutoff  = bounds.coneCutoffS8;

            m_geometry.meshlets.PushBack(m);
        }

        return meshlets.Size();
    }

}  // namespace C3D