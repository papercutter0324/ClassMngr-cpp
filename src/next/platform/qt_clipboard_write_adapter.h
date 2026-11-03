#pragma once

#include "next/application/clipboard_write_port.h"

namespace ClassMngr::Next::Platform
{

class QtClipboardWriteAdapter final : public Application::ClipboardWritePort
{
public:
    void writeText(const QString& text) const override;
};

} // namespace ClassMngr::Next::Platform
