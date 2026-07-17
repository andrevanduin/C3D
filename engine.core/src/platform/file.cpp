
#include "file.h"

namespace C3D
{
    FileV2::~FileV2()
    {
        // Ensure that we always close the file if needed on destruction
        if (m_pFile)
        {
            fclose(m_pFile);
            m_pFile = nullptr;
            m_path.Destroy();
        }
    }

    bool FileV2::Open(const String& path, const char* mode)
    {
        if (m_pFile)
        {
            // We already have an open file so let's close it first
            fclose(m_pFile);
            m_pFile = nullptr;
        }

        m_pFile = fopen(path.Data(), mode);
        if (!m_pFile)
        {
            ERROR_LOG("Failed to open file: '{}'.", path);
            return false;
        }

        // Store the path for later use
        m_path = path;

        return true;
    }

    bool FileV2::IsReadUntilEnd() { return fgetc(m_pFile) == -1; }

}  // namespace C3D