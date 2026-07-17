
#include "dds_deserializer.h"

#include "assets/types/dds_types.h"
#include "assets/types/texture_types.h"
#include "logger/logger.h"
#include "platform/file.h"

namespace C3D
{
    bool DDSDeserializer::Deserialize(const String& path, TextureAsset& asset) const
    {
        // Ensure we only parse DDS files
        if (!path.EndsWith(".dds"))
        {
            ERROR_LOG("Failed to deserialize file: '{}'. Not a DDS file.");
            return false;
        }

        // Ensure the asset contains a pointer to a valid buffer
        if (!asset.buffer || asset.bufferSize == 0)
        {
            ERROR_LOG("The asset does not contain a pointer to a valid buffer.");
            return false;
        }

        FileV2 file;
        // Open file for binary reading
        if (!file.Open(path, "rb"))
        {
            ERROR_LOG("Failed to deserialize file.");
            return false;
        }

        // Read the magic number
        u32 magic = 0;
        if (!file.Read(&magic))
        {
            ERROR_LOG("Failed to parse DDS file.");
            return false;
        }

        // Verify the magic number
        if (magic != DDS_MAGIC_NUMBER)
        {
            ERROR_LOG("Provided file: '{}' is not a valid DDS file.", path);
            return false;
        }

        // Read DDS header
        DDSHeader header = {};
        if (!file.Read(&header))
        {
            ERROR_LOG("Failed to read DDS header.");
            return false;
        }

        // Read DXT10 header if available
        DDSHeaderDXT10 headerDxt10 = {};
        if (header.ddspf.dwFourCC == 'DX10')
        {
            // We have a DXT10 header
            if (!file.Read(&headerDxt10))
            {
                ERROR_LOG("A DXT10 header was specified but it could not be read.");
                return false;
            }
        }

        // Verify header size
        if (header.dwSize != sizeof(header))
        {
            ERROR_LOG("DDS Header size is specified as '{}' but was expecting: '{}'.", header.dwSize, sizeof(header));
            return false;
        }

        // Verify pixel format header size
        if (header.ddspf.dwSize != sizeof(header.ddspf))
        {
            ERROR_LOG("DDS PixelFormat Header size is specified as '{}' but was expecting: '{}'.", header.ddspf.dwSize, sizeof(header.ddspf));
            return false;
        }

        // We don't support cubemaps or volumes (yet)
        if (header.dwCaps2 & (DDSCAPS2_CUBEMAP | DDSCAPS2_VOLUME))
        {
            ERROR_LOG("Failed to load DDS file. This DDS file stores an unsupported surface.");
            return false;
        }

        // We only support 2D Textures (for now)
        if (header.ddspf.dwFourCC == 'DX10' && headerDxt10.resourceDimension != D3D10ResourceDimesion::TEXTURE2D)
        {
            ERROR_LOG("Failed to load DDS file. This DDS file stores an unsupported texture type.");
            return false;
        }

        // Copy over the properties from the DDS header
        asset.width         = header.dwWidth;
        asset.height        = header.dwHeight;
        asset.mipLevelCount = header.dwMipMapCount;

        // Convert the DDS format to our own internal format
        asset.format = ConvertFormat(header, headerDxt10);
        if (asset.format == TextureFormat::UNDEFINED)
        {
            ERROR_LOG("Failed to parse to parse a supported TextureFormat.");
            return false;
        }

        // Determine the block size based on the format
        asset.blockSize = GetBlockSize(asset.format);
        // Determine the size of the entire texture
        asset.size = GetImageSizeBC(asset);

        // Ensure our buffer is large enough for all the data
        if (asset.bufferSize < asset.size)
        {
            ERROR_LOG("Buffer size is not large enough to store all DDS data.");
            return false;
        }

        // Read the actual image data
        if (!file.Read(asset.buffer, asset.size))
        {
            ERROR_LOG("Failed to read image data.");
            return false;
        }

        // Check if we read all the way until the end of the file
        if (!file.IsReadUntilEnd())
        {
            ERROR_LOG("Failed to parse entire file. Expected to be at the end of the file, but there is still data left.");
            return false;
        }

        return true;
    }

    TextureFormat DDSDeserializer::ConvertFormat(const DDSHeader& header, const DDSHeaderDXT10& headerDxt10) const
    {
        if (header.ddspf.dwFourCC == 'DXT1')
        {
            return TextureFormat::BC1_RGBA_UNORM_BLOCK;
        }
        if (header.ddspf.dwFourCC == 'DXT3')
        {
            return TextureFormat::BC2_UNORM_BLOCK;
        }
        if (header.ddspf.dwFourCC == 'DXT5')
        {
            return TextureFormat::BC3_UNORM_BLOCK;
        }
        if (header.ddspf.dwFourCC == 'DX10')
        {
            // TODO: We shouldn't just ignore the SRGB stuff!!
            switch (headerDxt10.dxgiFormat)
            {
                case DXGIFormat::BC1_UNORM:
                case DXGIFormat::BC1_UNORM_SRGB:
                    return TextureFormat::BC1_RGBA_UNORM_BLOCK;
                case DXGIFormat::BC2_UNORM:
                case DXGIFormat::BC2_UNORM_SRGB:
                    return TextureFormat::BC2_UNORM_BLOCK;
                case DXGIFormat::BC3_UNORM:
                case DXGIFormat::BC3_UNORM_SRGB:
                    return TextureFormat::BC3_UNORM_BLOCK;
                case DXGIFormat::BC4_SNORM:
                    return TextureFormat::BC4_SNORM_BLOCK;
                case DXGIFormat::BC4_UNORM:
                    return TextureFormat::BC4_UNORM_BLOCK;
                case DXGIFormat::BC5_UNORM:
                    return TextureFormat::BC5_UNORM_BLOCK;
                case DXGIFormat::BC6H_UF16:
                    return TextureFormat::BC6H_UFLOAT_BLOCK;
                case DXGIFormat::BC6H_SF16:
                    return TextureFormat::BC6H_SFLOAT_BLOCK;
                case DXGIFormat::BC7_UNORM:
                case DXGIFormat::BC7_UNORM_SRGB:
                    return TextureFormat::BC7_UNORM_BLOCK;
                default:
                    ERROR_LOG("Unknown TextureFormat: {} from DDS headers.", static_cast<u32>(headerDxt10.dxgiFormat));
                    return TextureFormat::UNDEFINED;
            }
        }

        ERROR_LOG("Failed to determine TextureFormat from DDS headers.");
        return TextureFormat::UNDEFINED;
    }

    u32 DDSDeserializer::GetBlockSize(TextureFormat format) const
    {
        switch (format)
        {
            // 1 byte per block (half a byte per pixel)
            case TextureFormat::BC1_RGBA_UNORM_BLOCK:
            case TextureFormat::BC4_SNORM_BLOCK:
            case TextureFormat::BC4_UNORM_BLOCK:
                return 8;
            // 2 bytes per block (1 byte per pixel)
            default:
                return 16;
        }
    }

    u32 DDSDeserializer::GetImageSizeBC(const TextureAsset& asset) const
    {
        u32 size   = 0;
        u32 width  = asset.width;
        u32 height = asset.height;

        for (u32 i = 0; i < asset.mipLevelCount; ++i)
        {
            size += ((width + 3) / 4) * ((height + 3) / 4) * asset.blockSize;

            width  = width > 1 ? width / 2 : 1;
            height = height > 1 ? height / 2 : 1;
        }
        return size;
    }

}  // namespace C3D