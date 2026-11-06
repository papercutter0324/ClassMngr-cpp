#pragma once

#include "next/application/default_output_directories_port.h"

namespace ClassMngr::Next::Platform
{

class QtDefaultOutputDirectoriesAdapter final
    : public Application::DefaultOutputDirectoriesPort
{
public:
    [[nodiscard]] Application::DefaultOutputDirectories read() const override;
};

} // namespace ClassMngr::Next::Platform
