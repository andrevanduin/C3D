
#include "random.h"

namespace C3D
{
    RGB RandomEngine::GenerateColor()
    {
        std::uniform_real_distribution distribution(0.0f, 1.0f);
        return { distribution(m_generator), distribution(m_generator), distribution(m_generator) };
    }

    String RandomEngine::GenerateString(u32 minLength, u32 maxLength)
    {
        // Generate a random length between our min and max
        u32 length = Generate(minLength, maxLength);
        // Create our string and reserve enough space
        String randomStr;
        randomStr.Reserve(length);
        // Generate random chars between '0' and 'z' length times
        constexpr auto firstChar = static_cast<u32>('0');
        constexpr auto lastChar  = static_cast<u32>('z');

        for (u32 i = 0; i < length; i++)
        {
            randomStr.Append(Generate(firstChar, lastChar));
        }
        return randomStr;
    }
}  // namespace C3D