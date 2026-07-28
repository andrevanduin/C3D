
#include "gltf_extensions.h"

#include "gltf_extension.h"

namespace C3D
{
    void CleanupGLTFExtension(GLTFExtension& extension)
    {
        if (extension.data)
        {
            switch (extension.type)
            {
                case GLTFExtensionType::LightsPunctual:
                {
                    auto& ext = *static_cast<GLTFLightsPunctualExtension*>(extension.data);
                    ext.lights.Destroy();
                    break;
                }
                default:
                    break;
            }

            Memory.Delete(extension.data);
            extension.data = nullptr;
        }
    }

}  // namespace C3D