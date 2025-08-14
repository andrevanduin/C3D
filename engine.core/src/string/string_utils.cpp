
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
}
