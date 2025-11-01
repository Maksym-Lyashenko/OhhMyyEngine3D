#pragma once
#include <string>
#include <filesystem>

namespace Core::Str
{

    inline std::string assetNameFromPath(const std::string &path)
    {
        std::filesystem::path p(path);
        return p.stem().string(); // file name
    }

} // namespace Core::Str
