
#pragma once
#include "defines.h"

#define C3D_ASSERTIONS_ENABLED 1

#ifdef C3D_ASSERTIONS_ENABLED
#if _MSC_VER
#include <intrin.h>
#define DebugBreak() __debugbreak();
#else
#define DebugBreak() __builtin_trap()
#endif

void C3D_API ReportAssertionFailure(const char* expression, const char* message, const char* file, i32 line);
void C3D_API ReportAssertionFailure(const char* expression, const char* file, i32 line);

#define C3D_FAIL(msg)                                            \
    {                                                            \
        ReportAssertionFailure("FAIL", msg, __FILE__, __LINE__); \
        DebugBreak();                                            \
    }

#define C3D_NOT_IMPLEMENTED()                                               \
    {                                                                       \
        ReportAssertionFailure("Not implemented yet.", __FILE__, __LINE__); \
        DebugBreak();                                                       \
    }

#define C3D_ASSERT(expr)                                       \
    {                                                          \
        if (expr)                                              \
        {                                                      \
        }                                                      \
        else                                                   \
        {                                                      \
            ReportAssertionFailure(#expr, __FILE__, __LINE__); \
            DebugBreak();                                      \
        }                                                      \
    }

#define C3D_ASSERT_MSG(expr, message)                                   \
    {                                                                   \
        if (expr)                                                       \
        {                                                               \
        }                                                               \
        else                                                            \
        {                                                               \
            ReportAssertionFailure(#expr, message, __FILE__, __LINE__); \
            DebugBreak();                                               \
        }                                                               \
    }

#ifdef _DEBUG
#define C3D_ASSERT_DEBUG(expr)                                 \
    {                                                          \
        if (expr)                                              \
        {                                                      \
        }                                                      \
        else                                                   \
        {                                                      \
            ReportAssertionFailure(#expr, __FILE__, __LINE__); \
            DebugBreak();                                      \
        }                                                      \
    }

#define C3D_ASSERT_DEBUG_MSG(expr, message)                             \
    {                                                                   \
        if (expr)                                                       \
        {                                                               \
        }                                                               \
        else                                                            \
        {                                                               \
            ReportAssertionFailure(#expr, message, __FILE__, __LINE__); \
            DebugBreak();                                               \
        }                                                               \
    }
#else
#define C3D_ASSERT_DEBUG(expr)      // Strip from release builds
#define C3D_ASSERT_DEBUG_MSG(expr)  // Strip from release builds
#endif
#else
#define C3D_ASSERT(expr)               // Strip from build if asserts are disabled
#define C3D_ASSERT_MSG(expr, message)  // Strip from build if asserts are disabled
#define C3D_ASSERT_DEBUG(expr)         // Strip from build if asserts are disabled
#endif
