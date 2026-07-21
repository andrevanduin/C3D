
#include "file.h"

#include <cstdio>

#include "logger/logger.h"

namespace C3D
{
    File::~File()
    {
        // Ensure that we always close the file if needed on destruction
        if (m_pFile)
        {
            fclose(m_pFile);
            m_pFile = nullptr;
            m_path.Destroy();
        }
    }

    bool File::Open(const String& path, const char* mode)
    {
        if (m_pFile)
        {
            // We already have an open file so let's close it first
            fclose(m_pFile);
            m_pFile = nullptr;
        }

        i32 error = fopen_s(&m_pFile, path.Data(), mode);
        if (error != 0 || !m_pFile)
        {
            ERROR_LOG("Failed to open file: '{}'.", path);
            return false;
        }

        // Store the path for later use
        m_path = path;

        return true;
    }

    bool File::ReadEntireFileIntoString(String& output) const
    {
        if (!m_pFile)
        {
            ERROR_LOG("File is not open.");
            return false;
        }

        // Get the entire size of the file in bytes
        auto size = Size();
        // Resize the string to have enough space for the entire contents of the file
        output.Reserve(size);
        // Prepare the string for reading
        output.PrepareForReadFromFile(size);
        // Read out the file
        fread(output.Data(), sizeof(char), size, m_pFile);
        // Check for errors
        u32 error = ferror(m_pFile);
        if (error != 0)
        {
            ERROR_LOG("Failed to read entire file with error: {}.", error);
            return false;
        }
        // Remove all null-terminators at the end of the string
        while (output.Last() == '\0')
        {
            output.RemoveLast();
        }

        return true;
    }

    bool File::IsReadUntilEnd() const { return fgetc(m_pFile) == -1; }

    u32 File::Size() const
    {
        // Seek to the end of the file
        fseek(m_pFile, 0, SEEK_END);
        // Get the current file pointer (which will be the number of bytes in the entire file)
        u32 size = ftell(m_pFile);
        // Seek back to the start of the file
        fseek(m_pFile, 0, SEEK_SET);
        // Finally return the size
        return size;
    }

}  // namespace C3D