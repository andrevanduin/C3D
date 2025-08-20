
#pragma once

#include "defines.h"
#include "string/string.h"

namespace C3D::StringUtils
{
    /**
     * @brief Builds a string from the format and the provided arguments.
     * Internally uses fmt::format to build out the string.
     */
    template <class Allocator, typename... Args>
    static BasicString<Allocator> FromFormat(Allocator* allocator, const char* format, Args&&... args)
    {
        BasicString<Allocator> buffer(allocator);
        fmt::format_to(std::back_inserter(buffer), format, std::forward<Args>(args)...);
        return buffer;
    }

    /**
     * @brief Compares two strings case-sensitive
     *
     * @param left The string you want to compare
     * @param right The string you want to compare to
     * @param length The maximum number of characters we should compare.
     *	Default is -1 which checks the entire string
     *
     * @return a bool indicating if the strings match case-sensitively
     */
    C3D_API bool Equals(const char* a, const char* b, i32 length = -1);

    /**
     * @brief Compares two strings case-insensitive
     *
     * @param left The string you want to compare
     * @param right The string you want to compare to
     * @param length The maximum number of characters we should compare.
     * Default is -1 which checks the entire string
     *
     * @return a bool indicating if the string match case-insensitively
     */
    C3D_API bool IEquals(const char* a, const char* b, i32 length = -1);

    /**
     * @brief Tries to parse an i32 from the provided string staring at start.
     *
     * @param s The pointer to the start of the string
     * @param end An output pointer to remainder of the string after the parsed int
     * @return The parsed i32
     */
    C3D_API i32 ParseI32(const char* s, const char** end);

    /**
     * @brief Tries to parse a f32 from the provided string staring at start.
     *
     * @param s The pointer to the start of the string
     * @param end An output pointer to remainder of the string after the parsed int
     * @return The parsed f32
     */
    C3D_API f32 ParseF32(const char* s, const char** end);

    /**
     * @brief Skips whitespace in string provided
     *
     * @param s The pointer to the start of a string
     * @return A pointer to the first non-whitespace character (or the end of the string)
     */
    C3D_API const char* SkipWhitespace(const char* s);

    static float parseFloat(const char* s, const char** end)
    {
        static const double digits[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        static const double powers[] = { 1e0,   1e+1,  1e+2,  1e+3,  1e+4,  1e+5,  1e+6,  1e+7,  1e+8,  1e+9,  1e+10, 1e+11,
                                         1e+12, 1e+13, 1e+14, 1e+15, 1e+16, 1e+17, 1e+18, 1e+19, 1e+20, 1e+21, 1e+22 };

        // skip whitespace
        while (*s == ' ' || *s == '\t') s++;

        // read sign
        double sign = (*s == '-') ? -1 : 1;
        s += (*s == '-' || *s == '+');

        // read integer part
        double result = 0;
        int power     = 0;

        while (unsigned(*s - '0') < 10)
        {
            result = result * 10 + digits[*s - '0'];
            s++;
        }

        // read fractional part
        if (*s == '.')
        {
            s++;

            while (unsigned(*s - '0') < 10)
            {
                result = result * 10 + digits[*s - '0'];
                s++;
                power--;
            }
        }

        // read exponent part
        if ((*s | ' ') == 'e')
        {
            s++;

            // read exponent sign
            int expsign = (*s == '-') ? -1 : 1;
            s += (*s == '-' || *s == '+');

            // read exponent
            int exppower = 0;

            while (unsigned(*s - '0') < 10)
            {
                exppower = exppower * 10 + (*s - '0');
                s++;
            }

            // done!
            power += expsign * exppower;
        }

        // return end-of-string
        *end = s;

        // note: this is precise if result < 9e15
        // for longer inputs we lose a bit of precision here
        if (unsigned(-power) < sizeof(powers) / sizeof(powers[0]))
            return float(sign * result / powers[-power]);
        else if (unsigned(power) < sizeof(powers) / sizeof(powers[0]))
            return float(sign * result * powers[power]);
        else
            return float(sign * result * pow(10.0, power));
    }

    /**
     * @brief Splits a CString on the provided delimiter
     *
     * @param string The CString that you want to split
     * @param delimiter The char that you want to split the string on
     * @param trimEntries An optional bool to enable the split strings to be trimmed (no started and ending whitespace)
     * @param skipEmpty An optional bool to enable skipping empty strings in the split array
     */
    template <u64 CCapacity, u64 OutputCapacity = CCapacity>
    C3D_API DynamicArray<CString<OutputCapacity>> Split(const CString<CCapacity>& string, char delimiter, const bool trimEntries = true,
                                                        const bool skipEmpty = true)
    {
        DynamicArray<CString<OutputCapacity>> elements;
        CString<OutputCapacity> current;

        const auto size = string.Size();
        for (u64 i = 0; i < size; i++)
        {
            if (string[i] == delimiter)
            {
                if (!skipEmpty || !current.Empty())
                {
                    if (trimEntries) current.Trim();

                    elements.PushBack(current);
                    current.Clear();
                }
            }
            else
            {
                current.Append(string[i]);
            }
        }

        if (!current.Empty())
        {
            if (trimEntries) current.Trim();
            elements.PushBack(current);
        }
        return elements;
    }

    /**
     * @brief Checks if provided string is empty or only contains whitespace
     *
     * @param string The string that you want to check.
     */
    template <u64 CCapacity>
    C3D_API bool IsEmptyOrWhitespaceOnly(const CString<CCapacity>& string)
    {
        // We can stop early if our string does not contain any characters
        if (string[0] == '\0') return true;
        // Otherwise we parse all chars in the string and check if they are all whitespace
        for (const auto c : string)
        {
            // If we find a non-whitespace character we stop our search
            if (!std::isspace(c)) return false;
        }
        return true;
    }

    C3D_API String Join(const char** array, u32 size, char delimiter);

    template <class T, class Allocator>
    C3D_API String Join(const DynamicArray<T, Allocator>& array, char delimiter)
    {
        String result;

        const auto size = array.Size();
        for (u32 i = 0; i < size; i++)
        {
            result += String(array[i]);
            if (i != size - 1)
            {
                result += delimiter;
            }
        }
        return result;
    }

    template <class T, class Allocator>
    C3D_API String Join(const DynamicArray<T, Allocator>& array, const String& delimiter)
    {
        String result;

        const auto size = array.Size();
        for (u32 i = 0; i < size; i++)
        {
            result += String(array[i]);
            if (i != size - 1)
            {
                result += delimiter;
            }
        }
        return result;
    }
}  // namespace C3D::StringUtils