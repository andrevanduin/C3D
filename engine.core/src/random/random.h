
#pragma once
#include <random>

#include "colors.h"
#include "containers/array.h"
#include "containers/dynamic_array.h"
#include "math/math_types.h"
#include "string/string.h"

namespace C3D
{
    class C3D_API RandomEngine
    {
    public:
        explicit RandomEngine(Array<u32, 8> seedData = GenerateSeedData())
        {
            std::seed_seq seedSeq(seedData.begin(), seedData.end());
            m_generator.seed(seedSeq);
        }

        template <typename T>
        [[nodiscard]] std::enable_if_t<std::is_integral_v<T>, T> Generate(const T low, const T high)
        {
            std::uniform_int_distribution distribution(low, high);
            return distribution(m_generator);
        }

        template <typename T>
        [[nodiscard]] std::enable_if_t<std::is_integral_v<T>, DynamicArray<T>> GenerateMultiple(const int amount, const T low, const T high)
        {
            std::uniform_int_distribution distribution(low, high);
            DynamicArray<T> values;
            values.Resize(amount);
            std::generate(values.begin(), values.end(), [&] { return distribution(m_generator); });
            return values;
        }

        template <typename T>
        [[nodiscard]] std::enable_if_t<std::is_floating_point_v<T>, T> Generate(const T low, const T high)
        {
            std::uniform_real_distribution distribution(low, high);
            return distribution(m_generator);
        }

        template <typename T>
        [[nodiscard]] std::enable_if_t<std::is_floating_point_v<T>, DynamicArray<T>> GenerateMultiple(const int amount, const T low, const T high)
        {
            std::uniform_real_distribution distribution(low, high);
            DynamicArray<float> values(amount);
            std::generate(values.begin(), values.end(), [&] { return distribution(m_generator); });
            return values;
        }

        /**
         * @brief Generate a RGB color
         *
         * @return RGB The RGB containing the generated color
         */
        RGB GenerateColor();

        /**
         * @brief Generates a random string of [minLength, maxLength] alphanumeric characters.
         *
         * @param minLength The minimum length of the string
         * @param maxLength The maximum length of the string
         * @return String The generated string
         */
        String GenerateString(u32 minLength, u32 maxLength);

    private:
        std::mt19937 m_generator;

        static Array<uint32_t, 8> GenerateSeedData()
        {
            Array<u32, 8> randomData{};
            std::random_device randomSource;
            std::generate(randomData.begin(), randomData.end(), std::ref(randomSource));
            return randomData;
        }
    };

    static inline RandomEngine Random = RandomEngine();
}  // namespace C3D