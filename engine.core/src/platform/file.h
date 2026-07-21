
#pragma once
#include <cstdio>

#include "defines.h"
#include "logger/logger.h"
#include "string/string.h"


namespace C3D
{
    class C3D_API File
    {
    public:
        File() = default;

        File(const File&) = delete;
        File(File&&)      = delete;

        File& operator=(const File&) = delete;
        File& operator=(File&&)      = delete;

        ~File();

        /**
         * @brief Opens a file with the provided path and mode.
         * @param path The path to the file
         * @param mode The mode to use to open the file
         */
        bool Open(const String& path, const char* mode);

        /**
         * @brief Returns true if the file was read until it's end
         */
        bool IsReadUntilEnd() const;

        /**
         * @brief Gets the size of the file in bytes. Useful if you want to allocate enough space to read the entire file in one go.
         *
         * @return u32 The size of the file in bytes
         */
        u32 Size() const;

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

        template <typename T>
        bool Write(T* pData, u32 count = 1)
        {
            if (fwrite(pData, sizeof(T), count, m_pFile) != count)
            {
                ERROR_LOG("Failed to write data to file: '{}'.", m_path);
                return false;
            }
            return true;
        }

        /**
         * @brief Reads the entire contents of the file into the provided string.
         *
         * @param output A reference to the output string (will be resized to fit the content)
         * @return True if succesful; false otherwise
         */
        bool ReadEntireFileIntoString(String& output) const;

    private:
        FILE* m_pFile = nullptr;
        String m_path;
    };
}  // namespace C3D