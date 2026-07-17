
#pragma once
#include "defines.h"
#include "string/string.h"

namespace C3D
{
    class C3D_API FileV2
    {
    public:
        FileV2() = default;

        FileV2(const FileV2&) = delete;
        FileV2(FileV2&&)      = delete;

        FileV2& operator=(const FileV2&) = delete;
        FileV2& operator=(FileV2&&)      = delete;

        ~FileV2();

        /**
         * @brief Opens a file with the provided path and mode.
         * @param path The path to the file
         * @param mode The mode to use to open the file
         */
        bool Open(const String& path, const char* mode);

        /**
         * @brief Returns true if the file was read until it's end
         */
        bool IsReadUntilEnd();

        /**
         * @brief Read data from a file that has been opened.
         * @param pData A pointer (of type T) to where the data should be stored
         * @param count The number of items of type T we should read (default == 1)
         */
        template <typename T>
        bool Read(T* pData, u32 count = 1)
        {
            if (fread(pData, sizeof(T), count, m_pFile) != count)
            {
                ERROR_LOG("Failed to read data from file: '{}'.", m_path);
                return false;
            }
            return true;
        }

    private:
        FILE* m_pFile = nullptr;
        String m_path;
    };
}  // namespace C3D