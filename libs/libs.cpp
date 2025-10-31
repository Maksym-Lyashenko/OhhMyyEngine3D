#define _CRT_SECURE_NO_WARNINGS
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define VMA_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100) // unused parameter
#pragma warning(disable : 4189) // local variable initialized but not used
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

#include "vk_mem_alloc.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif