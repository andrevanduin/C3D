
#pragma once
#include <fmt/format.h>

#include <cstdint>
#include <thread>
#include <type_traits>

typedef unsigned char byte;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

static_assert(sizeof(u8) == 1, "Expected u8  to be 1 byte");
static_assert(sizeof(u16) == 2, "Expected u16 to be 2 bytes");
static_assert(sizeof(u32) == 4, "Expected u32 to be 4 bytes");
static_assert(sizeof(u64) == 8, "Expected u64 to be 8 bytes");

static_assert(sizeof(i8) == 1, "Expected i8  to be 1 byte");
static_assert(sizeof(i16) == 2, "Expected i16 to be 2 bytes");
static_assert(sizeof(i32) == 4, "Expected i32 to be 4 bytes");
static_assert(sizeof(i64) == 8, "Expected i64 to be 8 bytes");

constexpr u64 FNV_PRIME = 1099511628211;

template <typename T>
constexpr const char* TypeToString(T)
{
    return "";
}

template <>
constexpr const char* TypeToString(u8)
{
    return "u8";
}

/* @brief A Range, typically of memory. */
struct Range
{
    u64 offset;
    u64 size;
};

template <typename E>
constexpr auto ToUnderlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

/**
 * @brief Any id set to this value should be considered invalid,
 * and not actually pointing to a real object.
 */
#define INVALID_ID_U64 std::numeric_limits<u64>::max()
#define INVALID_ID std::numeric_limits<u32>::max()
#define INVALID_ID_U32 INVALID_ID
#define INVALID_ID_U16 std::numeric_limits<u16>::max()
#define INVALID_ID_U8 std::numeric_limits<u8>::max()

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#define C3D_PLATFORM_WINDOWS 1
#ifndef _WIN64
#error "64-bit is required on Windows!"
#endif
#elif defined(__linux__) || defined(__gnu_linux__)
// Linux OS
#define C3D_PLATFORM_LINUX 1
#if defined(__ANDROID__)
#define C3D_PLATFORM_ANDROID 1
#endif
#elif defined(__unix__)
// Catch anything unix
#define C3D_PLATFORM_UNIX 1
#elif defined(_POSIX_VERSION)
// Posix
#define KPLATFORM_POSIX 1
#elif __APPLE__
// Apple platforms
#define C3D_PLATFORM_APPLE 1
#else
#error "Unknown platform!"
#endif

#ifdef C3D_EXPORT
#ifdef _MSC_VER
#define C3D_API __declspec(dllexport)
#else
#define C3D_API __attribute__((visibility("default")))
#endif
#else
#ifdef _MSC_VER
#define C3D_API __declspec(dllimport)
#else
#define C3D_API
#endif
#endif

#ifdef _MSC_VER
#define C3D_INLINE __forceinline
#define C3D_NO_INLINE __declspec(noinline)
#else
#define C3D_INLINE static inline
#define C3D_NO_INLINE
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])
#endif

/** @brief Gets the number of bytes from amount of gibibytes (GiB) (amount * 1024*1024*1024) */
constexpr u64 GibiBytes(const u64 amount) { return amount * 1024 * 1024 * 1024; }
/** @brief Gets the number of bytes from amount of mebibytes (MiB) (amount * 1024*1024) */
constexpr u64 MebiBytes(const u64 amount) { return amount * 1024 * 1024; }
/** @brief Gets the number of bytes from amount of kibibytes (KiB) (amount * 1024) */
constexpr u64 KibiBytes(const u64 amount) { return amount * 1024; }

/** @brief Converts the provided number of bytes to gibibytes. */
constexpr u64 BytesToGibiBytes(const u64 amount) { return amount / 1024 / 1024 / 1024; }
/** @brief Converts the provided number of bytes to mebibytes. */
constexpr u64 BytesToMebiBytes(const u64 amount) { return amount / 1024 / 1024; }
/** @brief Converts the provided number of bytes to kibibytes. */
constexpr u64 BytesToKibiBytes(const u64 amount) { return amount / 1024; }

/** @brief Gets the number of bytes from amount of gigabytes (GB) (amount * 1000 * 1000 * 1000) */
constexpr u64 GigaBytes(const u64 amount) { return amount * 1000 * 1000 * 1000; }
/** @brief Gets the number of bytes from amount of megabytes (MB) (amount * 1000 * 1000) */
constexpr u64 MegaBytes(const u64 amount) { return amount * 1000 * 1000; }
/** @brief Gets the number of bytes from amount of kilobytes (KB) (amount * 1000) */
constexpr u64 KiloBytes(const u64 amount) { return amount * 1000; }

C3D_INLINE u64 GetAligned(const u64 operand, const u64 granularity) { return (operand + (granularity - 1)) & ~(granularity - 1); }

C3D_INLINE Range GetAlignedRange(const u64 offset, const u64 size, const u64 granularity)
{
    return { GetAligned(offset, granularity), GetAligned(size, granularity) };
}