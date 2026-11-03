#include "next/platform/qt_clipboard_write_adapter.h"

#include <QClipboard>
#include <QGuiApplication>

namespace ClassMngr::Next::Platform
{

void QtClipboardWriteAdapter::writeText(const QString& text) const
{
    QGuiApplication::clipboard()->setText(text);
}

} // namespace ClassMngr::Next::Platform
