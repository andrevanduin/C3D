
#include "gltf_asset_types.h"

#include "platform/file_system.h"
#include "time/scoped_timer.h"

namespace C3D
{
    const u8* GLTFBuffer::GetData(u64 offset) const { return data + offset; }

    bool GLTFBuffer::ReadFromDisk(const String& basePath)
    {
        ScopedTimer timer(String::FromFormat("Reading buffer: '{}' from disk.", uri));

        const String fullPath = String::FromFormat("{}/{}", basePath, uri);

        FILE* file = fopen(fullPath.Data(), "rb");
        if (!file)
        {
            ERROR_LOG("Failed to open: '{}'.", fullPath);
            return false;
        }

        // Allocate enough bytes to store the buffer data
        data = Memory.Allocate<u8>(MemoryType::Scene, byteLength);

        // Read the data from the file
        u64 bytesRead = fread(data, sizeof(u8), byteLength, file);

        // Ensure we read enough data
        if (bytesRead != byteLength)
        {
            ERROR_LOG("Failed to read: '{}'. Expected: {} bytes but got: {} bytes.", fullPath, byteLength, bytesRead);
            fclose(file);
            Memory.Free(data);
            return false;
        }

        // Finally close our file
        fclose(file);

        return true;
    }

    void GLTFBuffer::Destroy()
    {
        if (data)
        {
            Memory.Free(data);
            data = nullptr;
        }
    }

    void GLTFNode::TransformLocal(f32* outMatrix) const
    {
        if (hasMatrix)
        {
            std::memcpy(outMatrix, matrix, sizeof(f32) * 16);
        }
        else
        {
            // We need to manually create a matrix from translation, rotation and scale
            f32 tx = translation[0];
            f32 ty = translation[1];
            f32 tz = translation[2];

            f32 qx = rotation[0];
            f32 qy = rotation[1];
            f32 qz = rotation[2];
            f32 qw = rotation[3];

            f32 sx = scale[0];
            f32 sy = scale[1];
            f32 sz = scale[2];

            outMatrix[0] = (1 - 2 * qy * qy - 2 * qz * qz) * sx;
            outMatrix[1] = (2 * qx * qy + 2 * qz * qw) * sx;
            outMatrix[2] = (2 * qx * qz - 2 * qy * qw) * sx;
            outMatrix[3] = 0.f;

            outMatrix[4] = (2 * qx * qy - 2 * qz * qw) * sy;
            outMatrix[5] = (1 - 2 * qx * qx - 2 * qz * qz) * sy;
            outMatrix[6] = (2 * qy * qz + 2 * qx * qw) * sy;
            outMatrix[7] = 0.f;

            outMatrix[8]  = (2 * qx * qz + 2 * qy * qw) * sz;
            outMatrix[9]  = (2 * qy * qz - 2 * qx * qw) * sz;
            outMatrix[10] = (1 - 2 * qx * qx - 2 * qy * qy) * sz;
            outMatrix[11] = 0.f;

            outMatrix[12] = tx;
            outMatrix[13] = ty;
            outMatrix[14] = tz;
            outMatrix[15] = 1.f;
        }
    }

    void GLTFNode::TransformWorld(f32* outMatrix) const
    {
        f32* localMatrix = outMatrix;
        TransformLocal(localMatrix);

        GLTFNode* parentNode = parent;

        while (parentNode)
        {
            f32 parentMatrix[16];
            parentNode->TransformLocal(parentMatrix);

            for (u32 i = 0; i < 4; ++i)
            {
                f32 l0 = localMatrix[i * 4 + 0];
                f32 l1 = localMatrix[i * 4 + 1];
                f32 l2 = localMatrix[i * 4 + 2];

                f32 r0 = l0 * parentMatrix[0] + l1 * parentMatrix[4] + l2 * parentMatrix[8];
                f32 r1 = l0 * parentMatrix[1] + l1 * parentMatrix[5] + l2 * parentMatrix[9];
                f32 r2 = l0 * parentMatrix[2] + l1 * parentMatrix[6] + l2 * parentMatrix[10];

                localMatrix[i * 4 + 0] = r0;
                localMatrix[i * 4 + 1] = r1;
                localMatrix[i * 4 + 2] = r2;
            }

            localMatrix[12] += parentMatrix[12];
            localMatrix[13] += parentMatrix[13];
            localMatrix[14] += parentMatrix[14];

            parentNode = parentNode->parent;
        }
    }

    const GLTFAccessor* GLTFAsset::FindAccessor(const GLTFMeshPrimitive& primitive, const String& name) const
    {
        u32 index = 0;
        if (!primitive.attributes.GetPropertyValueByName(name, index))
        {
            return nullptr;
        }
        return &accessors[index];
    }

    bool GLTFAsset::LoadAllBuffers()
    {
        for (auto& buffer : buffers)
        {
            // Ensure our buffer is loaded
            if (!buffer.IsLoaded())
            {
                if (!buffer.ReadFromDisk(root))
                {
                    ERROR_LOG("Could not read accessor data because the underlying buffer could not be read from disk.");
                    return false;
                }
            }
        }

        return true;
    }

    static u64 GetSourceData(const u8* source, u64 sourceStride)
    {
        u64 data = 0;
        switch (sourceStride)
        {
            case 2:
                data = *reinterpret_cast<const u16*>(source);
                break;
            case 4:
                data = *reinterpret_cast<const u32*>(source);
                break;
            default:
                C3D_FAIL("Invalid sourceStride: {}.", sourceStride);
        }
        return data;
    }

    bool GLTFAsset::UnpackIndexData(void* destination, u64 destinationElementSize, const GLTFAccessor& accessor) const
    {
        // Get the associated bufferView
        const auto& bufferView = bufferViews[accessor.bufferView];
        // Get the associated buffer
        auto& buffer = buffers[bufferView.buffer];

        // Get the source element size
        u64 sourceElementSize = accessor.elementSize;
        // Get the source size
        u64 sourceSize = bufferView.byteLength;

        // Get the source data
        const u8* source = buffer.GetData(bufferView.byteOffset);

        // If the elements in our destination are the same size as in our source so we can simply memcpy them
        if (destinationElementSize == sourceElementSize)
        {
            std::memcpy(destination, source, sourceSize);
            return true;
        }

        // Get then number of elements in the source
        u32 numElements = accessor.count;

        switch (destinationElementSize)
        {
            case 2:
            {
                for (u32 i = 0; i < numElements; ++i, source += sourceElementSize)
                {
                    static_cast<u16*>(destination)[i] = static_cast<u16>(GetSourceData(source, sourceElementSize));
                }
                break;
            }
            case 4:
            {
                for (u32 i = 0; i < numElements; ++i, source += sourceElementSize)
                {
                    static_cast<u32*>(destination)[i] = static_cast<u32>(GetSourceData(source, sourceElementSize));
                }
                break;
            }
        }

        return true;
    }

    bool GLTFAsset::UnpackFloats(f32* destination, const GLTFAccessor* accessor) const
    {
        // Get the associated bufferView
        const auto& bufferView = bufferViews[accessor->bufferView];
        // Get the associated buffer
        auto& buffer = buffers[bufferView.buffer];
        // Copy the data over with simply memcpy
        std::memcpy(destination, buffer.GetData(bufferView.byteOffset), bufferView.byteLength);
        return true;
    }

}  // namespace C3D