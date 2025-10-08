
#pragma once
#include "defines.h"
#include "types.h"

namespace C3D
{
    struct Window;
    struct Geometry;

    class RendererPlugin
    {
    public:
        virtual bool OnInit(const RendererPluginConfig& config) = 0;
        virtual void OnShutdown()                               = 0;

        /**
         * @brief Creates renderer-specfic resources.
         * Useful for when there are resources that need to be created that can't yet be created during OnInit()
         *
         * @return True if successful; false otherwise
         */
        virtual bool CreateResources() = 0;

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

        virtual bool UploadGeometry(const Window& window, const Geometry& geometry) = 0;

        virtual bool GenerateDrawCommands(const Window& window, const Geometry& geometry) = 0;

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
         * @brief Method that returns if the requested feature is supported by the current renderer backend.
         *
         * @param feature The feature you want to query
         * @return True if supported; false otherwise
         */
        virtual bool SupportsFeature(RendererSupportFlag feature) const = 0;

    protected:
        RendererPluginType m_type;
    };
}  // namespace C3D