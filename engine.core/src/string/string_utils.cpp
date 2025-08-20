
#include "string_utils.h"

namespace C3D
{
    bool StringUtils::Equals(const char* a, const char* b, const i32 length)
    {
        if (length == -1)
        {
            return strcmp(a, b) == 0;
        }
        return strncmp(a, b, length) == 0;
    }

    bool StringUtils::IEquals(const char* a, const char* b, i32 length)
    {
        if (length == -1)
        {
#ifdef C3D_PLATFORM_WINDOWS
            return _strcmpi(a, b) == 0;
#elif defined(C3D_PLATFORM_LINUX)
            return strcasecmp(a, b) == 0;
#endif
        }
        else
        {
#ifdef C3D_PLATFORM_WINDOWS
            return _strnicmp(a, b, length) == 0;
#elif defined(C3D_PLATFORM_LINUX)
            return strncasecmp(a, b, length) == 0;
#endif
        }
    }

    String StringUtils::Join(const char** array, u32 size, char delimiter)
    {
        String result;

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

    i32 StringUtils::ParseI32(const char* s, const char** end)
    {
        // Skip all whitespace
        while (*s == ' ' || *s == '\t') s++;

        // Read the sign bit
        int sign = (*s == '-');
        s += (*s == '-' || *s == '+');

        u32 result = 0;

        for (;;)
        {
            if (unsigned(*s - '0') < 10)
            {
                result = result * 10 + (*s - '0');
            }
            else
            {
                break;
            }
            s++;
        }

        // Return end-of-string for further processing
        *end = s;

        return sign ? -i32(result) : i32(result);
    }

    f32 StringUtils::ParseF32(const char* s, const char** end)
    {
        constexpr f64 digits[]  = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        constexpr f64 powers[]  = { 1e0,   1e+1,  1e+2,  1e+3,  1e+4,  1e+5,  1e+6,  1e+7,  1e+8,  1e+9,  1e+10, 1e+11,
                                    1e+12, 1e+13, 1e+14, 1e+15, 1e+16, 1e+17, 1e+18, 1e+19, 1e+20, 1e+21, 1e+22 };
        constexpr u32 numPowers = ARRAY_SIZE(powers);

        // Skip white-space
        s = StringUtils::SkipWhitespace(s);

        // Read the sign
        double sign = (*s == '-') ? -1 : 1;
        s += (*s == '-' || *s == '+');

        // Read integer part
        f64 result = 0;
        i32 power  = 0;

        while (static_cast<u32>(*s - '0') < 10)
        {
            result = result * 10 + digits[*s - '0'];
            s++;
        }

        // Read the fractional part
        if (*s == '.')
        {
            s++;

            while (static_cast<u32>(*s - '0') < 10)
            {
                result = result * 10 + digits[*s - '0'];
                s++;
                power--;
            }
        }

        // Read the exponent part
        if ((*s | ' ') == 'e')
        {
            s++;

            // Read exponent sign
            i32 expsign = (*s == '-') ? -1 : 1;
            s += (*s == '-' || *s == '+');

            // Read exponent
            i32 exppower = 0;

            while (static_cast<u32>(*s - '0') < 10)
            {
                exppower = exppower * 10 + (*s - '0');
                s++;
            }

            // Done!
            power += expsign * exppower;
        }

        // Return end-of-string
        *end = s;

        // NOTE: this is precise if result < 9e15 (for longer inputs we lose a bit of precision here)
        if (static_cast<u32>(-power) < numPowers)
        {
            return static_cast<f32>(sign * result / powers[-power]);
        }
        else if (static_cast<u32>(power) < numPowers)
        {
            return static_cast<f32>(sign * result * powers[power]);
        }
        else
        {
            return static_cast<f32>(sign * result * pow(10.0, power));
        }
    }

    const char* StringUtils::SkipWhitespace(const char* s)
    {
        while (*s == ' ' || *s == '\t') s++;

        return s;
    }
}  // namespace C3D
