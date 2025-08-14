
#include "asserts/asserts.h"

#include <iostream>

void ReportAssertionFailure(const char* expression, const char* message, const char* file, i32 line)
{
    std::cout << file << "(" << line << "): Assertion failed: '" << expression << "' with message: '" << message << "'\n";
}

void C3D_API ReportAssertionFailure(const char* expression, const char* file, i32 line)
{
    std::cout << file << "(" << line << "): Assertion failed: '" << expression << "'\n";
}
