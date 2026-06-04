#pragma once

#include <string>

namespace trackcamhub
{

class ServiceRunner
{
public:
    static int run(const std::string& config_path);
};

} // namespace trackcamhub
