
#pragma once
#include "defines.h"
#include "types.h"

namespace C3D
{
    struct Window;

    class RendererPlugin
    {
    public:
        virtual bool OnInit(const RendererPluginConfig& config) = 0;
        virtual void OnShutdown()                               = 0;

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
         * @brief Method that can be called to destroy the renderer specific internals for the provided window.
         *
         * @param window The window that needs to be destroyed
         */
        virtual void OnDestroyWindow(Window& window) = 0;

    protected:
        RendererPluginType m_type;
    };
}  // namespace C3D