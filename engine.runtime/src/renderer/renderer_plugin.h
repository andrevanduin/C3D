
#pragma once
#include "assets/types/texture_types.h"
#include "defines.h"
#include "mesh.h"
#include "types.h"

namespace C3D
{
    struct Window;
    struct Geometry;

    class RendererPlugin
    {
    public:
        virtual ~RendererPlugin() {}

        virtual bool OnInit(const RendererPluginConfig& config) = 0;
        virtual void OnShutdown()                               = 0;

        /**
         * @brief Gets called after the OnRun() method for the main application has ran.
         * Useful since the user might create some resources during OnRun() which we need to do something with in the Renderer after.
         *
         * @param geometry The geometry that should be rendered (which is managed by the RenderSystem)
         * @return True if successful; false otherwise
         */
        virtual bool OnRun(const Geometry& geometry) = 0;

        /**
         * @brief Begins rendering the frame.
         * If this method returns false you should stop rendering the current frame.
         *
         * @param window The window you want begin rendering towards
         * @return True if successful; false otherwise
         */
        virtual bool Begin(Window& window) = 0;

        /**
         * @brief Ends rendering the frame.
         * This method must be called after Begin() and only if Begin() returned true.
         *
         * @param window The window you want to end the rendering for
         * @return True if successful; false otherwise
         */
        virtual bool End(Window& window) = 0;

        /**
         * @brief Submits the frame for rendering.
         * Everything that was previously added to the frame (between Begin() and End()) will get submitted for rendering.
         *
         * @param window The window you want to submit for
         * @return True if successful; false otherwise
         */
        virtual bool Submit(Window& window) = 0;

        /**
         * @brief Present the frame that was just submitted.
         *
         * @param window The window you want to present the image to
         * @return True if successful; false otherwise
         */
        virtual bool Present(Window& window) = 0;

        /**
         * @brief Method that must be called for every new window you create.
         * This method creates the renderer specific internals needed to render to the window.
         *
         * @param window The new window
         * @return True if successful; false otherwise
         */
        virtual bool OnCreateWindow(Window& window) = 0;

        /**
         * @brief Method that must be called whenever the window resizes.
         * This method updates the renderer specific internals to fit with the new window dimensions.
         *
         * @param window The window that was resized
         * @return True if successful; false otherwise
         */
        virtual bool OnResizeWindow(Window& window) = 0;

        /**
         * @brief Method that can be called to destroy the renderer specific internals for the provided window.
         *
         * @param window The window that needs to be destroyed
         */
        virtual void OnDestroyWindow(Window& window) = 0;

        /**
         * @brief Method used to upload geometry to the GPU.
         *
         * @param geometry The geometry that you want to upload
         * @return True if successful; false otherwise
         */
        virtual bool UploadGeometry(const Geometry& geometry) = 0;

        /**
         * @brief Method used to upload a texture asset to the GPU memory.
         *
         * @param texture The path to the texture
         * @return True if successful; false otherwise
         */
        virtual bool UploadTexture(const TextureAsset& asset) = 0;

        /**
         * @brief Method to generate draw command (randomly) for the provdided geometry.
         *
         * @param geometry The geometry you want to generate draw commands for
         * @return True if successful; false otherwise
         */
        virtual bool GenerateDrawCommands(const Geometry& geometry) = 0;

        /**
         * @brief Method to upload draw commands based on the provided Geometry and Draws.
         *
         * @param geometry The geometry you want to upload draw commands for
         * @param draws An array containing the actual draws that you want the GPU to execute
         * @return True if successful; false otherwise
         */
        virtual bool UploadDrawCommands(const Geometry& geometry, const DynamicArray<MeshDraw>& draws) = 0;

        /**
         * @brief Sets the viewport.
         *
         * @param x The x coordinate
         * @param y The y coordinate
         * @param width The width of the viewport
         * @param height The height of the viewport
         * @param minDepth The minimum depth of the viewport
         * @param maxDepth The maximum depth of the viewport
         */
        virtual void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) = 0;

        /**
         * @brief Sets the scissor.
         *
         * @param offsetX The x offset of the scissor
         * @param offsetY The y offset of the scissor
         * @param width The width of the scissor
         * @param height The height of the scissor
         */
        virtual void SetScissor(i32 offsetX, i32 offsetY, u32 width, u32 height) = 0;

        /**
         * @brief Sets a Camera to be used by the Renderer.
         *
         * @param camera The camera to be used
         */
        virtual void SetCamera(const Camera& camera) = 0;

        /**
         * @brief Sets the Sun Direction to be used by the Renderer.
         *
         * @param sunDirection The sun direction to be used
         */
        virtual void SetSunDirection(const vec3& sunDirection) = 0;

        /**
         * @brief Method that returns if the requested feature is supported by the current renderer backend.
         *
         * @param feature The feature you want to query
         * @return True if supported; false otherwise
         */
        virtual bool SupportsFeature(RendererSupportFlag feature) const = 0;

        /**
         * @brief Gets a pointer to some memory that represents the render backend's staging buffer.
         *
         * @return u8* A pointer to the scratch buffer memory
         */
        virtual u8* GetStagingBuffer() const = 0;

        /**
         * @brief Gets the size of the memory that represents the render backend's staging buffer.
         *
         * @return u32 The size of the scratch buffer in bytes
         */
        virtual u32 GetStagingBufferSize() const = 0;

    protected:
        RendererPluginType m_type;
    };
}  // namespace C3D