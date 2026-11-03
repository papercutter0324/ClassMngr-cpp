#pragma once

#include <QString>

namespace ClassMngr::Next::Application
{

class ClipboardWritePort
{
public:
    virtual ~ClipboardWritePort() = default;

    virtual void writeText(const QString& text) const = 0;
};

} // namespace ClassMngr::Next::Application
