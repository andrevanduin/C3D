
#pragma once
#include <fstream>

#include "defines.h"
#include "string/cstring.h"
#include "string/string.h"

namespace C3D
{
    enum FileModes : u8
    {
        FileModeRead   = 0x1,
        FileModeWrite  = 0x2,
        FileModeBinary = 0x4,
    };

    class C3D_API File
    {
    public:
        File();
        File(const File& other) = delete;
        File(File&& other)      = delete;

        File& operator=(const File& other) = delete;
        File& operator=(File&& other)      = delete;

        ~File();

        [[nodiscard]] static bool Exists(const String& path);

        bool Open(const String& path, u8 mode);

        bool Close();

        bool ReadLine(String& line, char delimiter = '\n');

        bool WriteLine(const String& line);

        template <class... Args>
        bool WriteLine(const char* format, Args&&... args)
        {
            if (!isValid) return false;

            fmt::vformat_to_n(m_buffer, 512, format, fmt::make_format_args(args...));

            m_file << m_buffer << "\n";
            // Flush to ensure that we don't have data loss if the engine crashes before closing the file
            m_file.flush();
            return true;
        }

        template <u64 Capacity>
        bool WriteLine(const CString<Capacity>& line)
        {
            if (!isValid) return false;
            m_file << line << "\n";
            // Flush to ensure that we don't have data loss if the engine crashes before closing the file
            m_file.flush();
            return true;
        }

        bool Read(u64 dataSize, void* outData, u64* outBytesRead);

        bool Read(String& str);

        template <u64 CCapacity>
        bool Read(CString<CCapacity>& str)
        {
            u64 size;
            if (!Read(&size)) return false;

            // No need to keep reading if this is 0
            if (size == 0) return true;

            // Check if the size of the string will fit our provided CString
            if (size >= CCapacity) return false;

            // Get the string data
            if (!Read(str.Data(), size + 1)) return false;

            // Ensure null-termination character
            str[size] = '\0';

            return true;
        }

        template <typename T>
        bool Read(T* data, u64 count = 1)
        {
            static_assert(!std::is_pointer_v<T>, "T must not be a pointer.");
            static_assert(!std::is_array_v<T>, "T must not be an array. Pass a pointer to the first element instead.");

            if (!isValid) return false;

            m_file.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(sizeof(T) * count));
            bytesRead += m_file.gcount();
            return true;
        }

        template <typename T>
        bool Read(DynamicArray<T>& data)
        {
            static_assert(std::is_trivial_v<T>, "This method can only be used with simple types.");

            u64 size;
            if (!Read(&size)) return false;

            // No need to keep reading if this is 0
            if (size == 0) return true;

            data.Resize(size);
            if (!Read(data.GetData(), size)) return false;

            return true;
        }

        bool ReadAll(char* outBytes, u64* outBytesRead);
        bool ReadAll(String& outChars);

        // Deprecated
        bool Write(u64 dataSize, const void* data, u64* outBytesWritten);

        template <u64 CCapacity>
        bool Write(const CString<CCapacity>& str)
        {
            if (!isValid) return false;

            // Write out the size
            const auto size = str.Size();
            if (!Write(&size)) return false;

            if (size != 0)
            {
                // Write all chars up-to size and add 1 to account for the null-terminating character
                if (!Write(str.Data(), size + 1)) return false;
            }

            return true;
        }

        bool Write(const String& str);

        template <typename T>
        bool Write(const T* data, u64 count = 1)
        {
            static_assert(!std::is_pointer_v<T>, "T must not be a pointer.");
            static_assert(!std::is_array_v<T>, "T must not be an array. Pass a pointer to the first element instead.");

            if (!isValid) return false;

            // Get the position of the cursor at the start
            const auto before = m_file.tellp();
            // Write all the bytes
            m_file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(sizeof(T) * count));
            // Flush to ensure that the bytes are written even if the engine crashes before closing this file
            m_file.flush();
            // Check the position after writing
            const auto after = m_file.tellp();
            // The total bytes written will be equal to the current cursor position - the starting cursor position
            bytesWritten += after - before;

            return true;
        }

        template <typename T>
        bool Write(const DynamicArray<T>& data)
        {
            // Write out the size
            const u64 size = data.Size();
            if (!Write(&size)) return false;
            // Write out the elements in the array (if there are any)
            if (size != 0)
            {
                if (!Write(data.GetData(), size)) return false;
            }
            return true;
        }

        bool Size(u64* outSize = nullptr);

        [[nodiscard]] u64 GetSize() const { return m_size; }

        bool isValid;
        u64 bytesWritten;
        u64 bytesRead;

        String currentPath;

    private:
        u64 m_size;
        std::fstream m_file;

        char m_buffer[512];
    };

    class C3D_API FileSystem
    {
    public:
        static String DirectoryFromPath(const String& path);

        static String FileNameFromPath(const String& path, bool includeExtension = false);
    };
}  // namespace C3D