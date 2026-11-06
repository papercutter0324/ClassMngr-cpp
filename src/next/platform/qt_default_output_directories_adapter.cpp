#include "next/platform/qt_default_output_directories_adapter.h"

#include <QDir>
#include <QStandardPaths>

#include <cstddef>
#include <string>

namespace
{
std::string toUtf8(const QString& path)
{
    const QByteArray encoded = path.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}
}

namespace ClassMngr::Next::Platform
{

Application::DefaultOutputDirectories
QtDefaultOutputDirectoriesAdapter::read() const
{
    QString home = QStandardPaths::writableLocation(
        QStandardPaths::HomeLocation
        );
    if (home.trimmed().isEmpty())
    {
        home = QDir::homePath();
    }
    return {
        .documents = toUtf8(QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation)),
        .home = toUtf8(home)
    };
}

} // namespace ClassMngr::Next::Platform
